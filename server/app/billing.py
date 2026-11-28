"""Regras de cobranca: aplicar pagamento a licenca, cumprir pedidos e renovar licencas VENCIDAS.

Regra de ouro da renovacao: o cartao so e cobrado quando a licenca JA venceu (current_period_end <= agora),
com renovacao automatica ligada, cartao cadastrado e plano ativo com preco. Cancelar remove o cartao e desliga
a renovacao; a licenca segue valida ate o fim do periodo ja pago.
"""
from __future__ import annotations
import logging
import secrets
from datetime import datetime, timedelta

from sqlalchemy import select, update, delete
from sqlalchemy.orm import Session

from . import crypto
from .config import settings
from .models import AuditLog, License, Order, Payment, Plan, User
from .payments import CardInfo, IntentInfo, PaymentError, PaymentProvider
from .timeutil import utcnow

log = logging.getLogger("structura.billing")

PLAN_DEFAULTS = [("mensal", "Mensal", 30), ("trimestral", "Trimestral", 90)]
MIN_PRICE_CENTS = 50     # minimo do Stripe em BRL


def audit(db: Session, actor: str, action: str, target: str, meta: str = "") -> None:
    db.add(AuditLog(actor=actor, action=action, target=target, metadata_masked=meta))


def seed_plans(db: Session) -> None:
    """Cria mensal/trimestral inativos e sem preco, se nao existirem. O admin define o valor e ativa."""
    for code, name, days in PLAN_DEFAULTS:
        if db.get(Plan, code) is None:
            db.add(Plan(code=code, name=name, period_days=days, price_cents=0, active=False))
    db.commit()


def sellable(plan: Plan | None) -> bool:
    return plan is not None and plan.active and plan.price_cents >= MIN_PRICE_CENTS


def extend_license(lic: License, plan: Plan, now: datetime | None = None) -> None:
    """Soma o periodo do plano a partir do fim atual (ou de agora, se ja vencida) e reativa."""
    now = now or utcnow()
    base = lic.current_period_end if lic.current_period_end > now else now
    lic.current_period_end = base + timedelta(days=plan.period_days)
    lic.status = "ativa"
    lic.plan = plan.code
    lic.renew_attempts = 0
    lic.last_renew_attempt_at = None


def save_card(db: Session, user: User, card: CardInfo, provider: PaymentProvider) -> None:
    """Grava o cartao como o unico da conta (desanexa o anterior) e liga a renovacao automatica."""
    old = user.card_pm_id
    user.card_pm_id = card.pm_id
    user.card_brand = card.brand
    user.card_last4 = card.last4
    user.card_exp_month = card.exp_month
    user.card_exp_year = card.exp_year
    user.auto_renew = True
    for lic in user.licenses:
        lic.renew_attempts = 0
        lic.last_renew_attempt_at = None
    if old and old != card.pm_id:
        try:
            provider.detach(old)
        except PaymentError:
            log.warning("falha ao desanexar cartao antigo do usuario %s", user.id)


def clear_card(db: Session, user: User, provider: PaymentProvider) -> None:
    """Cancelamento: desanexa o cartao no provedor, apaga os dados locais e desliga a renovacao."""
    pm = user.card_pm_id
    if pm:
        provider.detach(pm)          # se falhar (PaymentError) nada local muda: o usuario pode tentar de novo
    user.card_pm_id = user.card_brand = user.card_last4 = None
    user.card_exp_month = user.card_exp_year = None
    user.auto_renew = False


def record_payment(db: Session, lic: License, ref: str, amount_cents: int, status: str) -> None:
    db.add(Payment(license_id=lic.id, provider="stripe", provider_ref=ref, amount=amount_cents / 100.0,
                   status=status, paid_at=utcnow() if status == "paid" else None))


def fulfill_order(db: Session, order: Order, intent: IntentInfo, provider: PaymentProvider):
    """Conclui um pedido PAGO (idempotente: so o primeiro chamador ve transitar pending->paid).
    Retorna (user, licenca, conta_nova) ou None se o pedido ja estava concluido/nao esta pago."""
    if intent.status != "succeeded" or intent.amount != order.amount_cents or intent.id != order.provider_intent:
        return None
    res = db.execute(update(Order).where(Order.id == order.id, Order.status == "pending").values(status="paid"))
    if res.rowcount != 1:
        return None
    db.refresh(order)
    plan = db.get(Plan, order.plan_code)
    period = plan.period_days if plan else 30
    created = False
    if order.user_id:
        user = db.get(User, order.user_id)
        lic = max(user.licenses, key=lambda l: l.id, default=None)
        if lic is None:
            lic = License(user_id=user.id, status="ativa", current_period_end=utcnow(), plan=order.plan_code)
            db.add(lic)
            db.flush()
    else:
        user = db.scalar(select(User).where(User.email == order.email))
        if user is None:
            user = User(email=order.email, password_hash=order.password_hash, cpf_enc=order.cpf_enc,
                        cpf_hmac=order.cpf_hmac, totp_enabled=False, status="ativa", stripe_customer_id=intent.customer_id)
            db.add(user)
            db.flush()
            created = True
            lic = License(user_id=user.id, status="ativa", current_period_end=utcnow(), plan=order.plan_code)
            db.add(lic)
            db.flush()
        else:
            lic = max(user.licenses, key=lambda l: l.id, default=None)
    if plan is not None:
        extend_license(lic, plan)
    else:                                              # plano removido entre o checkout e o pagamento: usa o periodo padrao
        lic.current_period_end = max(lic.current_period_end, utcnow()) + timedelta(days=period)
        lic.status = "ativa"
    if not user.stripe_customer_id:
        user.stripe_customer_id = intent.customer_id
    record_payment(db, lic, intent.id, order.amount_cents, "paid")
    if intent.payment_method_id:
        try:
            save_card(db, user, provider.get_card(intent.payment_method_id), provider)
        except PaymentError:
            log.warning("pedido %s pago, mas nao foi possivel ler o cartao", order.id)
    audit(db, f"user:{user.id}", "order_paid", f"order:{order.id}", f"plan={order.plan_code}")
    return user, lic, created


def new_public_id() -> str:
    return secrets.token_urlsafe(18)


# ---------------- renovacao automatica ----------------
def due_licenses(db: Session, now: datetime):
    """Licencas vencidas, ativas, com renovacao automatica e cartao. NUNCA inclui licenca ainda valida."""
    q = (select(License, User).join(User, User.id == License.user_id)
         .where(License.current_period_end <= now, License.status == "ativa",
                User.auto_renew.is_(True), User.card_pm_id.is_not(None), User.status == "ativa"))
    return db.execute(q).all()


def run_renewals(db: Session, provider: PaymentProvider, now: datetime | None = None) -> dict:
    now = now or utcnow()
    stats = {"charged": 0, "failed": 0, "skipped": 0, "unknown": 0}
    for lic, user in due_licenses(db, now):
        plan = db.get(Plan, lic.plan)
        if not sellable(plan):
            stats["skipped"] += 1
            continue
        if lic.renew_attempts >= settings.RENEW_MAX_ATTEMPTS:
            stats["skipped"] += 1
            continue
        if lic.last_renew_attempt_at and now - lic.last_renew_attempt_at < timedelta(hours=settings.RENEW_RETRY_HOURS):
            stats["skipped"] += 1
            continue
        # Chave idempotente por licenca + vencimento + tentativa + cartao (cartao novo = cobranca nova): um travamento depois da cobranca e uma nova rodada
        # devolvem a MESMA cobranca (nunca cobram duas vezes o mesmo vencimento).
        key = f"renew-{lic.id}-{int(lic.current_period_end.timestamp())}-{lic.renew_attempts}-{user.card_pm_id}"
        try:
            intent = provider.charge_off_session(user.stripe_customer_id, user.card_pm_id, plan.price_cents,
                                                 {"kind": "renewal", "license_id": str(lic.id), "plan": plan.code}, key)
        except PaymentError:
            stats["unknown"] += 1           # nao conta tentativa: a proxima rodada repete a mesma chave
            log.warning("renovacao da licenca %s: provedor indisponivel", lic.id)
            continue
        if intent.status == "succeeded":
            extend_license(lic, plan, now)
            record_payment(db, lic, intent.id, plan.price_cents, "paid")
            audit(db, f"license:{lic.id}", "renewal_paid", f"license:{lic.id}", f"plan={plan.code}")
            stats["charged"] += 1
        else:
            lic.renew_attempts += 1
            lic.last_renew_attempt_at = now
            ref = intent.id or f"failed-{key}"
            if db.scalar(select(Payment.id).where(Payment.provider_ref == ref)) is None:
                record_payment(db, lic, ref, plan.price_cents, "failed")
            audit(db, f"license:{lic.id}", "renewal_failed", f"license:{lic.id}", intent.error_code or intent.status)
            stats["failed"] += 1
        db.commit()
    db.execute(delete(Order).where(Order.status == "pending", Order.created_at < now - timedelta(days=7)))
    db.commit()
    return stats
