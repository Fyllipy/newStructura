# Diagramas da arquitetura D0 (Mermaid)

## 1. Arquitetura geral
```mermaid
flowchart TB
  subgraph Existente
    MS[ModelData<br/>SolverBridge]
    PM[Modulo parametrico<br/>NodeConnection + ConnectionCatalog + Check]
    JC[JointComponents EN<br/>2 tipologias]
    JCL[JointClassification NBR 8800]
    TS[TStub / TStubStiffness EN]
  end
  subgraph JME[Joint Mechanical Engine - proposto]
    AD[Joint Data Adapter<br/>FamilyAdapter + fingerprint + MissingData]
    LIB[Component Model Library<br/>ModelDescriptor por modelo]
    ASM[Connection Assembly Solver<br/>estrategias S1..S4]
    RE[Response Engine<br/>MechanicalResponse]
    CL[Classification Engine<br/>independente]
  end
  C7[C7 Piluso2001<br/>em desenvolvimento]
  FEM[Backend MEF externo<br/>referencia]
  MS --> AD
  PM --> AD
  AD --> ASM
  LIB --> ASM
  JC -. envolvido, nao reescrito .-> LIB
  TS -. complementar .-> LIB
  C7 -. por contrato, apos conclusao .-> LIB
  ASM --> RE
  FEM -. Reference .-> RE
  ASM -. exporta Assembly .-> FEM
  RE --> CL
  JCL -. envolvido .-> CL
  RE --> OUT[Relatorio / idealizacao no solver global]
```

## 2. Fluxo A — projeto de nova ligacao
```mermaid
flowchart TD
  A[Selecionar elementos conectados] --> B[Recuperar propriedades<br/>secoes, materiais, esforcos]
  B --> C[Definir funcao e comportamento desejado<br/>DesiredBehavior]
  C --> D[Escolher configuracao fisica]
  D --> E[Configurar componentes<br/>modulo parametrico existente]
  E --> F[Dimensionamento normativo<br/>Check existentes]
  F --> G[Adapter: ConnectionConfiguration + fingerprint]
  G --> H[Solver mecanico + Response Engine]
  H --> I[ComputedBehavior]
  C --> J{Comparar<br/>BehaviorComparison}
  I --> J
  J -- nao atende --> E
  J -- atende --> K[Classificacao normativa + relatorio]
```

## 3. Fluxo B — analise de ligacao existente
```mermaid
flowchart TD
  A[Elementos conectados] --> B[Configuracao fisica existente]
  B --> C[Geometria, materiais, tipos de uniao]
  C --> D[Contorno e esforcos]
  D --> E[Adapter + validacao de dados<br/>MissingData / ValidityStatus]
  E -- dados insuficientes --> X[Estado: InsufficientData<br/>sem numero]
  E --> F[Analise mecanica]
  F --> G[MechanicalResponse + Provenance]
  G --> H[Classificacao normativa quando aplicavel]
  H --> I[Hipoteses, limitacoes, mecanismo governante]
```

## 4. Integracao parametrica e invalidacao
```mermaid
sequenceDiagram
  participant UI
  participant P as Modulo parametrico
  participant A as FamilyAdapter
  participant S as Solver/JME
  participant R as MechanicalResponse
  UI->>P: edita values (geometria/aco)
  P->>A: NodeConnection
  A->>A: fingerprint(values, E, versoes)
  A->>S: ConnectionConfiguration
  S->>R: resposta + provenance.inputFingerprint
  UI->>P: edita novamente
  P->>A: novo fingerprint
  A-->>R: fingerprint difere
  R-->>UI: resultado OBSOLETO (nao exibido como atual)
```

## 5. Composicao mecanica
```mermaid
flowchart LR
  subgraph Assembly
    N1((no A)) -- T-stub superior --> N2((no B))
    N2 -- parafuso --> N3((no C))
    N3 -- T-stub inferior/chapa --> N4((no D))
    N2 -. interface contato .-> N4
    N1 -. mecanismo de resistencia<br/>ruptura/block shear .-> N4
  end
  AD[topologia declarada pelo adaptador<br/>nao inferida serie/paralelo] --> Assembly
  Assembly --> EQ[equilibrio + compatibilidade<br/>+ excentricidades]
  EQ --> SOL[estrategia: S1 EN / S2 analitico / S3 nao linear]
```

## 6. Caminho MEF
```mermaid
flowchart LR
  ASM[Assembly] --> EXP[Exportar para backend MEF<br/>solidos, contato, plasticidade]
  EXP --> RUN[Solver externo<br/>ex. CalculiX/Mecway]
  RUN --> IMP[Importar curvas]
  IMP --> REF[MechanicalResponse<br/>ResultKind::Reference]
  REF --> CAL[Calibracao / validacao de componentes]
  REF -. nunca .-> RES[resistencia normativa de calculo]
```

## 7. Classificacao normativa
```mermaid
flowchart TD
  R[MechanicalResponse] --> C{Contexto<br/>norma, edicao, tipo, grandeza}
  C -- NBR 8800:2024 6.1.2 --> N[NbrJointClassifier existente]
  C -- EN 1993-1-8 5.2.2 --> E[EnJointClassifier<br/>fase J6, fonte a ler]
  C -- sem criterio fundamentado --> NV[NaoVerificavel + limitacao]
  N --> V[ClassificationResult]
  E --> V
  D[DesiredBehavior] --> CMP[BehaviorComparison]
  V --> CMP
  R --> CMP
  CMP --> OUT[Atende / Nao atende / Indeterminado / Nao verificavel]
```
