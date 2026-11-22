"""Limitador de falhas de autenticacao por IP (janela deslizante, em memoria).

Complementa o lockout POR CONTA: sem isto, um atacante pode tentar senhas/CPFs contra
muitas contas (credential stuffing) sem nunca travar nenhuma. So FALHAS contam, entao
usuarios legitimos nao sao afetados. Em producao com varias instancias, use Redis/WAF
(o limitador de borda e a defesa principal; este e a ultima linha, fail-safe)."""
from __future__ import annotations
import threading
import time
from collections import defaultdict, deque

from .config import settings

_lock = threading.Lock()
_failures: dict[str, deque[float]] = defaultdict(deque)


def _prune(q: deque[float], now: float) -> None:
    horizon = now - settings.AUTH_FAIL_WINDOW_SECONDS
    while q and q[0] <= horizon:
        q.popleft()


def is_blocked(ip: str) -> bool:
    if settings.AUTH_FAIL_LIMIT_PER_IP <= 0:
        return False
    now = time.time()
    with _lock:
        q = _failures.get(ip)
        if not q:
            return False
        _prune(q, now)
        return len(q) >= settings.AUTH_FAIL_LIMIT_PER_IP


def register_failure(ip: str) -> None:
    now = time.time()
    with _lock:
        q = _failures[ip]
        _prune(q, now)
        q.append(now)
        if len(_failures) > 10000:  # evita crescimento ilimitado sob varredura de IPs
            for k in [k for k, v in _failures.items() if not v or v[-1] <= now - settings.AUTH_FAIL_WINDOW_SECONDS]:
                _failures.pop(k, None)


def reset() -> None:
    with _lock:
        _failures.clear()
