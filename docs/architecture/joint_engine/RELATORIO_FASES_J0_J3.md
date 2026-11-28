# Relatório das fases J0–J3 (Joint Mechanical Engine)

Sem validação profissional. Nenhum modelo constitutivo novo foi criado.

## Resultado
Build limpo (lib removida e reconstruída) e `ctest -C Release`: **39/39 aprovados** = os 35 anteriores (inalterados) + 4 novos (`TestJointEngineContracts`, `TestJointFingerprint`, `TestJointEngineRegression`, `TestJointEngineRepresentability`).

## Arquivos
**Novos:** `src/design/jointengine/{Types,Materials,Descriptor,Component,Response,Topology,Fingerprint,Capability,EnComponentMethodModel,NbrClassifier}.{h,cpp}` (Component e Capability só `.h`); `src/app/services/JointTopologyAdapter.{h,cpp}`; os 4 testes acima.
**Alterados:** apenas `CMakeLists.txt` e `src/tests/CMakeLists.txt` (inserções aditivas).
**Não tocados (SHA-256 idêntico ao baseline):** `TStub.cpp`, `TStubStiffness.cpp`, `JointComponents.cpp`, `JointClassification.{h,cpp}`, goldens. Sem alteração em C7, `DatModelIoService`, formato de projeto, UI, relatórios. `git status`: diferença = somente os arquivos novos acima.

## Entregas
- **J0:** `Origin/Quantity` (ausente ≠ zero), `ValidityStatus` (só Valid/ValidWithCaveats autorizam números), `ResultKind` (só NormativeResistance entra em aprovação), `MaterialSpec` com concreto e graute de primeira classe sem propriedades presumidas, `ModelDescriptor` validável, interfaces de componente com estado (preparadas para não linearidade), resposta tipada com invariante, classificação/comportamento desejado separados, `IAssemblyStrategy` (só interface; sem solver).
- **J1:** `JointTopology` (partes, uniões N-árias, caminhos de carga só com fundamento declarado), fingerprints CFP/AFP/RFP/NFP (SHA-256 verificado por vetores FIPS; canônico, independente de ordem; mudar só a norma invalida só a classificação), derivação a partir da ligação legada sem gravar nada.
- **J2:** wrappers `EnComponentMethodModel` e `NbrClassifier`, comparados **bit a bit** com as funções originais.
- **J3:** as 50 entradas do `ConnectionCatalog` e os 8 tipos legados são derivados (todas `Partial`, nenhuma inferência de caminho de carga); capacidade mecânica separada da topologia: **4 entradas** com modelo (EN, via `JointComponentService::isSupported`) e **46 com `NotYetAvailable`** justificado e com lista do que falta; 9 configurações conceituais (parafusada, soldada, híbrida, gusset, nó de treliça, emenda, tubular, mista, base com chumbadores) representáveis, só 2 com modelo mecânico.

## Limitações e divergências
- Toda topologia derivada é `Partial`: papéis de barras do nó, pareamento exato das partes por união, presença de soldas em várias famílias, propriedades de concreto/graute e origem dos valores legados não estão armazenados — declarados como lacunas.
- Wrapper EN usa só Mj,Rd da EN (sem o fallback NBR do serviço); capacidade de rotação não existe.
- Classificador NBR: o veredito "Atende" significa "classificação estabelecida"; Atende/NãoAtende em relação ao comportamento desejado fica para `BehaviorComparison` (ainda sem implementação).
- Divergência registrada (não corrigida, S-01): comentário desatualizado em `JointClassification.h` sobre `ComponentMethod`.
- Erro corrigido durante o J3: referência pendente a vetor temporário no próprio teste de representabilidade.

## Pendente
Pesquisas P0 de F-GUS e F-SOL (documentais) ainda não iniciadas.
