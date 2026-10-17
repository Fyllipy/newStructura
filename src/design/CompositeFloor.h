#pragma once

#include "design/Connections.h"
#include "design/DesignSection.h"

#include <string>
#include <vector>

/// Vigas mistas de aco e concreto (ABNT NBR 8800:2024, Anexo L) e lajes mistas com forma de aco incorporada (steel deck,
/// Anexo N). Unidades: mm, N, N*mm, MPa. As verificacoes reutilizam `Connections::Check` (razao demanda/resistencia e
/// regras de disposicao com razao 0 enquanto atendidas) e devolvem o memorial de calculo.
namespace Structura::Design::Composite {

using Check = Structura::Design::Connections::Check;

struct ConcreteInput {
    double fck {25.0};          ///< MPa
    double density {2400.0};    ///< kg/m3 (massa especifica; concreto de densidade normal)
    double gammaC {1.4};
    [[nodiscard]] double ecs() const;      ///< modulo secante (NBR 6118: 0,85 * 5600 sqrt(fck) para ate C50)
    [[nodiscard]] double etaC() const;     ///< (40/fck)^(1/3) <= 1 (L.1.3.4)
    [[nodiscard]] double fcd() const { return fck / gammaC; }
    [[nodiscard]] double ctkInf() const;   ///< 0,21 fck^(2/3)
};

enum class SlabKind { Solid, DeckPerpendicular, DeckParallel };

struct SlabInput {
    SlabKind kind {SlabKind::Solid};
    double tc {120.0};      ///< espessura de concreto acima da forma (ou total, laje macica)
    double hF {0.0};        ///< altura da nervura da forma (0 = laje macica)
    double bF {0.0};        ///< largura media da nervura (mesa/nervura sobre o perfil)
};

struct StudInput {
    double diameter {19.0};
    double height {0.0};          ///< comprimento apos a soldagem (0 = usa o minimo da norma para a laje)
    double fu {415.0};            ///< MPa
    int perRib {1};               ///< conectores por nervura (forma perpendicular) ou por secao
    double emh {60.0};            ///< distancia da borda do conector a alma da nervura (L.4.2.1.3)
    bool throughDeck {true};      ///< soldado atraves da forma (senao, direto no perfil)
    int rows {1};                 ///< linhas de conectores na mesa do perfil
    double spacing {200.0};       ///< espacamento longitudinal (mm)
    double gammaCs {1.25};
    [[nodiscard]] double area() const;
};

struct SteelIBeam {
    double d {310.0}, bf {165.0}, tf {9.7}, tw {5.8};   ///< mm (secao duplamente simetrica)
    double area {0.0};       ///< 0 = calcula pelas dimensoes (permite informar a area real com raios de concordancia)
    double inertia {0.0};    ///< idem, momento de inercia Ix
    double zpl {0.0};        ///< idem, modulo plastico
    double fy {345.0}, fu {450.0}, E {200000.0}, gammaA1 {1.10};
    [[nodiscard]] double hw() const { return d - 2.0 * tf; }
    [[nodiscard]] double aa() const;
    [[nodiscard]] double ia() const;
    [[nodiscard]] double wa() const { return 2.0 * ia() / d; }
    [[nodiscard]] double fyd() const { return fy / gammaA1; }
    [[nodiscard]] double webSlenderness() const { return hw() / tw; }
};

struct BeamInput {
    SteelIBeam steel;
    ConcreteInput concrete;
    SlabInput slab;
    StudInput studs;
    double span {8000.0};           ///< mm, entre apoios (viga biapoiada)
    double leftSpacing {3000.0};    ///< mm, distancia ao eixo da viga adjacente de cada lado (0 = sem laje desse lado)
    double rightSpacing {3000.0};
    bool shored {false};            ///< construcao escorada
    double lbConstruction {0.0};    ///< mm: comprimento destravado na fase de construcao (0 = mesa travada pela forma)
    double betaVm {1.0};
};

/// Solicitacoes e cargas lineares (N e mm; cargas em N/mm).
struct BeamDemand {
    double msd {0.0};          ///< N*mm, momento positivo maximo de calculo (viga mista)
    double vsd {0.0};          ///< N, cortante maximo de calculo
    double mGaSd {0.0};        ///< N*mm, momento de calculo na fase de construcao (so componente de aco)
    double mGaS {0.0};         ///< N*mm, servico, antes do concreto atingir 0,75 fck
    double mLS {0.0};          ///< N*mm, servico, apos
    // flechas (cargas uniformes de servico)
    double qBefore {0.0};      ///< N/mm: peso proprio do aco + concreto fresco + construcao (carregam so o perfil)
    double qPermanent {0.0};   ///< N/mm: permanentes aplicadas apos o endurecimento
    double qVariable {0.0};    ///< N/mm: sobrecarga
    double psi2 {0.3};         ///< fracao quase permanente da sobrecarga (efeitos de longa duracao)
    double camber {0.0};       ///< mm: contraflecha
    double deflectionLimit {350.0};   ///< L/n
};

struct BeamResult {
    bool ok {false};
    std::vector<Check> checks;
    std::vector<std::string> notes;
    // propriedades principais
    double bEff {0.0}, ecs {0.0}, etaC {1.0}, fcd {0.0}, fyd {0.0}, alphaE {0.0};
    double qRd {0.0}, rg {1.0}, rp {1.0};
    double sumQ {0.0}, fhd {0.0}, interaction {0.0}, interactionMin {0.0};
    double mRd {0.0}, mRdFull {0.0}, vRd {0.0}, mRdSteel {0.0};
    double itr {0.0}, iEf {0.0}, wEf {0.0};
    double deflection {0.0}, deflectionLimitMm {0.0};
    int connectorsHalfSpan {0}, connectorsFullInteraction {0};
    double maxRatio {0.0};
    std::string governing;
};

/// Verifica a viga mista biapoiada. `section` (opcional) permite usar as funcoes da NBR 8800 (secao 5) para o
/// momento resistente do perfil isolado e o cortante resistente; sem ela usam-se as dimensoes parametricas.
[[nodiscard]] BeamResult checkBeam(const BeamInput &input, const BeamDemand &demand, const DesignSection *section = nullptr);

/// Menor espacamento (multiplo de 25 mm entre o minimo e o maximo da norma) que atende o grau minimo de interacao;
/// 0 se nenhum atende.
[[nodiscard]] double proposeSpacing(const BeamInput &input, const BeamDemand &demand, const DesignSection *section = nullptr);

/// Largura efetiva (L.2.2.1.1): cada lado = menor entre L/8 e metade da distancia a viga vizinha.
[[nodiscard]] double effectiveWidth(double span, double leftSpacing, double rightSpacing);

/// Forca resistente de calculo de um pino com cabeca (L.4.2.1.1) e coeficientes Rg, Rp (L.4.2.1.2/3).
struct StudResistance {
    double qRd {0.0};
    double concreteCrushing {0.0};   ///< 0,5 Acs sqrt(fck Ec)/gamma
    double studRupture {0.0};        ///< Rg Rp Acs fu/gamma
    double rg {1.0}, rp {0.75};
};
[[nodiscard]] StudResistance studResistance(const StudInput &studs, const SlabInput &slab, const ConcreteInput &concrete);

// ── Laje mista (steel deck) ──────────────────────────────────────────────────
struct DeckInput {
    // geometria da laje (mm)
    double hF {75.0}, tc {70.0}, bn {150.0}, bb {110.0};   ///< nervura: altura, concreto sobre a forma, largura entre nervuras, largura media da nervura
    double ht() const { return hF + tc; }
    // forma de aco (por 1000 mm de largura)
    double tF {0.80}, fyF {345.0};
    double afEf {1100.0};         ///< A_F,ef: area efetiva (mm2/m)
    double ie {400000.0};         ///< I efetivo da forma (mm4/m)
    double wEfBottom {13000.0};   ///< modulo resistente efetivo (mm3/m)
    double wPlastic {15000.0};    ///< modulo plastico efetivo (mm3/m)
    double e {40.0};              ///< distancia do CG da secao efetiva a face inferior
    double ep {40.0};             ///< distancia da LNP da secao efetiva a face inferior
    double selfWeight {0.10};     ///< kN/m2 (peso proprio da forma)
    // resistencia ao cisalhamento longitudinal (ensaios): V = b dF/gamma [ m A/(b Ls) + k ]
    double mFactor {150.0}, kFactor {0.20}, gammaSl {1.25};
    double vvF {0.0};             ///< V_v,F,Rd da forma (N/m); 0 = despreza
    ConcreteInput concrete;
    double gammaA1 {1.10};
    double spanF {2500.0};        ///< vao teorico L_F (mm)
    int continuity {1};           ///< 1 = vao simples, 2 = dois vaos, 3 = tres ou mais vaos
    double steelModulus {200000.0};
};

struct DeckLoads {
    double permanent {1.5};       ///< kN/m2 apos a cura (revestimento, instalacoes) — peso proprio da laje e calculado
    double variable {3.0};        ///< kN/m2
    double construction {1.0};    ///< kN/m2 (N.4.1.3-a)
    double gammaG {1.4}, gammaQ {1.5};
    double psi2 {0.3};
};

struct DeckResult {
    bool ok {false};
    std::vector<Check> checks;
    std::vector<std::string> notes;
    double slabWeight {0.0};      ///< kN/m2 (concreto fresco)
    double mInitial {0.0};
    double mRdPositive {0.0};
    double vlRd {0.0}, vvRd {0.0};
    double inertiaEffective {0.0};
    double maxRatio {0.0};
    std::string governing;
};

[[nodiscard]] DeckResult checkDeck(const DeckInput &input, const DeckLoads &loads);

} // namespace Structura::Design::Composite
