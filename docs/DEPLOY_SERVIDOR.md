# Deploy do servidor e troca de hospedagem

Este guia cobre: (A) o que o servidor precisa para rodar em qualquer lugar, (B) o passo a passo na **Oracle Cloud
Always Free** (a hospedagem escolhida), (C) como **migrar para outro host** sem perder dados nem quebrar os apps
instalados, (D) operação, backup e restauração, (E) rotação de chaves e (F) alternativas de hospedagem.
O roteiro enxuto da Oracle também está em `server/deploy/oracle/DEPLOY.md`. **Google Cloud (e2-micro gratuito): `DEPLOY_GOOGLE_CLOUD.md`.**

## A. O que o servidor precisa (vale para qualquer host)
1. **Um processo Python 3.11+** rodando `uvicorn app.main:app` (dependências em `server/requirements.txt`).
2. **HTTPS** na frente (proxy reverso: Caddy, nginx, Cloudflare...). O app de release só aceita URL `https://` no override e
   deve falar com TLS válido. O uvicorn escuta só em `127.0.0.1`.
3. **Disco persistente** para o banco (SQLite em arquivo, ou PostgreSQL). Hospedagem com disco efêmero (ex.: Render gratuito)
   **perde as contas a cada reinício** — não use SQLite nela.
4. **Sempre ligado**: o app dá timeout em 5 s e bloqueia cálculo/exportação sem resposta (*fail-closed*). Serviços que "dormem"
   (cold start de ~1 min) não servem.
5. **Segredos** em variáveis de ambiente/arquivo `.env` (`0600`): `SERVER_SIGNING_SEED_B64`, `CPF_ENC_KEY_B64`, `CPF_HMAC_KEY_B64`,
   `RELEASE_PUBKEY_B64` (+ `DATABASE_URL`, `TRUST_PROXY_HEADERS=true`, `EXPOSE_DOCS=false`). Lista completa em
   `LICENCIAMENTO_E_SERVIDOR.md` §4.
6. **Painel**: `server/admin-ui/dist/` (precisa estar compilado antes de empacotar: `npm ci && npm run build`).
7. **Um único processo/worker** enquanto o anti-replay de nonces e o limitador de falhas forem em memória (`--workers 1`).
   Para várias instâncias, configure `REDIS_URL`.

## B. Passo a passo — Oracle Cloud Always Free
> Verifique na página oficial *Always Free Resources* da Oracle o que está incluído hoje; os limites já mudaram no passado
> (ex.: Ampere A1 pode estar em 2 OCPU/12 GB). Cartão de crédito é pedido só para verificar identidade.

### B1. Conta e VM (somente você consegue fazer)
1. Crie a conta em oracle.com/cloud/free. A **região não pode ser trocada depois** — escolha a mais próxima/com capacidade.
2. *Compute → Instances → Create instance*: imagem **Ubuntu 24.04**; shape **Ampere A1.Flex** (1 OCPU/6 GB bastam) **ou**
   **VM.Standard.E2.1.Micro** (AMD) se aparecer "Out of capacity" (tente outro *Availability Domain* ou outro horário);
   marque **Assign a public IPv4 address**; baixe a chave SSH (`.key`) e guarde-a.
3. *Networking → Virtual Cloud Networks → sua VCN → Security Lists → Default → Add Ingress Rules*: origem `0.0.0.0/0`,
   TCP, destino **80** e **443** (22 já vem aberto).
4. Anote o **IP público**. Opcional e recomendado: reservar o IP (*Reserved Public IPs*) para ele sobreviver à recriação da VM.
5. Seu domínio gratuito é `<IP com hífens>.sslip.io` (ex.: IP `129.146.10.20` → `129-146-10-20.sslip.io`). O Caddy obtém o
   certificado Let's Encrypt sozinho. Com domínio próprio, aponte um registro **A** para o IP e use-o no lugar.

### B2. Gerar os segredos de produção (uma única vez, na máquina de build)
```powershell
server\.venv\Scripts\python.exe server\deploy\gen_prod_env.py
```
Cria `server\deploy\secrets\prod.env` (chaves **novas**, nunca as de desenvolvimento) e `server_pubkey.hex`. Recusa sobrescrever.
A chave pública já fica embutida nos builds de release (`client/license_config.cpp`, ramo `#else`). **Faça backup de
`prod.env` em local seguro** (gerenciador de senhas / cofre): sem ele, CPFs e segredos 2FA cifrados no banco não são recuperáveis.
Teste sem tocar em nada real: `python server\deploy\smoke_test_prod_env.py`.

### B3. Enviar e instalar
```powershell
# na raiz do repo (painel compilado antes: cd server\admin-ui ; npm ci ; npm run build)
powershell -File server\deploy\make_package.ps1       # server\deploy\bundle\structura-server.tar.gz (sem segredos/banco/venv)
ssh -i <chave.key> ubuntu@<IP> "mkdir -p ~/upload"
scp -i <chave.key> server\deploy\bundle\structura-server.tar.gz server\deploy\secrets\prod.env server\deploy\oracle\setup.sh ubuntu@<IP>:~/upload/
ssh -i <chave.key> ubuntu@<IP>
```
Na VM:
```bash
cd ~/upload
sudo bash setup.sh 129-146-10-20.sslip.io     # SEU domínio
curl https://129-146-10-20.sslip.io/health     # {"status":"ok","db":"ok"}
shred -u ~/upload/prod.env                      # os segredos ficam só em /opt/structura/server/.env
```
O `setup.sh` instala Python/Caddy/fail2ban/ufw, cria o usuário de sistema `structura`, o venv em `/opt/structura/venv`, o serviço
`structura` (systemd, com *hardening*), o Caddy (HTTPS, limite de corpo 10 MB, HSTS), regras de firewall (iptables da Oracle + ufw 22/80/443) e o
backup diário. Layout resultante:
```
/opt/structura/server/   código + .env (0600)        /opt/structura/venv/   dependências
/opt/structura/data/     structura.db (0700)         /var/backups/structura/ backups (14 dias)
/etc/caddy/Caddyfile     proxy HTTPS                 /etc/systemd/system/structura.service
```
Se o `curl` externo não responder: confira as regras de entrada 80/443 na VCN (passo B1.3) e `sudo systemctl status structura caddy`.

### B4. Criar o administrador e publicar a versão do app
```bash
cd /opt/structura/server
sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_admin --email admin@exemplo.com --cpf <CPF>
```
Cadastre o TOTP mostrado. Depois, na máquina de build:
```powershell
echo https://129-146-10-20.sslip.io> server\deploy\production_url.txt
build_release.bat
scp ... server\deploy\bundle\{manifest.txt,manifest.sig,dll_key.txt} ubuntu@<IP>:~/upload/
ssh ... "sudo bash /opt/structura/server/deploy/oracle/publish_release.sh"
```
(detalhes em `BUILD_E_RELEASE.md`). Só então distribua o instalador. Crie usuários pelo painel (`PAINEL_ADMIN.md`).

## C. Migrar para outro host (sem perder dados nem quebrar os apps instalados)
Os apps instalados conhecem **dois** dados do servidor: a **URL** e a **chave pública** (derivada de `SERVER_SIGNING_SEED_B64`).
- Se a **chave não mudar** e o **domínio não mudar** (apenas o IP/servidor por trás): os apps instalados continuam funcionando — é só
  apontar o DNS. **Por isso, o ideal é usar um domínio próprio** (não `sslip.io`, que embute o IP): troca de host vira troca de registro DNS.
- Se o **domínio mudar** (ex.: de `sslip.io` para outro): é preciso um novo release com a nova URL, e os usuários reinstalam/atualizam.
  Mantenha o servidor antigo ligado até todos migrarem.
- A **chave** só muda se você a rotacionar de propósito (§E).

### Roteiro de migração (mesma chave)
1. **Preparar o novo host** conforme a seção A (qualquer Ubuntu/Debian serve: use o mesmo `setup.sh`, que é agnóstico de provedor;
   só o passo de firewall da VCN/Security Group é específico de cada nuvem — libere 80/443).
2. **Copiar os segredos**: o mesmo `prod.env` (do backup seguro) para `~/upload/` do novo host. **Não gere novos segredos.**
3. **Instalar** (`setup.sh <dominio-novo>`), mas **antes de abrir ao público**, restaurar os dados:
4. **Backup e restauração do banco** — no host antigo: `sudo /usr/local/bin/structura-backup` e pegue o último
   `/var/backups/structura/structura-*.db.gz`. No novo:
```bash
sudo systemctl stop structura
gunzip -c structura-AAAAMMDD-HHMMSS.db.gz | sudo tee /opt/structura/data/structura.db >/dev/null
sudo chown structura:structura /opt/structura/data/structura.db && sudo chmod 600 /opt/structura/data/structura.db
sudo systemctl start structura && curl https://<dominio>/health
```
5. Testar: login no painel e login de um usuário de teste no app (o `manifest` já está no banco restaurado).
6. **Trocar o DNS** (TTL baixo antes) para o novo IP. Aguardar propagação. Desligar o host antigo só depois de dias sem tráfego
   (`journalctl -u structura` / painel → Telemetria mostra os acessos chegando).

### Se mudar de SQLite para PostgreSQL
`DATABASE_URL=postgresql+psycopg://usuario:senha@host/db` (instale `psycopg[binary]` no venv e adicione ao `requirements.txt`). As tabelas são
criadas na subida; a coluna extra de migração é tratada por `_ensure_columns()`. Dados existentes: exporte do SQLite (`sqlite3 .dump`
adaptado, ou `pgloader`) — teste antes em um banco descartável. Com PostgreSQL gerenciado você pode usar PaaS sem disco (mas ainda
precisa de processo sempre ligado). Considere Alembic para migrações futuras.

### Mudar só o app (sem trocar de servidor)
`production_url.txt` → `build_release.bat` → publicar manifesto → distribuir. Nada no servidor muda.

## D. Operação
| Tarefa | Comando (na VM) |
|---|---|
| Ver logs | `journalctl -u structura -f` · `/var/log/caddy/access.log` |
| Reiniciar | `sudo systemctl restart structura` |
| Atualizar o servidor | `make_package.ps1` → `scp` do `.tar.gz` → `sudo tar -xzf structura-server.tar.gz -C /opt/structura/server && sudo chown -R structura:structura /opt/structura/server && sudo systemctl restart structura` (**não** sobrescreve `.env` nem `data/`, pois o pacote não os contém) |
| Backup manual | `sudo /usr/local/bin/structura-backup` (diário automático 03:17, guarda 14 dias, `sqlite3 .backup` seguro com o servidor ligado) |
| Cópia externa do backup | `scp ubuntu@<IP>:/var/backups/structura/structura-*.db.gz .` — faça periodicamente; o backup na mesma VM não protege contra perder a VM |
| Criar/destravar usuário | painel, ou `sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_user / app.unlock_user` (em `/opt/structura/server`) |
| Listar/revogar versões | `... -m app.manifest_cli list` · `set-status --version N --status revoked` |
| Certificado | Renovação automática pelo Caddy; se falhar: `sudo journalctl -u caddy` |
| Atualizar o SO | `sudo apt update && sudo apt upgrade` (há `unattended-upgrades` para segurança) |

Monitoramento simples: um verificador externo gratuito (UptimeRobot/Better Stack) chamando `GET /health` a cada 5 min com alerta por e-mail.

> **Nota (Ubuntu 25.10+/26.04):** `sudo -u structura ...` pode ser recusado ("I'm afraid I can't do that"). Use sempre `sudo runuser -u structura -- <comando>`
> (ou entre como root com `sudo -i`).

> Guia detalhado para iniciantes (o que cada chave é, como gerar e trocar): `SEGREDOS_E_CHAVES.md`.

## E. Rotação de chaves (evite; faça só se vazou)
| Chave | Efeito de trocar | Procedimento |
|---|---|---|
| `SERVER_SIGNING_SEED_B64` | Todos os tokens e a chave pública mudam: **todos os apps instalados deixam de validar o servidor** | Gerar novo `server_pubkey.hex`, atualizar `kServerPubKey` (ramo release), `build_release.bat`, redistribuir; republicar manifestos (a `dll_key` derivada muda — passe `--dll-key` do `dll_key.txt` exportado) |
| `CPF_ENC_KEY_B64` | CPFs e segredos 2FA cifrados ficam **ilegíveis** | Só com migração: decifre com a chave antiga e recifre com a nova antes de trocar. Sem isso, perde-se CPF/2FA de todos |
| `CPF_HMAC_KEY_B64` | Busca por CPF para de funcionar | Recalcular `cpf_hmac` de cada conta (a partir do CPF decifrado) |
| Chave de **release** (`server/release.key`) | Manifestos novos assinados com outra chave | Trocar `RELEASE_PUBKEY_B64` no servidor **e** `kReleasePubKey` no cliente; novo release |
Se o `.env` de produção vazar: troque a senha de todos os admins, rotacione `SERVER_SIGNING_SEED_B64` (planeje o release) e avalie
notificar os titulares dos CPFs (LGPD) — lembre que `CPF_ENC_KEY_B64` vazada junto com o banco expõe os CPFs.

## F. Alternativas de hospedagem (pesquisadas em 2026-11; **confirme nos sites oficiais**, os planos mudam)
| Opção | Serve? | Observações |
|---|---|---|
| **Oracle Cloud Always Free** | **Sim (escolhida)** | VM real sempre ligada, disco persistente. Cartão só para verificação; pode faltar capacidade ARM na região |
| Qualquer VPS pago (Hetzner, DigitalOcean, Contabo, Locaweb...) | Sim | Mesmo `setup.sh` (Ubuntu/Debian). ~US$ 4–6/mês. Mais previsível que gratuito |
| PythonAnywhere (gratuito) | Possível, com ressalvas | Sem cartão e sempre ligado, mas FastAPI exige adaptador WSGI/beta ASGI, quotas de CPU/memória (Argon2 usa memória), e o app precisa ser "estendido" periodicamente. Não há kit pronto no repositório |
| Render gratuito | **Não** como está | Dorme após 15 min (cold start ~1 min → app bloqueia) e disco efêmero (perde o SQLite). Só viável com PostgreSQL externo + pinger, o que é frágil |
| Fly.io / Railway / Koyeb | Pago ou trial | Sem camada gratuita permanente para novos usuários (na data da pesquisa) |
Qualquer destino exige: HTTPS, processo sempre ligado, banco persistente e os segredos do §A.

## G. Checklist antes de divulgar o instalador
- [ ] `https://<dominio>/health` responde `ok` e o certificado é válido.
- [ ] `/docs` e `/openapi.json` retornam 404; `X-Frame-Options`/CSP presentes (`smoke_test_prod_env.py` verifica em memória).
- [ ] Admin criado, 2FA cadastrado, senha forte; painel abre em `/admin/`.
- [ ] Manifesto da versão publicado **no servidor de produção** (`manifest_cli list` mostra `current`).
- [ ] Teste ponta a ponta com um usuário novo: criar no painel → instalar → login → QR Code → trocar senha → calcular → exportar.
- [ ] Backup diário rodando (`ls /var/backups/structura`) e uma cópia externa feita; `prod.env` guardado em cofre.
- [ ] Monitor externo no `/health` ativo.
