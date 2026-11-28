#pragma once

// Classificacao da rigidez rotacional de ligacoes (ABNT NBR 8800:2024, item 6.1.2): nucleo de calculo sem dependencia de interface.
//
// Unidades: rotacao em rad, momento em kN.m, rigidez em kN.m/rad, E em kN/m2, I em m4, comprimentos em m.
//
// Nada aqui inventa curvas: a rigidez Si sai de uma curva momento-rotacao informada (importada, experimental ou de analise numerica) ou
// de dados informados pelo usuario (Si, Mj,Rd, rotacao a 2/3 de Mj,Rd). O metodo dos componentes (EN 1993-1-8) NAO esta implementado:
// `JointBehaviorProvider` e o ponto de integracao previsto para ele.

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Structura::Design::JointClass {

struct CurvePoint {
    double theta {0.0};   ///< rotacao relativa da ligacao (rad), >= 0 (modulo)
    double moment {0.0};  ///< momento (kN.m), >= 0 (modulo)
};

/// Como o comportamento da ligacao foi obtido (rastreabilidade).
enum class DataOrigin {
    None,                  ///< nada informado
    ImportedCurve,         ///< curva momento-rotacao importada de arquivo
    ExperimentalCurve,     ///< curva experimental digitada/importada
    NumericalCurve,        ///< curva de analise numerica (MEF ou outro programa)
    UserStiffness,         ///< Si (e Mj,Rd) obtidos por procedimento independente
    ComponentMethod,       ///< metodo dos componentes (reservado: nao implementado)
    ModelSpring,           ///< mola rotacional de extremidade do modelo (so a rigidez)
    PreliminaryModel       ///< configuracao de referencia (nao dimensionada), hipoteses declaradas
};
[[nodiscard]] std::string originName(DataOrigin origin);

/// Comportamento de uma ligacao em um sentido do momento (positivo ou negativo): valores em modulo.
struct DirectionBehavior {
    std::vector<CurvePoint> curve;                ///< pontos crescentes em theta (a origem e implicita se faltar)
    std::optional<double> resistantMoment;        ///< Mj,Rd (kN.m)
    std::optional<double> theta23;                ///< rotacao (rad) a 2/3 de Mj,Rd informada sem curva completa
    std::optional<double> stiffness;              ///< Si (kN.m/rad) informada diretamente
    [[nodiscard]] bool hasData() const { return !curve.empty() || resistantMoment || theta23 || stiffness; }
};

struct JointBehavior {
    DataOrigin origin {DataOrigin::None};
    std::string description;       ///< fonte (arquivo, ensaio, programa)
    DirectionBehavior positive;    ///< momento positivo
    DirectionBehavior negative;    ///< momento negativo (nao se presume simetria)
};

enum class Direction { Positive, Negative };

/// Rigidez inicial no nivel de 2/3 de Mj,Rd: Si = (2/3 Mj,Rd) / theta(2/3 Mj,Rd).
struct StiffnessResult {
    bool valid {false};
    double stiffness {0.0};         ///< Si (kN.m/rad)
    double momentRd {0.0};          ///< Mj,Rd (kN.m)
    double moment23 {0.0};          ///< 2/3 Mj,Rd
    double theta23 {0.0};           ///< rotacao correspondente (rad)
    bool curveDriven {false};       ///< theta23 lido da curva (e nao informado ou derivado de Si)
    bool thetaDerived {false};      ///< theta23 derivado de Si informada (M23/Si)
    bool momentFromCurveMax {false};
    std::vector<std::string> messages;   ///< o que falta ou foi adotado
};

/// Calcula Si de um sentido. `momentRd` ausente nao e suprido pelo maximo da curva: o dado e exigido (mensagem em `messages`).
[[nodiscard]] StiffnessResult computeStiffness(const DirectionBehavior &direction);

/// Tipo de configuracao dos dois elementos selecionados no no.
enum class JointKind { BeamColumn, BeamBeam, ColumnColumn, DiagonalColumn, DiagonalBeam, Other };
[[nodiscard]] std::string kindName(JointKind kind);
enum class BarRole { Beam, Column, Diagonal };
[[nodiscard]] std::string roleName(BarRole role);

/// Papel de uma barra pela inclinacao do eixo (unitario, saindo do no) em relacao a horizontal: <= 10 graus viga, >= 80 graus pilar.
[[nodiscard]] BarRole roleOfDirection(const std::array<double, 3> &unitDirection);
[[nodiscard]] JointKind kindOf(BarRole a, BarRole b);

/// Barra de um pavimento para Kv/Kp.
struct StoreyBar {
    int memberId {0};
    double inertia {0.0};   ///< I no plano da estrutura (m4)
    double length {0.0};    ///< L (m)
};

enum class StoreyStatus { Ok, Pending };
struct StoreyStiffness {
    StoreyStatus status {StoreyStatus::Pending};
    double kv {0.0};         ///< media de Iv/Lv das vigas (m3)
    double kp {0.0};         ///< media de Ip/Lp dos pilares (m3)
    double ratio {0.0};      ///< Kv/Kp
    int beams {0};
    int columns {0};
    std::string reason;      ///< por que esta pendente
};
[[nodiscard]] StoreyStiffness storeyStiffness(const std::vector<StoreyBar> &beams, const std::vector<StoreyBar> &columns);

enum class StiffnessClass { Pinned, SemiRigid, Rigid, NotClassified };
[[nodiscard]] std::string className(StiffnessClass c);

/// Limites e classificacao para analise elastica (6.1.2): rotulada Si <= 0,5 EIv/Lv; rigida Si >= 25 EIv/Lv e Kv/Kp >= 0,1.
struct Limits {
    double referenceStiffness {0.0};   ///< E Iv / Lv (kN.m/rad)
    double sMin {0.0};                 ///< 0,5 E Iv/Lv
    double sMax {0.0};                 ///< 25 E Iv/Lv
    static constexpr double kPinnedFactor = 0.5;
    static constexpr double kRigidFactor = 25.0;
    static constexpr double kMinStoreyRatio = 0.1;
};
[[nodiscard]] Limits limitsFor(double youngModulus, double beamInertia, double beamLength);

struct Classification {
    bool applicable {false};               ///< criterio de 6.1.2 vale para a configuracao (viga-pilar)
    bool computed {false};                 ///< ha Si para classificar
    double stiffness {0.0};
    double eta {0.0};                      ///< Si / (E Iv/Lv)
    StiffnessClass byStiffness {StiffnessClass::NotClassified};   ///< so pela rigidez
    StiffnessClass final {StiffnessClass::NotClassified};         ///< com a condicao Kv/Kp
    bool reachedRigidLimit {false};
    bool storeyConditionNeeded {false};
    bool storeyConditionMet {false};
    bool storeyPending {false};
    std::string justification;
};

[[nodiscard]] Classification classify(double stiffness, const Limits &limits, const StoreyStiffness &storey, JointKind kind);

/// Ponto de integracao futuro (metodo dos componentes ou modulo de dimensionamento da ligacao).
class JointBehaviorProvider
{
public:
    virtual ~JointBehaviorProvider() = default;
    [[nodiscard]] virtual std::optional<JointBehavior> behavior(int nodeId, int memberA, int memberB) const = 0;
};

/// Interpola theta em M (primeiro cruzamento) em curva monotona por trechos; false se a curva nao alcanca M.
[[nodiscard]] bool thetaAtMoment(const std::vector<CurvePoint> &curve, double moment, double &theta);

/// Resultado completo de um sentido do momento.
struct DirectionAnalysis {
    DirectionBehavior behavior;
    StiffnessResult stiffness;
    Classification classification;
    bool hasData {false};
};

struct JointAnalysis {
    JointBehavior behavior;
    DirectionAnalysis positive;
    DirectionAnalysis negative;
    Limits limits;
    StoreyStiffness storey;
    JointKind kind {JointKind::Other};
    /// Sentido adotado no resumo: o positivo se tem Si; senao o negativo.
    [[nodiscard]] const DirectionAnalysis *primary() const;
};

/// Calcula Si e a classificacao de cada sentido com dados. Um sentido sem dados que tem o outro informado fica sem resultado (nao se presume simetria).
[[nodiscard]] JointAnalysis analyzeJoint(const JointBehavior &behavior, const Limits &limits, const StoreyStiffness &storey, JointKind kind);

/// Rotula bilinear equivalente a um sentido da ligacao (qualquer origem com curva): rigidez elastica Si ate 2/3 Mj,Rd e secante entre (theta 2/3, 2/3 Mj,Rd) e
/// (theta em Mj,Rd, Mj,Rd) depois. Falha (false) se nao ha Si, Mj,Rd e curva que alcance Mj,Rd.
[[nodiscard]] bool bilinearEquivalent(const DirectionAnalysis &direction, double &stiffness, double &yieldMoment, double &hardeningRatio);

/// Natureza do dado de entrada (para a interface: calculado, experimental, informado...).
[[nodiscard]] std::string originNature(DataOrigin origin);

/// Dados de contexto do no para a memoria de calculo (textos ja resolvidos pela camada de aplicacao).
struct MemoryContext {
    int nodeId {0};
    int memberA {0}, memberB {0};
    std::string planeNote;
    double youngModulus {0.0};   ///< kN/m2
    double beamInertia {0.0};    ///< m4
    double beamLength {0.0};     ///< m
    int referenceMember {0};
    std::vector<std::string> notes;
    std::vector<std::string> componentLines;   ///< memoria do metodo dos componentes (rigidez e Mj,Rd), se a curva veio dele
};

/// Memoria de calculo em texto (UTF-8): dados, Si, limites, Kv/Kp, resultado e referencias.
[[nodiscard]] std::string buildMemory(const JointAnalysis &analysis, const MemoryContext &context);

/// Referencias normativas e hipoteses fixas da ferramenta.
[[nodiscard]] std::string referencesText();

/// Le uma curva momento-rotacao de texto (linhas "theta;momento" ou "theta momento", separador ; , tab ou espaco; virgula decimal aceita;
/// linhas iniciadas por # ignoradas). `thetaUnit`: 1 = rad, 1e-3 = mrad; `momentUnit`: kN.m por unidade (1 = kN.m, 1e-3 = N.m, 0.01 = kN.cm).
/// Valores negativos vao para `negative` (modulo); devolve mensagem de erro vazia se ok.
[[nodiscard]] std::string parseCurveText(const std::string &text, double thetaUnit, double momentUnit, std::vector<CurvePoint> &positive, std::vector<CurvePoint> &negative);

} // namespace Structura::Design::JointClass
