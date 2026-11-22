# Licenciamento e servidor — guia de referência

Este documento descreve **como o licenciamento funciona hoje**: contas, login, 2FA, telemetria, o servidor
(FastAPI) e o cliente (C++). Para operar o painel veja `PAINEL_ADMIN.md`; para colocar o servidor no ar ou trocar
de hospedagem veja `DEPLOY_SERVIDOR.md`; para compilar veja `BUILD_E_RELEASE.md`. O mapa de segurança
(criptografia, anti-tamper, fail-closed) está em `../SECURITY.md`; o desenho original em `../SPEC.md`.

## 1. Visão geral

```
 Structura.exe (cliente)  ──HTTPS──►  Servidor FastAPI (server/app)  ──►  SQLite (ou PostgreSQL)
        │                                  │
        │  login / heartbeat / calcular    ├─ assina tokens e a chave K (Ed25519)
        │                                  ├─ confere integridade do EXE contra o manifesto assinado
        ▼                                  ├─ confere equilíbrio das reações (anti-crack do cálculo)
 client/ (StructuraClient.lib)             └─ grava telemetria / auditoria
 src/ui/LoginDialog.cpp
                                       Painel admin (React) servido pelo mesmo servidor em /admin/
```

Princípio: o cliente não decide nada. Qualquer falha (rede, licença, integridade, equilíbrio) **bloqueia** em vez
de liberar (*fail-closed*).

### Pastas
| Caminho | Conteúdo |
|---|---|
| `server/app/` | API (`main.py`), modelos, criptografia, telemetria, abuso, CLIs (`create_admin`, `create_user`, `unlock_user`, `manifest_cli`) |
| `server/admin-ui/` | Painel web (React 19 + Vite + TypeScript). O resultado compilado (`dist/`) é servido em `/admin/` |
| `server/deploy/` | Deploy: segredos de produção, pacote, scripts da VM Oracle, exportação de release |
| `server/tests/` | Testes `pytest` (M0–M14) |
| `client/` | Biblioteca do cliente de licenciamento (HTTP, tokens, integridade, gatekeeper) |
| `src/ui/LoginDialog.*` | Tela de login, assistente de 2FA e troca de senha |

## 2. Contas e login

| Dado | Regra |
|---|---|
| E-mail | Identificador de login (minúsculas, único) |
| Senha | Mínimo **7** caracteres (`MIN_PASSWORD_LEN`), hash Argon2id |
| **CPF** | **Cadastrado e guardado cifrado** (AES-256-GCM + HMAC para busca) na conta. **Não é usado no login** (nem no app, nem no painel). O campo `cpf` ainda é aceito e ignorado pela API para clientes antigos |
| 2FA (TOTP) | Obrigatório (`REQUIRE_2FA`). Configurado pelo próprio usuário no primeiro login |
| Licença | Uma conta pode ter várias; vale a mais recente utilizável. 1 dispositivo ativo por licença |

### Fluxo do primeiro acesso (conta criada pelo administrador)
1. O admin cria a conta no painel (ou `python -m app.create_user`). O servidor gera uma **senha temporária**
   (`must_change_password = true`), guarda o CPF cifrado e **não** cadastra 2FA.
2. O usuário abre o app e informa e-mail + senha temporária. O servidor responde `403 ENROLL_2FA`.
3. O app abre o **assistente de 2FA**: passo 1 instalar autenticador, passo 2 escanear o **QR Code** (ou digitar a
   chave), passo 3 digitar o código de 6 dígitos. Rotas: `POST /auth/2fa/setup` → `POST /auth/2fa/confirm`.
   A confirmação **não consome** o código; o login imediato com o mesmo código é que o consome.
4. Login concluído. Como a senha é temporária (`must_change_password`), o app abre a tela **"Defina sua senha
   definitiva"** (`POST /auth/password/change`, exige a senha atual). Só depois a janela principal abre. Cancelar
   faz logout (libera o vínculo do dispositivo).
5. Nos acessos seguintes: e-mail + senha + código 2FA.

### Códigos de erro (aparecem na tela como `[código: ...]`)
| Código | Significado / ação |
|---|---|
| `CREDENCIAIS` | E-mail/senha/2FA errados **ou conta bloqueada** (a resposta é uniforme de propósito, para não revelar quais e-mails existem). 5 falhas bloqueiam 15 min. Destravar: painel → Usuários → *Destravar* ou `python -m app.unlock_user <email>` |
| `TOTP_REQUIRED` | Falta o código 2FA |
| `ENROLL_2FA` | Conta sem 2FA: o assistente abre sozinho |
| `INTEGRITY_FAIL` | O EXE/arquivos instalados não batem com o manifesto publicado no servidor (instalação de outra versão, arquivo alterado, manifesto não publicado) |
| `CLIENT_OUTDATED` | Versão do app revogada: instalar a mais recente |
| `DEVICE_IN_USE` | A licença já está vinculada a outro computador (ou vínculo preso). Liberar: painel → Dispositivos, ou aguardar `DEVICE_STALE_DAYS` (7) |
| `LICENSE_INACTIVE` | Sem licença ativa (expirada, suspensa, inexistente) |
| `VM_NOT_ALLOWED` | Execução em máquina virtual sem estar na allowlist |
| `TOO_MANY_ATTEMPTS` (429) | Muitas falhas vindas do mesmo IP (`AUTH_FAIL_LIMIT_PER_IP`) |
| `SERVER_SIGNATURE_INVALID` | O servidor contatado não é o de produção (chave pública diferente da embutida no app) |
| `SERVER_OFFLINE_OR_TIMEOUT` | Sem conexão (5 s de timeout): cálculo e exportação ficam bloqueados |
| `COMPUTE_VALIDATE_REJECTED` | O servidor recusou o cálculo (HTTP ≠ 200). Ver `EQUILIBRIUM_FAIL`, `MODEL_INVALID`, etc. |

## 3. Cliente (C++)

- `client/license_client.cpp`: `ClientLogin`, `ClientHeartbeat`, `ClientTwoFASetup/Confirm`, `ClientChangePassword`,
  `RequestComputeKeystone`, `CheckFeatureGateLevelB`. Todas verificam a **assinatura Ed25519** do servidor
  (`kServerPubKey` em `client/license_config.cpp`); um servidor falso não consegue forjar o token.
- **Fingerprint** da máquina: SHA-256 de CPUID (sem o byte do núcleo lógico), serial do volume C: e MachineGuid.
  Estável entre execuções.
- **Sessão em disco** (`%APPDATA%\Structura\Structura-Metalicas\session.dat`): protegida por **DPAPI**
  (só o mesmo usuário Windows lê).
- **Endereço do servidor** (`GetLicenseServerUrl`): build de release usa a URL embutida
  (`STRUCTURA_PRODUCTION_SERVER_URL`); a variável de ambiente `STRUCTURA_LICENSE_SERVER` só é aceita se for `https://`
  (em build de desenvolvimento aceita qualquer URL). Padrão de dev: `http://127.0.0.1:8000`.
- **Chave pública do servidor**: dois valores em `license_config.cpp` — dev (`STRUCTURA_DEV_BUILD`) e produção.
- **Pinning de certificado TLS** (opcional): preencher `GetServerCertPins()` com os SHA-256 do SPKI; lista vazia = desligado.
- **JSON**: os extratores (`json_helper.h`) só leem chaves do nível superior.

### Contrato cliente ↔ servidor (cuidado ao mexer)
O cliente lê campos por nome. Alterar/remover quebra o login **silenciosamente**:
`LoginResponse`: `license_token`, `expires_at`, `license_id`, `user_id`, `must_change_password`, `dll_key`, `vm_allow_token`.
`HeartbeatResponse`: `license_token` (rotacionado), `expires_at`. `/compute/validate` recebe o modelo no formato
`{"nodes","members"(section_rotation_deg),"nodal_loads"([{node_id,values[6]}]),"member_loads"(coord_system sempre 1)}`.
Testes que protegem isso: `server/tests/test_m10_client_contract.py`, `test_m11_compute_contract.py`.

## 4. Servidor

### Configuração (`server/.env` ou variáveis de ambiente)
> Para entender cada segredo, como gerar e trocar: `SEGREDOS_E_CHAVES.md`.
| Variável | Padrão | Função |
|---|---|---|
| `DATABASE_URL` | `sqlite:///./dev.db` | Banco. Produção: `sqlite:////opt/structura/data/structura.db` ou PostgreSQL |
| `SERVER_SIGNING_SEED_B64` | — | Seed Ed25519 do servidor (**segredo**; define a chave pública embutida no app) |
| `CPF_ENC_KEY_B64`, `CPF_HMAC_KEY_B64` | — | Chaves de cifra/HMAC do CPF e do segredo TOTP (**segredos**) |
| `RELEASE_PUBKEY_B64` | — | Chave **pública** que valida os manifestos (a privada fica só na máquina de build) |
| `TRUST_PROXY_HEADERS` | `false` | `true` **somente** atrás de proxy próprio (Caddy/nginx): passa a usar o IP real do `X-Forwarded-For` |
| `AUTH_FAIL_LIMIT_PER_IP` / `AUTH_FAIL_WINDOW_SECONDS` | 30 / 300 | Limite de falhas por IP |
| `LOCKOUT_THRESHOLD` / `LOCKOUT_MINUTES` | 5 / 15 | Bloqueio de conta |
| `MIN_PASSWORD_LEN` | 7 | Política de senha |
| `AUTO_SUSPEND_FLAG_THRESHOLD` | 5 | Sinalizações de abuso em 7 dias que suspendem a licença |
| `TOKEN_TTL_SECONDS` | 691200 (8 d) | Validade do token de licença (heartbeat renova) |
| `ADMIN_TOKEN_TTL_SECONDS` | 1800 | Sessão do painel |
| `TELEMETRY_RETENTION_DAYS` | 180 | Expurgo automático da telemetria |
| `EXPOSE_DOCS` | `false` | Liga `/docs` e `/openapi.json` (só desenvolvimento) |
| `DEVICE_STALE_DAYS` | 7 | Libera vínculo preso após N dias sem heartbeat |
| `REDIS_URL` | vazio | Opcional (cache/anti-replay compartilhado entre instâncias) |

Gerar segredos novos: `python server/gen_keys.py` (dev) ou `python server/deploy/gen_prod_env.py` (produção).
**Nunca versione `.env`** (já está no `.gitignore`).

### Endpoints principais
| Rota | Uso |
|---|---|
| `POST /auth/login`, `/auth/logout`, `/license/heartbeat`, `/license/check`, `/compute/validate` | Cliente |
| `POST /auth/2fa/setup`, `/auth/2fa/confirm`, `/auth/password/change` | Primeiro acesso |
| `GET /health`, `GET /pubkey` | Monitoramento / chave pública |
| `POST /admin/auth/login` | Painel (e-mail + senha + 2FA) |
| `/admin/users` (GET, POST), `/admin/users/{id}/cpf`, `/reset-password`, `/reset-2fa`, `/unlock` | Contas |
| `/admin/licenses`, `/admin/devices`, `/admin/abuse`, `/admin/vm-allowlist`, `/admin/metrics` | Gestão |
| `GET /admin/telemetry`, `/admin/telemetry/signals` | Telemetria e antifraude |

### Linha de comando (rodar dentro de `server/`, com o venv)
```powershell
.venv\Scripts\python.exe -m app.create_admin --email admin@exemplo.com --cpf 00000000000   # senha por prompt oculto
.venv\Scripts\python.exe -m app.create_user  --email cliente@exemplo.com --cpf 00000000000 --days 30
.venv\Scripts\python.exe -m app.unlock_user  cliente@exemplo.com
.venv\Scripts\python.exe -m app.manifest_cli list
```
Na VM de produção, prefixe com `sudo runuser -u structura -- /opt/structura/venv/bin/python ...` dentro de `/opt/structura/server`.

## 5. Telemetria (antifraude) e LGPD

Registrada a cada **login**, **login falho** e **heartbeat** (tabela `telemetry_events`):
IP observado pelo servidor, e-mail (o tentado, em falhas), conta/licença, fingerprint, nome do computador,
versão do Windows, CPU, idioma, fuso, versão do app, VM e *user-agent*.
**Nunca** se grava senha, CPF ou conteúdo de projetos. A tela de login informa o usuário. Retenção de 180 dias
(`TELEMETRY_RETENTION_DAYS`), com expurgo automático.

Sinais de fraude calculados (painel → Telemetria): 3+ contas no mesmo IP, 4+ IPs numa licença, 2+ máquinas numa
licença, 10+ logins falhos de um IP — todos em 24 h. São indícios, a decisão é do administrador.
Revelar o CPF completo de uma conta no painel é registrado na auditoria (`reveal_cpf`).

## 6. Banco de dados e migrações
`Base.metadata.create_all` cria tabelas novas ao subir. Colunas novas em tabelas existentes são adicionadas por
`_ensure_columns()` em `main.py` (hoje: `users.must_change_password`). Ao criar uma coluna nova, **acrescente-a lá**
(ou adote Alembic ao migrar para PostgreSQL). Backup: ver `DEPLOY_SERVIDOR.md`.

## 7. Testes
```powershell
cd server
.venv\Scripts\python.exe -m pytest -q        # ~200 testes
```
Esperado no Windows: **4 falhas** em `test_m2_integrity.py` (`test_cpp_*`) quando o `g++` do ambiente não executa os
binários gerados — é limitação do ambiente, não do código. Cliente: `cmake --build build --config Release --target
TestUiLicenseIntegration` e executar o `.exe` gerado. Painel (e2e, opcional, **use um servidor de teste**): defina
`E2E_ADMIN_EMAIL`, `E2E_ADMIN_PASSWORD`, `E2E_ADMIN_TOTP_SECRET` e rode `npm run test:e2e` em `server/admin-ui`.

## 8. Diagnóstico rápido
| Sintoma | Causa provável | O que fazer |
|---|---|---|
| "E-mail ou senha incorretos" com dados certos | Conta bloqueada por tentativas, ou 2FA errado | Aguardar 15 min ou destravar no painel |
| `INTEGRITY_FAIL` | Manifesto do servidor ≠ instalação | Instalar o instalador do mesmo build; ver `BUILD_E_RELEASE.md` §Versão do manifesto |
| `SERVER_SIGNATURE_INVALID` | App de produção falando com servidor com outra chave | Usar o servidor certo / rebuild com a chave pública correta |
| `DEVICE_IN_USE` na própria máquina | Fingerprint mudou (reinstalação do Windows, troca de disco) | Painel → Dispositivos → liberar |
| Login falha só no build de release local | O release embute a chave do servidor **de produção** | Para testar localmente use `build_dev.bat` |
| `COMPUTE_VALIDATE_REJECTED` | Corpo rejeitado (422) ou erro 5xx | Ver log do servidor; verificar contrato do §3 |
| Painel "Sessão expirada" | Token de 30 min expirou ou a senha foi trocada | Entrar de novo |
