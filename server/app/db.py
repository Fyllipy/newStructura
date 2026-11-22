from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base
from sqlalchemy.pool import StaticPool
from .config import settings, _server_dir

_url = settings.DATABASE_URL
if _url.startswith("sqlite"):
    kwargs = {"connect_args": {"check_same_thread": False}}
    if _url in ("sqlite://", "sqlite:///:memory:"):
        kwargs["poolclass"] = StaticPool  # DB em memoria compartilhado (testes)
    elif _url.startswith("sqlite:///."):
        rel = _url[len("sqlite:///."):].lstrip("/\\")
        abs_db = (_server_dir / rel).resolve().as_posix()
        _url = f"sqlite:///{abs_db}"
    engine = create_engine(_url, **kwargs)
else:
    engine = create_engine(_url, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)
Base = declarative_base()


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
