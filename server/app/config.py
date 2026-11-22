import base64
import os

from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict

_KEY_FIELDS = ("SERVER_SIGNING_SEED_B64", "CPF_ENC_KEY_B64", "CPF_HMAC_KEY_B64")
# Chave PUBLICA de release (assina manifestos). A privada fica so no CI/maquina de build.
_PUBLIC_KEY_FIELDS = ("RELEASE_PUBKEY_B64",)


def _decode_b64url(value: str) -> bytes:
    return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))


class SecretConfigError(RuntimeError):
    """Erro de configuracao de segredo. A mensagem cita SO o nome do campo, nunca o valor."""


_server_dir = Path(__file__).resolve().parents[1]
_env_file = str(_server_dir / ".env") if (_server_dir / ".env").is_file() else ".env"
if os.environ.get("DATABASE_URL") == "sqlite://" and not any(k.endswith("_B64") for k in os.environ):
    _env_file = None


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=_env_file, extra="ignore")

    DATABASE_URL: str = "sqlite:///./dev.db"
    SERVER_SIGNING_SEED_B64: str = ""   # 32 bytes base64url (Ed25519) — obrigatorio
    CPF_ENC_KEY_B64: str = ""           # 32 bytes base64url (AES-256-GCM)
    CPF_HMAC_KEY_B64: str = ""          # 32 bytes base64url (HMAC-SHA256)
    RELEASE_PUBKEY_B64: str = ""        # pub Ed25519 de release (manifesto) — obrigatoria
    GATE_PASS_TTL_SECONDS: int = 60     # validade do passe do Nivel B (/license/check)
    REDIS_URL: str = ""                 # ex: redis://localhost:6379/0 (opcional; fallback para memoria/db)
    MODEL_CACHE_TTL_SECONDS: int = 86400  # 24 horas de cache para o modelo aplicado
    # Token: validade 8 dias (> ciclo semanal de heartbeat). Revogacao NAO depende do TTL:
    # todo uso valida device/licenca/usuario no DB (kill switch e logout efeito imediato).
    TOKEN_TTL_SECONDS: int = 8 * 86400
    LOCKOUT_THRESHOLD: int = 5          # falhas consecutivas ate bloquear a conta
    LOCKOUT_MINUTES: int = 15
    REQUIRE_2FA: bool = True            # SPEC §16.5: login exige TOTP
    TOTP_WINDOW: int = 1                # +-30 s
    # IP real so e confiavel atras de proxy reverso/CDN proprio. Com False (padrao) o X-Forwarded-For
    # e IGNORADO: um cliente poderia forja-lo para fugir de velocity/concorrencia/limite por IP.
    TRUST_PROXY_HEADERS: bool = False
    AUTH_FAIL_LIMIT_PER_IP: int = 30    # falhas de credencial por IP na janela (0 desliga)
    AUTH_FAIL_WINDOW_SECONDS: int = 300
    MIN_PASSWORD_LEN: int = 7
    TELEMETRY_RETENTION_DAYS: int = 180   # LGPD: expurgo automatico
    EXPOSE_DOCS: bool = False           # /docs, /redoc, /openapi.json: so em desenvolvimento
    ADMIN_TOKEN_TTL_SECONDS: int = 1800 # sessao do painel: 30 min
    AUTO_SUSPEND_FLAG_THRESHOLD: int = 5   # sinalizacoes de abuso em 7 dias que suspendem a licenca
    DEVICE_STALE_DAYS: int = 7          # libera vinculo preso apos N dias sem heartbeat
    EQUILIBRIUM_REL_TOL: float = 1e-6
    EQUILIBRIUM_ABS_FLOOR: float = 1e-3

    def missing_secrets(self) -> list[str]:
        return [f for f in _KEY_FIELDS if not getattr(self, f)]

    def validate_secrets(self) -> None:
        """Fail-closed no startup: segredo ausente ou malformado => servidor nao sobe.
        (Validacao fora do pydantic de proposito: ValidationError ecoaria o valor no log.)"""
        for f in _KEY_FIELDS + _PUBLIC_KEY_FIELDS:
            v = getattr(self, f)
            if not v:
                raise SecretConfigError(f"{f} ausente")
            try:
                ok = len(_decode_b64url(v)) == 32
            except Exception:
                ok = False
            if not ok:
                raise SecretConfigError(f"{f} invalido (esperado base64url de 32 bytes)")


settings = Settings()
