# Build de desenvolvimento e de release

Dois fluxos independentes, com pastas separadas. **Não misture**: o build de release embute a chave pública e a
URL do servidor de **produção**; o de desenvolvimento usa o servidor **local**.

| | Desenvolvimento | Release |
|---|---|---|
| Script | `build_dev.bat` | `build_release.bat` (completo: `build_full_release.bat`) |
| Pastas | `build-dev\`, `dist-dev\` | `build\`, `build-demo\`, `dist\` |
| `STRUCTURA_DEV_BUILD` | ON (aceita DLL do solver em claro; chave pública do servidor **local**) | OFF (DLLs cifradas `.enc`; chave pública do servidor de **produção**) |
| Servidor | `http://127.0.0.1:8000` (ou `STRUCTURA_LICENSE_SERVER`) | URL HTTPS embutida (`server\deploy\production_url.txt`) |
| Manifesto | versão fixa **9001**, republicada a cada build | versão **automática** crescente |
| Gera instalador | não | sim (Comercial e Demo, NSIS) |

## 1. Pré-requisitos
Qt 6.9 (msvc2022_64), VTK 9.5 compilado, Visual Studio (MSVC), CMake, NSIS (`makensis`, só release), Python 3 com o venv do
servidor (`server\.venv`), `server\release.key` (chave **privada** de release — guardar fora do git/da nuvem).
Os caminhos do Qt/VTK são descobertos do cache de um build anterior (`build\CMakeCache.txt`); para a primeira vez ou outra
máquina defina `STRUCTURA_QT_PREFIX` (ex.: `D:/Qt/6.9.3/msvc2022_64/lib/cmake`) e `STRUCTURA_VTK_DIR`
(ex.: `D:/src/VTK/build/lib/cmake/vtk-9.5`).

## 2. Desenvolvimento — `build_dev.bat`
```powershell
build_dev.bat              # compila (Release) e publica o manifesto 9001 no banco do servidor LOCAL
build_dev.bat debug        # Debug
build_dev.bat run          # idem + abre o servidor local numa janela
```
Depois: servidor local em `server` (`.venv\Scripts\python.exe -m uvicorn app.main:app --reload`), contas com
`python -m app.create_admin` / `app.create_user`, e execute `dist-dev\Structura-Metalicas\Structura.exe`.
Se mexer em código C++ e só quiser recompilar, rode o script de novo (é incremental).

## 3. Release — `build_release.bat`
Pré-requisitos extras: `rds.enc` e `mefs.enc` na raiz (gerados por `build_solvers.bat`; o fluxo todo em
`build_full_release.bat`), NSIS instalado e **`server\deploy\production_url.txt`** com a URL HTTPS do servidor
(uma linha, ex.: `https://129-146-10-20.sslip.io`). Sem esse arquivo o script para com erro (evita instalador apontando
para o lugar errado).

O script faz: compila Comercial → monta `dist\Structura-Metalicas` → **gera o manifesto assinado** (versão automática) →
publica no banco local → `export_bundle.py` (grava `server\deploy\bundle\{manifest.txt,manifest.sig,dll_key.txt}`) → NSIS →
repete para a edição Demo. Instaladores em `dist\installers\`.

### Versão do manifesto (automática)
`server\deploy\manifest_version.txt` guarda a última versão usada; cada `build_release.bat` usa `última + 1` e só grava depois do
sucesso. As versões anteriores **continuam válidas** no servidor, então instalações antigas não quebram. Para forçar a
atualização, revogue as antigas no servidor:
`python -m app.manifest_cli set-status --version N --status revoked` (o app antigo passa a receber `CLIENT_OUTDATED`).
Nunca republique a mesma versão com arquivos diferentes (as instalações dessa versão passam a dar `INTEGRITY_FAIL`).

### Demo: manifesto próprio
A Demo tem arquivos e `.exe` diferentes da Comercial, e o servidor valida raiz de arquivos + hash do exe contra o manifesto **da versão informada**.
Por isso `build_release.bat` usa **N** para a Comercial (`server\deploy\bundle\`) e **N+1** para a Demo (`server\deploy\bundle\demo\`), e os dois são
publicados no servidor com `server\deploy\oracle\publish_releases.sh`. Roteiro completo de um release: `docs/RELEASE_0.9.1.md`.

### Chave das DLLs dos solvers (`dll_key.txt`) — não é a versão do manifesto
`rds.enc`/`mefs.enc` são cifrados **uma vez** (`build_solvers.bat` → `encrypt_dll.py --version 2`) e reaproveitados em vários releases.
A chave que o servidor entrega ao app no login tem que ser **a mesma** com que eles foram cifrados. Ela vem da versão registrada em
`server\deploy\solver_key_version.txt` (hoje **2**), e **não** da versão do manifesto do app. O `export_bundle.py` confere que a chave abre os `.enc`
e aborta se não abrir. Se você re-cifrar os solvers com outra `--version`, atualize esse arquivo e publique de novo.
Sintoma de chave errada no app: `DECIPHER_FAILED_MAC_MISMATCH` ao calcular. Diagnóstico: `python server\deploy\check_dll_key.py rds.enc`.
Depois de corrigir a chave no servidor, o usuário precisa **sair e entrar de novo** no app (a chave vem no login e fica na sessão salva).

### Publicar o release no servidor de produção
O manifesto precisa existir no **servidor de produção** (não só no banco local), senão todo login dá `INTEGRITY_FAIL`:
```powershell
scp -i <chave> server\deploy\bundle\manifest.txt server\deploy\bundle\manifest.sig server\deploy\bundle\dll_key.txt ubuntu@<IP>:~/upload/
ssh -i <chave> ubuntu@<IP> "sudo bash /opt/structura/server/deploy/oracle/publish_release.sh"
```
(Outro host: `python -m app.manifest_cli publish --text manifest.txt --sig manifest.sig --dll-key <hex> --status current` dentro de `server/`.)
Ordem recomendada: **publicar o manifesto antes de distribuir o instalador**.

## 4. Mudou a URL ou a chave do servidor?
- **URL**: edite `server\deploy\production_url.txt` e gere novo release. Nada mais no código.
- **Chave pública do servidor** (`SERVER_SIGNING_SEED_B64` trocada): atualize `kServerPubKey` (ramo de release) em
  `client/license_config.cpp` com o novo `secrets\server_pubkey.hex` e gere novo release. Todos os usuários precisarão reinstalar
  e todos os tokens/segredos 2FA/CPFs cifrados antigos deixam de valer — por isso evite trocar (ver `DEPLOY_SERVIDOR.md`, §Rotação).

## 5. Armadilhas conhecidas (Windows)
- **Caminho com acento** (`C:\Users\João\...`): o `cmd.exe` pode não achar o `.bat` por causa da página de código. Rode pelo
  terminal do Visual Studio/PowerShell em UTF-8 (`chcp 65001`), ou execute a partir de um clone em caminho sem acentos. Evite o
  caminho curto 8.3 (`JOO~1`): o MSBuild deixa de recompilar e o `.exe` fica desatualizado.
- **`.bat` com final de linha LF**: use CRLF (o `build_dev.bat` já está). Se o `cmd` disser que o arquivo "não é reconhecido",
  confira isso e o diretório atual.
- **Testes presos** (`TestUiLicenseIntegration.exe`/`ctest` órfãos) seguram o `.exe` e causam `LNK1104`: encerre os processos.
- **`g++` do MinGW/Strawberry** no PATH pode quebrar `test_m2_integrity.py` (4 testes `test_cpp_*`): ambiente, não código.
- Cada rebuild muda o `.exe` → **republique o manifesto** (o script faz para o local; o de produção é o passo manual acima).
- O instalador instalado antes de um novo manifesto continua funcionando (versões coexistem); o problema só ocorre ao
  **sobrescrever a mesma versão**.
