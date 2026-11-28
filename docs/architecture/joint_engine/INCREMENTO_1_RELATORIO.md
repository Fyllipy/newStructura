# Incremento 1 — Comportamento estrutural requerido do nó (antes da ligação)

Sem validação profissional. Funcionalidade real e utilizável: o usuário seleciona um nó e abre **"Comportamento do nó"** (ribbon *Calcular → Ligações e mistas*); o programa mostra as barras do nó, a idealização adotada no modelo, as metas de rigidez da NBR 8800:2024 e as lacunas.

## 1. Inspeção (o que o código realmente oferece)
| Pergunta | Achado (arquivo) |
|---|---|
| Como se seleciona um nó | `SelectionModel::selectedNodes()` (QUuid); `SceneController::nodeInfos()` mapeia para `externalId`, que é o `id` do nó em `ModelData` (`AnalysisService.cpp:143`) |
| Como o modelo representa os elementos conectados | `SolverBridge::ModelData`: `members` (`nodeI/nodeJ`, `releases[12]`, offsets/zonas rígidas), `endSprings`, `sections`, `materials`, `nodes` com `restraints[6]` |
| Como se identificam vigas/pilares | só por geometria: `JointClass::roleOfDirection` (≤ 10° viga, ≥ 80° pilar, intermediário diagonal). **O modelo não declara papéis** |
| Condição de rotação do nó | liberação (`releases`), mola (`endSprings`) ou **continuidade padrão** (nada declarado) |
| Esforços | disponíveis na análise, mas **nenhum critério da NBR 8800:2024, 6.1.2 depende deles** |
| Norma/contexto | só NBR 8800:2024 é implementada para rigidez de ligação (`JointClassification`); não há seletor de norma por projeto. **Sistema estrutural (contraventado/deslocável) e contraventamentos não constam do modelo** |
| O que já existia | `JointAnalysisService::analyze` (contexto de um **par** de barras: limites `EIv/Lv`, `Kv/Kp`) e o diálogo "Gráfico Momento-Rotação" (classifica a ligação **já definida**, parte de **duas barras**) |

## 2. O que se pode e o que não se pode determinar
**Determinável a partir do modelo (implementado):**
1. a **idealização que a análise global adota** em cada ligação viga–pilar do nó: rotulada (liberação declarada), semirrígida (mola declarada com `k`) ou rígida — esta última, quando nada é declarado, é **continuidade implícita do modelo e NÃO é apresentada como decisão do projetista**;
2. as **metas de rigidez** que uma ligação física teria de atingir para sustentar essa idealização (NBR 8800:2024, 6.1.2.1: `Si ≤ 0,5 EIv/Lv` rotulada; `Si ≥ 25 EIv/Lv` rígida) e a **admissibilidade da meta rígida** pela 6.1.2.2 (`Kv/Kp ≥ 0,1` no andar), com a viga, o pavimento e E do modelo; critérios reutilizados de `JointClassification` (`limitsFor`, `storeyStiffness`), nada reimplementado;
3. o que **falta** (lacunas).
**Só depois de definida a ligação** (não feito aqui): a rigidez real `Si` (2/3 de `Mj,Rd`), a curva M–θ e a classificação normativa da ligação física. **O resultado não classifica ligação real**: isso está escrito na memória e no diálogo.
**Não determinável sem decisão de projeto (explicitado, não fabricado):** quando o modelo tem continuidade implícita, o programa **não escolhe** entre rotulada, semirrígida e rígida; mostra as três metas e registra "DECISÃO DE PROJETO pendente". **Lacuna indispensável não fundamentada:** um critério de **necessidade** (se o sistema estrutural exige momento na ligação, por estabilidade/deslocabilidade/contraventamento) exigiria dados que o modelo não tem; **não foi inventado**. Decisão necessária: declarar no projeto o sistema estrutural/contraventamento, ou autorizar uma análise de sensibilidade (reanalisar o modelo com o nó rotulado × rígido e comparar efeitos, com tolerância fornecida pelo projetista).

## 3. Implementação
| Arquivo | Papel |
|---|---|
| `src/design/jointengine/RequiredBehavior.{h,cpp}` (novo) | núcleo sem Qt: `NodeFacts` → `assessRequiredBehavior` → `RequiredBehaviorAssessment` (item por par, estados `ValidityStatus`, base da exigência, metas, `Kv/Kp`, critérios, hipóteses, lacunas, `memory()`) |
| `src/app/services/NodeBehaviorService.{h,cpp}` (novo) | extrai os fatos do `ModelData` reutilizando `JointAnalysisService` (um contexto por par viga–pilar, cada um no seu plano) |
| `src/ui/NodeBehaviorDialog.{h,cpp}` (novo) | janela somente leitura: tabela de barras, tabela por ligação e memória completa |
| `MainWindow.h`, `MainWindowCommercialActions.cpp`, `MainWindowRibbon.cpp`, `MainWindowJoints.cpp` | ação, botão e slot `onNodeRequiredBehavior` (habilitada com exatamente um nó selecionado); sem alterar `onJointMomentRotation` |
| `src/demo/DemoStubs.cpp` | ação e slot vazios (a edição Demonstrativa não contém código de cálculo; `StructuraDemo` compila) |
| `CMakeLists.txt`, `src/tests/CMakeLists.txt` | registro |
Reutilização: `JointAnalysisService`, `JointClassification`; **não duplicados:** `ConnectionCatalog`, `JointComponents`, módulo paramétrico, verificações normativas. O classificador **não depende de ligação cadastrada** (a assinatura de `assessRequiredBehavior` só recebe fatos do modelo).

## 4. Testes (`TestRequiredBehavior`, 20 casos) e regressão
Referência independente: `0,5`, `25`, `EIv/Lv = 2e8·1e-4/6` e `Kv/Kp = 0,5` calculados **no teste** a partir da definição de 6.1.2, não do código testado. Cobre: liberação → rotulado; mola → semirrígido (e k nas faixas rotulada/rígida); continuidade implícita não apresentada como decisão (lacuna "DECISÃO DE PROJETO"); `Kv/Kp` = 0,1 (admitido), 0,0999 e 0,05 (não admitido) e andar pendente (indeterminado); dados da viga ausentes → `InsufficientData` sem metas; condição desconhecida; par viga–viga → `OutOfDomain`/não aplicável; norma sem critério → `Unsupported`; memória com a distinção e as lacunas e determinística; serviço sobre pórtico plano real (pilar–viga–pilar): identificação das barras/papéis, E/I/L, condição de rotação (padrão, liberação, mola), pilares rígidos → meta rígida não admitida, nó apoiado/com uma barra/inexistente → dados insuficientes, nó com três pares (viga–viga não aplicável + viga–pilar avaliado), reprodutibilidade; diálogo (tabelas e memória).
**Regressão:** build limpo (biblioteca removida e reconstruída), `ctest`: **43/43** (42 anteriores + `TestRequiredBehavior`); goldens inalterados (`numerics.txt` e `momento_rotacao.txt` sem diff nesta etapa); `StructuraDemo` compila. SHA-256 inalterados: `TStub*`, `JointComponents.cpp`, `JointClassification.*`.

## 5. Limitações
- O resultado da Etapa 1 é a **idealização do modelo + metas**, não uma recomendação de necessidade estrutural (ver lacuna indispensável acima).
- Papéis de barra por inclinação (hipótese); excentricidades e viga oposta (parâmetro β do painel de alma) apenas notadas.
- A interface foi exercitada por teste offscreen; **não houve verificação visual manual** do diálogo no aplicativo.
- A Etapa 2 em diante (escolha/definição da ligação, resposta M–θ, dimensionamento, classificação da ligação real) **não foi iniciada nesta entrega**.

## 6. Próximo passo proposto (Incremento 2)
Ligar este diálogo ao configurador paramétrico existente: oferecer, por par viga–pilar, as tipologias do `ConnectionCatalog` compatíveis com o comportamento requerido, gravando a `NodeConnection` pelo módulo atual e derivando a topologia JME pelo adaptador já existente. Decisões necessárias: critério de compatibilidade tipologia × comportamento (por exemplo, quais tipologias do catálogo têm modelo mecânico disponível — hoje só 4 entradas de momento via EN — e quais são consideradas "rotuladas" por natureza).
