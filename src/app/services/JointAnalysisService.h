#pragma once

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"
#include "design/JointClassification.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Structura::App::Services {

/// Extrai do modelo (barras, nos, secoes e materiais ja em unidades do solver: comprimentos em m) o contexto de uma ligacao entre duas barras
/// que compartilham um no: papel de cada barra, plano de analise, Iv/Lv, E e a rigidez relativa do pavimento (Kv/Kp). Nao altera o solver.
class JointAnalysisService
{
public:
    using Vec3 = std::array<double, 3>;

    struct Bar {
        int memberId {0};
        int otherNode {0};
        bool nodeIsStart {true};          ///< o no analisado e a extremidade I da barra
        Vec3 direction {{0, 0, 0}};       ///< unitario, saindo do no
        double length {0.0};              ///< m
        Structura::Design::JointClass::BarRole role {Structura::Design::JointClass::BarRole::Beam};
        double inertia {0.0};             ///< momento de inercia em relacao ao eixo normal ao plano de analise (m4)
        double youngModulus {0.0};        ///< kN/m2
        bool selected {false};
        double rotationAlignment {1.0};   ///< |cos| entre a normal do plano e o eixo local de flexao escolhido (1 = alinhado a um eixo principal)
        bool hasEccentricity {false};     ///< offset de eixo ou zona rigida na extremidade do no
        int rotationDof {5};              ///< grau de liberdade local (0..11) da rotacao em torno do eixo normal ao plano, na extremidade do no
        bool rotationalRelease {false};   ///< liberacao de rotacao (em torno do eixo normal ao plano) no no
        std::optional<double> springKnm;  ///< mola rotacional de extremidade do modelo (kN.m/rad), em torno do eixo normal ao plano
    };

    struct PairInfo {
        int memberA {0};
        int memberB {0};
        Structura::Design::JointClass::JointKind kind {Structura::Design::JointClass::JointKind::Other};
        double angleDeg {0.0};
        bool selected {false};
    };

    struct Context {
        bool valid {false};
        std::string error;
        int nodeId {0};
        Vec3 position {{0, 0, 0}};
        std::array<int, 2> selectedMembers {{0, 0}};
        Structura::Design::JointClass::JointKind kind {Structura::Design::JointClass::JointKind::Other};
        Vec3 planeNormal {{0, 1, 0}};
        bool planeFromSelection {true};     ///< false = barras colineares: plano adotado (hipotese)
        std::vector<Bar> bars;              ///< todas as barras que chegam ao no (as selecionadas marcadas)
        std::vector<PairInfo> pairs;        ///< combinacoes duas a duas (cada uma e uma ligacao distinta)
        int referenceBar {-1};              ///< indice em `bars` da viga de referencia (Iv, Lv, E)
        double beamInertia {0.0};
        double beamLength {0.0};
        double youngModulus {0.0};          ///< kN/m2
        Structura::Design::JointClass::Limits limits;
        Structura::Design::JointClass::StoreyStiffness storey;
        std::vector<Structura::Design::JointClass::StoreyBar> storeyBeams;
        std::vector<Structura::Design::JointClass::StoreyBar> storeyColumns;
        std::vector<std::string> notes;     ///< hipoteses e limitacoes desta extracao
    };

    /// Nos compartilhados pelas duas barras (ids do solver).
    [[nodiscard]] static std::vector<int> sharedNodes(const Structura::App::SolverBridge::ModelData &model, int memberA, int memberB);

    /// `forceToKn`: kN por unidade de forca do modelo. A validade depende de ambos os membros chegarem ao no.
    [[nodiscard]] static Context analyze(const Structura::App::SolverBridge::ModelData &model, int nodeId, int memberA, int memberB, double forceToKn);

    /// Idealizacao da ligacao na analise global (decisao de modelagem, posterior a analise da ligacao): grava na ligacao de extremidade da barra o grau de
    /// liberdade rotacional `dof` (0..11): kind 0 = rotulada (k = 0), 1 = semirrigida (mola de `siKnm` kN.m/rad), 2 = rigida (continua, k < 0).
    /// Nao toca nos resultados da analise mecanica da ligacao.
    /// kind 3 = rotula bilinear (nao linear): mantem o no continuo e grava uma rotula plastica de rigidez `siKnm`, escoamento `yieldKnm` e razao de endurecimento `hardening`.
    static void idealize(Structura::App::ProjectData::MemberConnectionSpec &spec, int dof, int kind, double siKnm, double yieldKnm = 0.0, double hardening = 0.0);

    /// Comportamento da ligacao a partir da propria modelagem (liberacao ou mola rotacional de extremidade da viga de referencia); vazio se a
    /// viga e continua no modelo (continuidade e hipotese, nao propriedade da ligacao).
    [[nodiscard]] static std::optional<Structura::Design::JointClass::JointBehavior> behaviorFromModel(const Context &context);
};

} // namespace Structura::App::Services
