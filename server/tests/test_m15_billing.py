"""Loja, area do cliente, cobranca e renovacao automatica (provedor de pagamento FALSO, sem rede)."""
from datetime import timedelta

import pytest

from app import payments, totp, billing
from app.db import SessionLocal
from app.models import License, Order, Payment, Plan, User
from app.payments import CardInfo, IntentInfo, PaymentError
from app.timeutil import utcnow
from app.users import create_user_with_license

PW, CPF = "uma-senha-forte-123", "52998224725"


class FakeProvider:
    def __init__(self):
        self.n = 0
        self.intents = {}           # id -> IntentInfo
        self.cards = {}             # pm -> CardInfo
        self.detached = []
        self.charges = []           # (customer, pm, amount, key)
        self.charge_result = "succeeded"
        self.down = False
        self.seen_keys = {}

    def _id(self, p):
        self.n += 1
        return f"{p}_{self.n}"

    def publishable_key(self):
        return "pk_test_x"

    def create_customer(self, email, metadata):
        return self._id("cus")

    def create_setup_intent(self, customer_id):
        i = IntentInfo(id=self._id("seti"), status="requires_payment_method", customer_id=customer_id,
                       client_secret="seti_secret")
        self.intents[i.id] = i
        return i

    def retrieve_setup_intent(self, intent_id):
        return self.intents[intent_id]

    def get_card(self, pm_id):
        return self.cards[pm_id]

    def detach(self, pm_id):
        if self.down:
            raise PaymentError("down")
        self.detached.append(pm_id)

    def create_purchase_intent(self, customer_id, amount, metadata, key):
        i = IntentInfo(id=self._id("pi"), status="requires_payment_method", amount=amount, customer_id=customer_id,
                       metadata=metadata, client_secret="pi_secret")
        self.intents[i.id] = i
        return i

    def retrieve_payment_intent(self, intent_id):
        return self.intents[intent_id]

    def charge_off_session(self, customer_id, pm_id, amount, metadata, key):
        if self.down:
            raise PaymentError("down")
        if key in self.seen_keys:
            return self.seen_keys[key]
        self.charges.append((customer_id, pm_id, amount, key))
        i = IntentInfo(id=self._id("pi"), status=self.charge_result, amount=amount, customer_id=customer_id,
                       payment_method_id=pm_id, error_code="" if self.charge_result == "succeeded" else "card_declined")
        self.seen_keys[key] = i
        return i

    def parse_webhook(self, payload, signature):
        import json
        if signature != "ok":
            raise PaymentError("BAD_SIGNATURE")
        return json.loads(payload)

    # helpers de teste
    def pay(self, intent_id, brand="visa", last4="4242"):
        pm = self._id("pm")
        self.cards[pm] = CardInfo(pm, brand, last4, 12, 2030)
        i = self.intents[intent_id]
        i.status, i.payment_method_id = "succeeded", pm
        return pm

    def confirm_setup(self, si_id, last4="4242"):
        pm = self._id("pm")
        self.cards[pm] = CardInfo(pm, "visa", last4, 12, 2030)
        i = self.intents[si_id]
        i.status, i.payment_method_id = "succeeded", pm
        return pm


@pytest.fixture()
def prov():
    p = FakeProvider()
    payments.set_provider(p)
    db = SessionLocal()
    billing.seed_plans(db)
    db.close()
    yield p
    payments.set_provider(None)


def _admin(client):
    db = SessionLocal()
    pr = create_user_with_license(db, "adm@exemplo.com", PW, "11144477735", is_admin=True)
    db.commit()
    secret = pr.totp_secret
    db.close()
    r = client.post("/admin/auth/login", json={"email": "adm@exemplo.com", "password": PW, "totp": totp.code_at(secret)})
    return {"Authorization": f"Bearer {r.json()['admin_token']}"}


def _set_prices(client, h, mensal=9900, tri=26900):
    assert client.patch("/admin/plans/mensal", headers=h, json={"price_cents": mensal, "active": True}).status_code == 200
    assert client.patch("/admin/plans/trimestral", headers=h, json={"price_cents": tri, "active": True}).status_code == 200


def _user(email="cli@exemplo.com", days=30, expire=False):
    db = SessionLocal()
    pr = create_user_with_license(db, email, PW, CPF if email == "cli@exemplo.com" else "11144477735", days=days)
    if expire:
        pr.license.current_period_end = utcnow() - timedelta(hours=1)
    db.commit()
    secret, uid = pr.totp_secret, pr.user.id
    db.close()
    return secret, uid


def _portal(client, email, secret):
    r = client.post("/portal/auth/login", json={"email": email, "password": PW, "totp": totp.code_at(secret)})
    assert r.status_code == 200, r.text
    return {"Authorization": f"Bearer {r.json()['token']}"}


# ---------------- planos / admin ----------------
def test_plans_seeded_hidden_until_priced(client, prov):
    assert client.get("/shop/plans").json() == []
    h = _admin(client)
    rows = client.get("/admin/plans", headers=h).json()
    assert {r["code"] for r in rows} == {"mensal", "trimestral"}
    _set_prices(client, h)
    shop = client.get("/shop/plans").json()
    assert [p["code"] for p in shop] == ["mensal", "trimestral"] and shop[0]["price_cents"] == 9900


def test_admin_plans_validation_and_auth(client, prov):
    assert client.get("/admin/plans").status_code == 401
    h = _admin(client)
    assert client.patch("/admin/plans/mensal", headers=h, json={"price_cents": 10, "active": True}).status_code == 422
    assert client.patch("/admin/plans/mensal", headers=h, json={"price_cents": -1}).status_code == 422
    assert client.patch("/admin/plans/xx", headers=h, json={"price_cents": 1000}).status_code == 404
    r = client.post("/admin/plans", headers=h, json={"code": "anual", "name": "Anual", "period_days": 365,
                                                       "price_cents": 90000, "active": True})
    assert r.status_code == 201
    assert client.post("/admin/plans", headers=h, json={"code": "anual", "name": "x", "period_days": 1}).status_code == 409


# ---------------- login do portal ----------------
def test_portal_login_requires_2fa_and_me(client, prov):
    secret, _ = _user()
    r = client.post("/portal/auth/login", json={"email": "cli@exemplo.com", "password": PW})
    assert r.status_code == 401 and r.json()["detail"] == "TOTP_REQUIRED"
    r = client.post("/portal/auth/login", json={"email": "cli@exemplo.com", "password": "errada!!", "totp": "123456"})
    assert r.status_code == 401
    h = _portal(client, "cli@exemplo.com", secret)
    me = client.get("/portal/me", headers=h).json()
    assert me["email"] == "cli@exemplo.com" and me["cpf"] == CPF and me["card"] is None
    assert me["license"]["usable"] is True and me["auto_renew"] is False
    assert client.get("/portal/me").status_code == 401
    assert client.get("/portal/me", headers={"Authorization": "Bearer lixo"}).status_code == 401


def test_portal_token_does_not_open_admin(client, prov):
    secret, _ = _user()
    h = _portal(client, "cli@exemplo.com", secret)
    assert client.get("/admin/plans", headers=h).status_code == 401


# ---------------- cartao ----------------
def test_card_add_replace_cancel(client, prov):
    secret, uid = _user()
    h = _portal(client, "cli@exemplo.com", secret)
    s = client.post("/portal/card/setup", headers=h).json()
    # antes do Stripe confirmar nao grava
    assert client.post("/portal/card/confirm", headers=h, json={"setup_intent_id": s["setup_intent_id"]}).status_code == 422
    pm1 = prov.confirm_setup(s["setup_intent_id"], "1111")
    r = client.post("/portal/card/confirm", headers=h, json={"setup_intent_id": s["setup_intent_id"]})
    assert r.status_code == 200 and r.json()["card"]["last4"] == "1111"
    me = client.get("/portal/me", headers=h).json()
    assert me["card"]["last4"] == "1111" and me["auto_renew"] is True
    # trocar: o antigo e desanexado
    s2 = client.post("/portal/card/setup", headers=h).json()
    pm2 = prov.confirm_setup(s2["setup_intent_id"], "2222")
    client.post("/portal/card/confirm", headers=h, json={"setup_intent_id": s2["setup_intent_id"]})
    assert prov.detached == [pm1]
    assert client.get("/portal/me", headers=h).json()["card"]["last4"] == "2222"
    # cancelar: desanexa, apaga, desliga
    assert client.post("/portal/subscription/cancel", headers=h).status_code == 204
    assert prov.detached == [pm1, pm2]
    me = client.get("/portal/me", headers=h).json()
    assert me["card"] is None and me["auto_renew"] is False
    db = SessionLocal()
    u = db.get(User, uid)
    assert u.card_pm_id is None and u.card_last4 is None and u.auto_renew is False
    db.close()


def test_card_setup_intent_of_another_customer_is_rejected(client, prov):
    sa, _ = _user("cli@exemplo.com")
    sb, _ = _user("outro@exemplo.com")
    ha = _portal(client, "cli@exemplo.com", sa)
    hb = _portal(client, "outro@exemplo.com", sb)
    s = client.post("/portal/card/setup", headers=ha).json()
    client.post("/portal/card/setup", headers=hb)           # cria cliente do outro
    prov.confirm_setup(s["setup_intent_id"])
    r = client.post("/portal/card/confirm", headers=hb, json={"setup_intent_id": s["setup_intent_id"]})
    assert r.status_code == 422


def test_cancel_keeps_card_if_provider_fails(client, prov):
    secret, uid = _user()
    h = _portal(client, "cli@exemplo.com", secret)
    s = client.post("/portal/card/setup", headers=h).json()
    prov.confirm_setup(s["setup_intent_id"])
    client.post("/portal/card/confirm", headers=h, json={"setup_intent_id": s["setup_intent_id"]})
    prov.down = True
    assert client.post("/portal/subscription/cancel", headers=h).status_code == 503
    assert client.get("/portal/me", headers=h).json()["card"] is not None


# ---------------- compra ----------------
def _buy_body(**kw):
    b = {"plan_code": "mensal", "accept_terms": True, "email": "Novo@Exemplo.com", "cpf": "529.982.247-25",
         "password": PW}
    b.update(kw)
    return b


def test_guest_purchase_creates_account_after_payment(client, prov):
    h = _admin(client)
    _set_prices(client, h)
    r = client.post("/shop/checkout", json=_buy_body())
    assert r.status_code == 200, r.text
    j = r.json()
    assert j["amount_cents"] == 9900 and j["client_secret"] and j["publishable_key"] == "pk_test_x"
    db = SessionLocal()
    assert db.query(User).filter(User.email == "novo@exemplo.com").count() == 0     # ainda nao existe
    db.close()
    # nao pago: nao conclui
    assert client.post("/shop/complete", json={"order_id": j["order_id"]}).json()["status"] == "failed"
    pi = [k for k, v in prov.intents.items() if k.startswith("pi_")][0]
    prov.pay(pi)
    c = client.post("/shop/complete", json={"order_id": j["order_id"]}).json()
    assert c["status"] == "paid" and c["new_account"] is True
    # idempotente: 2a chamada nao cria nada nem soma periodo
    c2 = client.post("/shop/complete", json={"order_id": j["order_id"]}).json()
    assert c2["status"] == "paid" and c2["new_account"] is False
    db = SessionLocal()
    u = db.query(User).filter(User.email == "novo@exemplo.com").one()
    assert len(u.licenses) == 1 and u.auto_renew is True and u.card_last4 == "4242"
    assert 29 <= (u.licenses[0].current_period_end - utcnow()).days <= 30
    assert db.query(Payment).count() == 1
    assert u.totp_enabled is False and u.cpf_enc and "52998224725" not in u.cpf_enc
    db.close()
    # a conta recem-criada ainda precisa cadastrar o 2FA (o site usa /auth/2fa/setup)
    r = client.post("/portal/auth/login", json={"email": "novo@exemplo.com", "password": PW})
    assert r.status_code == 403 and r.json()["detail"] == "ENROLL_2FA"


def test_purchase_amount_mismatch_or_unpriced_plan(client, prov):
    assert client.post("/shop/checkout", json=_buy_body()).status_code == 404       # plano sem preco
    h = _admin(client)
    _set_prices(client, h)
    j = client.post("/shop/checkout", json=_buy_body()).json()
    pi = [k for k, v in prov.intents.items() if k.startswith("pi_")][0]
    prov.pay(pi)
    prov.intents[pi].amount = 1                                                       # valor diferente do pedido
    assert client.post("/shop/complete", json={"order_id": j["order_id"]}).json()["status"] == "failed"
    db = SessionLocal()
    assert db.query(User).filter(User.email == "novo@exemplo.com").count() == 0
    db.close()


def test_checkout_validation(client, prov):
    h = _admin(client)
    _set_prices(client, h)
    assert client.post("/shop/checkout", json=_buy_body(accept_terms=False)).status_code == 422
    assert client.post("/shop/checkout", json=_buy_body(cpf="111.111.111-11")).json()["detail"] == "CPF_INVALID"
    assert client.post("/shop/checkout", json=_buy_body(email="x")).json()["detail"] == "EMAIL_INVALID"
    assert client.post("/shop/checkout", json=_buy_body(password="123")).json()["detail"] == "PASSWORD_POLICY"
    assert client.post("/shop/checkout", json=_buy_body(email="adm@exemplo.com")).json()["detail"] == "EMAIL_EXISTS"
    assert client.post("/shop/checkout", json=_buy_body(plan_code="nao-existe")).status_code == 404


def test_logged_customer_purchase_extends_license(client, prov):
    h = _admin(client)
    _set_prices(client, h)
    secret, uid = _user(days=10)
    ph = _portal(client, "cli@exemplo.com", secret)
    j = client.post("/shop/checkout", headers=ph, json={"plan_code": "trimestral", "accept_terms": True}).json()
    pi = [k for k, v in prov.intents.items() if k.startswith("pi_")][0]
    prov.pay(pi)
    c = client.post("/shop/complete", json={"order_id": j["order_id"]}).json()
    assert c["status"] == "paid" and c["new_account"] is False
    db = SessionLocal()
    lic = db.query(License).filter(License.user_id == uid).one()
    assert 99 <= (lic.current_period_end - utcnow()).days <= 100       # 10 restantes + 90
    assert lic.plan == "trimestral"
    db.close()


def test_webhook_fulfills_once_and_rejects_bad_signature(client, prov):
    import json
    h = _admin(client)
    _set_prices(client, h)
    j = client.post("/shop/checkout", json=_buy_body()).json()
    pi = [k for k, v in prov.intents.items() if k.startswith("pi_")][0]
    prov.pay(pi)
    ev = json.dumps({"type": "payment_intent.succeeded",
                     "data": {"object": {"id": pi, "metadata": {"kind": "purchase"}}}})
    assert client.post("/webhook/payment", content=ev, headers={"stripe-signature": "x"}).status_code == 400
    assert client.post("/webhook/payment", content=ev, headers={"stripe-signature": "ok"}).status_code == 200
    assert client.post("/webhook/payment", content=ev, headers={"stripe-signature": "ok"}).status_code == 200
    client.post("/shop/complete", json={"order_id": j["order_id"]})
    db = SessionLocal()
    assert db.query(User).filter(User.email == "novo@exemplo.com").count() == 1
    assert db.query(Payment).count() == 1
    db.close()


# ---------------- renovacao automatica ----------------
def _with_card(client, prov, email="cli@exemplo.com", expire=False, days=30):
    h = _admin(client)
    _set_prices(client, h)
    secret, uid = _user(email, days=days, expire=expire)
    ph = _portal(client, email, secret)
    s = client.post("/portal/card/setup", headers=ph).json()
    prov.confirm_setup(s["setup_intent_id"])
    client.post("/portal/card/confirm", headers=ph, json={"setup_intent_id": s["setup_intent_id"]})
    return ph, uid


def _renew():
    db = SessionLocal()
    try:
        return billing.run_renewals(db, payments._provider)
    finally:
        db.close()


def test_renewal_never_charges_before_expiry(client, prov):
    _with_card(client, prov, expire=False)
    assert _renew() == {"charged": 0, "failed": 0, "skipped": 0, "unknown": 0}
    assert prov.charges == []


def test_renewal_charges_exactly_once_after_expiry(client, prov):
    ph, uid = _with_card(client, prov, expire=True)
    assert _renew()["charged"] == 1
    assert _renew()["charged"] == 0                 # agora esta valida: nao cobra de novo
    assert len(prov.charges) == 1 and prov.charges[0][2] == 9900
    db = SessionLocal()
    lic = db.query(License).filter(License.user_id == uid).one()
    assert 29 <= (lic.current_period_end - utcnow()).days <= 30 and lic.status == "ativa"
    assert db.query(Payment).filter(Payment.status == "paid").count() == 1
    db.close()
    assert client.get("/portal/me", headers=ph).json()["license"]["usable"] is True


def test_no_renewal_after_cancel(client, prov):
    ph, uid = _with_card(client, prov, expire=True)
    client.post("/portal/subscription/cancel", headers=ph)
    assert _renew() == {"charged": 0, "failed": 0, "skipped": 0, "unknown": 0}
    assert prov.charges == []


def test_renewal_failure_retries_spaced_then_stops(client, prov):
    _with_card(client, prov, expire=True)
    prov.charge_result = "failed"
    assert _renew()["failed"] == 1
    assert _renew()["skipped"] == 1                     # antes de 24 h nao tenta de novo
    now = utcnow()
    for hours in (25, 50):
        db = SessionLocal()
        assert billing.run_renewals(db, prov, now + timedelta(hours=hours))["failed"] == 1
        db.close()
    db = SessionLocal()
    assert billing.run_renewals(db, prov, now + timedelta(hours=100))["skipped"] == 1   # 3 tentativas: desiste
    assert db.query(Payment).filter(Payment.status == "failed").count() == 3
    db.close()
    assert len(prov.charges) == 3


def test_renewal_provider_down_does_not_count_attempt(client, prov):
    _with_card(client, prov, expire=True)
    prov.down = True
    assert _renew()["unknown"] == 1
    prov.down = False
    assert _renew()["charged"] == 1


def test_renewal_skips_unpriced_plan_and_suspended_license(client, prov):
    ph, uid = _with_card(client, prov, expire=True)
    h = _admin_headers_again(client)
    client.patch("/admin/plans/mensal", headers=h, json={"active": False})
    assert _renew()["skipped"] == 1 and prov.charges == []
    client.patch("/admin/plans/mensal", headers=h, json={"active": True})
    db = SessionLocal()
    db.query(License).filter(License.user_id == uid).one().status = "suspensa"
    db.commit()
    db.close()
    assert _renew() == {"charged": 0, "failed": 0, "skipped": 0, "unknown": 0}


def _admin_headers_again(client):
    db = SessionLocal()
    adm = db.query(User).filter(User.email == "adm@exemplo.com").one()
    from app import crypto
    tok, _ = crypto.issue_admin_token(adm.id, adm.email, 600, crypto.password_version(adm.password_hash))
    db.close()
    return {"Authorization": f"Bearer {tok}"}


def test_new_card_resets_failed_attempts(client, prov):
    ph, uid = _with_card(client, prov, expire=True)
    prov.charge_result = "failed"
    _renew()
    s = client.post("/portal/card/setup", headers=ph).json()
    prov.confirm_setup(s["setup_intent_id"], "9999")
    client.post("/portal/card/confirm", headers=ph, json={"setup_intent_id": s["setup_intent_id"]})
    db = SessionLocal()
    assert db.query(License).filter(License.user_id == uid).one().renew_attempts == 0
    db.close()
    prov.charge_result = "succeeded"
    assert _renew()["charged"] == 1


# ---------------- novo 2FA ----------------
def test_rotate_2fa(client, prov):
    old, uid = _user()
    ph = _portal(client, "cli@exemplo.com", old)
    assert client.post("/portal/2fa/setup", headers=ph, json={"password": "senha-errada"}).status_code == 401
    r = client.post("/portal/2fa/setup", headers=ph, json={"password": PW})
    assert r.status_code == 200
    new = r.json()["secret"]
    # antes de confirmar, o segredo antigo ainda vale
    assert client.post("/portal/auth/login", json={"email": "cli@exemplo.com", "password": PW,
                                                   "totp": totp.code_at(old, at=__import__("time").time() + 30)}).status_code == 200
    assert client.post("/portal/2fa/confirm", headers=ph, json={"totp": "000000"}).status_code == 401
    import time
    assert client.post("/portal/2fa/confirm", headers=ph, json={"totp": totp.code_at(new)}).status_code == 204
    # depois, so o novo
    db = SessionLocal()
    u = db.get(User, uid)
    u.failed_logins, u.locked_until, u.totp_last_step = 0, None, 0
    db.commit()
    db.close()
    bad = client.post("/portal/auth/login", json={"email": "cli@exemplo.com", "password": PW,
                                                  "totp": totp.code_at(old, at=time.time() + 30)})
    assert bad.status_code == 401
    ok = client.post("/portal/auth/login", json={"email": "cli@exemplo.com", "password": PW,
                                                 "totp": totp.code_at(new, at=time.time() + 30)})
    assert ok.status_code == 200
