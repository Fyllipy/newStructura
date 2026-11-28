from datetime import datetime
from sqlalchemy import (String, Integer, Float, Boolean, DateTime, ForeignKey, Text, Index,
                        event, text)
from sqlalchemy.orm import Mapped, mapped_column, relationship
from .db import Base
from .timeutil import utcnow


class User(Base):
    __tablename__ = "users"
    id: Mapped[int] = mapped_column(primary_key=True)
    email: Mapped[str] = mapped_column(String(255), unique=True, index=True)
    password_hash: Mapped[str] = mapped_column(String(255))
    cpf_enc: Mapped[str] = mapped_column(String(255))
    cpf_hmac: Mapped[str] = mapped_column(String(64), index=True)
    totp_secret_enc: Mapped[str | None] = mapped_column(String(255), nullable=True)
    totp_enabled: Mapped[bool] = mapped_column(Boolean, default=False)
    totp_last_step: Mapped[int] = mapped_column(Integer, default=0)  # anti-replay
    failed_logins: Mapped[int] = mapped_column(Integer, default=0)
    locked_until: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    status: Mapped[str] = mapped_column(String(32), default="ativa")
    is_admin: Mapped[bool] = mapped_column(Boolean, default=False)
    must_change_password: Mapped[bool] = mapped_column(Boolean, default=False)   # senha temporaria do admin
    # Cobranca (Stripe). So ids e dados de exibicao do cartao (marca, ultimos 4, validade): o numero NUNCA toca o servidor.
    stripe_customer_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    card_pm_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    card_brand: Mapped[str | None] = mapped_column(String(24), nullable=True)
    card_last4: Mapped[str | None] = mapped_column(String(4), nullable=True)
    card_exp_month: Mapped[int | None] = mapped_column(Integer, nullable=True)
    card_exp_year: Mapped[int | None] = mapped_column(Integer, nullable=True)
    auto_renew: Mapped[bool] = mapped_column(Boolean, default=False)     # renovacao automatica no vencimento
    totp_pending_enc: Mapped[str | None] = mapped_column(String(255), nullable=True)   # novo 2FA aguardando confirmacao
    created_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)
    licenses: Mapped[list["License"]] = relationship(back_populates="user")


class License(Base):
    __tablename__ = "licenses"
    id: Mapped[int] = mapped_column(primary_key=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"))
    status: Mapped[str] = mapped_column(String(32), default="ativa")  # ativa|expirada|suspensa|revogada
    current_period_end: Mapped[datetime] = mapped_column(DateTime)
    plan: Mapped[str] = mapped_column(String(64), default="mensal")
    created_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)
    renew_attempts: Mapped[int] = mapped_column(Integer, default=0)       # cobrancas de renovacao que falharam
    last_renew_attempt_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    user: Mapped[User] = relationship(back_populates="licenses")
    devices: Mapped[list["Device"]] = relationship(back_populates="license")


class Device(Base):
    __tablename__ = "devices"
    id: Mapped[int] = mapped_column(primary_key=True)
    license_id: Mapped[int] = mapped_column(ForeignKey("licenses.id"))
    fingerprint_hash: Mapped[str] = mapped_column(String(64), index=True)
    active: Mapped[bool] = mapped_column(Boolean, default=True)
    bound_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)
    last_heartbeat_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)
    released_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    license: Mapped[License] = relationship(back_populates="devices")

    # Garantia no BANCO de 1 dispositivo ativo por licenca (corrida entre logins simultaneos).
    __table_args__ = (
        Index("uq_one_active_device", "license_id", unique=True,
              sqlite_where=text("active"), postgresql_where=text("active")),
    )


class Payment(Base):
    __tablename__ = "payments"
    id: Mapped[int] = mapped_column(primary_key=True)
    license_id: Mapped[int] = mapped_column(ForeignKey("licenses.id"))
    provider: Mapped[str] = mapped_column(String(32))
    provider_ref: Mapped[str] = mapped_column(String(128), unique=True)
    amount: Mapped[float] = mapped_column(Float)
    status: Mapped[str] = mapped_column(String(32))
    paid_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)


class Plan(Base):
    """Tabela de precos das licencas (editada no painel admin). Preco em centavos de real."""
    __tablename__ = "plans"
    code: Mapped[str] = mapped_column(String(32), primary_key=True)          # mensal | trimestral | ...
    name: Mapped[str] = mapped_column(String(64))
    period_days: Mapped[int] = mapped_column(Integer)
    price_cents: Mapped[int] = mapped_column(Integer, default=0)
    active: Mapped[bool] = mapped_column(Boolean, default=False)            # so planos ativos aparecem na loja
    updated_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)


class Order(Base):
    """Compra iniciada na loja. Para cliente novo guarda os dados da conta ATE o pagamento confirmar
    (a conta so e criada depois de pago). Senha so como hash; CPF so cifrado."""
    __tablename__ = "orders"
    id: Mapped[int] = mapped_column(primary_key=True)
    public_id: Mapped[str] = mapped_column(String(32), unique=True, index=True)
    user_id: Mapped[int | None] = mapped_column(ForeignKey("users.id"), nullable=True)
    email: Mapped[str] = mapped_column(String(255), default="")
    cpf_enc: Mapped[str | None] = mapped_column(String(255), nullable=True)
    cpf_hmac: Mapped[str | None] = mapped_column(String(64), nullable=True)
    password_hash: Mapped[str | None] = mapped_column(String(255), nullable=True)
    plan_code: Mapped[str] = mapped_column(String(32))
    amount_cents: Mapped[int] = mapped_column(Integer)
    provider_intent: Mapped[str] = mapped_column(String(128), unique=True)
    status: Mapped[str] = mapped_column(String(16), default="pending")       # pending | paid
    created_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)


class AuditLog(Base):
    __tablename__ = "audit_log"
    id: Mapped[int] = mapped_column(primary_key=True)
    actor: Mapped[str] = mapped_column(String(128))
    action: Mapped[str] = mapped_column(String(64))
    target: Mapped[str] = mapped_column(String(128))
    metadata_masked: Mapped[str] = mapped_column(Text, default="")
    ts: Mapped[datetime] = mapped_column(DateTime, default=utcnow)


@event.listens_for(AuditLog, "before_update")
@event.listens_for(AuditLog, "before_delete")
def _audit_immutable(mapper, connection, target):  # imutavel (camada da aplicacao)
    raise RuntimeError("audit_log e imutavel")


class IntegrityManifest(Base):
    """Manifesto assinado publicado. A assinatura e REVERIFICADA a cada carga (DB nao e confiavel).
    Armazena a DLL_RELEASE_KEY especifica desta versao para decifracao segura em repouso (M5)."""
    __tablename__ = "integrity_manifest"
    version: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=False)
    text: Mapped[str] = mapped_column(Text)
    sig: Mapped[str] = mapped_column(String(128))
    status: Mapped[str] = mapped_column(String(16), default="current")  # current|allowed|revoked
    dll_release_key: Mapped[str | None] = mapped_column(String(64), nullable=True)  # 32 bytes hex por versao
    published_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)


class AbuseFlag(Base):
    __tablename__ = "abuse_flags"
    id: Mapped[int] = mapped_column(primary_key=True)
    license_id: Mapped[int] = mapped_column(ForeignKey("licenses.id"), index=True)
    kind: Mapped[str] = mapped_column(String(64), index=True)  # concurrent_access, excessive_fingerprints, velocity, unauthorized_vm
    score: Mapped[float] = mapped_column(Float, default=1.0)
    details: Mapped[str] = mapped_column(Text, default="")
    status: Mapped[str] = mapped_column(String(32), default="aberto")
    ts: Mapped[datetime] = mapped_column(DateTime, default=utcnow, index=True)


class VMAllowlist(Base):
    """Allowlist de maquinas virtuais autorizadas pelo admin para clientes corporativos (M6)."""
    __tablename__ = "vm_allowlist"
    id: Mapped[int] = mapped_column(primary_key=True)
    license_id: Mapped[int] = mapped_column(ForeignKey("licenses.id"), index=True)
    fingerprint_hash: Mapped[str] = mapped_column(String(64), index=True)
    notes: Mapped[str] = mapped_column(String(255), default="")
    active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=utcnow)


class TelemetryEvent(Base):
    """Telemetria de antifraude: quem, de onde (IP observado pelo servidor), em que maquina/versao.
    Retencao limitada (settings.TELEMETRY_RETENTION_DAYS). Nunca guarda senha, CPF nem conteudo de projetos."""
    __tablename__ = "telemetry_events"
    id: Mapped[int] = mapped_column(primary_key=True)
    ts: Mapped[datetime] = mapped_column(DateTime, default=utcnow, index=True)
    event: Mapped[str] = mapped_column(String(32), index=True)   # login, login_failed, heartbeat, check, compute
    user_id: Mapped[int | None] = mapped_column(Integer, nullable=True, index=True)
    license_id: Mapped[int | None] = mapped_column(Integer, nullable=True, index=True)
    email: Mapped[str] = mapped_column(String(254), default="", index=True)   # tentado, em falhas
    ip: Mapped[str] = mapped_column(String(64), default="", index=True)
    fingerprint: Mapped[str] = mapped_column(String(64), default="")
    app_version: Mapped[str] = mapped_column(String(32), default="")
    os_version: Mapped[str] = mapped_column(String(64), default="")
    hostname: Mapped[str] = mapped_column(String(64), default="")
    cpu: Mapped[str] = mapped_column(String(96), default="")
    locale: Mapped[str] = mapped_column(String(16), default="")
    timezone: Mapped[str] = mapped_column(String(48), default="")
    is_vm: Mapped[bool] = mapped_column(Boolean, default=False)
    vm_vendor: Mapped[str] = mapped_column(String(64), default="")
    user_agent: Mapped[str] = mapped_column(String(128), default="")
    detail: Mapped[str] = mapped_column(String(128), default="")


class DeviceAccessLog(Base):
    """Registro de telemetria de acessos para analise de velocity, concorrencia e fingerprints (M6)."""
    __tablename__ = "device_access_logs"
    id: Mapped[int] = mapped_column(primary_key=True)
    license_id: Mapped[int] = mapped_column(ForeignKey("licenses.id"), index=True)
    fingerprint_hash: Mapped[str] = mapped_column(String(64))
    ip_address: Mapped[str] = mapped_column(String(64))
    action: Mapped[str] = mapped_column(String(32))  # login, heartbeat, compute, check
    ts: Mapped[datetime] = mapped_column(DateTime, default=utcnow, index=True)
