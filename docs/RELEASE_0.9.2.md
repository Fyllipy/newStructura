# Release 0.9.2 beta — o que foi gerado e como publicar (sem afetar as versões anteriores)

Gerado em 28/11/2026. Coexiste com a 0.9.1, 0.9.0 e anteriores: o servidor guarda **um manifesto por versão** e as antigas continuam válidas.
Versões de manifesto: **6 = Comercial 0.9.2**, **7 = Demo 0.9.2** (a Demo tem manifesto próprio, como na 0.9.1).
No servidor local o `list` termina assim: `1 allowed, 2 current, 3 current (0.9.0), 4 current (0.9.1 Comercial), 5 current (0.9.1 Demo), 6 current, 7 current, 9001 dev`.

## O que mudou em relação à 0.9.1
- **Joint Mechanical Engine (Incrementos 1–6):** na janela "Comportamento do nó" o projetista vê o comportamento requerido (NBR 8800:2024, 6.1.2),
  escolhe e configura a ligação, obtém a resposta mecânica (método dos componentes, EN 1993-1-8) **nos dois sentidos do momento**, a classificação
  normativa da ligação real e a comparação com o requerido (Atende / Não atende / Indeterminado / Não aplicável).
- Tipologias com curva M–θ: chapa de extremidade, mesas soldadas (144–146), emenda de viga (135) e viga–pilar flangeada fora do nó (147).
  Perfis T (143): **só resistência** (nível C, NBR). As demais seguem sem modelo mecânico (a interface diz quais capacidades existem).
- A seção do pilar passa a ser exigida (barra I/H vinculada ou geometria confirmada) nas famílias de momento: nenhum pilar padrão silencioso.
- Declaração de sistema estrutural e contraventamento nas Propriedades do projeto.
- **Solvers inalterados:** `rds.enc`/`mefs.enc` são **idênticos** aos da 0.9.1 (conferido byte a byte) e a chave dos solvers é a mesma
  (`dll_key.txt` com o mesmo SHA-256 da 0.9.1). Nenhuma instalação precisa de chave nova.
- **O servidor não precisa ser atualizado nem reiniciado:** o protocolo cliente↔servidor não mudou e este pacote só adiciona dois manifestos.
  (As mudanças de servidor commitadas no repositório — cobrança/pagamentos/portal — **não fazem parte** deste pacote; são outra implantação.)
- Validação antes de gerar: `ctest` do build-dev 47/47; compilação limpa das edições Comercial e Demo (executável Demo ligado com sucesso).

## Arquivos
| O quê | Onde | Destino |
|---|---|---|
| Instalador Comercial | `dist/installers/Structura-Metalicas-Setup-0.9.2beta.exe` (SHA-256 `eea98fcd1e255f05a8d2fea2468e4aaf00ca9afdc098b23703b84c7ea89ad534`) | usuários (**depois** de publicar no servidor) |
| Instalador Demo | `dist/installers/Structura-Metalicas-Demo-Setup-0.9.2beta.exe` (SHA-256 `b3ebf66f2fe0f0ebfaf74e08ad2a7bbe8a2f6020a17a60c40fe50cf782f44c9b`) | idem |
| Pacote para o servidor | `dist/upload-servidor-0.9.2.tar.gz` (SHA-256 `89d93c87b8db5c2b51f3c1851593884b9e6f44da94caae71fb7ab01000091d36`; conteúdo em `dist/upload-servidor-0.9.2/`) | **servidor** |
| Backup dos artefatos da 0.9.1 | `dist/archive-0.9.1/` (bundle, `.enc`, versão de manifesto) e `dist/upload-servidor-0.9.1*` | só local |

O pacote contém `comercial/{manifest.txt,manifest.sig,dll_key.txt}`, `demo/{…}` e `publish_releases.sh`. **`dll_key.txt` é segredo** (chave dos solvers): não envie por
e-mail/chat, não versione, e o script o apaga do servidor depois de usar.

## Publicar no Google Cloud (VM do servidor) — passo a passo
Regra de ouro: **manifestos no servidor primeiro, instaladores aos usuários depois.** Nada abaixo altera as versões 1–5 já publicadas.

1. **Enviar** o arquivo único `dist/upload-servidor-0.9.2.tar.gz`:
   - *CLI* (PowerShell na máquina de build): `gcloud compute scp --zone <ZONA> dist\upload-servidor-0.9.2.tar.gz structura-server:~/`
   - *Navegador*: Compute Engine → Instâncias de VM → **SSH** → engrenagem → *Upload file* → escolha o `.tar.gz`.
2. **Conferir o arquivo e extrair** (no SSH da VM):
   ```bash
   sha256sum ~/upload-servidor-0.9.2.tar.gz     # deve ser 89d93c87b8db5c2b51f3c1851593884b9e6f44da94caae71fb7ab01000091d36
   mkdir -p ~/upload-0.9.2 && tar -xzf ~/upload-servidor-0.9.2.tar.gz --strip-components=1 -C ~/upload-0.9.2 && ls -R ~/upload-0.9.2
   ```
   Esperado: `~/upload-0.9.2/comercial/{manifest.txt,manifest.sig,dll_key.txt}`, `~/upload-0.9.2/demo/{…}` e `~/upload-0.9.2/publish_releases.sh`.
   (Pasta própria da 0.9.2: não mexe em nada que sobrou de publicações anteriores.)
3. **Antes de publicar, registre o estado atual** (somente leitura):
   ```bash
   cd /opt/structura/server && sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli list
   ```
   Anote a lista (deve conter `3`, `4` e `5` como `current` e **não** conter `6` nem `7`).
4. **Publicar**:
   ```bash
   sudo UPLOAD_DIR=$HOME/upload-0.9.2 bash ~/upload-0.9.2/publish_releases.sh
   ```
   O script mostra a lista de versões antes e depois, publica `comercial` (versão 6) e `demo` (versão 7) **sem `--overwrite`** — se a versão já existir com outro
   conteúdo a publicação **falha** e nada é alterado —, e apaga as chaves temporárias. Depois deve listar `… 3 current, 4 current, 5 current, 6 current, 7 current …`.
5. **Conferir**: `curl https://34-26-242-178.sslip.io/health` → `{"status":"ok","db":"ok"}` e `app.manifest_cli list` igual ao passo 3 acrescido de 6 e 7.
6. **Testar login** com um usuário real, **nesta ordem**: (a) uma instalação **0.9.1 ou 0.9.0 existente** continua entrando e calculando (prova que as versões antigas não foram afetadas);
   (b) instalar a 0.9.2 Comercial, entrar, abrir um modelo, selecionar um nó → "Comportamento do nó…", calcular; (c) instalar a 0.9.2 Demo e entrar.
7. **Distribuir** os instaladores 0.9.2.

## Se algo der errado
- `INTEGRITY_FAIL` na 0.9.2 → o manifesto 6/7 não foi publicado ou é de outro build. Confira o `list` na VM. **Nunca** republique a mesma versão com arquivos diferentes: gere um novo build (versão seguinte, 0.9.3 / manifestos 8 e 9).
- `DECIPHER_FAILED_MAC_MISMATCH` ao calcular → chave dos solvers errada; na máquina de build: `python server\deploy\check_dll_key.py rds.enc`. (Esperado não ocorrer: a chave é a mesma da 0.9.1.)
- Voltar atrás: continue distribuindo o instalador 0.9.1 (`dist/installers/Structura-Metalicas-Setup-0.9.1beta.exe`; as versões 4/5 seguem `current`). Para bloquear a 0.9.2 no servidor:
  `cd /opt/structura/server && sudo -u structura /opt/structura/venv/bin/python -m app.manifest_cli set-status --version 6 --status revoked` (e `--version 7` para a Demo).
  Isso não afeta as versões 1–5.

## Pendências e avisos
- Instalação/desinstalação dos instaladores **não foi exercitada** aqui (exige UAC): teste em máquina limpa antes de distribuir. O executável de produção (com login) não foi aberto nesta sessão; a verificação visual
  foi feita no build de desenvolvimento (sem login).
- A edição Demo não tem o fluxo de dimensionamento (só stubs): a janela "Comportamento do nó" da Demo não calcula ligações.
- A arte da tela de abertura ainda mostra "Beta 0.8.1".
- Decisão que fica com você: revogar a versão 3 (0.9.0), cujo `rds.dll` contém o desvio de chave de teste, depois que os usuários migrarem
  (`set-status --version 3 --status revoked`; o app 0.9.0 passa a receber `CLIENT_OUTDATED`).
- Os artefatos `dist/` e `build*/` não são versionados (ver `.gitignore`); o pacote e os instaladores existem só na máquina de build.
