# Manual passo a passo — colocar o servidor Structura no ar numa VM online

Para quem nunca fez deploy. Funciona em qualquer VM Linux (Ubuntu/Debian) de qualquer provedor (Google Cloud, Oracle, Hetzner,
Hostinger, Vultr...). Os trechos marcados **[GOOGLE CLOUD]** valem só para ele. Referências: `DEPLOY_SERVIDOR.md` (visão geral e
migração), `DEPLOY_GOOGLE_CLOUD.md`, `PAINEL_ADMIN.md`, `BUILD_E_RELEASE.md`.

**Tempo total:** ~1 hora na primeira vez. **Você vai precisar de:** a máquina de build do projeto (Windows), um cartão de crédito
para criar a conta na nuvem, um autenticador no celular (Google/Microsoft Authenticator ou Authy).

Convenções: `<IP>` = IP público da VM; `<DOMINIO>` = domínio com HTTPS (o gratuito é `<IP com hífens>.sslip.io`, ex.: IP `34.26.242.178`
→ `34-26-242-178.sslip.io`); comandos em `PowerShell` rodam na **sua máquina Windows**, em `bash` rodam **na VM**.

---

## Parte 0 — Preparar tudo na sua máquina (uma vez)

### 0.1 Gerar os segredos de produção
> Dúvidas sobre o que são esses segredos, como trocar ou guardar: `SEGREDOS_E_CHAVES.md`. Para conferir tudo: `python server\deploy\check_secrets.py`.
Na raiz do repositório:
```powershell
server\.venv\Scripts\python.exe server\deploy\gen_prod_env.py
```
Cria `server\deploy\secrets\prod.env` (chaves **novas**, só de produção) e `server_pubkey.hex`. O script **recusa sobrescrever** — de propósito.
> **Faça backup de `prod.env` agora** (gerenciador de senhas ou pendrive guardado). Sem ele, os CPFs e os segredos 2FA cifrados no
> servidor não podem ser recuperados, e trocar essas chaves invalida tudo. Nunca coloque no Git, e-mail ou chat.

A chave pública do servidor (`server_pubkey.hex`) já está embutida nos builds de **release** do app (`client/license_config.cpp`).
Se você gerar **novos** segredos, essa chave muda e é preciso atualizar o app (ver `BUILD_E_RELEASE.md` §4).

### 0.2 Compilar o painel e empacotar o servidor
```powershell
cd server\admin-ui
npm ci
npm run build          # gera dist/ (o painel que o servidor serve em /admin/)
cd ..\..
powershell -File server\deploy\make_package.ps1
```
Resultado: `server\deploy\bundle\structura-server.tar.gz` (código + painel; **sem** segredos, banco ou `.env`).

### 0.3 Os 3 arquivos que irão para a VM
1. `server\deploy\bundle\structura-server.tar.gz`
2. `server\deploy\secrets\prod.env`
3. `server\deploy\oracle\setup.sh` (o nome da pasta é histórico; serve para qualquer provedor)

---

## Parte 1 — Criar a VM

Requisitos mínimos: **Ubuntu 24.04 ou mais novo** (Debian 12+ também serve), 1 GB de RAM (o script cria swap), 10 GB de disco, IP público,
**disco persistente**, VM **sempre ligada** (nada de "Spot/preemptível"). Portas **80 e 443** liberadas para a internet (22 para SSH).

**[GOOGLE CLOUD]** Passo a passo completo em `DEPLOY_GOOGLE_CLOUD.md`. Resumo: projeto → ativar *Compute Engine API* → alerta de orçamento →
*Criar instância*: **e2-micro**, região **us-east1/us-central1/us-west1**, **Ubuntu 24.04**, disco **Standard 30 GB**, marcar
**Permitir tráfego HTTP e HTTPS** → *Rede VPC → Endereços IP → Promover para estático*.

**Outros provedores:** crie a VM Ubuntu, anote o IP, baixe/registre a chave SSH e libere as portas 80 e 443 no firewall **do provedor**
(Security Group / Security List / Firewall rules).

Anote o **IP público** e defina o `<DOMINIO>`. Dica: com um domínio próprio (registro **A** → IP) você pode trocar de host sem lançar um app novo.

---

## Parte 2 — Enviar os arquivos para a VM

### Pelo navegador (Google Cloud, sem instalar nada)
1. Em *Compute Engine → Instâncias de VM*, clique em **SSH** (abre um terminal no navegador).
2. No terminal: `mkdir -p ~/upload`
3. **Engrenagem → Upload file** e envie os 3 arquivos (um por vez).
4. `mv ~/structura-server.tar.gz ~/prod.env ~/setup.sh ~/upload/ && ls -la ~/upload` (devem aparecer os 3).

### Por linha de comando (qualquer provedor)
```powershell
ssh -i <chave> <usuario>@<IP> "mkdir -p ~/upload"
scp -i <chave> server\deploy\bundle\structura-server.tar.gz server\deploy\secrets\prod.env server\deploy\oracle\setup.sh <usuario>@<IP>:~/upload/
```
(usuário padrão: `ubuntu` na Oracle/AWS; no Google Cloud é o nome da sua conta.)

---

## Parte 3 — Instalar o servidor

```bash
cd ~/upload
sudo bash setup.sh <DOMINIO>
```
Leva 3–8 minutos. Ele: cria 2 GB de swap se a RAM for < 1,8 GB; instala Python, Caddy (HTTPS automático), fail2ban, ufw; cria o usuário de sistema
`structura`; instala as dependências num venv; registra o serviço `structura` (systemd, com isolamento); configura o Caddy; abre o firewall; agenda o backup diário.
No fim deve imprimir `{"status":"ok","db":"ok"}`.

Teste de fora e **apague o segredo da VM**:
```bash
curl https://<DOMINIO>/health          # {"status":"ok","db":"ok"}
shred -u ~/upload/prod.env             # o segredo agora vive só em /opt/structura/server/.env (0600)
```
Se o `curl` falhar logo após instalar, espere 1–2 min (o Caddy está pedindo o certificado) e repita. Se continuar:
`sudo systemctl status structura caddy`, `sudo journalctl -u caddy -n 50` e confirme as portas 80/443 no firewall do provedor.

Conferência opcional na sua máquina (deve dar 404/401/404 e o `pubkey` igual ao `server_pubkey.hex`):
```powershell
curl https://<DOMINIO>/pubkey ; curl -o NUL -w "%{http_code}" https://<DOMINIO>/docs ; curl -o NUL -w "%{http_code}" https://<DOMINIO>/admin/users
```

Estrutura criada na VM:
```
/opt/structura/server/   código + .env        /opt/structura/venv/    Python e dependências
/opt/structura/data/     structura.db         /var/backups/structura/ backups (14 dias)
/etc/caddy/Caddyfile     proxy HTTPS          /etc/systemd/system/structura.service
```

---

## Parte 4 — Criar o administrador

Na VM:
```bash
cd /opt/structura/server
sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_admin --email admin@seudominio.com --cpf 00000000000
```
- Troque e-mail e CPF pelos reais (o CPF é validado e fica guardado cifrado; **não** é usado no login).
- Pede a **senha** duas vezes, sem mostrar (mínimo 7 caracteres — use uma longa e única).
- Mostra **uma única vez** o **segredo TOTP** (chave de 32 letras) e o `otpauth://...`. No celular: autenticador → *Adicionar conta* → *Inserir chave de configuração* →
  cole a chave. **Faça isso antes de fechar o terminal.**
- Saída esperada: `USUARIO ADMINISTRADOR CRIADO COM SUCESSO`.

Entre em `https://<DOMINIO>/admin/` com e-mail + senha + código de 6 dígitos. (Sem CPF.)

> **Por que `runuser` e não `sudo -u`?** Em Ubuntus novos o `sudo -u structura` pode ser recusado. `sudo runuser -u structura -- ...` funciona.

### Se o `sudo` recusar tudo ("I'm afraid I can't do that") — diagnóstico
Isso significa que o `sudo` passou a pedir senha (usuários SSH da nuvem não têm senha). Tente, nesta ordem:
1. **Feche e reabra a sessão SSH** (no Google Cloud o agente reaplica o sudo no login). Teste: `sudo -n true && echo SUDO_OK`.
2. Veja o estado: `id`, `getent group google-sudoers`, `ls -la /etc/sudoers.d/`.
3. **Plano B (Google Cloud) — criar o admin por *startup script*** (roda como root no boot). Em *Editar VM → Metadados → Adicionar item*:
   chave `startup-script`, valor:
   ```bash
   #!/bin/bash
   cd /opt/structura/server
   STRUCTURA_ADMIN_EMAIL='admin@seudominio.com' STRUCTURA_ADMIN_CPF='00000000000' STRUCTURA_ADMIN_PASSWORD='SUA-SENHA-FORTE' \
     runuser -u structura -- /opt/structura/venv/bin/python -m app.create_admin
   ```
   Salve e **reinicie a VM**. A saída (com o segredo TOTP) aparece em *Console serial (porta 1)*. **Depois remova o metadado `startup-script`**
   (ele contém a senha) e troque a senha do admin pelo painel/CLI quando possível.

---

## Parte 5 — Publicar a versão do app (a cada release)

Sem isso, todo login dá `INTEGRITY_FAIL`: o servidor só aceita instalações cujo manifesto assinado foi publicado nele.

### Na sua máquina
```powershell
echo https://<DOMINIO>> server\deploy\production_url.txt
build_release.bat
```
(O script exige `production_url.txt`, embute a URL no app, usa uma versão de manifesto **automática** e gera os instaladores em `dist\installers\`.
Detalhes e armadilhas: `BUILD_E_RELEASE.md`.) Ele grava `server\deploy\bundle\manifest.txt`, `manifest.sig`, `dll_key.txt`.

### Enviar e publicar na VM
Faça upload desses 3 arquivos para `~/upload/` (como na Parte 2) e rode:
```bash
sudo bash /opt/structura/server/deploy/oracle/publish_release.sh
```
Deve listar as versões (a nova como `current`). **Só depois disso distribua o instalador.**

---

## Parte 6 — Criar usuários e liberar licenças

No painel (`https://<DOMINIO>/admin/`) → **Usuários → + Novo usuário**: e-mail, CPF, tipo de licença, validade (e, opcionalmente, senha inicial).
O painel mostra **uma vez** a senha temporária — envie ao cliente por canal seguro. Ele instala o app, entra com e-mail + senha temporária,
escaneia o **QR Code** do 2FA no assistente e define a senha definitiva. (Alternativa por linha de comando:
`sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_user --email ... --cpf ... --days 30`, dentro de `/opt/structura/server`.)
Manual completo do painel: `PAINEL_ADMIN.md`.

### Teste ponta a ponta antes de vender
Crie um usuário de teste → instale o instalador num PC limpo → login → QR Code → trocar senha → abrir/calcular um modelo → exportar. Confira em
**Telemetria** o acesso registrado (IP, máquina, versão).

---

## Parte 7 — Rotina e emergências

| Tarefa | Como |
|---|---|
| Ver se está no ar | `https://<DOMINIO>/health` (configure um monitor gratuito, ex. UptimeRobot, a cada 5 min) |
| Logs | `sudo journalctl -u structura -f` · `sudo tail -f /var/log/caddy/access.log` |
| Reiniciar | `sudo systemctl restart structura` |
| Backup manual | `sudo /usr/local/bin/structura-backup` (automático às 03:17, guarda 14 dias em `/var/backups/structura`) |
| **Cópia do backup para fora** | baixe `structura-*.db.gz` pelo SSH (no Google Cloud: engrenagem → *Download file*). O backup na mesma VM não protege contra perder a VM |
| Atualizar o servidor | refaça a Parte 0.2, envie o `.tar.gz` e: `sudo tar -xzf ~/upload/structura-server.tar.gz -C /opt/structura/server && sudo chown -R structura:structura /opt/structura/server && sudo systemctl restart structura` (não mexe em `.env` nem `data/`) |
| Restaurar backup | `sudo systemctl stop structura` → `gunzip -c backup.db.gz | sudo tee /opt/structura/data/structura.db >/dev/null` → `sudo chown structura:structura /opt/structura/data/structura.db && sudo chmod 600 ...` → `sudo systemctl start structura` |
| Conta travada (5 erros) | painel → Usuários → *Destravar* (ou `app.unlock_user <email>`); desbloqueia sozinha em 15 min |
| Trocou de computador | painel → Dispositivos → liberar |
| Revogar uma versão antiga do app | `... -m app.manifest_cli set-status --version N --status revoked` |
| Mudar de provedor | `DEPLOY_SERVIDOR.md` §C (mesmos segredos + restaurar o banco + trocar o DNS) |

### Problemas comuns
| Sintoma | Causa / solução |
|---|---|
| `curl https://<DOMINIO>/health` não responde | Portas 80/443 fechadas no **firewall do provedor**, ou certificado ainda sendo emitido (espere 2 min). `sudo systemctl status caddy` |
| `setup.sh`: "falta .../structura-server.tar.gz" | Arquivos não estão em `~/upload/` do mesmo usuário que roda o `sudo` |
| App: `INTEGRITY_FAIL` | Manifesto do instalador não foi publicado no servidor (Parte 5) |
| App: `SERVER_SIGNATURE_INVALID` | App aponta para um servidor com outra chave: use o instalador de release gerado com `production_url.txt` correto |
| App: `DECIPHER_FAILED_MAC_MISMATCH` ao calcular | A `dll_key` publicada não é a que cifrou `rds.enc`. Rode `check_dll_key.py`, reexporte o bundle (`export_bundle.py` confere sozinho), republique `dll_key.txt` com `publish_release.sh` e peça ao usuário para **sair e entrar de novo** no app |
| App: `DEVICE_IN_USE` | Vínculo antigo: painel → Dispositivos → liberar |
| Painel: erro de credenciais | 5 falhas bloqueiam 15 min; ou 2FA com relógio do celular errado (ative hora automática) |
| `sudo`/`runuser` recusados | Parte 4 (diagnóstico) |
| `publish_release.sh`: `PermissionError ... /home/<usuario>/upload/manifest.txt` | O usuário `structura` não lê a sua pasta pessoal. Use a versão atual do script (copia para uma área temporária). Manual: `sudo mkdir -p /opt/structura/rel && sudo cp ~/upload/manifest.* ~/upload/dll_key.txt /opt/structura/rel/ && sudo chown -R structura:structura /opt/structura/rel` e rode o `manifest_cli publish` apontando para `/opt/structura/rel/...`; depois `sudo rm -rf /opt/structura/rel ~/upload/dll_key.txt` |
| Servidor não sobe após editar `.env` | `sudo journalctl -u structura -n 30`: segredo ausente/malformado faz o servidor recusar subir (*fail-closed*) |

### Checklist final
- [ ] `/health` ok, certificado válido, `/docs` 404, painel abre.
- [ ] Admin criado, TOTP no autenticador, senha forte.
- [ ] `prod.env` em backup seguro **e** apagado da VM (`shred`).
- [ ] Manifesto da versão publicado (`manifest_cli list` mostra `current`).
- [ ] Teste ponta a ponta com usuário novo concluído.
- [ ] Backup diário confirmado (`ls /var/backups/structura`) e uma cópia feita fora da VM.
- [ ] Alerta de orçamento e monitor externo ativos.
