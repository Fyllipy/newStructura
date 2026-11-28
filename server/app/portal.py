"""Loja, area do cliente e cobranca. Mesmo banco/usuarios/licencas do licenciamento.

- /shop/*     publico: planos, config e compra (cliente novo ou logado).
- /portal/*   area do cliente (e-mail + senha + 2FA; o login do site NAO vincula dispositivo).
- /admin/plans  tabela de precos (painel admin).
- /webhook/payment  Stripe (assinatura verificada; idempotente).
O numero do cartao nunca passa por aqui (Stripe Payment Element no navegador).
"""
from __future__ import annotations
import hmac
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Request
from pydantic import BaseModel, StringConstraints
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from . import billing, crypto, totp
from .config import settings
from .db import SessionLocal, get_db
from .identity import is_valid_cpf, is_valid_email, normalize_cpf, normalize_email
from .main import (_audit, _check_credentials, _check_totp, _extract_ip, _fail, _register_failure,
                   get_current_admin)
from .models import License, Order, Payment, Plan, User
from .payments import PaymentError, get_provider
from .schemas import Credentials
from .timeutil import iso, iso_ts, utcnow
from . import ratelimit

router = APIRouter()
Code6 = Annotated[str, StringConstraints(min_length=6, max_length=6)]


def seed_plans() -> None:
    db = SessionLocal()
    try:
        billing.seed_plans(db)
    finally:
        db.close()


def _provider():
    try:
        return get_provider()
    except PaymentError:
        raise HTTPException(status_code=503, detail="PAYMENTS_UNAVAILABLE")


def _plan_json(p: Plan) -> dict:
    return {"code": p.code, "name": p.name, "period_days": p.period_days, "price_cents": p.price_cents,
            "active": p.active}


# =============================== vitrine / compra ===============================
@router.get("/shop/plans")
def shop_plans(db: Session = Depends(get_db)):
    plans = db.scalars(select(Plan).order_by(Plan.period_days)).all()
    return [_plan_json(p) for p in plans if billing.sellable(p)]


@router.get("/shop/config")
def shop_config():
    return {"publishable_key": settings.STRIPE_PUBLISHABLE_KEY, "currency": "brl"}


class CheckoutRequest(BaseModel):
    plan_code: Annotated[str, StringConstraints(min_length=1, max_length=32)]
    accept_terms: bool = False
    email: Annotated[str, StringConstraints(max_length=254)] | None = None
    cpf: Annotated[str, StringConstraints(max_length=14)] | None = None
    password: Annotated[str, StringConstraints(max_length=256)] | None = None


class CompleteRequest(BaseModel):
    order_id: Annotated[str, StringConstraints(min_length=8, max_length=64)]


def _optional_portal_user(req: Request, db: Session) -> User | None:
    h = req.headers.get("authorization")
    if not h:
        return None
    return _portal_user_from(h, db)


@router.post("/shop/checkout")
def shop_checkout(body: CheckoutRequest, http_req: Request, db: Session = Depends(get_db)):
    """Cria o pedido e o PaymentIntent. Cliente novo: a conta so nasce depois do pagamento confirmado."""
    if not body.accept_terms:
        _fail(422, "TERMS_REQUIRED")
    ip = _extract_ip(http_req)
    if ratelimit.is_blocked(ip):
        _fail(429, "TOO_MANY_ATTEMPTS")
    plan = db.get(Plan, body.plan_code)
    if not billing.sellable(plan):
        _fail(404, "PLAN_UNAVAILABLE")
    user = _optional_portal_user(http_req, db)
    provider = _provider()
    order = Order(public_id=billing.new_public_id(), plan_code=plan.code, amount_cents=plan.price_cents,
                  provider_intent="pending-" + billing.new_public_id())
    if user is not None:
        lic = max(user.licenses, key=lambda l: l.id, default=None)
        if lic is not None and lic.status in ("suspensa", "revogada"):
            _fail(403, "LICENSE_BLOCKED")
        order.user_id, order.email = user.id, user.email
    else:
        email = normalize_email(body.email or "")
        cpf = normalize_cpf(body.cpf or "")
        pw = body.password or ""
        if not is_valid_email(email):
            _fail(422, "EMAIL_INVALID")
        if not is_valid_cpf(cpf):
            _fail(422, "CPF_INVALID")
        if len(pw) < settings.MIN_PASSWORD_LEN:
            _fail(422, "PASSWORD_POLICY")
        # Mensagens distintas para e-mail/CPF ja usados sao um oraculo de existencia; aceitavel na compra (o
        # cliente precisa saber que deve entrar em vez de comprar de novo) e limitado por IP.
        if db.scalar(select(User.id).where(User.email == email)) is not None:
            ratelimit.register_failure(ip)
            _fail(409, "EMAIL_EXISTS")
        if db.scalar(select(User.id).where(User.cpf_hmac == crypto.cpf_hmac(cpf))) is not None:
            ratelimit.register_failure(ip)
            _fail(409, "CPF_EXISTS")
        order.email = email
        order.cpf_enc = crypto.cpf_encrypt(cpf)
        order.cpf_hmac = crypto.cpf_hmac(cpf)
        order.password_hash = crypto.hash_password(pw)
    try:
        customer = user.stripe_customer_id if user is not None and user.stripe_customer_id else None
        if customer is None:
            customer = provider.create_customer(order.email, {"source": "structura-site"})
            if user is not None:
                user.stripe_customer_id = customer
        intent = provider.create_purchase_intent(customer, plan.price_cents,
                                                 {"kind": "purchase", "order": order.public_id, "plan": plan.code},
                                                 "order-" + order.public_id)
    except PaymentError:
        _fail(503, "PAYMENTS_UNAVAILABLE")
    order.provider_intent = intent.id
    db.add(order)
    db.commit()
    return {"order_id": order.public_id, "client_secret": intent.client_secret, "amount_cents": order.amount_cents,
            "publishable_key": provider.publishable_key()}


def _do_complete(db: Session, order: Order):
    provider = _provider()
    try:
        intent = provider.retrieve_payment_intent(order.provider_intent)
    except PaymentError:
        _fail(503, "PAYMENTS_UNAVAILABLE")
    out = billing.fulfill_order(db, order, intent, provider)
    db.commit()
    return intent, out


@router.post("/shop/complete")
def shop_complete(body: CompleteRequest, db: Session = Depends(get_db)):
    """Chamado pelo navegador apos a confirmacao no Stripe. Confere o pagamento NO STRIPE (nunca confia no navegador)."""
    order = db.scalar(select(Order).where(Order.public_id == body.order_id))
    if order is None:
        _fail(404, "ORDER_NOT_FOUND")
    if order.status == "paid":
        return {"status": "paid", "new_account": False}
    intent, out = _do_complete(db, order)
    if out is None:
        db.refresh(order)
        if order.status == "paid":                      # webhook concluiu primeiro
            return {"status": "paid", "new_account": False}
        return {"status": "pending" if intent.status in ("processing", "requires_action") else "failed",
                "intent_status": intent.status}
    user, lic, created = out
    return {"status": "paid", "new_account": created, "email": user.email,
            "license_expires_at": iso(lic.current_period_end)}


# =============================== webhook Stripe ===============================
@router.post("/webhook/payment")
async def webhook_payment(request: Request, db: Session = Depends(get_db)):
    payload = await request.body()
    try:
        ev = _provider().parse_webhook(payload, request.headers.get("stripe-signature", ""))
    except PaymentError:
        _fail(400, "BAD_SIGNATURE")
    if ev.get("type") == "payment_intent.succeeded":
        obj = (ev.get("data") or {}).get("object") or {}
        if (obj.get("metadata") or {}).get("kind") == "purchase":
            order = db.scalar(select(Order).where(Order.provider_intent == obj.get("id")))
            if order is not None and order.status == "pending":
                _do_complete(db, order)
    return {"received": True}


# =============================== area do cliente ===============================
def _portal_user_from(header: str, db: Session) -> User:
    token = header[7:].strip() if header.startswith("Bearer ") else header
    p = crypto.verify_portal_token(token)
    if not p:
        _fail(401, "PORTAL_AUTH_REQUIRED")
    user = db.get(User, p.get("user_id"))
    if user is None or user.status != "ativa":
        _fail(401, "PORTAL_AUTH_REQUIRED")
    if not hmac.compare_digest(str(p.get("pwv", "")), crypto.password_version(user.password_hash)):
        _fail(401, "PORTAL_AUTH_REQUIRED")                  # senha trocada depois da emissao
    return user


def get_portal_user(req: Request, db: Session = Depends(get_db)) -> User:
    h = req.headers.get("authorization")
    if not h:
        _fail(401, "PORTAL_AUTH_REQUIRED")
    return _portal_user_from(h, db)


class PortalLogin(Credentials):
    totp: Code6 | None = None


@router.post("/portal/auth/login")
def portal_login(req: PortalLogin, db: Session = Depends(get_db), http_req: Request = None):
    user = _check_credentials(db, req, http_req)
    if not user.totp_enabled:
        return _json(403, {"detail": "ENROLL_2FA"})       # o site oferece o cadastro do 2FA (mesmo fluxo do app)
    if not req.totp:
        _fail(401, "TOTP_REQUIRED")
    if not _check_totp(db, user, req.totp):
        _register_failure(db, user, "bad_portal_totp")
        _fail(401, "CREDENCIAIS")
    user.failed_logins = 0
    user.locked_until = None
    token, exp = crypto.issue_portal_token(user.id, settings.PORTAL_TOKEN_TTL_SECONDS,
                                           crypto.password_version(user.password_hash))
    _audit(db, f"user:{user.id}", "portal_login", f"user:{user.id}")
    db.commit()
    return {"token": token, "expires_at": iso_ts(exp)}


def _json(status: int, content: dict):
    from fastapi.responses import JSONResponse
    return JSONResponse(status_code=status, content=content)


def _license_json(db: Session, lic: License | None) -> dict | None:
    if lic is None:
        return None
    plan = db.get(Plan, lic.plan)
    now = utcnow()
    expired = lic.current_period_end <= now
    return {"id": lic.id, "plan": lic.plan, "plan_name": plan.name if plan else lic.plan, "status": lic.status,
            "expires_at": iso(lic.current_period_end), "expired": expired,
            "usable": lic.status == "ativa" and not expired}


@router.get("/portal/me")
def portal_me(user: User = Depends(get_portal_user), db: Session = Depends(get_db)):
    lic = max(user.licenses, key=lambda l: l.id, default=None)
    lj = _license_json(db, lic)
    plan = db.get(Plan, lic.plan) if lic else None
    card = None
    if user.card_pm_id:
        card = {"brand": user.card_brand, "last4": user.card_last4, "exp_month": user.card_exp_month,
                "exp_year": user.card_exp_year}
    renewal = None
    if user.auto_renew and card and lic and billing.sellable(plan):
        renewal = {"date": iso(lic.current_period_end), "amount_cents": plan.price_cents, "plan_name": plan.name}
    pays = []
    if lic:
        rows = db.scalars(select(Payment).join(License, License.id == Payment.license_id)
                          .where(License.user_id == user.id).order_by(Payment.id.desc()).limit(24)).all()
        pays = [{"date": iso(p.paid_at) if p.paid_at else None, "amount_cents": int(round(p.amount * 100)),
                 "status": p.status} for p in rows]
    return {"email": user.email, "cpf": crypto.cpf_decrypt(user.cpf_enc), "license": lj, "card": card,
            "auto_renew": bool(user.auto_renew and card), "next_charge": renewal, "payments": pays,
            "renew_failed": bool(lic and lic.renew_attempts > 0 and lic.current_period_end <= utcnow())}


@router.post("/portal/card/setup")
def card_setup(user: User = Depends(get_portal_user), db: Session = Depends(get_db)):
    provider = _provider()
    try:
        if not user.stripe_customer_id:
            user.stripe_customer_id = provider.create_customer(user.email, {"user_id": str(user.id)})
            db.commit()
        si = provider.create_setup_intent(user.stripe_customer_id)
    except PaymentError:
        _fail(503, "PAYMENTS_UNAVAILABLE")
    return {"client_secret": si.client_secret, "setup_intent_id": si.id,
            "publishable_key": provider.publishable_key()}


class CardConfirm(BaseModel):
    setup_intent_id: Annotated[str, StringConstraints(min_length=4, max_length=128)]


@router.post("/portal/card/confirm")
def card_confirm(body: CardConfirm, user: User = Depends(get_portal_user), db: Session = Depends(get_db)):
    provider = _provider()
    try:
        si = provider.retrieve_setup_intent(body.setup_intent_id)
        if si.status != "succeeded" or not si.payment_method_id or si.customer_id != user.stripe_customer_id:
            _fail(422, "CARD_NOT_CONFIRMED")
        card = provider.get_card(si.payment_method_id)
    except PaymentError:
        _fail(503, "PAYMENTS_UNAVAILABLE")
    billing.save_card(db, user, card, provider)
    _audit(db, f"user:{user.id}", "card_saved", f"user:{user.id}")
    db.commit()
    return {"ok": True, "card": {"brand": card.brand, "last4": card.last4, "exp_month": card.exp_month,
                                 "exp_year": card.exp_year}}


@router.post("/portal/subscription/cancel", status_code=204)
def subscription_cancel(user: User = Depends(get_portal_user), db: Session = Depends(get_db)):
    try:
        billing.clear_card(db, user, _provider())
    except PaymentError:
        _fail(503, "PAYMENTS_UNAVAILABLE")      # nada foi alterado: tente de novo
    _audit(db, f"user:{user.id}", "subscription_cancelled", f"user:{user.id}")
    db.commit()
    return None


class NewTotpSetup(BaseModel):
    password: Annotated[str, StringConstraints(min_length=1, max_length=256)]


class NewTotpConfirm(BaseModel):
    totp: Code6


@router.post("/portal/2fa/setup")
def portal_2fa_setup(body: NewTotpSetup, user: User = Depends(get_portal_user), db: Session = Depends(get_db),
                     http_req: Request = None):
    """Novo 2FA: exige a senha de novo. O segredo atual segue valendo ate o novo ser confirmado."""
    _check_credentials(db, Credentials(email=user.email, password=body.password), http_req)
    secret = totp.gen_secret()
    user.totp_pending_enc = crypto.totp_encrypt(secret)
    _audit(db, f"user:{user.id}", "portal_2fa_setup", f"user:{user.id}")
    db.commit()
    return {"secret": secret, "otpauth_uri": totp.otpauth_uri(secret, user.email)}


@router.post("/portal/2fa/confirm", status_code=204)
def portal_2fa_confirm(body: NewTotpConfirm, user: User = Depends(get_portal_user), db: Session = Depends(get_db)):
    if not user.totp_pending_enc:
        _fail(409, "2FA_STATE")
    step = totp.verify(crypto.totp_decrypt(user.totp_pending_enc), body.totp, last_step=0, window=settings.TOTP_WINDOW)
    if step is None:
        _register_failure(db, user, "bad_totp_rotate")
        _fail(401, "CREDENCIAIS")
    user.totp_secret_enc = user.totp_pending_enc
    user.totp_pending_enc = None
    user.totp_enabled = True
    user.totp_last_step = step
    _audit(db, f"user:{user.id}", "portal_2fa_rotated", f"user:{user.id}")
    db.commit()
    return None


# =============================== admin: precos ===============================
class PlanUpsert(BaseModel):
    name: Annotated[str, StringConstraints(min_length=1, max_length=64)] | None = None
    period_days: int | None = None
    price_cents: int | None = None
    active: bool | None = None


class PlanCreate(PlanUpsert):
    code: Annotated[str, StringConstraints(pattern=r"^[a-z0-9_-]{2,32}$")]
    name: Annotated[str, StringConstraints(min_length=1, max_length=64)]
    period_days: int


def _validate_plan(p: Plan) -> None:
    if not (1 <= p.period_days <= 3660):
        raise HTTPException(status_code=422, detail="PERIOD_INVALID")
    if p.price_cents < 0 or p.price_cents > 100_000_000:
        raise HTTPException(status_code=422, detail="PRICE_INVALID")
    if p.active and p.price_cents < billing.MIN_PRICE_CENTS:
        raise HTTPException(status_code=422, detail="PRICE_TOO_LOW")


@router.get("/admin/plans")
def admin_plans(db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    return [_plan_json(p) | {"updated_at": iso(p.updated_at)} for p in db.scalars(select(Plan).order_by(Plan.period_days))]


@router.post("/admin/plans", status_code=201)
def admin_plan_create(body: PlanCreate, db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    if db.get(Plan, body.code) is not None:
        raise HTTPException(status_code=409, detail="PLAN_EXISTS")
    p = Plan(code=body.code, name=body.name, period_days=body.period_days, price_cents=body.price_cents or 0,
             active=bool(body.active))
    _validate_plan(p)
    db.add(p)
    _audit(db, f"admin:{admin.id}", "plan_create", f"plan:{p.code}", f"price={p.price_cents}")
    db.commit()
    return _plan_json(p)


@router.patch("/admin/plans/{code}")
def admin_plan_update(code: str, body: PlanUpsert, db: Session = Depends(get_db),
                      admin: User = Depends(get_current_admin)):
    p = db.get(Plan, code)
    if p is None:
        raise HTTPException(status_code=404, detail="PLAN_NOT_FOUND")
    for f in ("name", "period_days", "price_cents", "active"):
        v = getattr(body, f)
        if v is not None:
            setattr(p, f, v)
    _validate_plan(p)
    p.updated_at = utcnow()
    _audit(db, f"admin:{admin.id}", "plan_update", f"plan:{p.code}", f"price={p.price_cents} active={p.active}")
    db.commit()
    return _plan_json(p)
