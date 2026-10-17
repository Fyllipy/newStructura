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
    bool rangeRule {false};   ///< faixa de validade/disposicao: razao 0 enquanto atendida, > 1 se violada (demand = valor, resistance = limite)
    bool rangeLower {false};  ///< rangeRule com limite inferior (valor >= limite); senao limite superior (valor <= limite)
    std::vector<std::string> lines;   ///< memoria: formulas e valores substituidos
    double ratio() const
    {
        if (rangeRule) {
            if (rangeLower) {
                return demand + 1e-9 >= resistance ? 0.0 : (demand > 0.0 ? resistance / demand : 1e9);
            }
            return demand <= resistance + 1e-9 ? 0.0 : (resistance > 0.0 ? demand / resistance : 1e9);
        }
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
    double grip {0.0};              ///< comprimento de pega (mm); 0 = nao informado. Acima de 5 db reduz Fv,Rd (6.3.7)
    bool pretensioned {false};      ///< parafuso de alta resistencia com protensao inicial: a pega longa nao reduz (6.3.7)
    double connectionLength {0.0};  ///< emenda de barra tracionada por contato: comprimento na direcao da forca (mm); > 950 majora Fv,Sd e Fc,Sd em 1,20 (6.3.8)
    // esforcos de calculo (N, N.mm)
    double shearX {0.0};
    double shearY {0.0};
    double momentZ {0.0};           ///< momento no plano do grupo, em torno do centroide
    double tension {0.0};           ///< tracao total, dividida igualmente (efeito alavanca nao considerado)
    double gammaA2 {1.35};
};

std::vector<Check> checkBolted(const BoltedInput &input);

/// Efeito de alavanca (NBR 8800:2024, 6.3.5): espessura da placa tracionada por parafusos (placa rigida, 6.3.5.3, e flexivel, 6.3.5.4).
struct PryingInput {
    double db {20.0};
    double tensionForce {0.0};      ///< Ft,0,Sd: tracao de calculo por parafuso, sem alavanca (N)
    double boltResistance {0.0};    ///< Ft,Rd: resistencia de calculo do parafuso a tracao (N)
    double a {35.0};                ///< distancia do eixo do parafuso a borda livre da placa (mm); limitada a 1,25 b
    double b {35.0};                ///< distancia do eixo do parafuso a face da alma/aba que traciona a placa (mm)
    double p {70.0};                ///< largura tributaria do parafuso (mm)
    double holeWidth {22.0};        ///< df: dimensao do furo na direcao da largura tributaria (mm)
    double thickness {10.0};        ///< t da placa (mm)
    double fu {400.0};              ///< MPa
    double gammaA1 {1.10};
};

std::vector<Check> checkPrying(const PryingInput &input);

/// Ligacao por atrito com parafusos de alta resistencia (NBR 8800:2024, 6.3.4). Furos-padrao: deslizamento e estado-limite de
/// servico (6.3.4.4); furos alargados/alongados: estado-limite ultimo (6.3.4.3).
struct SlipInput {
    BoltGrade grade;
    double db {20.0};
    int bolts {1};                  ///< parafusos na ligacao
    int planes {1};                 ///< planos de deslizamento por parafuso
    double shear {0.0};             ///< cortante total de calculo (N), dividido igualmente
    double tension {0.0};           ///< tracao total de calculo que reduz a protensao (N)
    int surfaceClass {0};           ///< 0 = classe A (0,30), 1 = classe B (0,50), 2 = galvanizada (0,20)
    bool fillers {false};           ///< duas ou mais chapas de enchimento (Ce = 0,85)
    bool oversized {false};         ///< furos alargados/alongados: verificacao no ELU
};

/// Forca de protensao minima F_Tb (kN, Tabela 19) de parafusos ASTM F3125 A325/A490; 0 quando a norma nao a fornece.
double minimumPretension(const BoltGrade &grade, double boltDiameter);

std::vector<Check> checkSlip(const SlipInput &input);

struct WeldInput {
    double leg {6.0};               ///< perna do filete (mm)
    double length {200.0};          ///< comprimento efetivo total dos filetes (mm)
    double fw {485.0};              ///< resistencia do metal da solda (MPa), Tabela A.4
    double angleDeg {0.0};          ///< angulo entre a resultante e o eixo do grupo (0 = paralela)
    bool useAngleFactor {true};     ///< 6.2.5.2-a (1 + 0,5 sen^1,5 theta)
    double lengthTransverse {0.0};  ///< parte de `length` transversal a forca; > 0 e < length: grupo com filetes longitudinais e transversais (6.2.5.2-c)
    double plateThickness {10.0};   ///< espessura da parte ligada mais fina (mm); define a perna minima (Tabela 11)
    double plateFy {250.0};
    double plateFu {400.0};
    double force {0.0};             ///< resultante de calculo (N)
    double gammaW2 {1.35};
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkFilletWeld(const WeldInput &input);

/// Grupo de filetes carregado concentricamente com elementos longitudinais e transversais (6.2.5.2-c):
/// o maior entre Fwl + Fwt e 0,85 Fwl + 1,5 Fwt (resistencias da Tabela 9 de cada parcela, N).
double weldCombinedResistance(double longitudinal, double transverse);

/// Grupo de filetes no plano sob carga excentrica, pelo metodo do centro instantaneo de rotacao (6.2.5.2-b).
struct WeldSegment {
    double x1 {0.0}, y1 {0.0}, x2 {0.0}, y2 {0.0};   ///< extremos do filete no plano (mm)
};
struct IcrInput {
    std::vector<WeldSegment> segments;
    double leg {6.0};               ///< perna do filete (mm); a garganta efetiva e 0,707 a
    double fw {485.0};              ///< MPa
    double loadX {0.0}, loadY {0.0};///< ponto de aplicacao da carga no plano (mm)
    double loadAngleDeg {0.0};      ///< direcao da carga em relacao ao eixo x (graus)
    double gammaW2 {1.35};
};
struct IcrResult {
    bool converged {false};
    double forceRd {0.0};           ///< carga resistente de calculo (N)
    double icrX {0.0}, icrY {0.0};  ///< centro instantaneo de rotacao (mm)
};
IcrResult weldGroupIcr(const IcrInput &input);

} // namespace Structura::Design::Connections
