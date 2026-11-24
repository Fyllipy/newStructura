# Etapa 4 — Relatório intermediário (WP1, WP2 e build das DLLs candidatas)

Estado: **parcial**. Executados WP1, WP2 (auditoria da chave de teste) e a compilação isolada de WP3. **Não executados:** `.enc`, testes diretos nas DLLs (WP5), aplicativo completo (WP6), instaladores (WP8), reprodutibilidade (WP9). Nenhum artefato comercial foi alterado.

## 1. WP1 — Estado inicial e congelamento

- Commit base: `d9e48385799ae2274be4f6d3fad6a567d60ed9e8` (branch `implantacao_dll`). Há alterações não commitadas (Etapas 3 e 4 + gerador de escadas anterior). Nenhum commit foi feito, pois não foi pedido.
- Congelamento por arquivos: `freeze_working_tree.patch` (SHA-256 `635a9c82…e7e0`) e `freeze_source_manifest.txt` (39 arquivos com SHA-256), em `scratchpad\etapa4\`. Os fontes compilados vêm da cópia `src_freeze\`, feita depois das edições abaixo.
- Hashes de referência, preservados (cópia em `scratchpad\etapa4\referencia_antiga\`) e conferidos de novo no fim desta fase:

| Artefato | SHA-256 (16 primeiros) |
|---|---|
| rds.dll | f40c09a9d0329b00 |
| mefs.dll | 09ffc08a6001b2c4 |
| rds.enc | c3a9b8b2c3b625ed |
| mefs.enc | ff6c9550e2d2f750 |
| dist/Structura-Metalicas/Structura.exe | 04298a1a0bffdde2 |
| dist/Structura-Metalicas-Demo/StructuraDemo.exe | 61c4812938f6d750 |

- Ferramentas: MSVC `cl` 19.51.36248 (toolset 14.51.36231), linker 14.51.36248.0, CMake 4.3.2, Ninja 1.12.0 (não usado: o projeto original usa o gerador padrão do CMake, multi-config, `build\bin\Release`). Gerador usado: o padrão, para manter a equivalência com `build_solvers.bat`.
- Rastreabilidade (mínima; arquivos confirmados por `git diff`):

| Correção | Arquivos | Testes |
|---|---|---|
| D1 | `Structuradll/src/core/solver_core.cpp` | Etapa 3 WP1 (híbrido Vlasov) |
| D2 | `Femdll/src/fem_model.cpp/.h` | Etapa 3 (400 problemas) + `fem_tests` |
| Qualidade de malha | `src/shell/ShellMeshQuality.*`, `src/ui/ShellDialog.cpp` | `TestShellMeshQuality` |
| Combinações | `src/shell/ShellJob.*`, `src/app/ShellSolverBridge.*`, `src/app/SolverBridge.h`, `src/ui/ShellDialog.cpp` | `TestShell`, `TestShellCoupling` |

  Observação: D1 e D2 estão **nas DLLs candidatas**. Qualidade de malha e combinações ficam **no aplicativo**, não nas DLLs.

## 2. WP2 — Chave de teste (achado da Etapa 3 corrigido)

Os caminhos que liam `STRUCTURA_TEST_DLL_KEY` sem `#if` foram colocados sob `STRUCTURA_DEV_BUILD`. Nenhuma lógica de proteção foi enfraquecida: em build comercial os caminhos foram **removidos**, e o comportamento passa a ser o do ramo que já valia sem a variável.

| Arquivo | Alteração |
|---|---|
| `client/dll_cipher.cpp` (`DeriveDllSessionKey`, `DecryptDllFileInMemory`) | leitura da variável só em dev; comercial: sessão offline ⇒ `false` |
| `Structuradll/src/core/solver_security.h` (`antiDebugPoison`, `computeKeystonePoison`) | desvio por variável só em dev |
| `src/app/SolverBridge.cpp` | comercial: sem Keystone válido ⇒ bloqueio sempre (antes, havia modo de teste por variável também no comercial); `s_mask` de teste só em dev |

Evidência:
- Pré-processamento do mesmo TU (`constraints.cpp`, que inclui `solver_security.h`) com e sem `STRUCTURA_DEV_BUILD`: **0** ocorrências do nome no comercial, **2** no dev.
- Binários compilados, busca por `STRUCTURA_TEST_DLL_KEY`: DLLs antigas **1/1** (rds/mefs); candidatas comerciais **0/0**; candidatas dev 1/1.
- A dependência `api-ms-win-crt-environment` (usada por `getenv`) desapareceu do `rds.dll` comercial candidato.

Consequências a registrar:
- A DLL comercial **não pode** ser exercitada com a chave de teste (por desenho). A bateria numérica da WP5 vai usar a **variante de teste** (`cand_dev`), compilada do mesmo fonte e diferindo apenas por `STRUCTURA_DEV_BUILD`. Isso **não** valida a DLL comercial; o que ficar dependente de sessão de servidor será classificado como NÃO TESTADO.
- Testes do projeto que dependem da variável só funcionam em build de dev. O `ctest` de `build-dev` precisará, após a promoção, de uma DLL de variante dev ao lado dos testes (hoje usa `rds.dll`/`mefs.dll` da raiz).
- Outros usos de ambiente em Release (não relacionados à chave): `STRUCTURA_SESSION_FILE` (`client/license_config.cpp:56`, redireciona o arquivo de sessão, sem `#if`) e `STRUCTURA_LICENSE_SERVER` (já limitado a HTTPS). Registrados como observação, **não alterados**.

`dist/Structura/`: contém `rds.dll`/`mefs.dll` em claro e `fem_dll.dll`/`solver_dll.dll`. Os hashes de `rds.dll` e `mefs.dll` (`9b9a6b95…`, `d44d23f6…`) **não** coincidem com os da raiz; nenhum `.bat` ou `CMakeLists.txt` referencia `dist/Structura`. Origem: não identificada por script (provável cópia manual antiga). Nenhum arquivo foi apagado. Correção do empacotamento fica para WP8.

## 3. WP3 — DLLs candidatas (build isolado)

Compiladas em cópias do fonte congelado (`scratchpad\etapa4\cand_commercial` e `cand_dev`), com as mesmas flags do projeto (`/O2 /GR- /Gy /Gw /GL /GS- /Ob2`, `/LTCG`, `/BREPRO`, pós-build `protect_pe.py` na cópia). O pós-build copia para a raiz **da cópia**, nunca para o repositório.

| DLL | Tamanho | SHA-256 |
|---|---|---|
| rds.dll antiga | 825 344 | f40c09a9d0329b00… |
| **rds.dll candidata comercial** | 826 368 | caa23031964637cc9f1e6303f8aaaba26dbdc1d1eae43163f3f78eedd8b13201 |
| rds.dll candidata de teste (dev) | 826 880 | 37b107a3da82e9da4876205b8838f3d392afba8efe5bd0aab6b5e4f76362189d |
| mefs.dll antiga | 223 744 | 09ffc08a6001b2c4… |
| **mefs.dll candidata comercial** | 249 856 | 2e890b13adad548b079093e3d280c4f6f90bc7bf22d844cacd4b1886411bc8ce |
| mefs.dll candidata de teste (dev) | 249 856 | 5f72381bea08d43f445634622c3e50046c73b4d4412b887b4277c51efb58d7ad |

Comparação estrutural com as antigas (`dumpbin`): x64, 6 seções, mesmos exports (`rds`: `_z8r4q`, `_z8r4q_lock`, `_z8r4q_unlock`; `mefs`: `_z8m2p`), mesmas DLLs de dependência, salvo a CRT de ambiente (rds, esperado) e a CRT de matemática (mefs). Offsets dos exports mudaram (esperado).

**Diferença não explicada ainda:** `mefs.dll` cresce 11,7 % (223 744 → 249 856 B). Causas plausíveis: o código de D2 e a versão do compilador. **Não verificado**; será investigado antes do `.enc`. A candidata comercial e a de teste do `mefs` têm o mesmo tamanho, mas hashes diferentes.

Datas/`timestamp` do PE: 0 (`/BREPRO`), então a comparação de data não se aplica.

## 4. Crescimento do `mefs.dll` — explicado
Controle: o fonte **antigo** de `fem_model.*` (HEAD) compilado com o toolchain atual dá exatamente **223 744 B** (hash `e3e3a2c1…`, diferente da DLL distribuída, que não é reproduzível bit a bit com outro compilador). O crescimento de 11,7 % (seção de código `.s0` 0x2FD28 → 0x35518) vem portanto do **código de D2** (conjunto ativo por caso, detecção de ciclo, verificações de resíduo/equilíbrio/contato), e não do compilador. `rds.dll`: `.s0` +0x460 (≈1,1 KB), compatível com D1 e com a remoção do caminho de teste.

## 5. WP5 — testes diretos nas DLLs (variante de teste `cand_dev`; DLL antiga como controle)
Mecanismo: o caminho já usado pelo projeto (DLL em claro + chave de teste só no processo filho), nunca nos binários comerciais. Logs em `scratchpad\etapa4\logs\`.

| Teste | DLL antiga | DLL candidata (teste) |
|---|---|---|
| **D2** — 400 problemas aleatórios (caso junto × cada caso isolado), via gatekeeper de `mefs.dll` | 215 coincidem, **172 discordâncias**, 13 falhas coerentes | 215 coincidem (desvio máx. 0), **0 discordâncias**, 185 falhas coerentes (nenhum caso isolado resolve) |
| D2 — molas tracionadas (A isolado × A junto de B), F=1e4/2e4/3e4 | desvio 0,38 / 0,78 / 0,98; **3/6/8 molas tracionadas** | desvio 0; 0 molas tracionadas; ordem [B,A]×[A,B] igual |
| D2 — superposição A+B+C × carregamento combinado direto | erro 2,4e-2 / 0,23 / 0,38 | erro 0,45 / 1,34 / 17,6 (a superposição **não vale**; protegida no app) |
| D2 — limite de iterações e partida inicial das molas | NÃO TESTADO na DLL (o limite é fixo em 100 e a partida inicial só existe no `fem_core` instrumentado); testado no `fem_core` na Etapa 3 |
| **D1** — viga I, barra Vlasov + casca por `RigidLink`, via `SolverBridge` e `rds.dll` | φ ponta **−99,4 %**, reação de torção na raiz **3,86 %** do torque, 21 avisos "no stiffness… fixed automatically" | φ ponta **−63,2 %**, reação **99,99 %**, 1 aviso (matriz mal condicionada, razão de pivôs 4,8e17) |
| D1 — barra Vlasov isolada (sem casca) | φ = Vlasov (0,0 %), reação 100 % | idêntico |
| D1 — barra isolada sem empenamento | +180 % de Vlasov (esperado: sem Cw), reação 100 % | idêntico |
| D1 — barra sem apoio de torção (mecanismo isolado) | erro explícito | erro explícito |
| Equivalência `mefs.dll` × `fem_core` corrigido (8 modelos) | `rel` 0 em 7 modelos; molas: diverge (D2) | `rel` 0 em 7 modelos; o caso de molas agora dá erro nos dois |
| API e ciclo de vida do `mefs.dll` (1 000 ciclos, 4 instâncias, handle inválido, recuperação, 4 threads, erro de montagem) | OK | OK; diff antigo × novo só em ruído (memória, chave de sessão) |
| Unidades/sinais e `springs` no `mefs.dll` | — | idênticos à antiga, exceto as molas (D2) |
| Suíte do aplicativo (23 executáveis, 342 verificações, 6 SKIP de ambiente) usando cada par de DLLs | 342 ok / 0 falhas | 342 ok / 0 falhas |

Limitações e achados novos:
1. **Interface barra→casca (−63 %)**: continua, como esperado. Não é falha corrigida.
2. **Mecanismo em grupo ligado por vínculos penalizados NÃO é detectado como erro (achado novo, não corrigido).** Modelo "Vlasov + casca, raiz livre à torção": a DLL antiga resolve com aviso de "fixed automatically" (φ=−4,3e-4, que é falso); a candidata devolve **φ = −137 rad** com o mesmo aviso não específico de mau condicionamento (4,8e17) que também aparece no caso válido. O aviso não distingue os dois. O mecanismo da barra isolada é detectado nas duas. **Restrição ao uso:** o aplicativo não pode tratar "mal condicionada" como prova de modelo válido.
3. **Tie 63 barra→casca**: reação de torção na raiz **0,01 %** do torque nas duas DLLs e igual ao fonte. O tie sem braço não é um caminho de torção válido. Já era limitação conhecida, agora confirmada na DLL. Não é regressão.
4. Escala ×1000 no modelo híbrido: as duas DLLs devolvem "estrutura instável" (3–4 GDL). Pode ser limitação do modelo de teste (seção não escalada), **não investigado**; fica como NÃO CONCLUÍDO.
5. Benchmarks analíticos de casca (placas SS/engastada, balanço, membrana) foram rodados no `fem_core` corrigido. A DLL entra por equivalência bit a bit (`rel` = 0 em 7 modelos), **não** por benchmark próprio: PARCIAL.
6. API/ciclo de vida do **`rds.dll`** (entradas NaN/Inf, ids duplicados, estado global `g_shells`/`g_ties` entre modelos): NÃO TESTADO diretamente nesta fase. Só há 8 execuções sequenciais no mesmo processo, sem comparação de bits.

## 6. Reprodutibilidade do build (WP9, parcial)
Mesmo diretório de build, 2 compilações: `rds.dll` e `mefs.dll` **idênticos bit a bit** (`caa23031…`, `2e890b13…`). Em **diretório diferente**, `mefs.dll` é idêntico e `rds.dll` difere em 9 314 bytes, com mesmo tamanho. Causa não identificada (provavelmente caminho); registrada como reprodutibilidade bit a bit **condicionada ao caminho de build**. Manifesto com SHA-256 de 89 insumos/produtos: `docs/etapa4_manifest.json`.

## 7. Empacotamento (WP2.3/WP8, parcial)
O `CMakeLists.txt` já remove `rds.dll`/`mefs.dll` em claro de builds com `STRUCTURA_DEV_BUILD=OFF` e `Distribute.cmake` monta `dist/<edição>`. `dist/Structura/` é uma pasta **órfã** (nome anterior à marca; nenhum script a gera). Ela não é lida pelo NSIS (`installer/structura.nsi` usa a pasta da edição). Proposta: movê-la para fora de `dist/` depois da aprovação. **Nada foi apagado.**

## 8. Pendente e decisão de que dependo
- **WP4 (`.enc`)**: `server/tools/encrypt_dll.py` obtém a chave de release do banco do servidor (`IntegrityManifest`) ou deriva por versão com segredos do servidor, e o servidor guarda o manifesto de integridade das DLLs. Gerar `.enc` comerciais exige **você indicar qual servidor/manifesto/chave de release** usar. Com o `dev.db` do repositório, o `.enc` só serviria ao servidor local. Não gerei nenhum `.enc`.
- WP6 (aplicativo completo pelo fluxo comercial) e WP8 (instaladores): dependem dos `.enc` e de sessão de servidor. NÃO TESTADO até lá.
- Decisão de liberação continua: **NÃO PODE SER DISTRIBUÍDO**. O motivo é concluir o WP4–WP8, não uma falha encontrada nas correções D1/D2.
