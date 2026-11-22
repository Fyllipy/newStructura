"""Cache de resultantes aplicadas de modelos (SPEC §6, §10).

Suporta Redis (quando configurado via REDIS_URL) com fallback gracioso para memória local (LRU/TTL).
Estrutura armazenada: (applied_F, applied_M) onde F e M sao listas de 3 floats.
"""
from __future__ import annotations
import json
import logging
import time
from typing import Tuple, List, Optional
from .config import settings

logger = logging.getLogger(__name__)

# Fallback em memória (processo local)
_LOCAL_CACHE: dict[str, tuple[list[float], list[float]]] = {}
_LOCAL_NONCE_CACHE: dict[str, float] = {}
_REDIS_CLIENT = None
_REDIS_INITIALIZED = False


def _get_redis():
    global _REDIS_CLIENT, _REDIS_INITIALIZED
    if not _REDIS_INITIALIZED:
        _REDIS_INITIALIZED = True
        if settings.REDIS_URL:
            try:
                import redis
                _REDIS_CLIENT = redis.Redis.from_url(settings.REDIS_URL, decode_responses=True)
                _REDIS_CLIENT.ping()
            except Exception as e:
                logger.warning("Falha ao conectar no Redis (%s). Usando fallback em memoria: %s", settings.REDIS_URL, e)
                _REDIS_CLIENT = None
    return _REDIS_CLIENT


def get_model_cache(model_hash: str) -> Optional[Tuple[List[float], List[float]]]:
    r = _get_redis()
    if r is not None:
        try:
            val = r.get(f"structura:model:{model_hash}")
            if val:
                data = json.loads(val)
                return data["F"], data["M"]
        except Exception as e:
            logger.warning("Erro de leitura no Redis: %s", e)
    return _LOCAL_CACHE.get(model_hash)


def set_model_cache(model_hash: str, applied_F: List[float], applied_M: List[float]) -> None:
    _LOCAL_CACHE[model_hash] = (applied_F, applied_M)
    r = _get_redis()
    if r is not None:
        try:
            val = json.dumps({"F": applied_F, "M": applied_M})
            r.set(f"structura:model:{model_hash}", val, ex=settings.MODEL_CACHE_TTL_SECONDS)
        except Exception as e:
            logger.warning("Erro de escrita no Redis: %s", e)


def clear_model_cache() -> None:
    _LOCAL_CACHE.clear()
    _LOCAL_NONCE_CACHE.clear()
    r = _get_redis()
    if r is not None:
        try:
            # Em testes, limpa as chaves com prefixo
            keys = r.keys("structura:model:*") + r.keys("structura:nonce:*")
            if keys:
                r.delete(*keys)
        except Exception as e:
            logger.warning("Erro ao limpar Redis: %s", e)


def check_and_record_nonce(license_id: int, nonce: str, ttl_seconds: int = 300) -> bool:
    """Verifica e registra o nonce para a licença (anti-replay dentro da janela de validade).
    Retorna True se o nonce é NOVO e foi registrado com sucesso.
    Retorna False se o nonce já foi utilizado (ataque de replay detectado).
    """
    if not nonce:
        return False
    r = _get_redis()
    key = f"structura:nonce:{license_id}:{nonce}"
    if r is not None:
        try:
            # set com nx=True só grava se a chave não existir
            res = r.set(key, "1", nx=True, ex=ttl_seconds)
            return bool(res)
        except Exception as e:
            logger.warning("Erro no Redis para nonce anti-replay: %s", e)
    # Fallback em memória com expiração por TTL
    now = time.time()
    # Limpeza periódica de chaves expiradas
    expired = [k for k, exp in _LOCAL_NONCE_CACHE.items() if exp <= now]
    for k in expired:
        _LOCAL_NONCE_CACHE.pop(k, None)

    local_key = f"{license_id}:{nonce}"
    if local_key in _LOCAL_NONCE_CACHE:
        if _LOCAL_NONCE_CACHE[local_key] > now:
            return False  # Replay!
    _LOCAL_NONCE_CACHE[local_key] = now + ttl_seconds
    return True

