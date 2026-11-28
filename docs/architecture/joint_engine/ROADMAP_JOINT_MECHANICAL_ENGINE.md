# Roadmap revisado (v2) do Joint Mechanical Engine

Substitui a v1 (J0–J15, centrada em uma sequência única). **Proposta aguardando aprovação; nenhuma alteração de código foi feita.** Decisões D1–D7 do D0 estão incorporadas (ver §0). Nenhuma família é subordinada a outra: após a infraestrutura comum, as famílias avançam em **faixas independentes**, cada uma com sua pesquisa de fontes, modelo, critérios e limitações. Sem estimativas de prazo (sem base).

## 0. Decisões e diretrizes incorporadas
| ID | Decisão | Efeito no plano |
|---|---|---|
| D1 | `src/design/jointengine/`; módulo paramétrico separado; adaptadores; **sem reorganização geral** | novos arquivos só ali; adaptadores em `src/app/services/` (convenção plana existente), um por família |
| D2 | topologia em estrutura **separada**, sem usar `values`; compatível com o legado | `JointTopology` (VALIDACAO_CONCEITUAL §1); derivada do legado quando ausente |
| D3 | N1→N2→N3 progressivo | N2/N3 só com necessidade demonstrada (critério em §3, fase S3) |
| D4 | EN 1993-1-8 §5.2.2 como fase própria e **obrigatória** antes do classificador; investigar NBR sem misturar | faixa N (§2) |
| D5 | S1 prioritário para o que já existe; **arquitetura pronta para não linear desde o início**; modelos não-EN coexistem | `IDeformableComponent` com estado e `tangent` opcional desde J0; S3 só com critério de entrada |
| D6 | MEF externo = validação/calibração, sem backend oficial | faixa V; interface `IFemBackend` apenas **especificada**, não implementada |
| D7 | tabela C7 provisória; atualizada após C7.1 | fase J3-T só após C7.1 |
| S-01 | **Tarefa de saneamento independente:** comentário desatualizado em `src/design/JointClassification.h` (cabeçalho e `DataOrigin::ComponentMethod` dizem "reservado: não implementado", mas `JointComponentService.cpp:274` o usa) | registrada; **não** alterada nesta fase; executável separadamente, só comentário, sem impacto em testes |

## 1. Visão em faixas
```
Faixa I  (infraestrutura comum)   J0 → J1 → J2 → J3
Faixa N  (normas/classificação)   N0 (leitura) → N1 (NBR) → N2 (EN 5.2.2)
Faixa S  (solver)                 S1 (EN) → [critério] → S3 (não linear geral) → S4 (N2/N3 sob demanda)
Faixas F (famílias, em paralelo após J1/J2):
  F-PAR parafusadas   F-SOL soldadas   F-HIB híbridas   F-GUS gusset/nós/tubulares
  F-PLT chapas e auxiliares   F-BAS base e ancoragem   F-SPL emendas   F-MIS mista/pino/formado a frio
Faixa V  (validação)              V-A analítica, V-N numérica/MEF externo, V-E experimental, V-P profissional
```
**Paralelismo seguro:** as faixas F só compartilham a Faixa I (tipos, topologia, fingerprint, kit de conformidade); entre si não há dependência de código, exceto onde indicado (F-HIB depende de F-PAR e F-SOL; F-BAS de F-PLT para a placa). Duas faixas F podem ser desenvolvidas por sessões distintas desde que os tipos de J0/J1 estejam congelados e os testes de J2/J3 passem.

## 2. Faixa I — infraestrutura comum
| Fase | Escopo | Arquivos **novos** | Arquivos **existentes afetados** | Aceitação | Testes |
|---|---|---|---|---|---|
| **J0 Contratos** | `Origin`, `Quantity`, `ValidityStatus`, `ResultKind`, `ModelDescriptor`, `Provenance`, `DofSpace`, interfaces `IDeformableComponent` (com `ComponentState` e `tangent` opcional), `IResistanceMechanism`, `IInterfaceModel`, `IClassifier`, `IAssemblyStrategy`, `MechanicalResponse` | `src/design/jointengine/{Types,Descriptor,Component,Response,Provenance}.h` (+ `.cpp` mínimos) | `CMakeLists.txt` (lista de fontes), `src/tests/CMakeLists.txt` | compila isolado; **nenhum** resultado existente muda; ctest anterior (35) passa | `TestJointEngineContracts` (invariantes 1–6 do API_CONTRACT §9) |
| **J1 Topologia e fingerprint** | `JointTopology`/`PartRef`/`Link`/`LoadPath`; fingerprint CFP/AFP/NFP; canonicalização; derivação da topologia legada (por família, sem gravar) | `jointengine/{Topology,Fingerprint}.{h,cpp}` ; `src/app/services/JointTopologyAdapter.{h,cpp}` (adaptadores) | `CMakeLists.txt`, `tests/CMakeLists.txt` (sem tocar em `ConnectionCatalog`/`DatModelIoService`) | derivação cobre **todas** as tipologias do catálogo (lista por id) gerando partes/links ou `Unsupported` explícito; regras de invalidação de FINGERPRINT §3 | `TestJointFingerprint`, `TestJointTopologyDerivation` (um caso por família do catálogo) |
| **J2 Envolver o existente** | `JointComponents` (EN) como `IDeformableComponent`/estratégia S1 e `JointClassification` (NBR) como `IClassifier`; **resultado bit a bit igual** | `jointengine/{EnComponentMethodModel,NbrClassifier}.{h,cpp}` | nenhum arquivo existente alterado (wrappers chamam as funções atuais) | M–θ e classificação idênticos ao golden `momento_rotacao.txt` e a `TestJointClassification` | `TestJointEngineRegression` (comparação exata) + os testes existentes sem alteração |
| **J3 Kit de conformidade** | fixtures **somente de dados** com uma configuração por família (os 7 casos de VALIDACAO_CONCEITUAL + emenda, tubular, mista); testam representação, não análise | `src/tests/jointengine_fixtures/*`, `TestJointEngineRepresentability` | `tests/CMakeLists.txt` | cada fixture é representada (partes, links, load paths, dados ausentes) **ou** falha com `Unsupported` justificado; nenhuma exige modelo constitutivo | idem |
| **J3-T Adaptador T-stub** | componentes `EnTStub` e `Piluso2001` (C7 final) como modelos independentes, sem mistura de rigidezes | `jointengine/{EnTStubModel,PilusoTStubModel}.{h,cpp}` | nenhum do C7; **após** C7.1 e autorização | estados do C7 propagados; `ModelUltimate` fora de aprovação; tabela de correspondência atualizada (D7) | contrato T-stub + 12 ensaios via adaptador |

## 3. Faixa S — solver
| Fase | Escopo | Critério de **entrada** | Arquivos novos | Aceitação |
|---|---|---|---|---|
| **S1** | método dos componentes EN para as tipologias hoje suportadas, via `IAssemblyStrategy` | J2 | `jointengine/StrategyComponentMethodEn.{h,cpp}` | igual ao `JointComponents` |
| **S3 Não linear geral** | equilíbrio incremental com estados por componente, interface de contato, convergência reportada | **todos**: C7 concluído; ≥ 2 componentes não lineares com fonte; ≥ 3 casos de referência (analítico ou MEF externo) onde S1 não se aplica; autorização | `jointengine/StrategyNonlinear.{h,cpp}` | reproduz S1 com componentes lineares; casos de referência dentro de tolerância documentada; `ConvergenceInfo` obrigatório |
| **S4 N2/N3** | acoplamento entre GDL | necessidade demonstrada por um modelo (D3) | — | por modelo |
Nada de S3/S4 é implementado antes do critério de entrada.

## 4. Faixa N — normas e classificação
| Fase | Escopo | Entregável | Aceitação |
|---|---|---|---|
| **N0 Leitura** | ler **EN 1993-1-8 §5.2.2** (critérios de classificação por rigidez e resistência, contraventamento) e os critérios da NBR 8800:2024 6.1.2 já implementados; **sem misturar** | documento `docs/technical/Classificacao_EN_NBR_Fontes.md` com texto fonte, símbolos, domínios | nenhuma lacuna não declarada; se a norma não puder ser obtida, a fase conclui "indisponível" |
| **N1** `NbrClassifier` completo sob `IClassifier` e `NFP` | J2 | casos publicados reproduzidos |
| **N2** `EnClassifier` | N0 obrigatório | `jointengine/EnClassifier.{h,cpp}` | cada critério citado; sem critério ⇒ `NaoVerificavel` |
`DesiredBehavior`/`BehaviorComparison` entram aqui (objetos separados).

## 5. Faixas por família
Cada faixa tem a mesma estrutura: **P0 pesquisa de fontes** (leitura; pode concluir "indisponível") → **P1 modelo declarado** (descritor, domínio, estados, limitações) → **P2 reprodução + verificação numérica** → **P3 comparação com referência** (analítica/MEF externo) → **P4 evidência experimental**. Nenhuma sobe de nível sem o anterior. O rótulo "validada" só é permitido com P4 (e P-profissional).
Arquivos novos de modelo ficam em `src/design/jointengine/models/<familia>/`; adaptador de família em `src/app/services/Joint<Familia>Adapter.{h,cpp}`; testes `TestJointEngine<Familia>*`. **Nenhuma faixa altera arquivos existentes de design/ConnectionCatalog.**

| Faixa | Escopo (tipologias da matriz) | P0 fontes a ler | P1–P3 primeiros modelos | Critérios de aceitação da faixa | Limitações explícitas | Depende de |
|---|---|---|---|---|---|---|
| **F-PAR** (A1–A10) | chapa de extremidade, chapa de topo, perfis T, cantoneiras (A4), talas (A5), parafuso (A10) | EN §6.2/6.3, cantoneiras ⚠, parafuso ⚠ | A1 via J2/J3-T; parafuso F–δ; cantoneira só se houver fonte | A1: reproduz EN e exemplos publicados ⚠; A4/A5: só entra com fonte; cada componente com domínio | rotação última e pré-tensão cíclica fora até haver fonte; Piluso sem mec. 3 | J2, J3-T |
| **F-SOL** (B1–B6) | mesas soldadas, filete, penetração, tubular, enrijecedores | EN §4/§6.2.6, CIDECT ⚠, literatura de deformabilidade de solda ⚠ | B1 via J2 (solda rígida **rotulada**); B2–B3 **só resistência** até haver fonte constitutiva | B1 igual ao existente; qualquer lei de solda exige fonte lida + teste | **sem rigidez/ductilidade de solda** até fonte; não converter verificação em lei | J2 |
| **F-HIB** (C1–C3) | parafuso+solda por compatibilidade | literatura de ligações combinadas ⚠ | modelo só se F-PAR e F-SOL tiverem rigidezes fundamentadas | sem rigidezes ⇒ `InsufficientData` (comportamento testado); com elas: MEF de referência + (idealmente) ensaio | alto risco científico; nunca partilha igual | F-PAR (A10), F-SOL |
| **F-GUS** (D1–D4, B5) | contraventamento, nó de treliça, tubulares, espaciais | AISC Whitmore/Thornton/Cheng ⚠, NBR 8800 5.2/6.5, CIDECT ⚠, EN §7 | P1: `IResistanceMechanism` para Whitmore, escoamento, ruptura, block shear, flambagem (as verificações **já existem** em `TrussConnections` — reaproveitadas, não reescritas) ; P2: distribuição de forças entre N barras; MEF de referência para fora do plano | separação dos 4 objetivos mantida (normativo / modelo mecânico / numérico detalhado / ligação completa); saída `GeneralizedVector`; nunca M–θ obrigatório; nunca T-stub | Whitmore com domínio angular; segunda ordem e excentricidade pendentes de fonte | J1 (topologia N-ária) |
| **F-PLT** (E1–E6) | chapas, enrijecedores, diafragmas, consolos, cantoneiras auxiliares | EN §6.2.6, NBR 6.5; diafragma/consolo ⚠ | chapa em flexão/tração como componente local; diafragma só com fonte | cada componente: R + V | diafragma e consolo **inexistentes** hoje | J1 |
| **F-BAS** (F1–F2) | placa de base, chumbadores, contato placa–concreto, graute, interação | EN §6.2.5/6.3.3 ⚠, EN 1992-4 ⚠, ACI 318 ⚠, NBR 8800 6.7 | `IInterfaceModel` de contato; chumbador F–δ ⚠; interação N–V | **concreto e ancoragem não existem no programa**: a faixa começa pela decisão (com o usuário) de incluir ou não propriedades de concreto/graute no modelo de dados; sem isso, só topologia e `ResistanceOnly` normativo (NBR 6.7) | tudo além de NBR 6.7 depende de fontes | J1, F-PLT |
| **F-SPL** (A6–A7, B4) | emendas parafusadas/soldadas de vigas, pilares, barras axiais | EN §6.2.7, NBR 5.2/6.3, deslizamento ⚠ | emenda de viga com chapa de extremidade via A1 **se** a fonte confirmar que o modelo cobre emenda (marcado ⚠ na matriz) | cada emenda: N, V, M separados; escorregamento só com fonte | cobrejunta: N–δ sem fonte | F-PAR, F-SOL |
| **F-MIS** (G) | mista (Anexo O), pino, formado a frio (NBR 14762) | NBR 8800 Anexo O, NBR 14762, EN 1994 ⚠ | só G+N existentes; M quando houver fonte | **somente representabilidade (J3)** nesta fase | domínios constitutivos próprios | J3 |

## 6. Faixa V — validação (por nível, nunca confundidos)
| Nível | O que prova | Quando se aplica | Artefatos |
|---|---|---|---|
| R reprodução | equações impressas reproduzidas | todo modelo (P2) | testes unitários com valores publicados |
| V verificação numérica | código = solução do mesmo modelo | todo modelo | convergência, equilíbrio, invariância |
| S/MEF referência | resposta coerente com solução independente | modelos sem ensaio e componentes calibrados (D6: MEF **externo**, Mecway/CalculiX, fora do produto) | relatórios em `docs/validation/`, scripts em `research/` |
| E experimental | resposta coerente com ensaios **independentes dos autores do modelo** | só aqui "validada" | base de ensaios catalogada com origem |
| P profissional | revisão por engenheiro habilitado | antes de uso comercial | registro |

## 7. Regressão (todas as fases)
Antes e depois de cada fase: build limpo, `ctest -C Release` completo (35 testes anteriores + novos), `TestMomentRotationRegression`, `TestTStub`, `TestTStubStiffness`, `TestJointClassification`, goldens **não regravados**; `git status` sem arquivos fora da lista da fase; a lista "arquivos afetados" da tabela é verificada no relatório da fase. Fases F nunca alteram arquivos de outras faixas.

## 8. Relação de arquivos afetados por fase (resumo)
| Fase | Novos | Existentes alterados |
|---|---|---|
| J0 | `jointengine/*.h/.cpp` (contratos) | `CMakeLists.txt`, `src/tests/CMakeLists.txt` |
| J1 | `jointengine/{Topology,Fingerprint}`, `services/JointTopologyAdapter` | idem CMake |
| J2 | `jointengine/{EnComponentMethodModel,NbrClassifier}` | CMake |
| J3 | fixtures + `TestJointEngineRepresentability` | CMake de testes |
| J3-T | `jointengine/{EnTStubModel,PilusoTStubModel}` | CMake (após C7.1) |
| S1 / S3 | `jointengine/Strategy*` | CMake |
| N0–N2 | doc; `jointengine/EnClassifier` | CMake |
| F-* | `jointengine/models/<familia>/*`, `services/Joint<Familia>Adapter`, testes | CMake |
| **Persistência / UI / relatório** | seção `[JOINT_TOPOLOGY]`, painéis, relatórios | `DatModelIoService.cpp`, `src/ui/*`, `src/report/*`, `ReportServiceDesign.cpp` — **somente em fases separadas e com autorização própria** |
| S-01 (saneamento) | — | `JointClassification.h` (apenas comentário), isolado |
Nenhuma fase altera `TStub*`, `JointComponents.cpp`, `TestMomentRotationRegression`, goldens ou verificações normativas; arquivos do C7 nunca.

## 9. Pontos que pedem sua decisão antes de J0
1. Autorizar J0–J3 (infraestrutura) como primeira entrega, sem nenhuma faixa de família.
2. Confirmar a **persistência** da topologia como fase posterior e separada (J1 apenas deriva).
3. F-BAS: decidir se o modelo de dados passa a ter concreto/graute (hoje inexistentes).
4. Escolher a **primeira faixa F** a iniciar P0 em paralelo à Faixa I (sugestão sem hierarquia arquitetural: F-GUS e F-SOL, por serem as de maior lacuna de fonte, para que a leitura de fontes não seja gargalo).
5. Aprovar o critério de entrada de S3.
