# D0 — Auditoria arquitetural do Structura para o Joint Mechanical Engine

Somente leitura do repositório; nenhum arquivo de produção, teste, golden, CMake ou do C7 foi alterado. Cada afirmação cita arquivo. O que não foi lido está marcado **NÃO AUDITADO**. Sem validação profissional.

## 1. Estado do Git e do C7 (verificado)
| Item | Achado |
|---|---|
| Branch | `implantacao_dll`; último commit `6e32974` (Release 0.9.1 beta). Nada da A1–C6 está commitado |
| Alterados (M) / removidos (D) / não rastreados (??) | 57 / 2 / 63 |
| Núcleo T-stub (não rastreado) | `src/design/TStub.{h,cpp}`, `TStubStiffness.{h,cpp}`, `JointComponents.{h,cpp}`, `src/app/services/JointComponentService.{h,cpp}`, `src/tests/TestTStub.cpp`, `TestTStubStiffness.cpp` |
| **`TStubForceDisplacement`** | **não existe** em `src/` (nenhum arquivo, teste ou entrada de CMake). Existe só o protótipo Python `research/tstub_piluso/`, as decisões C7.0 (`docs/technical/TStub_C7_Methodological_Decisions.md`) e a especificação de API (`docs/architecture/TStubForceDisplacement_API_Specification.md`, a ser revisada na C7.1) |
| Arquivos tocados por esta fase | apenas `docs/architecture/joint_engine/*` (novos) |
**Consequência:** o D0 acomoda o C7 por **contrato** (§ API_CONTRACT §8), nunca por detalhe de implementação.

## 2. Arquitetura atual (tamanho e camadas)
`src/design` 68 arquivos / 17,5 mil linhas (normas e cálculo, **sem Qt**, namespaces `Structura::Design::*`); `src/app` (+`services`) lógica de aplicação com Qt; `src/ui` 231 arquivos / 50 mil linhas; `src/shell` (cascas MEF); `src/report`; `src/viz`. Solvers em DLL (`rds` barras+cascas, `mefs` cascas) acessados por `SolverBridge`/`ShellSolverBridge`.

## 3. Módulo paramétrico de ligações (o que existe)
| Elemento | Local | Papel |
|---|---|---|
| `NodeConnection` | `app/ProjectDataTypes.h:701` | dado persistido: `id, nodeId, memberId, atEnd, type, grade, QMap<QString,double> values`, `eta/governing/verified` (resultado só informativo). **Uma única barra** (`memberId`) mais o apoio implícito |
| Tipos legados | idem `kShearTab, kDoubleAngle, kGusset, kSplice, kBasePlate, kTubular, kColdFormed, kEndPlate` | despacho por `type` em `ConnectionService` |
| Catálogo | `app/services/ConnectionCatalog.h` | ids ≥ 100, 9 famílias (`WebShear, Moment, CoverSplice, TubeSplice, EndPlateSplice, TrussNode, FlexEndPlate, Composite, Pin`); `FamilyOps` = `fields/makeDefault/evaluate/describe/solids/propose/dims/notes` |
| Cálculo normativo | `design/*.h` (ex.: `EndPlateConnection`, `MomentConnection`, `WebShearConnection`, `CoverSplice`, `TrussConnections`, `BasePlate`, `TubularJoints`, `PinConnection`, `SupportWeld`, `StiffenerDesign`, `ShearConnections`) | `struct XxxInput` → `Check` |
| Resultado | `design/Connections.h:28–58` | `Check{name, clause, demand, resistance, unit, status, reason, origin, lines}`; `CheckStatus{Evaluated, Unverifiable, NotApplicable, Information, Complementary}`; `CheckOrigin{Design, ColumnFlangeLeverNbr, ColumnFlangeLeverEn, DivergenceWarning, …}` |
| Sólidos/desenho | `ConnectionSolids`, `ConnectionDrawing`, `FabricationModel`, `DstvExporter` | 3D e detalhamento a partir dos mesmos `values` |
| Persistência | `DatModelIoService.cpp` bloco `[CONNECTIONS]` | `values` por chave textual |
| Relatório | `ReportServiceDesign.cpp`, `report/*` | memória por `Check.lines` |
**Reutilizável pelo motor:** (i) `values` + `FamilyOps` como **fonte da configuração física**; (ii) `Check` como padrão de estados "não verificável / não aplicável" (§ 12 do prompt); (iii) `fingerprint` (item 4) para invalidação.
**Lacunas:** `NodeConnection` descreve uma barra e um apoio, **não** vários elementos conectados; os `values` são um mapa plano de `double` sem esquema/unidade/origem (a geometria é reinterpretada por cada família); não há modelo geométrico comum (cada `toXxx(c, forces)` reconstrói o seu `Input`); `ConnectionCatalog::Family` é tipologia de **cálculo normativo**, não de comportamento mecânico.

## 4. Módulos estruturais
`SolverBridge::ModelData` (`app/SolverBridge.h:259`) é a representação do modelo estrutural: `NodeData`, `MemberData`, `SectionData`, `MaterialData`, cargas, `EndSpringData`, `PlasticHingeData`, `DiaphragmData`, `RigidLinkData`, `InclinedSupportData`, `LinearConstraintData`, `LinkData`, cascas (`Shell*Data`). Unidades do solver (m, kN). `JointAnalysisService::analyze(model, nodeId, memberA, memberB, forceToKn)` já extrai de `ModelData` o **contexto de um nó** (barras que chegam, papel viga/pilar/diagonal, plano de análise, Iv/Lv, E, Kv/Kp) **sem criar representação paralela** — é o precedente a seguir.
**Pontos de saída do motor para o modelo global já existentes:**
- `MemberConnectionSpec.springK[12]` (liberação/mola por GDL local) e `PlasticHingeSpec{kElastic, yieldMoment, hardeningRatio, axialYield, exponent}`;
- `LinkData` entre dois nós com **6 leis** `LinkLawData{kind: linear, só-tração, só-compressão, folga, elastoplástica, multilinear(table), rígida}` — **já aceita lei força–deslocamento multilinear por GDL**; verificado na declaração, **comportamento no solver NÃO AUDITADO** nesta fase;
- `JointAnalysisService::idealize(spec, dof, kind, …)` grava rótula/mola/bilinear a partir de Sj,ini e Mj,Rd.

## 5. Núcleo T-stub e componentes (o que existe de fato)
| Peça | Conteúdo | Observação |
|---|---|---|
| `TStub.{h,cpp}` | resistência EN 1993-1-8 §6.2.4 (modos 1/2/3, `EnCoefficients`) | resultado `Complementary`, fora de aprovação |
| `TStubStiffness.{h,cpp}` | rigidez EN §6.3.2 (`k4`, `k10`), padrão NaN = "não informado" | série EN; sem lei F–δ |
| `JointComponents.{h,cpp}` | método dos componentes EN §6.2/6.3: `ModelInput` plano; `Component{k, resistance, failureMode, lines}`; `RowResult`; `computeStiffness`, `computeResistance`, `curveMetrics`, `momentRotationCurve` (§6.3.1), `deriveEquivalent`; **só duas tipologias**: `WeldedFlanges`, `ExtendedEndPlate` | `ModelInput` mistura viga, pilar e chapa num único struct; solda = só resistência (`flangeThroat`); sem distribuição não proporcional à rigidez |
| `JointComponentService` | `evaluate(connection, E)`, `evaluatePreliminary`, `isSupported`, **`fingerprint`** | o `Result` traz `behavior`, `stiffness`, `resistance`, `metrics`, `memory`, `hypotheses`; `supported=false` para as demais tipologias |
| `JointClassification` | NBR 8800:2024 6.1.2: `JointBehavior{origin, positive, negative}` (curva M–θ por sentido, `Mj,Rd`, `Si`), `classify`, `JointBehaviorProvider` (interface virtual) | **só M–θ**; `DataOrigin::ComponentMethod` já é usado (`JointComponentService.cpp:274`) embora o comentário do cabeçalho o chame "reservado: não implementado" (**comentário desatualizado**, não alterar nesta fase) |
| `TStubForceDisplacement` | **inexistente** (ver §1) | — |
Testes existentes: `TestTStub`, `TestTStubStiffness`, `TestJointClassification`, `TestConnectionCatalog`, `TestMomentRotationRegression` (+ golden `momento_rotacao.txt`).

## 6. Cobertura por família (fatos do código; sem capacidades presumidas)
| Família | O que existe | Modelo mecânico (F–δ) |
|---|---|---|
| Chapa de extremidade (viga–pilar, emenda, flexível) | verificações NBR; método dos componentes EN só para `ExtendedEndPlate` | **Sj,ini, Mj,Rd, curva EN 6.3.1**; sem capacidade de rotação |
| Cisalhamento (chapa/cantoneira de alma) | `WebShearConnection`, `ShearConnections` (NBR 8800: parafuso, solda, chapa, rasgamento, block shear) | **não** |
| Emenda (cobrejunta, tubo) | `CoverSplice` (inclui Ct, rasgamento) | **não** |
| Nó de treliça / gusset | `TrussConnections`: largura de **Whitmore**, escoamento/ruptura/flambagem da chapa, block shear (`ConnectionHelpers.h:108`) | **não** (somente resistência) |
| Base de pilar | `BasePlate`: placa, chumbadores (`AnchorDetail`, Tab. 18 NBR), bloco de concreto, NBR 8800 6.7 | **não**; concreto/graute/ancoragem profunda: só o que a NBR 8800 6.7 cobre; **não** há modelo de contato/pressão distribuída (NÃO AUDITADO em detalhe) |
| Tubular | `TubularJoints` (CHS, verificações) | **não** |
| Pino | `PinConnection` (+ fadiga) | **não** |
| Soldas | `SupportWeld` e verificações de solda por família | **só resistência normativa**; sem lei constitutiva |
| Híbridas | nenhuma verificação combina distribuição parafuso×solda por rigidez | **não** |
| Mista aço-concreto | `ConnectionCatalog::Family::Composite` | **não** (normativo) |

## 7. MEF disponível (resumo do estudo de validação anterior)
Cascas Q4 (MITC4+Wilson+drilling) facetadas; **sem plasticidade, sem contato (só molas só-compressão), sem grandes deslocamentos em cascas, sem sólidos**; ties por penalidade; flambagem de casca curva não capturada; validação por Mecway (CalculiX) existe como **ferramenta externa**, não integrada. Logo o solver de cascas **não é suficiente** para ligações parafusadas/soldadas (sem contato, pré-tensão, plasticidade, solda) — apenas para chapas/almas em regime elástico ou como referência de rigidez de componente.

## 8. Dependências, riscos, lacunas
1. **Geometria duplicada**: cada família reconstrói seu `Input` a partir de `values`; o motor precisa de um adaptador por família (não de um novo módulo paramétrico).
2. **`NodeConnection` com um só elemento conectado**: insuficiente para gusset com 3+ barras e para soldas/parafusos híbridos: exige **modelo de referência** (ids de barras) sem alterar o `NodeConnection` existente.
3. **Origem dos parâmetros ausente nos `values`**: a rastreabilidade pedida só existe nos novos núcleos (`DataOrigin`, `Check.origin`).
4. **`JointBehavior` é M–θ**: única saída do domínio atual; outras grandezas generalizadas não têm tipo.
5. **Comentário/documentação desatualizados** (item `ComponentMethod`).
6. **Risco científico:** dois modelos T-stub (EN e Piluso) com rigidezes ≈ 2× diferentes (C2/C6) → o motor não pode misturá-los.
7. **Risco de integração do C7:** API em revisão (C7.1); qualquer acoplamento precoce quebra.
8. **NÃO AUDITADO:** comportamento numérico do solver de `LinkData`; `ReportService*` por completo; `ConnectionSolids`; `FabricationModel`; UI de ligações (`MainWindowJoints`, `NodeConnectionsDialog`) além de ocorrências por grep.

## 9. Oportunidades de reutilização
`Check/CheckStatus` (estados), `JointComponentService::fingerprint` (invalidação), `JointAnalysisService` (extração de contexto sem duplicar), `LinkData`/`PlasticHingeSpec` (saída para o modelo global), `JointComponents::Component` (padrão de componente com `lines` de memória), `momentRotationCurve` (formato de curva), `TStub*` (componente EN complementar), documentação `CATALOGO_LIGACOES.md`/`AUDITORIA_LIGACOES.md` (a consultar na fase de matriz).
