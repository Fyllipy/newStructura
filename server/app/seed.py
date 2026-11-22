"""Cria um usuario + licenca demo (APENAS DEV). Imprime o segredo TOTP uma vez.
Uso: python -m app.seed"""
from .db import Base, engine, SessionLocal
from .models import User
from .users import create_user_with_license

Base.metadata.create_all(bind=engine)

DEMO_EMAIL = "demo@exemplo.com"
DEMO_PASSWORD = "senha-demo-123"
DEMO_CPF = "12345678909"


def main():
    db = SessionLocal()
    try:
        if db.query(User).filter(User.email == DEMO_EMAIL).first():
            print("Usuario demo ja existe.")
            return
        p = create_user_with_license(db, DEMO_EMAIL, DEMO_PASSWORD, DEMO_CPF)
        db.commit()
        print(f"Criado: {DEMO_EMAIL} / {DEMO_PASSWORD}")
        print(f"TOTP secret (cadastre no autenticador): {p.totp_secret}")
    finally:
        db.close()


if __name__ == "__main__":
    main()
