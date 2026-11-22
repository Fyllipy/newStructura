# Segredos, chaves e arquivos `.env` — guia para quem está começando

Este documento explica, do zero, **todos os segredos do Structura**: o que são, onde ficam, como gerar novos, como mudar uma
configuração, o que acontece se você trocar ou perder cada um, e como conferir se tudo está certo. Se você só quer saber
"está tudo coerente?", rode:

```powershell
server\.venv\Scripts\python.exe server\deploy\check_secrets.py
```
Ele confere tudo (sem mostrar nenhum segredo) e diz `TUDO COERENTE` ou aponta o que está errado.

Documentos relacionados: `MANUAL_SETUP_SERVIDOR_PASSO_A_PASSO.md` (colocar o servidor no ar), `DEPLOY_SERVIDOR.md` (trocar de host),
`BUILD_E_RELEASE.md` (gerar instaladores), `LICENCIAMENTO_E_SERVIDOR.md` (como o sistema funciona).

---

## 1. Conceitos em 2 minutos

| Palavra | O que é (explicação simples) |
|---|---|
| **Segredo** | Informação que, se vazar, permite que alguém se passe por você ou leia dados protegidos. Ex.: senha, chave privada. |
| **Chave pública** | Metade "aberta" de um par de chaves. **Não é segredo**: pode ficar dentro do programa. Ela só serve para *conferir* assinaturas, nunca para criá-las. |
| **Chave privada / seed** | Metade secreta do par. Quem a tem consegue *assinar* como se fosse você. A "seed" é o número aleatório de 32 bytes que gera a chave privada. |
| **Assinatura (Ed25519)** | "Carimbo" matemático. O servidor assina o token de login; o app confere o carimbo com a chave pública do servidor — por isso um servidor falso não engana o app. |
| **Cifrar (AES-GCM)** | Embaralhar um dado com uma chave. Só quem tem a chave desembaralha. Usado no CPF e nos segredos 2FA guardados no banco. |
| **HMAC** | Uma "impressão digital" com chave. Usado para localizar um CPF sem guardá-lo em claro. |
| **TOTP / 2FA** | Os 6 dígitos que mudam a cada 30 s no autenticador do celular. Cada conta tem um *segredo TOTP* (a chave de 32 letras do QR Code). |
| **`.env`** | Um arquivo de texto com as configurações e segredos do **servidor**, uma por linha (`NOME=valor`). O servidor lê esse arquivo ao iniciar. |
| **base64url / hex** | Duas formas de escrever bytes como texto. As chaves do `.env` são base64url (43 caracteres); a `dll_key` é hex (64 caracteres). Não precisa converter na mão: os scripts fazem. |
| **Manifesto** | Lista assinada de todos os arquivos do programa instalado. O servidor só deixa entrar quem tem exatamente esses arquivos. |

**Regra de ouro:** segredo nunca vai para o Git, e-mail, WhatsApp, print, chat nem para dentro do instalador.
Chave pública pode ir (e vai) dentro do programa.

---

## 2. Mapa completo: todo segredo e toda chave

| O quê | Para que serve | Onde fica | É segredo? | Se PERDER | Se VAZAR |
|---|---|---|---|---|---|
| `SERVER_SIGNING_SEED_B64` | Assina tokens de licença, a chave de cálculo (K) e o passe de exportação | `.env` do servidor (dev: `server/.env`; produção: `/opt/structura/server/.env` e cópia em `server/deploy/secrets/prod.env`) | **Sim** | Gere outra → **todos os apps instalados deixam de reconhecer o servidor** (§6.2) | Quem tiver pode forjar licenças. Rotacionar (§6.2) |
| `CPF_ENC_KEY_B64` | Cifra os **CPFs** e os **segredos 2FA** guardados no banco | idem | **Sim** | **CPFs e 2FA de todos ficam ilegíveis para sempre** | Com o banco, expõe os CPFs (LGPD). Rotacionar (§6.3) |
| `CPF_HMAC_KEY_B64` | Gera a impressão digital do CPF (busca) | idem | **Sim** | Impacto baixo hoje (o CPF não é mais usado no login) | Rotacionar (§6.3) |
| `RELEASE_PUBKEY_B64` | Chave **pública** que o servidor usa para conferir os manifestos | `.env` do servidor | Não | Copie de `server/.env` | — |
| `server/release.key` | Chave **privada** de release: **assina o manifesto** de cada instalador | **só** na máquina de build, `0600` | **Sim** | Sem ela você não consegue mais gerar releases aceitos pelos apps já instalados (§6.4) | Qualquer um poderia assinar uma versão adulterada. Rotacionar (§6.4) |
| `kReleasePubKey` | A mesma chave de release, parte **pública**, dentro do app | `client/license_config.cpp` | Não | — | — |
| `kServerPubKey` | Chave pública do servidor dentro do app (um valor para build de **dev**, outro de **produção**) | `client/license_config.cpp` | Não | — | — |
| `dll_key` (64 hex) | Decifra `rds.enc`/`mefs.enc` (o solver do cálculo) no computador do usuário | banco do servidor (tabela do manifesto) e `server/deploy/bundle/dll_key.txt` | **Sim** (protege o motor de cálculo em repouso) | Re-cifrar os solvers (`build_solvers.bat`) e republicar | Quem tem a chave e o `.enc` abre o solver. Re-cifrar com nova chave (§6.5) |
| Senha do admin | Entrar no painel e no app como admin | só você (e hash Argon2id no banco) | **Sim** | Recriar com `create_admin` (§5.4) | Trocar a senha (§5.4) |
| Segredo TOTP de cada conta | Gera o código de 6 dígitos | autenticador do usuário + cifrado no banco | **Sim** | Painel → *Resetar 2FA* (usuários) / `create_admin` (admin) | Resetar 2FA |
| `session.dat` | Sessão do usuário no PC dele | `%APPDATA%\Structura\...` protegido por DPAPI | Sim (só desse usuário Windows) | Apagar; entrar de novo | Apagar; *logout* no painel (liberar dispositivo) |
| Chave SSH / conta da nuvem | Acessar a VM e o console | seu computador / conta Google | **Sim** | Recuperar pelo provedor | Revogar e trocar |
| Banco `structura.db` | Contas, licenças, telemetria | `/opt/structura/data/` na VM + backups | **Sim** (dados pessoais) | Restaurar backup (`DEPLOY_SERVIDOR.md` §C) | Notificar conforme LGPD |

> **Dois "ambientes" separados:** *desenvolvimento* (servidor no seu PC, `server/.env`, `build_dev.bat`) e *produção* (VM, `prod.env`,
> `build_release.bat`). Têm chaves **diferentes** de propósito. Nunca copie o `.env` de dev para produção.

---

## 3. O arquivo `.env` na prática

Exemplo (valores fictícios):
```
DATABASE_URL=sqlite:////opt/structura/data/structura.db
SERVER_SIGNING_SEED_B64=Xx...43 caracteres...
CPF_ENC_KEY_B64=Yy...43 caracteres...
CPF_HMAC_KEY_B64=Zz...43 caracteres...
RELEASE_PUBKEY_B64=jLiZDIlCKNI6uGt-eqgbqre37huCF0bQh3pFiHsLKpM
TRUST_PROXY_HEADERS=true
EXPOSE_DOCS=false
```
Regras do formato: uma configuração por linha, `NOME=valor`, **sem aspas, sem espaços** em volta do `=`, linhas começando com `#` são comentários.
O servidor **se recusa a subir** (de propósito) se uma chave obrigatória faltar ou estiver malformada; o erro cita só o *nome* do campo
(`SecretConfigError: CPF_ENC_KEY_B64 invalido`), nunca o valor. Para ver: `sudo journalctl -u structura -n 30`.

Quais valores você pode mudar à vontade e quais **não** (a lista completa de opções está em `LICENCIAMENTO_E_SERVIDOR.md` §4):

| Configuração | Mudar é seguro? |
|---|---|
| `MIN_PASSWORD_LEN`, `LOCKOUT_*`, `AUTH_FAIL_*`, `AUTO_SUSPEND_FLAG_THRESHOLD`, `ADMIN_TOKEN_TTL_SECONDS`, `TELEMETRY_RETENTION_DAYS`, `DEVICE_STALE_DAYS`, `TOKEN_TTL_SECONDS`, `EXPOSE_DOCS` | ✅ Sim. Só reiniciar o serviço |
| `TRUST_PROXY_HEADERS` | ⚠️ `true` **só** atrás do Caddy/nginx (produção na VM); `false` no PC |
| `DATABASE_URL` | ⚠️ Mudar aponta para **outro banco** (os dados atuais "somem" da vista). Só ao migrar |
| `SERVER_SIGNING_SEED_B64`, `CPF_ENC_KEY_B64`, `CPF_HMAC_KEY_B64`, `RELEASE_PUBKEY_B64` | ❌ **Não troque sem ler a §6.** Têm consequências graves |

---

## 4. Como mudar uma configuração simples (servidor em produção)

1. Abra o terminal SSH da VM e faça uma cópia de segurança do arquivo atual:
   ```bash
   sudo cp -p /opt/structura/server/.env /opt/structura/server/.env.bak
   ```
2. Edite (no `nano`: setas para andar, digite, **Ctrl+O** + Enter para salvar, **Ctrl+X** para sair):
   ```bash
   sudo nano /opt/structura/server/.env
   ```
3. Reinicie e confira:
   ```bash
   sudo systemctl restart structura
   sudo systemctl status structura --no-pager | head -5
   curl https://SEU-DOMINIO/health
   ```
4. Deu certo? **Apague a cópia** (ela contém segredos): `sudo shred -u /opt/structura/server/.env.bak`.
   Deu errado? Volte: `sudo cp -p /opt/structura/server/.env.bak /opt/structura/server/.env && sudo systemctl restart structura`.
5. Guarde a mudança também no seu `server\deploy\secrets\prod.env` (para que a cópia local continue igual à da VM).

**Enviar um `.env` inteiro novo** (por exemplo, o `prod.env` regenerado): faça upload para a pasta pessoal da VM e rode
```bash
sudo install -m 600 -o structura -g structura ~/prod.env /opt/structura/server/.env
sudo shred -u ~/prod.env
sudo systemctl restart structura
```

---

## 5. Como gerar segredos novos

> Todos os scripts abaixo rodam **na sua máquina Windows**, na raiz do repositório, com o Python do projeto
> (`server\.venv\Scripts\python.exe`). Eles **recusam sobrescrever** arquivos existentes — de propósito.

### 5.1 `.env` de desenvolvimento (servidor no seu PC)
```powershell
cd server
.venv\Scripts\python.exe gen_keys.py --write      # cria server\.env com chaves novas (0600). Recusa se já existir
```
Para refazer do zero: renomeie o `.env` antigo **e o banco `dev.db`** (as contas antigas não abrem com chaves novas), gere de novo e recrie o admin (§5.4).
Depois atualize `kServerPubKey` do ramo **DEV** em `client/license_config.cpp` (§6.2) para o `build_dev.bat` continuar entrando.

### 5.2 `.env` de produção (VM)
```powershell
server\.venv\Scripts\python.exe server\deploy\gen_prod_env.py
```
Cria `server\deploy\secrets\prod.env` (chaves novas, `TRUST_PROXY_HEADERS=true`, docs desligados, etc.) e `server_pubkey.hex`. Copia a chave pública de release
do `server/.env`. **Só use na primeira instalação.** Em um servidor que já tem usuários, gerar novamente quebra tudo (ver §6). Depois: faça backup do arquivo (§7) e
siga o `MANUAL_SETUP_SERVIDOR_PASSO_A_PASSO.md`.

### 5.3 Chave de release (assina os instaladores)
```powershell
server\.venv\Scripts\python.exe server\deploy\gen_release_key.py
```
Cria `server\release.key` (privada) e imprime a chave pública. **Só faça se a chave antiga vazou ou foi perdida** — veja §6.4, porque invalida os instaladores já distribuídos.

### 5.4 Senha e 2FA do administrador (criar ou refazer)
Na VM, dentro de `/opt/structura/server`:
```bash
sudo runuser -u structura -- /opt/structura/venv/bin/python -m app.create_admin --email admin@exemplo.com --cpf 00000000000
```
- Se o e-mail **já existe**, o comando **troca a senha e gera um segredo 2FA novo** (o autenticador antigo deixa de valer). Use isto para "esqueci a senha do admin" ou "perdi o celular".
- Pede a senha duas vezes (oculta) e mostra o segredo TOTP **uma vez**: cadastre no autenticador antes de fechar o terminal.
- Dica de senha forte aleatória: `server\.venv\Scripts\python.exe server\deploy\gen_secret.py password` (guarde no gerenciador de senhas, não digite de memória).
- Usuários comuns: pelo painel (*Redefinir senha* / *Resetar 2FA*), nunca por script.

### 5.5 Um segredo avulso (para trocar só um valor)
```powershell
server\.venv\Scripts\python.exe server\deploy\gen_secret.py key        # chave de 32 bytes (CPF_ENC_KEY_B64 / CPF_HMAC_KEY_B64)
server\.venv\Scripts\python.exe server\deploy\gen_secret.py signing    # seed nova + chave pública (SERVER_SIGNING_SEED_B64)
server\.venv\Scripts\python.exe server\deploy\gen_secret.py pubkey <SEED>   # chave pública de uma seed que você já tem
```
O que aparece na tela é segredo (exceto as linhas "PUBLICA"): copie direto para o `.env`/cofre e limpe o terminal.

### 5.6 Converter uma chave pública para o código do app
```powershell
server\.venv\Scripts\python.exe server\deploy\pubkey_to_cpp.py <chave-publica-base64url-ou-hex> --name kServerPubKey
```
Imprime o `const uint8_t kServerPubKey[32] = {...};` pronto para colar em `client/license_config.cpp`. Também aceita `--file server\deploy\secrets\server_pubkey.hex`.

---

## 6. Trocar ou rotacionar uma chave (com consequências)

**Antes de qualquer troca em produção:** (1) backup do banco (`sudo /usr/local/bin/structura-backup` e baixe o arquivo), (2) backup do `.env` atual, (3) avise os usuários
se o app vai parar de funcionar, (4) rode `check_secrets.py` ao final.

### 6.1 Resumo
| Chave | Efeito da troca | Precisa novo instalador? |
|---|---|---|
| `SERVER_SIGNING_SEED_B64` | Todos os apps param de reconhecer o servidor; todos precisam reinstalar | **Sim** |
| `CPF_ENC_KEY_B64` | CPFs e 2FA ficam ilegíveis | Não (mas exige migração de dados) |
| `CPF_HMAC_KEY_B64` | Impressões digitais de CPF desatualizadas (sem efeito no login) | Não |
| Chave de release | Instalados e novos deixam de se reconhecer | **Sim** |
| `dll_key` | Só vale para os `.enc` novos | Sim (instalador com `.enc` novos) |

### 6.2 Chave de assinatura do servidor (`SERVER_SIGNING_SEED_B64`)
Faça só se vazou. Todos os usuários terão que instalar a nova versão e entrar de novo.
1. `gen_secret.py signing` → anote a SEED (secreta) e a PUBLICA em **hex**.
2. Em `server\deploy\secrets\prod.env`, troque o valor de `SERVER_SIGNING_SEED_B64`; salve a pública em `server\deploy\secrets\server_pubkey.hex`.
3. `pubkey_to_cpp.py --file server\deploy\secrets\server_pubkey.hex` → cole o array no `kServerPubKey` do ramo `#else` (**produção**) de `client/license_config.cpp`.
   (Para a chave do servidor **de desenvolvimento**, é o ramo `#if defined(STRUCTURA_DEV_BUILD)`.)
4. Envie o `prod.env` novo para a VM (§4, "Enviar um .env inteiro") e reinicie.
5. Gere o instalador (`build_release.bat`) e **republique o manifesto** (a `dll_key` continua a mesma: o banco guarda a chave de cada versão e o `export_bundle.py` a pega do banco local).
6. `check_secrets.py` deve dar `TUDO COERENTE`. Todos os tokens antigos deixam de valer: cada usuário reinstala e faz login de novo.

### 6.3 Chaves do CPF (`CPF_ENC_KEY_B64`, `CPF_HMAC_KEY_B64`)
Trocar simplesmente **destrói** os CPFs e os segredos 2FA já gravados (não dá para decifrar com a chave nova). O caminho seguro é uma **migração**:
decifrar tudo com a chave antiga e recifrar com a nova, antes de trocar o `.env`. **Esse script de migração ainda não existe no projeto** — peça para criá-lo se
precisar (é uma tarefa pequena, mas deve ser testada em uma cópia do banco). Se a chave vazou *sem* o banco, o risco é baixo; se vazou *junto* com o banco, os CPFs estão expostos
(avalie a obrigação de comunicar os titulares conforme a LGPD).
Se só perdeu a chave e aceita perder esses dados: gere novas, apague o banco (ou os CPFs), recrie o admin (§5.4) e peça a cada usuário para configurar o 2FA de novo (painel → *Resetar 2FA*).

### 6.4 Chave de release (assina manifestos)
Perda ou vazamento: gere um novo par e **redistribua** o programa.
1. Guarde o `server\release.key` antigo (renomeie para `release.key.antigo`) e rode `gen_release_key.py` → mostra a nova `RELEASE_PUBKEY_B64`.
2. Atualize a `RELEASE_PUBKEY_B64` em `server/.env`, `server\deploy\secrets\prod.env` **e** na VM (§4).
3. Atualize `kReleasePubKey` em `client/license_config.cpp` (`pubkey_to_cpp.py <nova-publica> --name kReleasePubKey`).
4. `build_release.bat` (assina com a chave nova) e publique o manifesto na VM.
5. O servidor reconfere a assinatura de cada manifesto ao usar: os **manifestos antigos deixam de valer**, então todo mundo precisa do instalador novo.

### 6.5 Chave das DLLs (`dll_key`) — a que mais confunde
- O motor de cálculo viaja como `rds.enc`/`mefs.enc` (cifrados). No login o servidor entrega a `dll_key`; o app decifra o solver na memória.
- Os `.enc` são cifrados **uma vez** (`build_solvers.bat` → `encrypt_dll.py --version 2`) e **reaproveitados em todos os releases**. A chave que o servidor entrega tem que ser
  **exatamente** a que cifrou esses arquivos. Ela vem da versão registrada em `server\deploy\solver_key_version.txt` (hoje `2`) — **não** do número do manifesto do app.
- Erro típico se estiver errada: `DECIPHER_FAILED_MAC_MISMATCH` ao calcular. Confira com `server\deploy\check_dll_key.py rds.enc`.
- Para **trocar** a chave (ex.: vazou): rode `build_solvers.bat` com a nova `--version` (ele grava `rds.enc`/`mefs.enc` novos), atualize `solver_key_version.txt`,
  rode `build_release.bat` (o `export_bundle.py` confere que a chave abre os `.enc`) e publique manifesto + `dll_key.txt` na VM.
- Ao publicar, **sempre** passe `--dll-key` válida (64 hex). Sem isso o servidor mantém a chave que já estava gravada. (O `publish_release.sh` já faz a conta; o CLI atual recusa chave vazia/inválida.)

---

## 7. Onde guardar o quê (backup)

| Guarde | Onde | Frequência |
|---|---|---|
| `server\deploy\secrets\prod.env` | Gerenciador de senhas (ex.: Bitwarden/1Password) como "nota segura" **e** uma cópia offline (pendrive guardado) | Sempre que mudar |
| `server\release.key` | Idem (cofre + offline). **Nunca** na VM | Uma vez |
| Backup do banco `structura-*.db.gz` | Baixar da VM para seu PC/nuvem pessoal criptografada | Semanal (o automático diário fica na própria VM) |
| `server\deploy\bundle\dll_key.txt` | Cofre (é regenerável a partir do banco local, mas guarde) | A cada solver novo |
| Senha do admin + segredo TOTP | Cofre; o TOTP também pode ficar em autenticador com backup (ex.: Authy/Microsoft com backup na nuvem) | Uma vez |
| Chave SSH / acesso à conta da nuvem | Cofre + ative **2FA na conta do Google/provedor** | Uma vez |

Perguntas que você deve conseguir responder: *"se meu PC quebrar hoje, consigo restaurar o servidor e continuar lançando versões?"* Para isso precisa de **`prod.env` + `release.key` +
backup do banco** guardados fora do PC. Sem a `release.key`, os apps já instalados não aceitam releases novos (§6.4).

**Nunca:** colocar segredos no Git (o `.gitignore` já protege `.env`, `*.key`, `*.db`, `secrets/`), colar em conversas, tirar print do terminal com segredo, deixar `.bak` com segredos na VM,
reutilizar a mesma senha em vários lugares.

---

## 8. "Perdi / vazou / não sei" — o que fazer

| Situação | Ação |
|---|---|
| Esqueci a senha do admin | §5.4 (`create_admin` com o mesmo e-mail; gera 2FA novo) |
| Perdi o celular do admin | §5.4 (idem) |
| Cliente perdeu o celular | Painel → Usuários → *Resetar 2FA* |
| Perdi o `prod.env` mas a VM está de pé | O arquivo vivo está em `/opt/structura/server/.env` na VM: `sudo cat` e guarde no cofre imediatamente |
| Perdi o `prod.env` **e** a VM | Se tiver o backup do banco mas não as chaves: CPFs e 2FA irrecuperáveis (§6.3); apps precisam de novo servidor/chave (§6.2) |
| Perdi o `release.key` | §6.4: nova chave + novo instalador para todos |
| Suspeito que alguém viu meu `.env` | Rotacione conforme a tabela da §6.1 (comece pela seed do servidor), troque senhas de admin, revise Telemetria e Auditoria |
| Segredo apareceu no Git | Trate como vazado: rotacione **e** remova do histórico (`git filter-repo`/BFG); apagar o commit sozinho não basta |
| Não sei se está tudo certo | `check_secrets.py` (§9) |
| O servidor não sobe depois de mexer no `.env` | `sudo journalctl -u structura -n 30` mostra qual campo; restaure o `.env.bak` (§4) |

---

## 9. Conferir tudo: `check_secrets.py`

```powershell
server\.venv\Scripts\python.exe server\deploy\check_secrets.py
```
O que ele confirma (sem imprimir segredos): formato das 4 chaves do `.env` de dev e do `prod.env`; `release.key` ↔ `RELEASE_PUBKEY_B64` (dev e prod) ↔ `kReleasePubKey` no app;
seed do `prod.env` ↔ `server_pubkey.hex` ↔ `kServerPubKey` (produção) no app; `dll_key.txt` abre `rds.enc` e `mefs.enc`; nenhum segredo rastreado pelo Git.
Quando aparece `[FALHA]`, a frase diz qual par está incoerente — use a seção correspondente (§6) para corrigir. Rode-o **antes de cada release** e **depois de qualquer troca de chave**.

Ferramentas deste guia: `gen_keys.py` (dev), `gen_prod_env.py` (prod), `gen_release_key.py`, `gen_secret.py`, `pubkey_to_cpp.py`, `check_secrets.py`, `check_dll_key.py`
(todas em `server/` ou `server/deploy/`).

---

## 10. Glossário rápido de nomes que aparecem nos arquivos

| Nome | Significa |
|---|---|
| `*_B64` | valor em base64url (texto de ~43 caracteres que representa 32 bytes) |
| `SECRET`/`SEED` | informação secreta |
| `PUBKEY` | chave pública |
| `.enc` | arquivo cifrado (`rds.enc`, `mefs.enc` = motor de cálculo) |
| `manifest.txt` / `manifest.sig` | lista de arquivos do instalador e sua assinatura |
| `fingerprint` | identificador da máquina (não é segredo) |
| `K` / "keystone" | chave de curta duração que o servidor entrega a cada cálculo validado |
| `DPAPI` | proteção do Windows para o arquivo de sessão do usuário |
