#pragma once

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Parafuso (ABNT NBR 8800:2024, Tabela A.3). Unidades: MPa e mm.
struct BoltGrade {
    std::string name;
    double fyb {0.0};
    double fub {0.0};
    bool highStrength {false};   ///< plano de corte pode ficar fora da rosca (0,56) quando a ligacao o garantir
};

const std::vector<BoltGrade> &boltGrades();

/// Diametro do furo-padrao (Tabela 14) para o diametro do parafuso (mm).
double standardHoleDiameter(double boltDiameter);
/// Distancia minima do centro do furo-padrao a borda (Tabela 16), mm.
double minimumEdgeDistance(double boltDiameter);

struct Check {
    std::string name;
    std::string clause;
    double demand {0.0};      ///< solicitacao de calculo
    double resistance {0.0};  ///< resistencia de calculo
    std::string unit;         ///< "kN", "mm", ...
    bool minimumRule {false}; ///< true: regra de valor minimo (requisito: valor >= minimo); false: razao demanda/resistencia
    std::vector<std::string> lines;   ///< memoria: formulas e valores substituidos
    double ratio() const
    {
        if (minimumRule) {
            return demand > 0.0 ? resistance / demand : 0.0;   // resistance guarda o minimo exigido, demand o valor adotado
        }
        return resistance > 0.0 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
    }
    bool ok() const { return ratio() <= 1.0 + 1e-9; }
};

struct BoltedInput {
    BoltGrade grade;
    double db {20.0};               ///< mm
    bool threadInShearPlane {true}; ///< plano de corte passa pela rosca (0,45) ou nao (0,56, so alta resistencia)
    int shearPlanes {1};
    int nx {2};                     ///< parafusos ao longo de x do grupo
    int ny {1};
    double sx {70.0};               ///< espacamento em x (mm)
    double sy {70.0};
    double plateThickness {10.0};   ///< menor espessura da parte ligada que governa a pressao de contato (mm)
    double plateFu {400.0};         ///< MPa
    double endDistance {35.0};      ///< distancia do centro do furo extremo a borda, na direcao da forca (mm)
    double edgeDistance {35.0};     ///< distancia a borda transversal (mm)
    bool deformationLimit {true};   ///< deformacao no furo em servico e limitacao de projeto (1,2 / 2,4)
    // esforcos de calculo (N, N.mm)
    double shearX {0.0};
    double shearY {0.0};
    double momentZ {0.0};           ///< momento no plano do grupo, em torno do centroide
    double tension {0.0};           ///< tracao total, dividida igualmente (efeito alavanca nao considerado)
    double gammaA2 {1.35};
};

std::vector<Check> checkBolted(const BoltedInput &input);

struct WeldInput {
    double leg {6.0};               ///< perna do filete (mm)
    double length {200.0};          ///< comprimento efetivo total dos filetes (mm)
    double fw {485.0};              ///< resistencia do metal da solda (MPa), Tabela A.4
    double angleDeg {0.0};          ///< angulo entre a resultante e o eixo do grupo (0 = paralela)
    bool useAngleFactor {true};     ///< 6.2.5.2-a (1 + 0,5 sen^1,5 theta)
    double plateThickness {10.0};   ///< espessura da parte ligada mais fina (mm); define a perna minima (Tabela 11)
    double plateFy {250.0};
    double plateFu {400.0};
    double force {0.0};             ///< resultante de calculo (N)
    double gammaW2 {1.35};
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkFilletWeld(const WeldInput &input);

} // namespace Structura::Design::Connections
