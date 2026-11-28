# Arquitetura do Joint Mechanical Engine (JME) — especificação D0

Especificação, não implementação. Deriva do código real (ver `RELATORIO_D0_AUDITORIA_ARQUITETURAL.md`). Nada aqui declara capacidade existente que não conste na auditoria. Os contratos conceituais estão em `API_CONTRACT_JOINT_MECHANICAL_ENGINE.md`; diagramas em `DIAGRAMAS_ARQUITETURA_D0.md`; fases em `ROADMAP_JOINT_MECHANICAL_ENGINE.md`.

## 1. Princípios
1. **O módulo paramétrico continua sendo a origem da configuração física.** O JME *consome* `NodeConnection`/`ConnectionCatalog` por adaptadores; não cria segundo módulo paramétrico nem duplica dados (referencia ids e `fingerprint`).
2. **Nada de "mola universal".** Componente, mecanismo resistente, interface e condição de contorno são conceitos distintos com interfaces distintas.
3. **Nenhum modelo é universal.** Cada modelo declara id, versão, formulação, domínio de validade, parâmetros obrigatórios com origem, estados e limitações. Piluso 2001 é **um** modelo possível de T-stub, não "o" T-stub.
4. **Resistência normativa ≠ lei constitutiva.** A verificação de uma solda ou gusset (`Check`) não gera F–δ; o JME só usa F–δ de um modelo que o declare e o fundamente.
5. **Sem extrapolação silenciosa; dado ausente nunca vira aprovação** (reaproveita `CheckStatus::Unverifiable/NotApplicable`).
6. **Camadas sem Qt** (`src/design`-like, namespace `Structura::Design::JointEngine`), adaptadores com Qt em `src/app/services`.
7. **Desired Behavior, Computed Behavior e Normative Classification são objetos separados.**
8. Incremental: cada fase entrega algo testável sem exigir as seguintes; o que já existe (`JointComponents`, `JointClassification`) **não é refatorado** — é envolvido.

## 2. Visão geral das camadas
```
Modelo estrutural (ModelData)  Módulo paramétrico (NodeConnection, FamilyOps, Check)
          \                              /
           Joint Data Adapter  ── JointModelRef (ids, fingerprint, origens, ausências)
                      |
        Component Model Library (modelos declarados)  ◄── C7 Piluso2001, TStub EN, JointComponents EN, (futuros)
                      |
        Connection Assembly Solver (estratégias)  ──►  FEM externo (caminho complementar)
                      |
              Response Engine  ──► MechanicalResponse (curvas, rigidezes, resistências, mecanismos, proveniência)
                      |
   Classification Engine (independente)  ◄── DesiredBehavior;  saída: ClassificationResult (por norma)
                      |
   Saídas: relatório, idealização no solver global (MemberConnectionSpec / LinkData / PlasticHingeSpec)
```

## 3. Modelo de domínio
### 3.1 Structural Joint (referência, não cópia)
`StructuralJoint{id, anchorNode(s), ConnectedElement[]}` com `ConnectedElement{memberId|plateId, role, endSide, localAxes, sectionRef, materialRef}`; `role ∈ {Beam, Column, Diagonal, Post, Chord, Brace, Plate, Tube, Other}`. **Lista aberta de elementos** (não existe "principal/secundário" fixo): uma gusset conecta N elementos. Extraído de `ModelData` pelo mesmo padrão de `JointAnalysisService::analyze` (precedente auditado). Ações de projeto entram como `ActionSet` (por combinação), não como atributo da ligação.
**Lacuna a tratar na integração (não agora):** `NodeConnection` guarda uma barra (`memberId`); para N elementos o adaptador monta a lista a partir das barras que chegam ao nó (`bars` de `JointAnalysisService::Context`) + referências adicionais gravadas no mapa `values` (decisão futura, §12).

### 3.2 Connection Configuration
Descrição **física** montada por *adaptadores de família* a partir de `NodeConnection.values` (um adaptador por família do catálogo, reaproveitando os `toXxx(c, forces)` já existentes em `ConnectionService`): conjunto de `PhysicalPart` (chapa, perfil auxiliar, cantoneira, T-stub, enrijecedor, diafragma, gusset, placa de base), `Fastener` (parafuso, chumbador, pino), `Weld`, posições, excentricidades, condições de contato/montagem. Cada valor carrega `Quantity{value, unit, Origin, source}` (padrão já definido na especificação do T-stub). Configurações mistas = lista heterogênea.

### 3.3 Mechanical Component — três interfaces, não uma
| Categoria | Interface | Pergunta que responde | Exemplos |
|---|---|---|---|
| **Deformable component** | `IDeformableComponent` | `F = f(u)` e `K = ∂F/∂u` em seu espaço de GDL, com estado interno opcional | T-stub (EN k, Piluso), parafuso (alongamento), chapa à flexão, alma do pilar, enrijecedor, gusset (rigidez), placa de base |
| **Resistance mechanism** | `IResistanceMechanism` | capacidade `R(estado)` e modo governante; pode **acoplar** componentes (interação N–V, M–N) ou **terminar** a resposta (ruptura) | escoamento, ruptura líquida, block shear, flambagem da gusset, rasgamento, falha de solda (norma) |
| **Interface** | `IInterfaceModel` | transferência dependente de estado (aberto/fechado, aderência/deslizamento) | contato placa–concreto, atrito, separação, deslizamento do parafuso |
| *Condição de contorno* | dado do `ActionSet`/`Assembly` | não é modelo | apoio, simetria, deslocamento imposto |
**Regra de classificação** (quando é o quê): se tem lei de deformação própria → componente; se só delimita capacidade/termina o caminho → mecanismo; se o estado muda a *topologia* de transferência → interface; se vem de fora da ligação → condição de contorno. Soldas **sem** modelo constitutivo declarado entram apenas como `IResistanceMechanism` (norma) e o solver trata sua rigidez como **dado explícito do usuário/hipótese rotulada** (nunca presumida rígida em silêncio).

### 3.4 Mechanical Assembly
Grafo `Assembly{nodes = pontos de equilíbrio com GDL generalizados, edges = componentes/interfaces, constraints, eccentricities}`. Topologia **declarada** pelo adaptador de família (não inferida como série/paralelo). Compatibilidade e equilíbrio são equações do grafo; excentricidades entram como transformações rígidas explícitas. Estratégias de solução (§6) são selecionadas conforme a declaração dos componentes.

### 3.5 Mechanical Analysis Result
`MechanicalResponse` (§5) é um contêiner **tipado por grandeza**: `ResponseKind ∈ {MomentRotation, ForceDisplacement, GeneralizedVector, ResistanceOnly, StiffnessOnly}`; guarda curvas, rigidezes (inicial/tangente/secante), resistências, capacidade de deformação, distribuição de esforços, mecanismo governante, estado de convergência, hipóteses, limitações, `Provenance`. Um resultado pode **não ter** curva M–θ (contraventamento, gusset, base).

## 4. Generalização das grandezas mecânicas
Definição proposta:
```
GeneralizedDof { id, kind ∈ {Force, Moment}, axis (local da ligação), unit }   // conjugado: Displacement/Rotation
DofSpace { dofs[] }                 // ex.: {N}, {V}, {M}, {N,V,M}, {Mx,My,T}
ConstitutiveLaw : F = f(u, state)   com   K = ∂F/∂u  (tangente) opcional
```
**Estratégia incremental** (aderente ao que existe): **N1** leis escalares por GDL independentes (`F_i = f_i(u_i)`; é o que `LinkData`/`MemberConnectionSpec`/`JointBehavior` já representam); **N2** vetor com tangente **diagonal por blocos** (acoplamento N–M apenas por mecanismos de interação, não por matriz plena); **N3** tangente **matricial plena** só para componentes que a formulação fundamente (ex.: MEF reduzido). A curva M–θ é **um** `ResponseKind`, produzida a partir de N1 para o GDL de flexão; **não** é a saída obrigatória. Avaliação de adequação: N1 reutiliza o que existe; N2/N3 só quando houver modelo que justifique; implementar F=f(u) matricial desde já seria especulação (não implementar nesta fase).

## 5. Contrato de resposta e proveniência
Todo resultado carrega `Provenance{modelId, modelVersion, standard+edition, parameters[Quantity com Origin], assumptions[], limitations[], inputFingerprint, solverStrategy, convergence, timestamp-less}` e `ResultKind ∈ {NormativeResistance, ModelUltimate, ModelEstimate, Reference}`. `ModelUltimate` (ex.: Piluso) **nunca** entra em aprovação/`maxRatio`; só resultados `NormativeResistance` o fazem (regra já adotada em `CheckStatus::Complementary`). Domínio de validade é verificado **antes** e o resultado devolve `ValidityStatus` (`Valid`, `ValidWithCaveats`, `OutOfDomain`, `InsufficientData`, `Unsupported`). Consulta fora da curva ⇒ estado, não extrapolação.
**Invalidação:** o resultado guarda o `fingerprint` da configuração (padrão de `JointComponentService::fingerprint`, já existente e testado) + versões de modelos; qualquer mudança paramétrica, de aço ou de ação invalida o `MechanicalResponse` (a UI/relatório nunca mostra resultado obsoleto como atual). Isso resolve "inconsistência entre geometria dimensionada e analisada" sem copiar geometria.

## 6. Solver e métodos de modelagem
`IAssemblyStrategy::applicable(assembly) → {Yes, No, WithCaveats, reasons}` e `solve(assembly, actions, controls)`. Estratégias previstas (nenhuma é universal):
| Estratégia | Quando | Observação |
|---|---|---|
| **S1 Método dos componentes (EN)** | ligações viga–pilar/emendas com componentes tabelados; topologia declarada (fileiras, série/paralelo **da norma**) | já existe para 2 tipologias em `JointComponents` (será **envolvido**, não reescrito); a distribuição por fileira usa o procedimento da EN (6.2.7.2), não média |
| **S2 Modelo especializado analítico** | gusset (Whitmore + estabilidade), base, tubular | fórmulas com domínio; distinguir "resistência normativa" de "modelo mecânico" |
| **S3 Equilíbrio numérico não linear** | componentes com leis F–δ não lineares, interfaces, múltiplos GDL, mudança de mecanismo | Newton/passo incremental com controle de convergência, detecção de mudança de estado de interface; estado por componente; **estados de convergência reportados** |
| **S4 MEF (complementar)** | geometria sem componente fundamentado, calibração/validação | ver §10 |
Mudanças de mecanismo (ex.: T-stub 1→2) são tratadas **dentro** do modelo do componente; o solver só enxerga a lei resultante e seus estados (`MechanismNearTransition` etc.).
**Não presumir série/paralelo.** Compartilhamento de esforços entre parafusos e soldas decorre de compatibilidade + rigidez relativa declaradas; se faltarem rigidezes fundamentadas, o resultado é `InsufficientData`, **não** repartição igualitária.

## 7. Response Engine
Deriva do resultado do solver (ou do modelo) as respostas: curvas M–θ / F–δ, `Si`, rigidezes tangente e secante (`secantStiffnessAt`/`tangentStiffnessAt` já existem para a curva EN), resistências, **capacidade de rotação/deformação** (apenas quando o modelo a fornece; a curva EN atual **não** a tem), distribuição por componente, mecanismo governante. A resposta é restrita ao domínio dos modelos; fora dele, estado explícito.

## 8. Classification Engine (independente do solver)
Entrada: `MechanicalResponse` + `ClassificationContext{norma, edição, tipo de ligação, grandeza, contexto estrutural (contraventada?), Kv/Kp...}`; saída: `ClassificationResult{status ∈ {Atende, NaoAtende, Indeterminado, NaoVerificavel}, classe, justificativa, critérios usados}`. **Já existe** a classificação NBR 8800:2024 6.1.2 (`JointClassification`); ela será *adaptada* (não reescrita) a este contrato. **EN 1993-1-8 §5.2.2** (critérios de rigidez/resistência para classificar) **não foi lida nesta fase** → fase própria com leitura da fonte; **não misturar critérios**; configuração sem critério normativo fundamentado → `NaoVerificavel` com a limitação. `DesiredBehavior{kind ∈ {Rigid, SemiRigid, Pinned, Custom(req)}, requirement[]}` é objeto separado; a **comparação** desejado × calculado é um terceiro objeto (`BehaviorComparison`), sem variável compartilhada.

## 9. Tratamento por família
- **Parafusadas:** componentes T-stub (modelo escolhido por *id*), parafuso, chapa, alma/mesa de pilar, enrijecedor; S1 ou S3. Pré-tensão é parâmetro do componente, não do solver.
- **Soldadas:** a norma dá **resistência**; rigidez/ductilidade de solda só entra com modelo declarado (hoje inexistente na base: matriz item 10). Mesas soldadas de viga–pilar: hipótese de solda rígida **rotulada** como tal.
- **Híbridas:** componentes de naturezas diferentes ligados por compatibilidade; exige rigidezes fundamentadas de ambos; sem elas → `InsufficientData`. Fase própria com validação MEF/experimental.
- **Gusset/chapa de nó:** ver §9.1.
- **Chapas/placas/enrijecedores/diafragmas:** `PhysicalPart` + componentes de flexão/tração/cisalhamento; diafragma como transferência de esforços (componente futuro, matriz 19).
- **Base e ancoragem:** extensão futura; concreto/graute/ancoragem **não estão contemplados no programa** (matriz 16–17); exige modelo de contato e fontes (⚠ ACI 318, EN 1992-4, EN 1993-1-8 §6.2.5 a confirmar).
### 9.1 Gusset plates (requisito específico)
Os quatro objetivos permanecem separados: **(1) verificação normativa** (já em `TrussConnections`: Whitmore, escoamento, ruptura, flambagem, block shear); **(2) modelo mecânico** do comportamento (rigidez/distribuição entre barras, não existe); **(3) análise numérica detalhada** (MEF de cascas com contato dos parafusos e plasticidade — o solver atual **não** basta, §10); **(4) avaliação da ligação completa**. Domínio: `StructuralJoint` com N barras e `PhysicalPart{kind: Gusset, outline, thickness, material}`; `Fastener`/`Weld` nas linhas de conexão; excentricidades explícitas; mecanismos como `IResistanceMechanism` (Whitmore é *método de distribuição de tensões simplificado* com domínio angular — sua aplicabilidade é declarada, não presumida). Saída: `ResponseKind::GeneralizedVector` (forças em cada barra × deslocamentos), **sem M–θ obrigatório**. Gusset **não** é T-stub nem "molas independentes": proibido por contrato (não há adaptador T-stub→gusset).

## 10. MEF (caminho complementar)
Papel: (a) **referência** para validar/calibrar componentes (como na C6), (b) análise detalhada de peças sem modelo analítico. O MEF existente (cascas Q4 facetadas, sem contato real, sem plasticidade, sem sólidos, ties por penalidade) **não é suficiente** para parafusadas/soldadas/híbridas; sólidos/contato/plasticidade exigem **solver externo** (a validação C6/Mecway–CalculiX demonstrou o fluxo, fora do produto) ou novo desenvolvimento. Contrato: o JME exporta a `Assembly` para um *back-end MEF* (`IFemBackend`) e importa `MechanicalResponse` com `ResultKind::Reference`/`NumericalCurve` (`DataOrigin::NumericalCurve` **já existe**). **MEF não produz resistência de cálculo normativa**: o resultado é `Reference`, nunca `NormativeResistance`. Fora do escopo agora: geração automática de malha de ligação.

## 11. Contrato com o módulo paramétrico (resumo; detalhe no API_CONTRACT)
`FamilyAdapter` por família (`toConfiguration(NodeConnection, ModelContext) → ConnectionConfiguration + MissingData`), `Provenance` e `fingerprint`. Informação ausente → `MissingData[]` + estados, nunca preenchimento silencioso. Alterar geometria muda `fingerprint` → resultados mecânicos invalidados; resultados normativos já seguem esse padrão (`verified=false`).

## 12. Decisões abertas (dependem de aprovação) — resumo; lista completa no relatório final
Local dos tipos (`src/design/jointengine/` vs `src/design/`), representação de N elementos em `NodeConnection` (extensão de `values` vs nova estrutura), estratégia N1→N2→N3, EN 5.2.2, prioridade entre S1/S3, back-end MEF.

## 13. Posição do C7
Modelo **especializado** na biblioteca (id `Piluso2001`), implementa `IDeformableComponent` (GDL: força axial do T-stub × deslocamento), `ResultKind::ModelUltimate`, estados próprios (`MechanismNearTransition`, `FailureMode3Unavailable`, …), `K_i` fornecido, aço obrigatório. O motor o trata **como caixa preta declarada**: lê seu `ModelDescriptor`, domínio e parâmetros obrigatórios; **não** presume que a API final seja a hoje proposta (C7.1 pode alterá-la): o D0 só exige que o modelo publique *descritor + estados + proveniência*, o que qualquer revisão do C7 já deve fornecer. Nada será integrado antes da conclusão do C7 e de nova autorização.

## 14. Testabilidade e rastreabilidade
Testes independentes por camada: componentes (reprodução de fórmulas publicadas — como `TestTStub`), mecanismos (limiares e estados), montagem (equilíbrio e compatibilidade com soluções analíticas), solver (convergência, determinismo), respostas (métricas), classificação (casos normativos), adaptador (fingerprint e dados ausentes), integração MEF (casos de referência externos).
**Níveis de evidência, nunca confundidos:** (1) *reprodução de formulação* (equações impressas), (2) *verificação numérica* (código = solução do mesmo modelo), (3) *validação com soluções de referência* (analíticas/MEF), (4) *validação experimental* (ensaios), (5) *validação profissional* (revisão por engenheiro habilitado). Cada modelo declara o nível alcançado; **teste unitário aprovado ≠ validado**.
