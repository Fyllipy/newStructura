# Relatório da Etapa 3 — correções críticas e regressão do solver MEF

Data: 2026-11-23 · Branch `implantacao_dll` (base `d9e4838`) · Scratchpad de trabalho: `C:\Users\JOO~1\AppData\Local\Temp\claude\4eea84a9354b42c2\ab2ddb1e-50ff-4e0c-a69b-4eddfdb56da1\scratchpad\etapa3\` (pasta `results\` = saídas citadas).
Convenção: **[EXECUTADO]** = medido nesta etapa; **[NÃO EXECUTADO]** = não rodado; nenhuma aprovação é inferida de teste não executado.

## A. Resumo executivo

| Item | Resultado |
|---|---|
| **D1** (`rds`: limiar de "GDL sem rigidez" inflado por penalidades) | Defeito **reproduzido** e **corrigido no fonte** (`Structuradll/src/core/solver_core.cpp`). Híbrido barra-Vlasov→casca: reação de torção na raiz **3,9 % → 99,99 %** do torque aplicado. Bateria de regressão sem regressões; `solver_tests` 147/147. **A DLL `rds.dll` distribuída NÃO foi reconstruída** — a correção só vale para builds futuros. |
| **D2** (`mefs`: conjunto de molas só-compressão compartilhado entre casos; limite de 30 iterações com sucesso silencioso) | Defeito **reproduzido** e **corrigido no fonte** (`Femdll/src/fem_model.cpp/.h`): conjunto ativo por caso, histerese, detecção de ciclo, erro explícito, verificação de resíduo, equilíbrio e consistência de contato. `fem_tests` todos passam. **`mefs.dll` distribuída NÃO foi reconstruída.** |
| Proteção no app (superposição inválida) | Implementada e testada: apoios só-compressão ⇒ cada caso resolvido isoladamente, cada combinação resolvida como análise independente, aviso na UI; no caminho acoplado (`rds`) a combinação é resolvida com cargas ponderadas (já era o caminho dos itens não lineares). Vale **mesmo com a DLL antiga** (é app-side). |
| Qualidade de malha | Módulo `ShellMeshQuality` com 3 níveis (aceitável/aviso/erro), integrado ao `ShellDialog`; limites ancorados em varreduras executadas (docs/QUALIDADE_DE_MALHA.md). **Anomalia aberta** em cascas curvas finas (ver E). |
| Interface barra-Vlasov × casca | Documento técnico entregue (`docs/INTERFACE_BARRA_CASCA_VLASOV.md`); **nada implementado**. |
| Regressão geral | `ctest` do build-dev: **23/23**; `fem_tests` (fonte corrigido) 0 falhas; `solver_tests` (fonte corrigido) 147 verificações, 0 falhas. |
| Integridade comercial | Hashes de `rds.dll`, `rds.enc`, `mefs.dll`, `mefs.enc` e dos executáveis de `dist/` **inalterados**. Nenhum bypass executado. |

## B. Registro de alterações

### B.1 Arquivos de produção modificados (sem recompilar nem substituir DLLs)
| Arquivo | Alteração |
|---|---|
| `Structuradll/src/core/solver_core.cpp` | D1: `physicalMaxDiagonal` (diagonal da matriz **física**: barras + cascas + molas + links, sem penalidades de vínculo/tie) como referência do limiar `1e-12·max` nos dois pontos (GDL sem rigidez e diagnóstico de LDLT falho). Patch: `results/D1_solver_core.patch`. |
| `Femdll/src/fem_model.cpp`, `fem_model.h` | D2: bloco de molas reescrito (por caso); `maxSpringIterations` 30 → 100 e falha ao atingir o limite; verificação de resíduo normwise `‖Ku−f‖ ≤ 1e-9(‖K‖F‖u‖+‖f‖)`; equilíbrio global de forças (limite `1e-4`, justificado em 3× o maior desbalanço medido com a penalidade padrão); consistência de contato a posteriori (mola ativa não traciona; inativa não penetra). |
| `src/shell/ShellJob.h/.cpp` | `Job::hasUnilateralSprings()`, `combinationJob(...)`. |
| `src/shell/ShellResults.h` | `Results::superposable`. |
| `src/app/ShellSolverBridge.h/.cpp` | Com apoios unilaterais cada caso é resolvido isoladamente; `solveCombination(...)`. |
| `src/app/SolverBridge.h` | `ModelData::hasNonlinearElements()` agora é verdadeiro com mola de casca só-compressão (impede superposição no caminho do `rds`). |
| `src/ui/ShellDialog.cpp` | Combinações com apoios unilaterais: análise independente (ou aviso no modo acoplado); verificação de qualidade de malha antes de resolver (erro bloqueia). |
| `src/shell/ShellMeshQuality.h/.cpp` (novos), `CMakeLists.txt` | Módulo de qualidade de malha. |
| Testes: `src/tests/TestShell.cpp` (+3), `TestShellCoupling.cpp` (+1), `TestShellMeshQuality.cpp` (novo, 8), `src/tests/CMakeLists.txt` | Ver C. |
| Docs: `docs/INTERFACE_BARRA_CASCA_VLASOV.md`, `docs/QUALIDADE_DE_MALHA.md`, este relatório | |

Os demais arquivos modificados em `git status` (gerador de escadas, build de dev, etc.) são de tarefas anteriores e **não** fazem parte desta etapa.

### B.2 Comandos principais
Todos em cópias isoladas no scratchpad: `cmake -S . -B build` / `cmake --build build --config Release --target mefval_{baseline,fix} fem_tests_{baseline,fix,repo} solver_tests_repo mefs_repo_check`; `mefval_*.exe d1|d2|shells|units|penalty|malha|domo`; patches `patch_d1_v3.py`, `patch_d2_v3.py` (com `d2_block_repo.txt`, sem instrumentação); no repositório: `cmake --build build-dev --target <23 alvos de teste>` e `ctest -C Release`. Alvos de teste construídos individualmente (sem o alvo `Structura`).

### B.3 Hashes (SHA-256)
| Artefato | Antes | Depois |
|---|---|---|
| `rds.dll` | `f40c09a9d0329b00…` | idêntico |
| `rds.enc` | `c3a9b8b2c3b625ed…` | idêntico |
| `mefs.dll` | `09ffc08a6001b2c4…` | idêntico |
| `mefs.enc` | `ff6c9550e2d2f750…` | idêntico |
| `dist/Structura-Metalicas/Structura.exe` | `04298a1a0bffdde2…` | idêntico |
| `dist/Structura-Metalicas-Demo/StructuraDemo.exe` | `61c4812938f6d750…` | idêntico |
| `build-dev/Release/Structura.exe` (dev, **não** comercial) | `76af0fdcf70329e9…` | `e9f076f2cd970635…` — **alterado**: alguns alvos de teste dependem do executável e o recompilaram com as alterações desta e das etapas anteriores. É binário de desenvolvimento (`STRUCTURA_DEV_BUILD`), mas registro a mudança porque o hash estava na lista de invariantes. |

## C. Evidências por teste [EXECUTADO]

| # | Teste | Resultado | Arquivo |
|---|---|---|---|
| 1 | D1 bateria (barra, Vlasov, RigidLink, tie 6 GDL, excentricidade, diafragma, RigidLink kind 1 + `LinearConstraint`, mecanismos, nó escravo sem rigidez; penalidade 1e5–1e9; escala m e mm) | Sem regressão; só mudam os casos de diafragma em mm (GDL "fixados automaticamente" 6–7 → 2; os 2 restantes já existiam no controle em escala 1 e não foram investigados nesta etapa); mecanismos físicos continuam sendo erro | `d1_regressao_{baseline,fix}_stdout.txt`, `.csv` |
| 2 | D1 híbrido barra-Vlasov→casca (RigidLink kind 0, `p` = 1e5…1e11) | Reação Mx na raiz: baseline `p=1e7` **3,86 %** do torque; fix **99,99 %**, estável de 1e5 a 1e8; `p=1e11` passa a ser erro explícito de mecanismo. `φ_ponta` permanece −63 % da referência (limitação de modelagem, §F/Vlasov) | `hibrido_{baseline,fix}_stdout.txt` |
| 3 | D2 independência de casos (modelo Winkler só-compressão, F = 1e4…3e4) | baseline: `|A_junto − A_isolado|` = 0,38 / 0,78 / 0,98 e **molas tracionadas** (3/6/8); fix: 0 em todos, 0 molas tracionadas | `d2_regressao_*_stdout.txt` |
| 4 | D2 ordem dos casos / partidas | fix: ordem idêntica; 40 partidas aleatórias levam à mesma solução | idem |
| 5 | D2 limite de iterações (F = 3e4 precisa de 9) | baseline: limite 2 ⇒ **sucesso** com resultado 93 % errado e 10 molas tracionadas; fix: **erro explícito** | idem |
| 6 | D2 400 problemas aleatórios, 2 casos | baseline: **172 discordâncias** (desvio até 98,7 %); fix: **0 discordâncias**, 215 coincidem exatamente, 185 falham coerentemente (caso isolado também sem equilíbrio/mecanismo) | idem |
| 7 | D2 superposição × solução direta | erro de 1,34 e 17,6 (dois níveis de carga) na métrica do harness: **a superposição não vale** (motivo da proteção no app) | idem |
| 8 | Regressão do solver de cascas (benchmarks, unidades/sinais, penalidade) | `wp4_benchmarks.csv` e `wp8_unidades_sinais.csv`: **idênticos** baseline × fix. `wp3_penalidade.csv`: 8 linhas passam a **FALHA** (`p ≥ 1e8–1e9` em ties de placa fina/mm e `p = 1e11`): são os resultados que o critério de equilíbrio (4e-3 a 1e-1) rejeita — eram devolvidos sem aviso antes. Não houve relaxamento de critérios | `reg_{baseline,fix}/` |
| 9 | `fem_tests` com o **fonte de produção corrigido** | 0 falhas | scratchpad `build\Release\fem_tests_repo.exe` |
| 10 | `solver_tests` com o fonte de produção corrigido (D1) | 147 verificações, 0 falhas | `solver_tests_repo.exe` |
| 11 | App: `combinationJobScalesAndMergesCases`, `unilateralCasesDoNotDependOnEachOther`, `unilateralCombinationIsSolvedAsIndependentAnalysis`, `unilateralShellSpringsForbidSuperposition` | passam. **Nota:** com a `mefs.dll` antiga o teste de independência passa por causa do isolamento no app; o defeito em si é reproduzido apenas no harness (itens 3–6) | `testshell.txt` |
| 12 | Qualidade de malha (8 testes: regular, trapézio/skew, aspecto, empenamento, invertido/reentrante/degenerado, normais opostas e dobra, malhas geradas) | passam; malhas do próprio app (chapa, triângulo, cilindro, domo, tanque) sem ERRO; domo gera avisos nos elementos do polo | `testmeshquality.txt` |
| 13 | Sensibilidade do Q4 (aspecto, skew, afilamento, empenamento, polo de domo) | tabelas em `docs/QUALIDADE_DE_MALHA.md` | `wp3_sensibilidade_malha.csv`, `wp3_domo_polo.csv` |
| 14 | `ctest` completo (build-dev) | 23/23 | — |

## D. Antes × depois

### D1
- **Causa:** `max|diag|` da matriz já penalizada (`fullStiffness = membros + cascas + ties + molas + vínculos + links`, penalidade `1e7·K_ss`) virava o referencial do limiar `1e-12·max`; GDL legítimos, cuja rigidez física fica abaixo de `1e-12` do máximo **penalizado** (observado no híbrido barra-Vlasov→casca e em diafragmas em mm), eram "fixados automaticamente" com apenas um aviso; no híbrido isso zerava a transmissão de torção.
- **Correção:** referencial = maior diagonal **física**. Não se alterou a penalidade nem o limiar relativo (`1e-12`).
- **Alternativas consideradas:** (i) escalonamento por GDL (Jacobi) — mudaria o condicionamento de todas as análises e precisa de validação própria; (ii) eliminação exata dos vínculos (MPC por condensação) — correção estrutural maior, fora do escopo; (iii) relaxar o limiar — rejeitada (mascara o defeito).
- **Limite conhecido:** continua válido que um GDL sem rigidez *física* e **sem carga** é fixado com aviso (comportamento prévio, preservado).

### D2
- **Causa:** um único vetor `active` para todos os casos ("ativa se algum caso comprime") e saída silenciosa após 30 iterações.
- **Correção:** conjunto ativo por caso; histerese (`tol = 1e-12·u_max`); detecção de ciclo por conjunto visitado; erro em caso de mecanismo após desativação, limite de iterações, ciclo, resíduo, desequilíbrio ou contato inconsistente.
- **Critério de convergência:** conjunto estável, nenhuma mola ativa tracionada, nenhuma inativa penetrando (tolerância `1e-9·u_max`), resíduo normwise e equilíbrio global.

## E. Limitações e pendências

1. **DLLs não reconstruídas.** D1 e D2 estão corrigidos só no fonte. Equivalência entre o fonte corrigido e as DLLs distribuídas **não** foi declarada nem testada (a compilação do fonte corrigido gerou `mefs_repo_check.dll` somente no scratchpad, não copiada). O isolamento por caso no app (`ShellSolverBridge`) protege também a `mefs.dll` antiga.
2. **Ties de casca com penalidade ≥ 1e8:** passam a ser rejeitados pelo critério de equilíbrio (limite `1e-4`). O limite foi calibrado com uma única família de modelos (ties de placa); outras famílias podem pedir reavaliação.
3. **Tirantes (só tração) e molas só-compressão em `rds`:** o app resolve cada combinação com cargas ponderadas, mas a **independência de conjuntos ativos entre casos dentro do `rds`** [NÃO EXECUTADO] não foi re-verificada nesta etapa (o app resolve um caso por vez, o que a torna irrelevante para o fluxo atual).
4. **Anomalia aberta (WP3):** hemisfério sob pressão com malha `h ≤ 0,15 m` (3 318+ elementos) retorna deslocamento radial do equador **−33 %** da solução de membrana, embora esforços de membrana (erro médio 0,5 %) e equilíbrio vertical estejam corretos. Causa não determinada. **Cascas curvas finas não são aprovadas para deslocamentos** até investigação.
5. **Elementos do polo de domos** (ângulo ≈ 173°): erro local de 4–6 % em `n` que não diminui com refino ⇒ classificados como **aviso**; faixa 173°–179° sem dados.
6. **Sensibilidade de momentos/cortantes à distorção** do Q4 não foi medida (só flecha central e membrana no domo).
7. **Interface barra-Vlasov × casca:** continua restringindo o empenamento (φ −63 %, θ' −52 % após D1). Só o documento técnico foi entregue.
8. **Aviso 2 do app:** modo acoplado (`m_callbacks.coupledResults`) com apoios só-compressão não apresenta combinações superpostas; a solução da combinação deve ser pedida como análise do projeto (o app não oferece esse botão ainda).
9. **Hash de `build-dev/Release/Structura.exe` mudou** (B.3).

## F. Auditoria estática de integridade comercial (sem bypass, sem decifrar artefatos)

| Item | Achado | Risco |
|---|---|---|
| `dist/Structura-Metalicas` e `-Demo` | Só `rds.enc`/`mefs.enc`; nenhum `rds.dll`/`mefs.dll` em claro; chave de teste de 64 hex **ausente** dos executáveis | ok |
| Nome da variável `STRUCTURA_TEST_DLL_KEY` | Presente (1 ocorrência) nos dois executáveis comerciais: o ramo que lê a variável de ambiente em `client/dll_cipher.cpp` e `solver_security.h` **não está sob `#if`** (achado da Etapa 2) | **Médio** — a variável permite sessão offline de teste em qualquer build; recomenda-se condicioná-la a `STRUCTURA_DEV_BUILD`/testes. **Não corrigido** (alterar proteção comercial está fora do escopo e vedado) |
| `dist/Structura/` (01/11/2026) | Contém `mefs.dll`, `rds.dll`, `fem_dll.dll`, `solver_dll.dll` em claro; é resíduo ignorado pelo git (`dist/` no `.gitignore`) e **não** é a pasta usada pelos instaladores (`product.nsh` aponta para `dist\Structura-Metalicas[-Demo]`) | Médio se distribuída por engano — recomenda-se remover |
| `dist-dev/` | Pasta separada, ignorada pelo git (`.gitignore:24`); `build_dev.bat` usa `build-dev` e `dist-dev` e declara não mexer em `build/dist`; `CMakeLists.txt:786-800` remove as DLLs em claro do diretório do executável quando `STRUCTURA_DEV_BUILD=OFF` | ok |
| DLLs em claro versionadas | `rds.dll`, `mefs.dll`, `rds.enc`, `mefs.enc` estão em `git ls-files` (commit `d9e4838`) | Baixo/Médio — repositório contém DLLs em claro |
| Instaladores | `installer/structura.nsi` empacota `${PRODUCT_DIST_DIR}\*.*` (somente a pasta do produto) | ok |
| CI | `.github/workflows/server-ci.yml` só trata o servidor | n/a |

Não testável sem violar a regra de integridade [NÃO EXECUTADO]: comportamento da edição Release com proteções ativas (requer servidor/sessão), e qualquer teste dinâmico contra artefatos de Release.

## G. Matriz de aprovação técnica

| Funcionalidade | Veredito | Condição / evidência |
|---|---|---|
| Correção D1 no fonte do solver de barras | **PODE AVANÇAR** (no fonte) | Itens 1, 2, 10; **requer reconstruir `rds.dll` e repetir a bateria contra a DLL**; antes disso, a DLL distribuída segue defeituosa |
| Correção D2 no fonte do solver de cascas | **PODE AVANÇAR** (no fonte) | Itens 3–9; **requer reconstruir `mefs.dll`** |
| Proteção de superposição no app | **PODE AVANÇAR** | Itens 11, 14 |
| Qualidade de malha (verificação no app) | **PODE AVANÇAR** como alerta/bloqueio de malha inválida | Itens 12–14; limites calibrados para flecha de placa; momentos/cortantes **NÃO TESTADOS** |
| F. Legenda + probe | **NÃO TESTADO** nesta etapa | fora do escopo desta etapa |
| C/D. Apoios por clique e visualização | **NÃO TESTADO** | idem |
| E. Cargas desenhadas | **NÃO TESTADO** | idem |
| B. Sweep/revolução (cascas curvas) | **DEPENDE DE CORREÇÕES** | investigar anomalia E.4; Scordelis-Lo/pinched/hemisfério 18° executados? **NÃO** nesta etapa; polo de domo = aviso |
| A. Barra → casca (conversão) | **BLOQUEADO** para barras com empenamento restringido; **DEPENDE DE CORREÇÕES** para as demais | interface Vlasov restringe empenamento (D1 resolvido, erro de modelagem remanesce); recomendação: converter o elemento inteiro (docs/INTERFACE_BARRA_CASCA_VLASOV.md §6) |
| Equivalência fonte × DLLs distribuídas | **NÃO TESTADO** / não declarada | nenhuma DLL foi reconstruída nem substituída |
| Integridade comercial do pacote | **DEPENDE DE CORREÇÕES** | condicionar `STRUCTURA_TEST_DLL_KEY` ao build de teste; remover `dist/Structura` |
