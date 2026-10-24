#pragma once

#include "design/BarDesign.h"

#include <memory>
#include <string>
#include <vector>

namespace Structura::Design {

/// Pilar misto de aco e concreto (ABNT NBR 8800:2024, Anexo M, metodo simplificado): secoes totalmente revestidas (M.1-a), parcialmente revestidas (M.1-b)
/// e tubulares retangulares ou circulares preenchidas com concreto (M.1-c, M.1-d). Unidades: mm, N, MPa.
struct CompositeColumnSection {
    enum class Type { FullyEncased = 0, PartiallyEncased = 1, FilledRectangular = 2, FilledCircular = 3 };
    Type type {Type::FullyEncased};

    // perfil de aco I/H (revestidas)
    double d {0.0}, bf {0.0}, tw {0.0}, tf {0.0};
    // tubular retangular (b1 = altura, b2 = largura, na direcao x horizontal / y vertical da secao) ou circular (b1 = b2 = D)
    double b1 {0.0}, b2 {0.0}, t {0.0}, innerRadius {0.0};
    // concreto: dimensoes externas (revestidas); fck (MPa)
    double hc {0.0}, bc {0.0};
    double fck {30.0};
    double fsk {500.0};          ///< escoamento caracteristico da armadura (MPa)
    double alphaE {1.0};         ///< 1,2 basalto/diabasio; 1,0 granito/gnaisse; 0,9 calcario; 0,7 arenito (NBR 6118)

    struct Bar {
        double area {0.0};       ///< mm2
        double x {0.0};          ///< distancia ao eixo y (mm), + para a direita
        double y {0.0};          ///< distancia ao eixo x (mm), + para cima
    };
    std::vector<Bar> bars;

    // propriedades derivadas (preenchidas por `finalize`)
    double Aa {0.0}, Ac {0.0}, As {0.0};
    double IaX {0.0}, IaY {0.0}, IcX {0.0}, IcY {0.0}, IsX {0.0}, IsY {0.0};
    double ZaX {0.0}, ZaY {0.0}, ZcX {0.0}, ZcY {0.0}, ZsX {0.0}, ZsY {0.0};
    double Ec {0.0};             ///< modulo de elasticidade secante do concreto (MPa), NBR 6118
    static constexpr double kEs = 210000.0;   ///< modulo da armadura (MPa)

    [[nodiscard]] bool filled() const { return type == Type::FilledRectangular || type == Type::FilledCircular; }
    [[nodiscard]] bool circular() const { return type == Type::FilledCircular; }
    /// (EI)e = Ea Ia + alpha_c Ec Ic + Es Is (M.3.4) para flexao em relacao ao eixo x (horizontal) ou y.
    [[nodiscard]] double alphaC() const;
    [[nodiscard]] double effectiveStiffness(bool axisX, double steelE) const;
    [[nodiscard]] double effectiveAxialStiffness(double steelE) const;
    /// Contorno da armadura: n barras por face horizontal (inclui os cantos) e por face vertical (sem os cantos), de area `barArea`, a `coverToBarCentre` da face do concreto.
    void ringOfBars(int perHorizontalFace, int perVerticalFace, double barArea, double coverToBarCentre);
    void finalize();
};

/// Esforcos resistentes e verificacoes de um pilar misto. `steelFy`/`steelFu` do perfil; gamma_a1, gamma_c e gamma_s (Tabela 3 / NBR 6118).
struct CompositeColumnResistance {
    bool valid {false};
    double Npl_a {0.0}, Npl_c {0.0}, Npl_s {0.0}, Npl {0.0};   ///< N_pl,Rd por componente e total (M.4), N
    double Npl_R {0.0};          ///< N_p,R com resistencias caracteristicas (M.3.2)
    double Ny {0.0};             ///< N_y,Rd (M.4)
    double Np {0.0};             ///< N_p,Rd conforme a classe da secao
    double delta {0.0};          ///< fator de contribuicao do aco (M.1.3-d)
    double fcd1 {0.0}, fcd2 {0.0}, fcd {0.0}, fyd {0.0}, fsd {0.0};
    double lambdaAxial {0.0}, lambdaAxialP {0.0}, lambdaAxialR {0.0};   ///< esbeltez local sob forca axial (tubulares)
    int classAxial {0};          ///< 0 compacta, 1 semicompacta, 2 esbelta
    double EIx {0.0}, EIy {0.0}, EA {0.0};                    ///< rigidezes efetivas (M.3.4)
    double Ne_x {0.0}, Ne_y {0.0};                            ///< N_e = pi^2 (EI)e / L^2 em cada plano
    double lambda0_x {0.0}, lambda0_y {0.0};                  ///< esbeltez reduzida (M.3.2)
    double chi_x {0.0}, chi_y {0.0};
    double NRd_x {0.0}, NRd_y {0.0};                          ///< N_Rd = chi N_p,Rd (M.3.1)
    double Mpl_x {0.0}, Mpl_y {0.0};                          ///< M_pl,Rd (M.5.5.1)
    double MmaxPl_x {0.0}, MmaxPl_y {0.0};                    ///< M_max,pl,Rd (M.5.5.2)
    double Me_x {0.0}, Me_y {0.0};                            ///< M_e,Rd ou M_cr,Rd (tubulares semicompactas/esbeltas)
    double MRd_x {0.0}, MRd_y {0.0};                          ///< M_Rd conforme a classe (M.5.5.1)
    int classFlexX {0}, classFlexY {0};
    double hnX {0.0}, hnY {0.0};                              ///< linha neutra plastica (M.5.5.3/M.5.5.4)
    std::vector<std::string> lines;                           ///< memoria de calculo
};

/// Calcula N_pl,Rd, rigidezes, esbeltez reduzida, N_Rd e os momentos resistentes de `section` para o aco, comprimentos de flambagem Lx e Ly (mm) e fck da secao.
CompositeColumnResistance compositeColumnResistance(const CompositeColumnSection &section, double steelFy, double steelE, double gammaA1, double gammaC, double gammaS, double Lx, double Ly);

/// Verificacao da barra (compressao, flexao, interacao pelo modelo de calculo I, II ou III, esforco cortante e limites de aplicabilidade) a partir dos esforcos de `input`.
/// `input.section->composite` deve estar preenchido. `model` = 1, 2 ou 3 (M.5.2, M.5.3, M.5.4).
BarDesignOutput designCompositeColumn(const BarDesignInput &input, int model);

/// Cisalhamento nas superficies de contato (M.2): tensao resistente tau_Rd da Tabela M.2 / M.2.1.8 (MPa; 0 = conectores obrigatorios) para tubulares preenchidos.
double compositeInterfaceShearStrength(const CompositeColumnSection &section, double steelFy, double steelE, bool alternative);

} // namespace Structura::Design
