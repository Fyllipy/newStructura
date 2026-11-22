# Deploy do servidor Structura na Oracle Cloud (Always Free)

> Guia completo (trocar de host, backup/restauração, rotação de chaves, alternativas): `docs/DEPLOY_SERVIDOR.md`.

Resultado: servidor de licenciamento + painel admin em `https://<IP>.sslip.io` (HTTPS automático, sem domínio pago),
sempre ligado, com backup diário. Tudo que não exige a sua conta já está pronto neste repositório.

> Os segredos de **produção** (`server/deploy/secrets/prod.env`) são novos e diferentes dos de desenvolvimento.
> A chave pública correspondente já está embutida nos builds de **release** do app.
> Faça um backup seguro de `secrets/prod.env` (gerenciador de senhas): sem ele não há como recuperar CPFs/2FA cifrados.

## 1. Criar a conta e a VM (parte que só você pode fazer)
1. https://www.oracle.com/cloud/free/ → *Start for free*. Escolha a **região** com cuidado (não dá para trocar depois);
   cartão só para verificação de identidade. Confira na página oficial *Always Free Resources* o que está incluído hoje.
2. *Compute → Instances → Create instance*:
   - Imagem: **Ubuntu 24.04** (ou 22.04).
   - Shape: *Ampere A1.Flex* (1 OCPU / 6 GB bastam) **ou** *VM.Standard.E2.1.Micro* (AMD, 1 GB) se faltar capacidade ARM
     ("Out of capacity": tente outro *Availability Domain*, outro horário, ou a E2.1.Micro).
   - Rede: marque **Assign a public IPv4 address**. Baixe/guarde a chave SSH.
3. *Networking → VCN → Security Lists → Default → Add Ingress Rules*: origem `0.0.0.0/0`, TCP, portas **80** e **443**.
4. Anote o IP público (ex.: `129.146.10.20`). Seu domínio será `129-146-10-20.sslip.io` (pontos viram hífens).
   (Opcional: reserve o IP em *Networking → Reserved Public IPs* para ele não mudar se a VM for recriada.)

## 2. Enviar os arquivos (na sua máquina, na raiz do repo)
```powershell
powershell -File server\deploy\make_package.ps1          # gera server\deploy\bundle\structura-server.tar.gz
ssh -i <chave.key> ubuntu@<IP> "mkdir -p ~/upload"
scp -i <chave.key> server\deploy\bundle\structura-server.tar.gz server\deploy\secrets\prod.env server\deploy\oracle\setup.sh ubuntu@<IP>:~/upload/
```

## 3. Instalar na VM
```bash
ssh -i <chave.key> ubuntu@<IP>
cd ~/upload
sudo bash setup.sh 129-146-10-20.sslip.io      # use o SEU domínio
curl https://129-146-10-20.sslip.io/health      # {"status":"ok","db":"ok"}
shred -u ~/upload/prod.env                       # segredos agora só em /opt/structura/server/.env (0600)
```

## 4. Criar o administrador (na VM)
```bash
cd /opt/structura/server
sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_admin --email admin@jfyllipy.com --cpf <CPF>
```
Pede a senha (oculta, mín. 7 caracteres) e **mostra uma vez** o segredo TOTP: cadastre no autenticador agora.
Painel: `https://<dominio>/admin/` (login: e-mail + senha + código 2FA; sem CPF).

Usuários comuns (2FA é configurado por eles no primeiro login, com QR Code):
```bash
sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_user --email cliente@exemplo.com --cpf <CPF> --days 30
```

## 5. Publicar a versão do app (a cada release)
1. Grave a URL do servidor para o build: `echo https://129-146-10-20.sslip.io > server\deploy\production_url.txt`
2. Rode o `build_release.bat` (embute a URL, gera o instalador e exporta `server\deploy\bundle\{manifest.txt,manifest.sig,dll_key.txt}`).
3. Envie e publique:
```powershell
scp -i <chave.key> server\deploy\bundle\manifest.txt server\deploy\bundle\manifest.sig server\deploy\bundle\dll_key.txt ubuntu@<IP>:~/upload/
ssh -i <chave.key> ubuntu@<IP> "sudo bash /opt/structura/server/deploy/oracle/publish_release.sh"
```
> O número da versão do manifesto agora é **automático** (`server\deploy\manifest_version.txt`, incrementado a cada
> `build_release.bat`). Versões anteriores continuam válidas, então instalações antigas não quebram.
> Para forçar atualização, revogue as antigas: `python -m app.manifest_cli set-status --version N --status revoked`.

## 6. Operação
- Logs: `journalctl -u structura -f` e `/var/log/caddy/access.log`. Reiniciar: `sudo systemctl restart structura`.
- Backup: automático às 03:17 em `/var/backups/structura` (14 dias). Copie periodicamente para fora da VM.
- Atualizar o servidor: repita o passo 2 e `sudo tar -xzf ... -C /opt/structura/server && sudo systemctl restart structura`
  (não sobrescreva `.env` nem `data/`).
- Destravar conta: painel → Usuários → *Destravar*, ou `python -m app.unlock_user <email>`.
- Telemetria (IP, máquina, versão) e sinais de fraude: painel → *Telemetria*. Expurgo automático após 180 dias.

## Limites conhecidos
- Servidor único com SQLite: suficiente para centenas de usuários; para escalar, migrar para PostgreSQL.
- O Let's Encrypt limita certificados por domínio; `sslip.io` é compartilhado. Para produção séria, use um domínio próprio
  (basta trocar `<dominio>` no `setup.sh`/Caddyfile e apontar o DNS).
