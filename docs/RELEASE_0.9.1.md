# Release 0.9.1 beta — o que foi gerado e como publicar

Gerado em 24/11/2026. Coexiste com a 0.9.0: o servidor guarda **um manifesto por versão** e as antigas continuam válidas
(`app.manifest_cli list` no servidor local: `1 allowed, 2 current, 3 current (0.9.0), 4 current (0.9.1 Comercial), 5 current (0.9.1 Demo), 9001 dev`).

## O que mudou em relação à 0.9.0
- Solvers recompilados (`rds.dll`/`mefs.dll` → novos `rds.enc`/`mefs.enc`) com as correções D1/D2 do código-fonte e **sem** o desvio de teste
  `STRUCTURA_TEST_DLL_KEY` dentro das DLLs (o `rds.dll` da 0.9.0 ainda o continha; conferido por busca de texto nos binários).
- Cifra dos solvers com a **mesma chave da versão 2**: as instalações 0.9.0 continuam recebendo a chave certa para os `.enc` delas.
- Recursos de casca e modos de flambagem/modal de casca no diálogo, e demais alterações do ramo `implantacao_dll`.
- **Demo agora tem manifesto próprio (versão 5).** Antes a Demo usava a mesma versão da Comercial, mas o servidor compara a raiz de
  arquivos e o hash do `.exe` com o manifesto daquela versão: a Demo dava `INTEGRITY_FAIL` no servidor de produção. `build_release.bat`
  passou a gerar/publicar Comercial = N e Demo = N+1 (`server/deploy/bundle/demo/`). **A Demo 0.9.0 continua com esse problema**
  (o manifesto 3 publicado é o da Comercial); só a 0.9.1 corrige.
- Validação antes de gerar: `fem_tests` e `solver_tests` sem falhas; `ctest` do build-dev 28/28 (inclui `TestTowerModel`, `TestShellCoupling`);
  `check_attestation` local OK para as versões 3, 4 e 5 e `FAIL` se a Demo apresentar a versão da Comercial.

## Arquivos
| O quê | Onde | Destino |
|---|---|---|
| Instalador Comercial | `dist/installers/Structura-Metalicas-Setup-0.9.1beta.exe` | distribuir aos usuários (**depois** de publicar no servidor) |
| Instalador Demo | `dist/installers/Structura-Metalicas-Demo-Setup-0.9.1beta.exe` | idem |
| Pacote para o servidor (manifestos + chave + script) | `dist/upload-servidor-0.9.1.tar.gz` (conteúdo em `dist/upload-servidor-0.9.1/`) | **servidor** |
| Backup da 0.9.0 | `dist/archive-0.9.0/` (DLLs, `.enc`, instaladores, manifesto) | só local, para voltar atrás |

`dll_key.txt` é segredo (chave dos solvers): não envie por e-mail/chat nem versione. O servidor não precisa ser atualizado: o protocolo
cliente↔servidor não mudou.

## Publicar no Google Cloud (VM do servidor)
Regra: **manifesto no servidor primeiro, instalador aos usuários depois.**

1. **Enviar** o arquivo único `dist/upload-servidor-0.9.1.tar.gz` (contém `comercial/`, `demo/` e `publish_releases.sh`):
   - *CLI* (PowerShell na máquina de build): `gcloud compute scp --zone <ZONA> dist/upload-servidor-0.9.1.tar.gz structura-server:~/`
   - *Navegador*: Compute Engine → Instâncias de VM → **SSH** → engrenagem → *Upload file* → escolha o `.tar.gz`.

   Depois, no terminal da VM:
   ```bash
   mkdir -p ~/upload && tar -xzf ~/upload-servidor-0.9.1.tar.gz -C ~/upload && ls -R ~/upload
   ```
   Esperado: `~/upload/comercial/{manifest.txt,manifest.sig,dll_key.txt}`, `~/upload/demo/{…}` e `~/upload/publish_releases.sh`.
2. **Publicar** (no SSH da VM):
   ```bash
   sudo bash ~/upload/publish_releases.sh
   ```
   O script mostra a lista de versões antes e depois, **sem** `--overwrite` (não altera versões já publicadas) e apaga as chaves temporárias.
   Depois deve listar `… 3 current, 4 current, 5 current …`.
3. **Conferir**: `curl https://34-26-242-178.sslip.io/health` → `{"status":"ok","db":"ok"}`.
4. **Testar login** com um usuário real: (a) instalar a 0.9.1 Comercial e entrar, calcular um modelo; (b) instalar a 0.9.1 Demo e entrar;
   (c) abrir uma instalação **0.9.0** existente e confirmar que ainda entra e calcula (sai e entra de novo se ela estiver com sessão antiga).
5. **Distribuir** os instaladores 0.9.1.

## Se algo der errado
- `INTEGRITY_FAIL` na 0.9.1 → o manifesto da versão não foi publicado (ou foi publicado outro conteúdo). Confira `list` na VM; **nunca** republique
  a mesma versão com arquivos diferentes: gere novo build (versão seguinte).
- `DECIPHER_FAILED_MAC_MISMATCH` ao calcular → chave dos solvers errada: `python server\deploy\check_dll_key.py rds.enc` na máquina de build.
- Voltar atrás: distribua o instalador 0.9.0 de `dist/archive-0.9.0/` (a versão 3 segue `current`). Para bloquear a 0.9.1 no servidor:
  `python -m app.manifest_cli set-status --version 4 --status revoked` (e 5 para a Demo) dentro de `/opt/structura/server`.

## Decisão que fica com você
- **Forçar a migração da 0.9.0**: a 0.9.0 continua aceita. Como o `rds.dll` dela contém o desvio de chave de teste, vale revogar a versão 3 depois que os
  usuários migrarem: `sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli set-status --version 3 --status revoked`
  (o app 0.9.0 passa a receber `CLIENT_OUTDATED`).

## Pendências conhecidas
- Instalação/desinstalação dos instaladores **não foi exercitada** aqui (exige UAC); só a geração. Teste em uma máquina limpa antes de distribuir.
- O `.exe` da 0.9.1 não foi aberto nesta sessão, nem os diálogos de flambagem/modal com modos de casca (a ligação foi testada em unidade).
- A arte da tela de abertura ainda mostra "Beta 0.8.1".
- Alterações do ramo estão **sem commit** (ProductInfo 0.9.1, scripts de release, DLLs/.enc novos): faça o commit/tag `v0.9.1` quando aprovar.
