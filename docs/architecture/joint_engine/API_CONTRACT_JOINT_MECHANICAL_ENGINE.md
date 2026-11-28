# Contratos conceituais do Joint Mechanical Engine

**Pseudocódigo conceitual, não é código a criar nesta fase.** Nomes e campos são propostas para revisão; alinhados ao padrão do repositório (namespaces `Structura::Design::*`, unidades N/mm/MPa no domínio de design, `kN/m` no solver, NaN/`std::optional` para "não informado", `Check`/`CheckStatus` existentes). Todos os tipos do domínio ficam sem Qt.

## 1. Tipos básicos (proveniência e estados)
```cpp
enum class Origin { Normative, ManufacturerDeclared, Test, Correlation, ModelAssumption, UserProvided, Derived };
struct Quantity { double value; std::string unit; Origin origin; std::string source; };   // sem default silencioso

enum class ValidityStatus { Valid, ValidWithCaveats, OutOfDomain, InsufficientData, InvalidGeometry,
                            MissingMaterialProperties, Unsupported };
enum class ResultKind { NormativeResistance, ModelUltimate, ModelEstimate, Reference };

struct ModelDescriptor {          // o que TODO modelo publica
  std::string id;                 // "Piluso2001", "EnTStubStiffness", "EnComponentMethodEndPlate"...
  std::string version;
  std::string formulation;        // texto curto + referências
  std::vector<std::string> references;
  DofSpace supportedDofs;         // grandezas conjugadas suportadas
  std::vector<ParameterSpec> required;     // nome, unidade, Origin admitidas, obrigatório?
  std::vector<std::string> hypotheses, limitations;
  DomainSpec domain;              // faixas/regras de validade (inclusive lista de condições)
  ResultKind resultKind;
};
struct Provenance { std::string modelId, modelVersion; std::string standard, edition;
                    std::vector<std::pair<std::string,Quantity>> parameters; std::vector<std::string> assumptions, limitations;
                    std::string inputFingerprint; std::string solverStrategy; ConvergenceInfo convergence; };
```

## 2. Joint Data Adapter ↔ domínio
```cpp
struct ConnectedElement { int memberId; Role role; EndSide end; Frame localAxes; SectionRef section; MaterialRef material; };
struct StructuralJoint { int id; std::vector<int> nodeIds; std::vector<ConnectedElement> elements; ActionSetRef actions; };

struct ConnectionConfiguration {                 // vem SEMPRE do módulo paramétrico via FamilyAdapter
  std::string familyId; std::string fingerprint; // fingerprint no padrão JointComponentService::fingerprint
  std::vector<PhysicalPart> parts;  std::vector<Fastener> fasteners; std::vector<Weld> welds;
  std::vector<Eccentricity> eccentricities; std::vector<MissingData> missing;   // ausência é dado, não zero
};
class IFamilyAdapter {                           // um por família do ConnectionCatalog (reusa toXxx(c, forces))
 public:
  virtual bool handles(const NodeConnectionRef&) const = 0;
  virtual ConnectionConfiguration toConfiguration(const NodeConnectionRef&, const ModelContext&) const = 0;
};
```
Regras: o adaptador **não** executa dimensionamento normativo e **não** copia geometria além do necessário (guarda `fingerprint` + valores com `Quantity`).

## 3. Componentes (três interfaces)
```cpp
struct ComponentState;   // estado interno opaco (plasticidade, abertura de contato, etc.)
class IDeformableComponent {
 public:
  virtual const ModelDescriptor& descriptor() const = 0;
  virtual Applicability applicable(const ConnectionConfiguration&, const ParameterSet&) const = 0; // ValidityStatus + motivos
  virtual Response1D  evaluate(const Vector& u, ComponentState&) const = 0;    // F = f(u), status por consulta
  virtual std::optional<Matrix> tangent(const Vector& u, const ComponentState&) const { return std::nullopt; }
  virtual ~IDeformableComponent() = default;
};
class IResistanceMechanism {
 public:
  virtual const ModelDescriptor& descriptor() const = 0;
  virtual MechanismResult capacity(const ActionVector&, const ConnectionConfiguration&) const = 0; // R, modo, status (Check-like)
};
class IInterfaceModel {
 public:
  virtual const ModelDescriptor& descriptor() const = 0;
  virtual InterfaceState update(const Vector& relDisp, const InterfaceState&) const = 0;           // aberto/fechado/deslizando
};
```
`Response1D{ std::vector<double> force; double stiffness; std::optional<double> ultimateDisplacement; ValidityStatus status; std::vector<Message> messages; }`. Consulta fora do domínio → `status = OutOfDomain`, **sem número**.

## 4. Montagem e solver
```cpp
struct Assembly {
  std::vector<AssemblyNode> nodes;                 // GDL generalizados
  std::vector<AssemblyEdge> edges;                 // componente|interface|mecanismo, com ligação declarada
  std::vector<Constraint> constraints;  std::vector<Eccentricity> eccentricities;
};
class IAssemblyStrategy {
 public:
  virtual std::string id() const = 0;
  virtual Applicability applicable(const Assembly&) const = 0;
  virtual SolveResult solve(const Assembly&, const ActionSet&, const SolveControls&) const = 0;  // ConvergenceInfo obrigatório
};
```
O solver **não** infere série/paralelo: a topologia vem do adaptador (ex.: o da EN declara as fileiras e o procedimento 6.2.7.2).

## 5. Resposta mecânica
```cpp
enum class ResponseKind { MomentRotation, ForceDisplacement, GeneralizedVector, ResistanceOnly, StiffnessOnly };
struct MechanicalResponse {
  ResponseKind kind;  DofSpace dofs;
  std::vector<CurveN> curves;                      // N-dim (N=1 para M–θ/F–δ)
  std::optional<Stiffness> initial, tangent, secant;
  std::vector<ResistanceItem> resistances;         // com ResultKind e modo governante
  std::optional<DeformationCapacity> capacity;     // só se o modelo fornece (curva EN atual NÃO tem)
  std::vector<ComponentContribution> distribution; std::string governingMechanism;
  ValidityStatus status;  Provenance provenance;   // sempre
  ConvergenceInfo convergence;  std::vector<std::string> hypotheses, limitations;
};
```
Invalidação: `response.provenance.inputFingerprint != adapter.fingerprint(current)` ⇒ **obsoleto** (a UI/relatório indicam; nunca exibem como atual).

## 6. Classificação e comportamento desejado
```cpp
struct DesiredBehavior { enum Kind{Rigid,SemiRigid,Pinned,Custom} kind; std::vector<Requirement> req; };   // intenção do projetista
struct ClassificationContext { Standard std; std::string edition; JointKind joint; Quantity measured; StructuralContext ctx; };
enum class Verdict { Atende, NaoAtende, Indeterminado, NaoVerificavel };
struct ClassificationResult { Verdict verdict; std::string cls; std::string justification; std::vector<std::string> criteria; };
struct BehaviorComparison { Verdict verdict; std::vector<std::string> deviations; };   // desejado × calculado
class IClassifier { public: virtual ClassificationResult classify(const MechanicalResponse&, const ClassificationContext&) const = 0; };
```
Três objetos **independentes**; nenhuma variável compartilhada. Implementações: `NbrJointClassifier` (envolve `JointClass::classify`), `EnJointClassifier` (**fase própria**, fonte a ler).

## 7. Contrato com o modelo estrutural global (saídas existentes)
`MechanicalResponse` → `idealize(spec, dof, kind, Si, Mj,Rd, hardening)` (rotula/mola/bilinear), ou `LinkData.laws[6]` para multilinear por GDL. **Idealização é decisão do projetista posterior à análise** (já é o padrão de `JointAnalysisService::idealize`) e **não altera** a resposta mecânica.

## 8. Contrato com o T-stub (C7) — por declaração, sem acoplamento
O JME só exige que um modelo de T-stub publique: `ModelDescriptor` (id `Piluso2001`, domínio, parâmetros obrigatórios com `Origin`), `applicable()` com os estados do C7 (`MechanismNearTransition`, `MechanismNotIdentifiable`, `MechanismUnsupported`/`FailureMode3Unavailable`, entradas ausentes), `evaluate()` com consulta fora do domínio como estado, `ResultKind::ModelUltimate`. Tabela de correspondência (provisória, sujeita à C7.1):
| C7 | Contrato JME |
|---|---|
| `ModelId::Piluso2001` | `ModelDescriptor.id` |
| 4 pontos característicos + tabela | `Response1D` (curva) |
| `K_i` fornecida (`UserProvided`) | parâmetro obrigatório com `Origin` |
| `areaResistance`/`areaKind` | parâmetro obrigatório com `Origin` e aviso |
| estados de mecanismo | `ValidityStatus` + `Message` |
| `ResultKind::ModelUltimate` | idem |
**Se a C7.1 mudar nomes/estruturas, apenas esta tabela é atualizada.** O adaptador T-stub→componente só é escrito após a conclusão do C7 e autorização. Proibido por contrato: usar a lei de Piluso para resistência de aprovação; misturar `K_i` EN com `δ_p` de Piluso sem decisão explícita.

## 9. Invariantes verificáveis (testes de contrato)
1. nenhuma saída numérica com `status ∈ {OutOfDomain, InsufficientData, Unsupported, InvalidGeometry}`;
2. `ResultKind::ModelUltimate`/`Reference` nunca entram em aprovação;
3. determinismo (mesma entrada → mesma saída, bit a bit);
4. `fingerprint` muda com qualquer parâmetro ou versão de modelo;
5. nenhuma dependência de Qt/UI em `src/design`-like;
6. todo `Quantity` obrigatório tem `Origin`; ausência vira `MissingData`, não zero.
