# Structura Licensing — Servidor

API de licenciamento e validação por equilíbrio. Base: `../SPEC.md` e `../PLAN.md`.
Documentação completa: `../docs/LICENCIAMENTO_E_SERVIDOR.md`, `../docs/PAINEL_ADMIN.md`, `../docs/DEPLOY_SERVIDOR.md`.
Progresso por marco: ver `REVIEW.md` (relatório adversarial) e `../PLAN.md`.

## Rodar (dev)

```powershell
cd server
python -m venv .venv ; .venv\Scripts\activate        # Linux/mac: source .venv/bin/activate
pip install -r requirements.txt -r requirements-dev.txt
python gen_keys.py --write      # cria .env (0600) com chaves novas; RECUSA sobrescrever
python -m app.seed              # usuário+licença demo (apenas dev)
uvicorn app.main:app --reload   # docs: http://127.0.0.1:8000/docs
```

`gen_keys.py` imprime também `SERVER_PUBKEY_B64` — a **única** chave que vai embutida no cliente.
Sem `--write`, imprime as linhas para colar no secret manager.

### Problemas comuns ao rodar os comandos acima no Windows

- **`;` no cmd.exe (Prompt de Comando):** o `;` é separador de comando só no PowerShell e no bash. No cmd.exe
  ele é passado como argumento literal, e `python -m venv .venv ; .venv\Scripts\activate` tenta criar um venv
  chamado `;` (erro: *"Refusing to create a venv ... because it contains the PATH separator"*). Solução: rode
  cada comando em uma linha separada, ou use `activate.bat` em vez de `activate` se estiver no cmd.exe.
- **`Activate.ps1` bloqueado por política de execução do PowerShell:** erro `UnauthorizedAccess` /
  `PSSecurityException` ao rodar `.venv\Scripts\activate`. Duas soluções:
  - Sem alterar nada no sistema: não ative o venv, chame `.venv\Scripts\python.exe` diretamente em cada comando
    (ex.: `.venv\Scripts\python.exe -m pip install -r requirements.txt -r requirements-dev.txt`).
  - Para liberar `activate` permanentemente (só para o seu usuário, sem precisar de admin):
    `Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned`.

## Testes

```powershell
cd server ; pytest -q
```

Os testes geram chaves efêmeras e usam SQLite em memória: não precisam de `.env`.

## Segredos
- `.env` (e `*.db`, `*.pem`, `*.key`) estão no `.gitignore`; só `.env.example` (vazio) é versionado.
- Em produção: variáveis de ambiente do provedor (Render/Fly secrets, Oracle Vault). Nunca no binário do cliente.
- O servidor **falha fechado**: chave ausente → erro ao usar; chave malformada → não sobe.

## CI (`../.github/workflows/server-ci.yml`)
pytest + `pip-audit` + Semgrep + gitleaks + Trivy (todos gratuitos).

## Hospedagem
Ver `DEPLOY.md` (criado no M9; rascunho dos requisitos ali).

## Interface Web Administrativa (Painel Admin)

A pasta `admin-ui` contém a interface gráfica React 19 + TypeScript + Vite.

- **URL no servidor:** `http://127.0.0.1:8000/admin` (servida estaticamente via FastAPI)
- **Dev com Hot-Reload:** `cd admin-ui && npm run dev` (porta 5173 com proxy para FastAPI)
- **Compilar para Produção:** `cd admin-ui && npm run build` (gera `admin-ui/dist/`)
- **Testes E2E (Playwright):** `cd admin-ui && npm run test:e2e`

Ver detalhes completos em `admin-ui/README.md`.

## Politicas e endurecimentos (auditoria 2026-11)
- Senha: minimo 7 caracteres (`MIN_PASSWORD_LEN`). Admin: `python -m app.create_admin` (senha por prompt oculto/`STRUCTURA_ADMIN_PASSWORD`).
- Auto-suspensao: 5 sinalizacoes de abuso em 7 dias (`AUTO_SUSPEND_FLAG_THRESHOLD`); aviso `abuse_warning` na 4a.
- Limite de falhas por IP (`AUTH_FAIL_LIMIT_PER_IP`/`AUTH_FAIL_WINDOW_SECONDS`) e `TRUST_PROXY_HEADERS` (so atras de proxy proprio).
- `GET /health` para monitoramento/cliente. Contrato com o cliente C++: `tests/test_m10_client_contract.py`.
- Cliente: pinning TLS em `client/license_config.cpp::GetServerCertPins()` (vazio = desativado); em Release o
  override `STRUCTURA_LICENSE_SERVER` so aceita `https://`.

## Comandos úteis (dentro de `server/`)
```powershell
.venv\Scripts\python.exe -m app.create_admin --email admin@exemplo.com --cpf 00000000000
.venv\Scripts\python.exe -m app.create_user  --email cliente@exemplo.com --cpf 00000000000 --days 30
.venv\Scripts\python.exe -m app.unlock_user  cliente@exemplo.com
.venv\Scripts\python.exe -m app.manifest_cli list
```
Deploy em produção: `deploy/oracle/DEPLOY.md` e `../docs/DEPLOY_SERVIDOR.md`. Painel: `admin-ui/` (`npm run build` antes de empacotar).
