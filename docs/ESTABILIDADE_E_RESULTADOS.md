# Estabilidade global, comprimentos efetivos e material ortotrópico de cascas

Este documento cobre a segunda rodada de extensões do solver (itens 4 e 7 do plano). Escopo excluído por decisão do usuário: recalque de apoio, protensão/pós-tensão, pré-tensão, apoio elástico contínuo (Winkler) e neve.

## 1. Comprimentos efetivos a partir da flambagem (`EffectiveLengthService`)

Entrada: `ModelData` e `Result` de uma análise de flambagem linear (`bucklingFactors`, `bucklingModes`, `memberResults`).

Para cada barra comprimida e para cada plano de flexão (X = eixo forte, Y = eixo fraco):

1. Barras colineares ligadas em nós de grau 2, com mesmo material e seção, formam um **grupo** (`groupSize`, `groupLength`).
2. A energia de flexão do modo é `EI∫v''² dx`, estimada por segundas diferenças em 24 amostras. A participação da barra é essa energia dividida pelo pico entre as barras do modo; no grupo toma-se o máximo.
3. O primeiro modo com participação ≥ `minParticipation` (padrão 0,10) governa.
4. `Ncr = λ·|N|`, `K = π·√(EI/Ncr) / L_grupo`, `Lef = K·L_grupo`.

Uma energia baseada na rigidez geométrica foi descartada: ela se anula no meio do vão. A energia de curvatura não tem esse problema.

Resultados globais: `firstLambda`, amplificação `1/(1−1/λ1)` e classe de deslocabilidade (≤1,10 pequena; ≤1,40 média; acima disso grande). Valores de K fora de 0,3–6 são sinalizados como suspeitos.

Interface: botão "Comprimentos efetivos (K) das barras…" no diálogo de flambagem. "Aplicar Kx e Ky às barras" grava `kx`/`ky` (ou `lx`/`ly` explícitos, em m, para grupos) em `m_extras.design.members`.

Teste: `effectiveLengthsFromBuckling`. Pilar de 6 m em 8 elementos, biapoiado (K=1) e em balanço (K=2), com erro ≤ 5%.

Limites: o K vem de um único modo e assume um pilar isolado, sem a interação entre pilares do pórtico. Imperfeições geométricas como estado do modelo e as classificações B1/B2 por pavimento não foram implementadas.

## 2. Material ortotrópico de cascas

Modelo no elemento Q4 (`Femdll/src/shell_q4.cpp`, compartilhado por `mefs.dll` e `rds.dll`):

- Membrana: `Q = [[E1/d, ν12E2/d, 0],[ν12E2/d, E2/d, 0],[0, 0, G12]]`, com `d = 1 − ν12ν21` e `ν21 = ν12E2/E1`.
- Flexão: o mesmo `Q` com `E1·bend1` e `E2·bend2`, e `G12·√(bend1·bend2)`.
- O eixo 1 do material é um vetor global projetado no plano de cada elemento. Se for normal ao elemento, usa-se o eixo local e1.
- Rotação: `D = TεᵀQTε`, com θ = ângulo do eixo no sistema local.
- Cortante transversal: `G12`.

Os mesmos `D` valem para a rigidez, a carga térmica e as resultantes.

APIs:

- mefs: `fem_set_material_orthotropy` (retorna 4 se `1 − ν12ν21 ≤ 0`) e `fem_add_tie_masked`.
- rds: `structura_set_shell_orthotropy`, chamada logo após `structura_set_shells`.

Persistência: linha `ORTHO idx E2 nu12 G12 bend1 bend2 ax ay az` na seção `[SHELL_MODEL]`. A assinatura de resultados inclui a ortotropia. Interface: botão "Ortotropia…" na aba de materiais.

Testes:

- `orthotropicCantilever` (fem_tests): erro de 0,3% e 0,7% contra Euler–Timoshenko.
- `orthotropicShellMaterial` (app): o caso degenerado reproduz o isótropo, o giro de 90° inverte a rigidez e `bend1 = 1/16` multiplica a flecha por 16.

## 3. Ligação só de translações (`dofMask`)

`Tie.dofMask` e `fem_add_tie_masked` permitem uma ligação nó-aresta que transmite só ux, uy e uz (máscara 7), como uma articulação entre chapas. O plumbing existe em Femdll, `ShellMesh`, `ShellCouplingService` e `ShellSolverBridge`. Ainda não há interface que gere essas ligações entre objetos.

## 4. Não implementado

| Recurso | Motivo |
|---|---|
| Triângulos de 3 e 6 nós | O mesher já gera só quadriláteros (polígonos são triangulados e cada triângulo vira 3 quadriláteros), então a flexibilidade geométrica existe. |
| Plasticidade J2 em cascas | Fora do escopo desta rodada. |
| Furos em chapas, zonas rígidas, retração e bandas de integração | Planejados e ainda não feitos. A carga térmica uniforme equivale à retração (ε/α). |

## 5. Combinações, fases e resultados por posição (item 8)

Serviços em `src/app/services/`. Estão implementados e testados, mas ainda **sem diálogo na interface**.

### 5.1 `LoadHypothesisService` — hipóteses condicionais

Expande uma combinação-base em várias. Tipos de hipótese:

- **Optional**: o caso age ou não age.
- **Exclusive**: exatamente um caso do grupo por vez (posições de carga móvel, direções de vento).
- **Alternating**: qualquer subconjunto não vazio do grupo (sobrecarga alternada por vão ou pavimento), com até 12 casos por grupo.

As variantes se nomeiam `Base | casos`. O total é limitado por `maxCombinations`, e `truncated` avisa quando o limite corta a lista. Hipóteses que citam casos ausentes da combinação a deixam intacta.

### 5.2 `ResultsEnvelopeService` — envoltórias por posição

Recebe um provedor `(combinação, barra, x) → {N, Vy, Vz, T, My, Mz}` e devolve, em `samples` abscissas, o máximo e o mínimo de cada componente com o índice da combinação governante. Também devolve o pico absoluto, sua posição e sua combinação. O provedor fica fora do serviço porque `viz` não pode depender de `app`. A ligação com `UniformLoadForceEvaluator` fica para a interface.

### 5.3 `ConstructionStageService` — fases construtivas

Cada fase tem cargas incrementais, barras ativas e reduções de área e inércia por barra. O serviço clona as seções reduzidas, resolve cada fase com a rigidez dela e soma os incrementos com `CombinationService::combine`. Barras inativas ficam com rigidez 10⁻⁶ da original e perdem suas cargas de barra.

Validade: o resultado é exato para respostas lineares em cada fase. Não há retração, fluência nem envelhecimento do material, e não há protensão (excluída pelo usuário).

### 5.4 Testes

`loadHypothesesExpandCombinations`, `envelopeFindsGoverningCombinationAlongTheBar` e `constructionStagesAccumulateAndReduceStiffness` estão em `TestProjectIoAndSolver`.

## 6. Infraestrutura do solver (item 10, sem paralelismo)

### 6.1 Autovalores esparsos (`Structuradll/src/core/iterative_solvers.{h,cpp}`)

Flambagem (`K x = λ(−Kg) x`) e modal (`K x = ω² M x`) escrevem-se como `B x = μ A x` com A = K positiva definida. `largestGeneralizedEigenpairs` extrai os maiores μ por iteração em subespaço com a fatoração LDLᵀ esparsa de A (ordenação AMD) e Rayleigh–Ritz em base ortonormal (QR) de `max(2n, n+8)` vetores. A convergência exige variação relativa dos μ positivos desejados menor que `iterativeTolerance` (padrão 1e-10). Se a iteração parar no limite, o solver avisa e mantém os resultados.

- A opção `eigenSolver` é 0 = automático (denso até `eigenDenseLimit` = 400 graus de liberdade livres, esparso acima), 1 = denso ou 2 = esparso. O limite antigo de 2200 graus de liberdade no caminho denso foi eliminado.
- Limite conhecido: autovalores negativos grandes de `−Kg` (barras tracionadas) competem pelo subespaço, e a iteração prioriza o módulo. O bloco tem folga, mas um modelo com muitos modos negativos dominantes pode perder um modo positivo.
- Teste `sparseEigenAndIterativeSolver`: os 4 fatores de flambagem e as 4 frequências do caminho esparso coincidem com o denso (tolerância 1e-6), e o fator crítico confere com a carga de Euler.

### 6.2 Gradiente conjugado (PCG)

Com `linearSolver = 1`, o sistema estático é resolvido por PCG com Cholesky incompleta (`Eigen::IncompleteCholesky`), sem fatoração exata. A verificação de mecanismo por pivôs não é feita nesse caminho, e o aviso informa iterações e resíduo. Se o PCG não convergir, o solver direto assume. É opcional e não é o padrão, porque o LDLᵀ esparso é mais robusto em pórticos.

### 6.3 Renumeração e mecanismos

- O LDLᵀ esparso (`SimplicialLDLT`) já usa ordenação AMD, e ela agora é explícita no caminho dos autovalores.
- Quando a decomposição falha por pivô nulo (estrutura hipostática), o solver refaz a análise com `K + εI` e lista até 8 graus de liberdade (nó e grau) não contidos, junto com a contagem total. O erro antigo só indicava um grau de liberdade ou "falha na decomposição". Teste: corrente pinada em um único nó com 3 mecanismos de corpo rígido.
- O relatório de condicionamento (razão entre o maior e o menor pivô) permanece como aviso.

### 6.4 API e aplicativo

`structura_set_solver_options(linearSolver, eigenSolver, eigenDenseLimit, tolerance, maxIterations)` está na DLL. `SolverBridge::ModelData` tem os campos `linearSolver` e `eigenSolver`, e o aplicativo usa o padrão (o esparso entra sozinho em modelos grandes). Ainda não há tela para escolher o PCG.

### 6.5 Resultados em binário versionado (`ResultsBinaryStore`)

Formato `STRR` v1: cabeçalho (assinatura, versão, impressão digital do modelo), índice de seções com deslocamento, tamanho e soma de verificação, e seções `run:<hipótese>`, `design`, `buckling` e `modal`. Cada hipótese (caso ou combinação) é independente: `readRun` lê só uma sem carregar as outras, e uma seção corrompida ou uma versão futura é recusada com mensagem. O `.dat` continua gravando os resultados em texto. **O armazenamento binário ainda não está ligado ao salvar e abrir do projeto**; é um serviço testado (`binaryResultsStoreRoundTripsPerHypothesis`).

### 6.6 Execução em lote com cancelamento

`BusyProgress` ganhou `setCancellable`, `requestCancel` e `isCancelled`. Com a tarefa cancelável, a janela "Calculando…" mostra "Esc cancela", e Esc (verificado por `GetAsyncKeyState`, só no Windows) pede o cancelamento. A análise estrutural verifica o cancelamento entre casos, estruturas nt e combinações, lança `OperationCancelled` e mantém os resultados anteriores. Um caso em andamento no solver termina antes de o cancelamento valer.

## 7. Dimensionamento com todas as combinações

A verificação de barras já usa todas as combinações últimas calculadas (`MainWindowDesign.cpp` reúne todas as `isCombination` que não são ELS; a seleção do usuário só filtra) e, em cada combinação, todas as estações da barra. Em cada verificação (`CheckResult`), `BarDesign.cpp` guarda o pior caso entre combinação e estação (`col.offer`), e a combinação governante e sua posição aparecem no resultado e no relatório. Combinações excepcionais (nome iniciado por EXC) são verificadas à parte, com γa1 = 1,00 e γa2 = 1,15, e prevalece a pior razão. O dimensionamento, portanto, não escolhe uma única combinação crítica de antemão, e nada precisou ser corrigido.
