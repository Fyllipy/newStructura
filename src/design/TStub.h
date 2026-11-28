#pragma once

// Nucleo compartilhado de T-stub (EN 1993-1-8:2005, 6.2.4 e 6.2.6). Matematica pura: sem Qt, sem interface, sem NBR.
//
// - Geometria, material, parafusos e coeficientes sao estruturas separadas; os coeficientes sao EXCLUSIVAMENTE da EN (EnCoefficients).
// - Nada aqui conhece a NBR 8800. A verificacao NBR (6.3.5.4) vive em ColumnFlangeLever e nao importa EnCoefficients.
// - Unidades: N, mm, MPa.
// Documentacao: docs/NUCLEO_COMPARTILHADO_COMPONENTES_TSTUB.md. Validacao: docs/validacao_alavanca/.

#include <string>
#include <utility>
#include <vector>

namespace Structura::Design::TStub {

/// Coeficientes parciais e fator de parafuso da EN 1993-1-8 (valores recomendados; o Anexo Nacional pode alterar).
struct EnCoefficients {
    double gammaM0 {1.0};
    double gammaM2 {1.25};
    double k2 {0.9};
};

enum class Method { One, Two };   ///< Tabela 6.2: metodo 1 (4 Mpl/m) ou metodo 2 (alternativo, usa ew)

/// Geometria do T-stub equivalente (Figura 6.2).
struct Geometry {
    double m {0.0};      ///< distancia do eixo do parafuso a face da alma (descontado 0,8 r ou 0,8 sf)
    double e {0.0};      ///< distancia do eixo do parafuso a borda livre
    double emin {0.0};   ///< emin; <= 0: usa e
    double ew {0.0};     ///< dw/4 (metodo 2)
};

struct Material {
    double fy {0.0};     ///< MPa
};

struct Bolts {
    double ftRd {0.0};   ///< Ft,Rd de UM parafuso (N)
    int count {2};       ///< parafusos do T-stub (2 por fileira)
    [[nodiscard]] double total() const { return ftRd * count; }
};

/// Um par de comprimentos efetivos (Tabelas 6.4/6.6).
struct EffectiveLength {
    double circular {0.0};
    double nonCircular {0.0};
    std::string source;   ///< tabela e configuracao
    [[nodiscard]] double mode1() const { return circular < nonCircular ? circular : nonCircular; }
    [[nodiscard]] double mode2() const { return nonCircular; }
};

/// Resultado de um T-stub (Tabela 6.2).
struct Modes {
    bool valid {false};
    std::string error;
    double f1 {0.0};
    double f2 {0.0};
    double f3 {0.0};
    double resistance {0.0};   ///< FT,Rd = min(f1, f2, f3)
    int governing {0};         ///< 1, 2 ou 3
    double n {0.0};
    double mpl1 {0.0};
    double mpl2 {0.0};
    EffectiveLength leff;
    std::vector<std::string> lines;
};

/// Tabela 6.2 com prying (EN 6.2.4.1, Nota 1: em ligacoes viga-pilar a alavanca se desenvolve).
Modes resistance(const Geometry &g, const EffectiveLength &leff, double tf, const Material &steel, const Bolts &bolts, const EnCoefficients &c, Method method,
                 const std::string &label);

// ---- comprimentos efetivos --------------------------------------------------------------------------------------------------
/// EN Tabela 6.4, mesa de pilar nao enrijecida, fileira isolada: cp = 2 pi m; nc = 4 m + 1,25 e.
EffectiveLength columnFlangeRow(double m, double e);
/// EN Tabela 6.4, grupo de fileiras consecutivas (spacings = espacamentos entre fileiras do grupo): fileiras de extremidade cp = pi m + p,
/// nc = 2 m + 0,625 e + 0,5 p; fileiras internas cp = 2 p, nc = p (p = media dos dois espacamentos). Somados sobre o grupo.
EffectiveLength columnFlangeGroup(double m, double e, const std::vector<double> &spacings);
/// EN Tabela 6.6, chapa de extremidade, fileira na extensao (acima da mesa tracionada da viga): mx, ex, e (borda lateral), w (gabarito), bp (largura).
EffectiveLength endPlateExtensionRow(double mx, double ex, double e, double w, double bp);

// ---- fileiras e grupos (EN 6.2.4.2 e 6.2.7.2, procedimento do SCI P398) -----------------------------------------------------
struct ColumnFlangeInput {
    double m {0.0};
    double e {0.0};
    double emin {0.0};                   ///< emin = min(e do pilar ; e da chapa); <= 0: usa e
    double tf {0.0};
    double fy {0.0};
    double ew {0.0};
    double boltFtRd {0.0};               ///< N por parafuso
    std::vector<double> rowSpacings;     ///< espacamentos entre fileiras tracionadas consecutivas (n - 1 valores)
    int rows {1};
};

struct RowResult {
    int index {0};                 ///< 1 = fileira mais afastada do centro de compressao
    Modes isolated;                ///< fileira sozinha
    double effective {0.0};        ///< resistencia efetiva apos os grupos (N)
    std::string governingGroup;    ///< "isolada" ou "grupo i-j"
};

struct ColumnFlangeResult {
    bool valid {false};
    std::string error;
    std::vector<RowResult> rows;
    std::vector<std::pair<std::string, Modes>> groups;   ///< grupos avaliados ("1-2", ...)
    double total {0.0};                                    ///< soma das resistencias efetivas das fileiras (N)
    std::vector<std::string> lines;
};

/// Mesa do pilar tracionada por `rows` fileiras: fileiras isoladas e grupos; a resistencia efetiva da fileira r e
/// min(Rr ; min_j [Rgrupo(j..r) - soma das efetivas de j..r-1]).
ColumnFlangeResult columnFlange(const ColumnFlangeInput &input, const EnCoefficients &c, Method method);

/// Ft,Rd da EN 3.6.1: k2 fub As / gammaM2.
double boltTensionResistance(double fub, double stressArea, const EnCoefficients &c);
/// Area resistente As de parafusos comuns (M12 a M36); 0 se o diametro nao esta tabelado.
double boltStressArea(double db);
/// dw usado no metodo 2 (ew = dw/4). GRANDEZA IMPLEMENTADA: LARGURA ENTRE VERTICES de uma cabeca/porca sextavada regular, dw = s x 2/raiz(3) (s = largura entre faces,
/// fator 1,1547005), com s da tabela abaixo (serie ISO 4014/4017: M12 18, M16 24, M20 30, M22 34, M24 36, M27 41, M30 46, M36 55 mm).
/// A EN 1993-1-8 (Tabela 6.2) define dw como "o diametro da arruela OU a largura entre vertices da cabeca/porca, conforme o caso": quando ha arruela sob a cabeca
/// ou a porca, o dw da EN e o diametro da arruela, que este codigo NAO usa. A serie EN 14399-4 (parafusos de alta resistencia, ex.: M24 s = 41, M30 s = 50) tambem
/// difere da tabela. Ambos sao limitacoes conhecidas (docs/A3_...).
/// Para diametros fora da tabela NAO ha valor tabelado: `tabulated = false` e a largura entre faces e ESTIMADA como s = 1,5 db, logo
/// dw = 1,5 db x 1,1547005 = 1,732 db (largura entre VERTICES, nao entre faces). E hipotese empirica, NAO e dado normativo nem de catalogo.
struct HeadWidth {
    double acrossPoints {0.0};   ///< mm
    bool tabulated {false};
};
HeadWidth boltHeadWidth(double db);
/// Compatibilidade: apenas o valor de `boltHeadWidth` (inclusive a estimativa). Prefira `boltHeadWidth`, que informa se o valor e tabelado.
double boltHeadAcrossPoints(double db);

} // namespace Structura::Design::TStub
