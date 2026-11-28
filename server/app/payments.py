"""Provedor de pagamento (Stripe) atras de uma interface pequena.

O numero do cartao NUNCA chega ao nosso servidor: o navegador o envia direto ao Stripe (Payment Element, iframe
do Stripe) e recebemos so ids (cliente, metodo de pagamento, intent) e dados de exibicao (marca, ultimos 4).
Os testes usam um provedor falso (tests/test_m15_billing.py) trocando `set_provider`.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol

from .config import settings


class PaymentError(Exception):
    """Falha de comunicacao/configuracao com o provedor (resultado da cobranca DESCONHECIDO)."""


@dataclass
class CardInfo:
    pm_id: str
    brand: str
    last4: str
    exp_month: int
    exp_year: int
    customer_id: str | None = None


@dataclass
class IntentInfo:
    id: str
    status: str                  # succeeded | requires_payment_method | requires_action | processing | canceled ...
    amount: int = 0
    currency: str = "brl"
    customer_id: str | None = None
    payment_method_id: str | None = None
    metadata: dict | None = None
    client_secret: str | None = None
    error_code: str = ""         # preenchido em cobrancas off-session recusadas


class PaymentProvider(Protocol):
    def publishable_key(self) -> str: ...
    def create_customer(self, email: str, metadata: dict) -> str: ...
    def create_setup_intent(self, customer_id: str) -> IntentInfo: ...
    def retrieve_setup_intent(self, intent_id: str) -> IntentInfo: ...
    def get_card(self, pm_id: str) -> CardInfo: ...
    def detach(self, pm_id: str) -> None: ...
    def create_purchase_intent(self, customer_id: str, amount: int, metadata: dict, key: str) -> IntentInfo: ...
    def retrieve_payment_intent(self, intent_id: str) -> IntentInfo: ...
    def charge_off_session(self, customer_id: str, pm_id: str, amount: int, metadata: dict, key: str) -> IntentInfo: ...
    def parse_webhook(self, payload: bytes, signature: str) -> dict: ...


class StripeProvider:
    def __init__(self) -> None:
        if not settings.STRIPE_SECRET_KEY:
            raise PaymentError("STRIPE_SECRET_KEY ausente")
        import stripe
        self._stripe = stripe
        self._c = stripe.StripeClient(settings.STRIPE_SECRET_KEY)

    def publishable_key(self) -> str:
        return settings.STRIPE_PUBLISHABLE_KEY

    def _call(self, fn, *a, **kw):
        try:
            return fn(*a, **kw)
        except self._stripe.CardError:
            raise
        except self._stripe.StripeError as e:   # rede, autenticacao, limite, parametro: resultado desconhecido
            raise PaymentError(type(e).__name__) from e

    @staticmethod
    def _intent(o) -> IntentInfo:
        pm = o.get("payment_method")
        cust = o.get("customer")
        err = (o.get("last_payment_error") or {}).get("code") or ""
        return IntentInfo(
            id=o["id"], status=o["status"], amount=int(o.get("amount") or 0), currency=o.get("currency") or "brl",
            customer_id=cust if isinstance(cust, str) else (cust or {}).get("id"),
            payment_method_id=pm if isinstance(pm, str) else (pm or {}).get("id"),
            metadata=dict(o.get("metadata") or {}), client_secret=o.get("client_secret"), error_code=err)

    def create_customer(self, email: str, metadata: dict) -> str:
        c = self._call(self._c.v1.customers.create, params={"email": email, "metadata": metadata})
        return c["id"]

    def create_setup_intent(self, customer_id: str) -> IntentInfo:
        # Sem payment_method_types: os metodos vem do Dashboard (deixe so "Cartoes"; o SetupIntent so mostra reutilizaveis).
        o = self._call(self._c.v1.setup_intents.create, params={
            "customer": customer_id, "usage": "off_session", "automatic_payment_methods": {"enabled": True}})
        return self._intent(o)

    def retrieve_setup_intent(self, intent_id: str) -> IntentInfo:
        return self._intent(self._call(self._c.v1.setup_intents.retrieve, intent_id))

    def get_card(self, pm_id: str) -> CardInfo:
        pm = self._call(self._c.v1.payment_methods.retrieve, pm_id)
        card = pm.get("card")
        if not card:
            raise PaymentError("NOT_A_CARD")
        cust = pm.get("customer")
        return CardInfo(pm_id=pm["id"], brand=card.get("brand", ""), last4=card.get("last4", ""),
                        exp_month=int(card.get("exp_month", 0)), exp_year=int(card.get("exp_year", 0)),
                        customer_id=cust if isinstance(cust, str) else (cust or {}).get("id"))

    def detach(self, pm_id: str) -> None:
        try:
            self._c.v1.payment_methods.detach(pm_id)
        except self._stripe.InvalidRequestError:
            pass                                  # ja desanexado: o objetivo (cartao fora) esta cumprido
        except self._stripe.StripeError as e:
            raise PaymentError(type(e).__name__) from e

    def create_purchase_intent(self, customer_id: str, amount: int, metadata: dict, key: str) -> IntentInfo:
        o = self._call(self._c.v1.payment_intents.create, params={
            "amount": amount, "currency": settings_currency(), "customer": customer_id,
            "setup_future_usage": "off_session",     # guarda o cartao para a renovacao automatica
            "automatic_payment_methods": {"enabled": True}, "metadata": metadata,
        }, options={"idempotency_key": key})
        return self._intent(o)

    def retrieve_payment_intent(self, intent_id: str) -> IntentInfo:
        return self._intent(self._call(self._c.v1.payment_intents.retrieve, intent_id))

    def charge_off_session(self, customer_id: str, pm_id: str, amount: int, metadata: dict, key: str) -> IntentInfo:
        try:
            o = self._c.v1.payment_intents.create(params={
                "amount": amount, "currency": settings_currency(), "customer": customer_id,
                "payment_method": pm_id, "off_session": True, "confirm": True, "metadata": metadata,
            }, options={"idempotency_key": key})
            return self._intent(o)
        except self._stripe.CardError as e:          # recusado / exige autenticacao: falha DEFINITIVA desta tentativa
            pi = (getattr(e, "error", None) and getattr(e.error, "payment_intent", None)) or {}
            return IntentInfo(id=(pi.get("id") if pi else "") or "", status="failed", amount=amount,
                              customer_id=customer_id, payment_method_id=pm_id,
                              error_code=(getattr(e, "code", None) or "card_error"))
        except self._stripe.StripeError as e:
            raise PaymentError(type(e).__name__) from e

    def parse_webhook(self, payload: bytes, signature: str) -> dict:
        if not settings.STRIPE_WEBHOOK_SECRET:
            raise PaymentError("STRIPE_WEBHOOK_SECRET ausente")
        try:
            ev = self._stripe.Webhook.construct_event(payload, signature, settings.STRIPE_WEBHOOK_SECRET)
        except Exception as e:                      # assinatura invalida / payload malformado
            raise PaymentError("BAD_SIGNATURE") from e
        return ev.to_dict() if hasattr(ev, "to_dict") else dict(ev)


def settings_currency() -> str:
    return "brl"


_provider: PaymentProvider | None = None


def get_provider() -> PaymentProvider:
    global _provider
    if _provider is None:
        _provider = StripeProvider()
    return _provider


def set_provider(p: PaymentProvider | None) -> None:
    """Para testes."""
    global _provider
    _provider = p
