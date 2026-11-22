from datetime import datetime
from typing import Annotated
from pydantic import BaseModel, Field, StringConstraints, field_validator, model_validator
from .modules import LevelBFeature

Hex64 = Annotated[str, StringConstraints(pattern=r"^[0-9a-f]{64}$")]
Token = Annotated[str, StringConstraints(min_length=10, max_length=2048)]


class Integrity(BaseModel):
    manifest_version: int = Field(default=0, ge=0, le=10**9)
    files_root: Hex64        # raiz Merkle / hash do conjunto de arquivos
    file_count: int = Field(ge=0, le=10**6)   # contagem esperada de arquivos na pasta
    exe_hash: Hex64


class Credentials(BaseModel):
    email: Annotated[str, StringConstraints(min_length=3, max_length=254)]
    password: Annotated[str, StringConstraints(min_length=1, max_length=256)]
    # CPF NAO e mais fator de login: continua cadastrado na conta, mas aqui e opcional e ignorado
    # (clientes antigos que ainda enviam o campo seguem funcionando).
    cpf: Annotated[str, StringConstraints(max_length=14)] | None = None


class TelemetryIn(BaseModel):
    """Dados que o programa informa para antifraude. O IP NAO vem daqui: o servidor o observa."""
    app_version: Annotated[str, StringConstraints(max_length=32)] = ""
    os_version: Annotated[str, StringConstraints(max_length=64)] = ""
    hostname: Annotated[str, StringConstraints(max_length=64)] = ""
    cpu: Annotated[str, StringConstraints(max_length=96)] = ""
    locale: Annotated[str, StringConstraints(max_length=16)] = ""
    timezone: Annotated[str, StringConstraints(max_length=48)] = ""


class LoginRequest(Credentials):
    telemetry: TelemetryIn | None = None
    fingerprint: Hex64
    client_version: Annotated[str, StringConstraints(max_length=32)] = ""
    integrity: Integrity
    totp: Annotated[str, StringConstraints(min_length=6, max_length=6)] | None = None
    is_vm: bool = False
    vm_vendor: Annotated[str, StringConstraints(max_length=64)] = ""

    @field_validator("totp", mode="before")
    @classmethod
    def _empty_totp_is_none(cls, v):
        # Clientes que enviam "totp":"" (campo vazio no formulario) nao devem levar 422;
        # o fluxo normal responde TOTP_REQUIRED / ENROLL_2FA.
        if isinstance(v, str) and v.strip() == "":
            return None
        return v.strip() if isinstance(v, str) else v


class TwoFASetupRequest(Credentials):
    pass


class TwoFASetupResponse(BaseModel):
    secret: str
    otpauth_uri: str


class TwoFAConfirmRequest(Credentials):
    totp: Annotated[str, StringConstraints(min_length=6, max_length=6)]


class LoginResponse(BaseModel):
    ok: bool = True                  # contrato com o cliente C++ (JsonGetBool("ok"))
    license_token: str
    expires_at: str
    license_id: int = 0              # o cliente usa no HKDF de s e na verificacao de K/passe
    user_id: int = 0
    must_change_password: bool = False
    server_pubkey_id: str = "k1"
    dll_key: str | None = None  # 32 bytes hex entregues apenas se versao for current/allowed
    vm_allow_token: str | None = None  # Token emitido se VM for autorizada em allowlist (M6)


class LogoutRequest(BaseModel):
    license_token: Token


class HeartbeatRequest(BaseModel):
    license_token: Token
    telemetry: TelemetryIn | None = None
    fingerprint: Hex64
    integrity: Integrity
    is_vm: bool = False
    vm_vendor: Annotated[str, StringConstraints(max_length=64)] = ""


class LicenseCheckRequest(BaseModel):
    """Nivel B (detalhamento, memorial): gate de licenca online, sem keystone."""
    license_token: Token
    fingerprint: Hex64
    integrity: Integrity
    feature: LevelBFeature
    nonce: Annotated[str, StringConstraints(pattern=r"^[0-9a-f]{16,64}$")]
    is_vm: bool = False
    vm_vendor: Annotated[str, StringConstraints(max_length=64)] = ""


class LicenseCheckResponse(BaseModel):
    ok: bool
    reason: str | None = None
    pass_sig: str | None = None   # Ed25519 sobre (feature, license_id, nonce, exp); cliente verifica
    exp: int | None = None        # Segundos Unix reais para conferência de assinatura criptográfica
    watermark_token: str | None = None  # Token de marca d'agua forense assinado pelo servidor (M6)


class HeartbeatResponse(BaseModel):
    ok: bool = True
    license_token: str
    expires_at: str
    license_id: int = 0
    user_id: int = 0
    status: str
    vm_allow_token: str | None = None


# --- modelo estrutural (minimo para recompor o lado aplicado) ---
class NodeIn(BaseModel):
    id: int
    x: float
    y: float
    z: float


class MemberIn(BaseModel):
    id: int
    node_i: int
    node_j: int
    rotation_deg: float = 0.0      # giro da secao em torno do eixo x local (graus)

    @model_validator(mode="before")
    @classmethod
    def _client_alias(cls, d):
        # O cliente C++ envia "section_rotation_deg" (SolverBridge::buildModelJsonAndReactions).
        if isinstance(d, dict) and "rotation_deg" not in d and "section_rotation_deg" in d:
            d = {**d, "rotation_deg": d["section_rotation_deg"]}
        return d


class NodalLoadIn(BaseModel):
    node_id: int
    F: list[float] = Field(min_length=3, max_length=3)   # Fx Fy Fz (global)
    M: list[float] = Field(default=[0, 0, 0], min_length=3, max_length=3)  # Mx My Mz (global)

    @model_validator(mode="before")
    @classmethod
    def _client_values(cls, d):
        # O cliente C++ envia {"node_id", "values":[Fx,Fy,Fz,Mx,My,Mz]} em vez de F/M separados.
        if isinstance(d, dict) and "F" not in d and isinstance(d.get("values"), list) and len(d["values"]) == 6:
            v = d["values"]
            d = {"node_id": d.get("node_id"), "F": v[:3], "M": v[3:]}
        return d


class MemberLoadIn(BaseModel):
    member_id: int
    coord_system: int = 1          # 0 local, 1 global (esqueleto trata global)
    q: list[float] = Field(min_length=3, max_length=3)   # qx qy qz (forca/comprimento)


class MemberMomentIn(BaseModel):
    member_id: int
    coord_system: int = 1            # 0 local, 1 global
    m: list[float] = Field(min_length=3, max_length=3)   # mx my mz (momento/comprimento, uniforme)


class MemberActionIn(BaseModel):
    member_id: int
    kind: int                        # 0 trapezoidal/parcial, 1 forca pontual, 2 momento pontual, 3 def. imposta
    coord_system: int = 1            # 0 local, 1 global (kind 3 sempre local)
    a: float = 0.0                   # posicao a partir do no I (unidade de comprimento do modelo)
    b: float | None = None           # so kind 0 (fim do trecho); None => fim da barra (L)
    v1: list[float] = Field(min_length=3, max_length=3)
    v2: list[float] = Field(default=[0.0, 0.0, 0.0], min_length=3, max_length=3)  # so kind 0


class ModelIn(BaseModel):
    nodes: list[NodeIn]
    members: list[MemberIn]
    nodal_loads: list[NodalLoadIn] = []
    member_loads: list[MemberLoadIn] = []
    member_moments: list[MemberMomentIn] = []
    member_actions: list[MemberActionIn] = []


class ReactionTotals(BaseModel):
    F: list[float] = Field(min_length=3, max_length=3)         # soma das reacoes (global)
    M_origin: list[float] = Field(min_length=3, max_length=3)  # soma dos momentos de reacao em torno da origem


class ComputeValidateRequest(BaseModel):
    license_token: str
    nonce: str
    fingerprint: str
    integrity: Integrity
    model_hash: str
    model: ModelIn | None = None     # enviado so se o servidor nao tiver model_hash em cache
    reaction_totals: ReactionTotals
    result_digest: str
    is_vm: bool = False
    vm_vendor: Annotated[str, StringConstraints(max_length=64)] = ""


class ComputeValidateResponse(BaseModel):
    ok: bool
    K: str | None = None
    ts: int | None = None   # Segundos Unix reais para conferência de assinatura criptográfica
    exp: int | None = None  # Segundos Unix reais para conferência de assinatura criptográfica e killswitch
    reason: str | None = None
    watermark_token: str | None = None  # Token de marca d'agua forense assinado pelo servidor (M6)
    vm_allow_token: str | None = None   # Token de autorizacao de VM se verificado em allowlist (M6)


class AdminVMAllowlistRequest(BaseModel):
    license_id: int
    fingerprint: Hex64
    notes: Annotated[str, StringConstraints(max_length=256)] = ""


class AdminLicenseStatusRequest(BaseModel):
    status: Annotated[str, StringConstraints(pattern=r"^(ativa|suspensa|cancelada|revogada|expirada)$")] | None = None
    current_period_end: datetime | None = None
    add_days: int | None = None
    reason: Annotated[str, StringConstraints(max_length=256)] = ""


# ---------------- M7: Modelos de Painel Administrativo ----------------
class AdminLoginRequest(Credentials):
    totp: Annotated[str, StringConstraints(pattern=r"^\d{6}$")]


class AdminLoginResponse(BaseModel):
    admin_token: str
    expires_at: str
    user_id: int
    email: str


class AdminCreateLicenseRequest(BaseModel):
    user_id: int | None = None
    email: str | None = None
    plan: Annotated[str, StringConstraints(max_length=64)] = "mensal"
    days: int = Field(default=30, ge=1, le=3650)
    status: Annotated[str, StringConstraints(pattern=r"^(ativa|suspensa|revogada|expirada)$")] = "ativa"


class AdminCreateUserRequest(BaseModel):
    email: Annotated[str, StringConstraints(min_length=3, max_length=254)]
    cpf: Annotated[str, StringConstraints(min_length=11, max_length=14)]
    plan: Annotated[str, StringConstraints(max_length=64)] = "mensal"
    days: int = Field(default=30, ge=1, le=3650)
    # Opcional: se omitida, o servidor gera uma senha temporaria forte.
    password: Annotated[str, StringConstraints(min_length=7, max_length=256)] | None = None


class AdminCreateUserResponse(BaseModel):
    user_id: int
    license_id: int
    email: str
    temporary_password: str          # exibida UMA vez; o usuario e obrigado a troca-la no primeiro login
    must_change_password: bool = True
    twofa: str = "pending_first_login"


class AdminResetPasswordResponse(BaseModel):
    user_id: int
    temporary_password: str


class ChangePasswordRequest(Credentials):
    new_password: Annotated[str, StringConstraints(min_length=7, max_length=256)]


class AdminLicenseResponse(BaseModel):
    id: int
    user_id: int
    status: str
    current_period_end: datetime
    plan: str
    created_at: datetime
    devices_count: int = 0
    user_email: str | None = None


class AdminDeviceResponse(BaseModel):
    id: int
    license_id: int
    fingerprint_hash: str
    active: bool
    bound_at: datetime
    last_heartbeat_at: datetime
    released_at: datetime | None = None


class AdminReviewAbuseFlagRequest(BaseModel):
    status: Annotated[str, StringConstraints(pattern=r"^(aberto|revisado|resolvido|ignorado)$")]
    notes: Annotated[str, StringConstraints(max_length=256)] | None = None


class AdminAbuseFlagResponse(BaseModel):
    id: int
    license_id: int
    kind: str
    score: float
    details: str
    status: str
    ts: datetime


class AdminVMAllowlistEntryResponse(BaseModel):
    ok: bool = True
    id: int
    license_id: int
    fingerprint_hash: str
    notes: str
    active: bool
    created_at: datetime


class AdminUserResponse(BaseModel):
    id: int
    email: str
    status: str
    is_admin: bool
    totp_enabled: bool
    created_at: datetime
    licenses_count: int = 0
    must_change_password: bool = False
    cpf_masked: str = ""          # CPF fica guardado na conta; a lista mostra so os 2 ultimos digitos


class AdminRecentLogin(BaseModel):
    id: int
    actor: str
    action: str
    target: str
    ts: datetime


class AdminMetricsLicenses(BaseModel):
    total: int
    active: int
    suspended: int
    revoked: int
    expired: int


class AdminMetricsDevices(BaseModel):
    total: int
    active: int


class AdminMetricsAbuse(BaseModel):
    total: int
    total_open: int
    open_7d: int


class AdminMetricsResponse(BaseModel):
    licenses: AdminMetricsLicenses
    devices: AdminMetricsDevices
    abuse: AdminMetricsAbuse
    recent_logins: list[AdminRecentLogin]



class AdminTelemetryResponse(BaseModel):
    id: int
    ts: datetime
    event: str
    user_id: int | None = None
    license_id: int | None = None
    email: str = ""
    ip: str = ""
    fingerprint: str = ""
    app_version: str = ""
    os_version: str = ""
    hostname: str = ""
    cpu: str = ""
    locale: str = ""
    timezone: str = ""
    is_vm: bool = False
    vm_vendor: str = ""
    user_agent: str = ""
    detail: str = ""


class AdminFraudSignal(BaseModel):
    kind: str          # ip_many_accounts | account_many_ips | account_many_machines | failed_burst
    subject: str       # ip, e-mail ou licenca
    count: int
    detail: str = ""
