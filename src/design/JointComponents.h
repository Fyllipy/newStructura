#pragma once

// MODELO FISICO DA LIGACAO pelo metodo dos componentes (EN 1993-1-8:2005, 6.2 resistencia e 6.3 rigidez).
//
// O mesmo conjunto de dados (geometria, acos, parafusos) alimenta, em uma unica passada: a rigidez de cada componente (ki), a resistencia de
// cada componente (F,Rd), a distribuicao de forcas por fileira (6.2.7.2), o momento resistente Mj,Rd, a rigidez inicial Sj,ini e a curva
// momento-rotacao (6.3.1). Nada aqui depende de interface nem do solver global.
//
// O MODELO ESTRUTURAL EQUIVALENTE (liberacao, mola rotacional, rotula bilinear) e DERIVADO do modelo fisico por `deriveEquivalent`: a idealizacao
// nunca altera o modelo fisico nem os resultados mecanicos.
//
// Implementado: (1) mesas da viga soldadas ao pilar (6.2.7.1, Fig. 6.1); (2) chapa de extremidade parafusada com fileiras tracionadas (6.2.7.2).
// Unidades: N, mm, MPa. Os coeficientes ki tem unidade de comprimento (mm).
// Validado contra exemplos publicados (ver docs/AUDITORIA_LIGACOES.md e TestJointClassification).

#include "design/JointClassification.h"

#include <string>
#include <vector>

namespace Structura::Design::JointComponents {

constexpr double kPsiWeldedAndEndPlate = 2.7;   ///< EN 1993-1-8, Tabela 6.8 (soldada e chapa de extremidade parafusada)

/// Fileira de parafusos tracionada (2 parafusos), ja com os comprimentos efetivos dos T-stubs da mesa do pilar e da chapa de extremidade.
struct BoltRow {
    int row {1};                   ///< 1 = fileira mais afastada do centro de compressao
    double h {0.0};                ///< do centro de compressao ao eixo da fileira (mm)
    // mesa do pilar (T-stub, Tabela 6.4/6.5)
    double mColumn {0.0};
    double eColumn {0.0};
    double leffColumnCircular {0.0};
    double leffColumnNonCircular {0.0};
    // chapa de extremidade (T-stub, Tabela 6.6)
    double mPlate {0.0};
    double ePlate {0.0};
    double leffPlateCircular {0.0};
    double leffPlateNonCircular {0.0};
    [[nodiscard]] double leffColumn() const;   ///< menor dos comprimentos efetivos da fileira isolada (rigidez: k3 e k4)
    [[nodiscard]] double leffPlate() const;    ///< idem para a chapa (k5)
};

struct ModelInput {
    enum Kind { WeldedFlanges = 0, ExtendedEndPlate = 1 };
    int kind {WeldedFlanges};
    double youngModulus {210000.0};   ///< MPa

    // viga (perfil I/H)
    double beamDepth {300.0};
    double beamBf {150.0};
    double beamTf {10.7};
    double beamTw {7.1};
    double beamFy {250.0};
    double beamPlasticModulus {0.0};  ///< Wpl (mm3); 0 = aproximacao sem raios de concordancia

    // pilar (perfil I/H laminado)
    double colDepth {260.0};
    double colBf {146.0};
    double colTf {12.7};
    double colTw {7.6};
    double colRadius {10.0};
    double colFy {250.0};
    double colArea {0.0};             ///< A do pilar (mm2); 0 = calculada das dimensoes (com raios de concordancia)

    // mesas soldadas ao pilar / solda mesa-chapa
    double flangeThroat {0.0};        ///< garganta ap do filete da mesa (mm); 0 = solda de penetracao total (conservador)

    // chapa de extremidade
    double plateWidth {160.0};
    double plateThickness {19.0};
    double plateFy {250.0};
    double weldLeg {8.0};             ///< perna do filete mesa-chapa (mm): m da chapa desconta 0,8 x perna
    double gauge {90.0};              ///< w: entre as colunas de parafusos (mm)
    int tensionRows {2};              ///< fileiras (de 2 parafusos) na extensao tracionada da chapa
    double firstRow {40.0};           ///< face externa da mesa da viga ao eixo da 1a fileira (mm)
    double pitch {70.0};              ///< entre fileiras (mm)
    double edgeVertical {35.0};       ///< ultima fileira a borda da chapa (mm)
    double plateBelowFlange {200.0};  ///< comprimento da chapa abaixo da mesa comprimida (mm): limita a dispersao sp (6.2.6.2)
    /// INCREMENTO 5: EMENDA DE VIGA com duas chapas de extremidade parafusadas (EN 1993-1-8, 6.2.7.2(10): resistencia pelo mesmo metodo "omitindo os itens relativos ao pilar";
    /// Tabela 6.10 e 6.3.3.1(5): keq baseado em k5 de CADA chapa e em k10). Exige kind = ExtendedEndPlate; os dados do pilar e beta sao IGNORADOS (nao ha pilar) e as duas
    /// chapas sao iguais (mesma espessura e aco). Falso (padrao) = ligacao viga-pilar, comportamento anterior inalterado.
    bool beamSplice {false};
    std::vector<BoltRow> rows;        ///< se nao vazio, substitui as fileiras derivadas da geometria acima (validacao e tipologias futuras)

    // parafusos
    double boltDiameter {20.0};
    double boltStressArea {245.0};    ///< As (mm2)
    double boltTensionResistance {0.0};   ///< Ft,Rd de UM parafuso (N); 0 = 0,9 fub As / 1,25 com fub = 800 MPa
    double washerThickness {4.0};     ///< mm, sob a cabeca e sob a porca
    double boltHeadHeight {0.0};      ///< 0 = 0,7 db
    double boltNutHeight {0.0};       ///< 0 = 0,8 db

    // no
    double beta {1.0};                ///< parametro de transformacao (EN 1993-1-8, 5.3(7)): 1 = viga de um lado; 0 = momentos iguais dos dois lados; 2 = opostos
    bool webStiffened {false};        ///< enrijecedores horizontais alinhados com as mesas da viga: k2, k3 e Fc,wc, Ft,wc deixam de limitar
    double doublerThickness {0.0};    ///< chapa de reforco da alma do painel (mm)
    double gammaYield {1.10};         ///< gamma_M0 / gamma_a1 (NBR 8800: 1,10)
    double columnAxialStressRatio {0.0};   ///< sigma_com,Ed / fy,wc do pilar: > 0,7 reduz Fc,wc (kwc, 6.2.6.2(2))
};

/// Um coeficiente de rigidez ki (mm) e a resistencia do componente, com a memoria do calculo.
struct Component {
    std::string name;
    std::string symbol;               ///< "k1", "k4", ...
    std::string clause;               ///< item da EN 1993-1-8
    int row {0};                      ///< 0 = componente do no; r >= 1 = fileira de parafusos
    double k {0.0};                   ///< mm
    bool infinite {false};            ///< nao contribui para a flexibilidade (rigido)
    double resistance {-1.0};         ///< F,Rd (N); < 0 = componente sem resistencia propria no modelo
    std::string failureMode;          ///< modo de ruptura governante (T-stub)
    std::vector<std::string> lines;
};

struct RowResult {
    int row {0};
    double h {0.0};
    double keff {0.0};                ///< 1 / soma(1/ki) da fileira (mm)
    double leffColumn {0.0};
    double leffPlate {0.0};
    double individual {-1.0};         ///< Ft,r,Rd como fileira isolada (N)
    std::string governingComponent;
    double effective {0.0};           ///< Ftr,Rd apos a distribuicao (N)
    std::string limitedBy;            ///< "" | componente que reduziu a fileira
};

struct Resistance {
    bool ok {false};
    std::string error;
    double panelShear {0.0};          ///< Vwp,Rd / beta (N)
    double columnWebCompression {0.0};   ///< Fc,wc,Rd (N)
    double beamFlangeCompression {0.0};  ///< Fc,fb,Rd (N)
    double cap {0.0};                 ///< menor dos tres, limite do somatorio das forcas de tracao (N)
    std::string capComponent;
    std::vector<RowResult> rows;   ///< resistencia por fileira (chapa de extremidade)
    double momentResistance {0.0};    ///< Mj,Rd (N.mm)
    double momentResistanceKnm() const { return momentResistance * 1.0e-6; }
    std::vector<std::string> lines;
};

struct StiffnessResult {
    bool ok {false};
    std::string error;
    std::vector<Component> components;
    std::vector<RowResult> rows;
    double leverArm {0.0};            ///< z (mm)
    double kEquivalent {0.0};         ///< keq (mm)
    double initialStiffness {0.0};    ///< Sj,ini (N.mm/rad)
    [[nodiscard]] double initialStiffnessKnm() const { return initialStiffness * 1.0e-6; }   ///< kN.m/rad
    std::vector<std::string> lines;   ///< memoria completa
};

/// Rigidez inicial (EN 1993-1-8, 6.3): coeficientes ki, fileiras, keq, zeq e Sj,ini.
[[nodiscard]] StiffnessResult computeStiffness(const ModelInput &input);
/// Resistencia (EN 1993-1-8, 6.2): T-stubs, almas, painel, distribuicao por fileira (6.2.7.2) e Mj,Rd. Usa as MESMAS fileiras da rigidez.
[[nodiscard]] Resistance computeResistance(const ModelInput &input);
/// Fileiras derivadas da geometria da chapa (todas na extensao tracionada), a partir de `input`.
[[nodiscard]] std::vector<BoltRow> deriveExtensionRows(const ModelInput &input);

/// Metricas da curva momento-rotacao da EN (6.3.1) em kN.m e rad.
struct CurveMetrics {
    double initialStiffness {0.0};        ///< Sj,ini (kN.m/rad)
    double momentResistance {0.0};        ///< Mj,Rd (kN.m)
    double onsetOfNonlinearity {0.0};     ///< 2/3 Mj,Rd: inicio da perda de rigidez (kN.m)
    double rotationAtOnset {0.0};         ///< rad
    double rotationAtResistance {0.0};    ///< rad (fim do dominio de validade do modelo)
    double secantAtResistance {0.0};      ///< M/theta em Mj,Rd (kN.m/rad)
    double tangentAtResistance {0.0};     ///< dM/dtheta em Mj,Rd (kN.m/rad)
    double momentAtHalfStiffness {0.0};   ///< M em que a secante cai a Sj,ini/2 (kN.m)
    double rotationAtHalfStiffness {0.0}; ///< rad
};
[[nodiscard]] CurveMetrics curveMetrics(double sIniKnm, double mjRdKnm, double psi = kPsiWeldedAndEndPlate);
/// Rigidezes secante e tangente em um momento M (kN.m): secante = M/theta; tangente = dM/dtheta.
[[nodiscard]] double secantStiffnessAt(double m, double sIniKnm, double mjRdKnm, double psi = kPsiWeldedAndEndPlate);
[[nodiscard]] double tangentStiffnessAt(double m, double sIniKnm, double mjRdKnm, double psi = kPsiWeldedAndEndPlate);

/// Curva momento-rotacao da EN 1993-1-8 (6.3.1): theta = M/Sj,ini para M <= 2/3 Mj,Rd e theta = M/Sj,ini * (1,5 M/Mj,Rd)^psi acima,
/// ate Mj,Rd (capacidade de rotacao nao calculada). `sIniKnm` em kN.m/rad, `mjRdKnm` em kN.m.
[[nodiscard]] std::vector<Structura::Design::JointClass::CurvePoint> momentRotationCurve(double sIniKnm, double mjRdKnm, double psi = kPsiWeldedAndEndPlate, int points = 40);

/// MODELO ESTRUTURAL EQUIVALENTE derivado do modelo fisico (para o solver global). Nao altera o modelo fisico.
struct EquivalentModel {
    enum Kind { Pinned = 0, SemiRigidLinear = 1, Rigid = 2, SemiRigidBilinear = 3 };
    int kind {Rigid};
    double stiffness {0.0};       ///< kN.m/rad: mola linear (SemiRigidLinear) ou rigidez elastica da rotula (SemiRigidBilinear)
    double yieldMoment {0.0};     ///< kN.m: inicio da plastificacao da rotula bilinear (2/3 Mj,Rd)
    double hardeningRatio {0.0};  ///< rigidez pos-escoamento / elastica da rotula bilinear
    bool valid {false};
    std::string note;
};
/// Rotula bilinear equivalente a curva da EN: elastica Sj,ini ate 2/3 Mj,Rd e secante entre (theta23, 2/3 Mj,Rd) e (theta,Rd, Mj,Rd) depois.
[[nodiscard]] EquivalentModel deriveEquivalent(int kind, double sIniKnm, double mjRdKnm, double psi = kPsiWeldedAndEndPlate);

} // namespace Structura::Design::JointComponents
