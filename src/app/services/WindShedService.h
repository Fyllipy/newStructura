#pragma once

#include "app/services/WindStaticService.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

namespace Structura::App::Services {

/**
 * @brief Vento em galpoes e edificacoes de cobertura de uma ou duas aguas (ABNT NBR 6123:2023).
 *
 * Detecta, a partir das barras do modelo, as colunas (barras verticais) e as tesouras/vigas de cobertura (barras inclinadas num plano transversal a cumeeira) e aplica
 * sobre elas as pressoes q (Ce - Cpi): paredes (Tabela 6, zonas A1 a D2), cobertura de duas aguas (Tabela 7) e cobertura de uma agua (Tabela 8, h/b < 2). A largura de
 * influencia de cada barra e a faixa entre os pontos medios das barras vizinhas; quando a faixa cruza zonas de coeficientes diferentes, o Ce e a media ponderada pelo comprimento.
 *
 * Sentidos do vento: transversal T+ / T- (perpendicular a cumeeira, alfa = 90 graus nas tabelas) e longitudinal L+ / L- (paralelo a cumeeira, alfa = 0).
 * Coeficientes de pressao interna Cpi segundo 6.3.2: o programa gera um caso por valor de Cpi (sobrepressao e succao interna).
 *
 * Nao coberto: telhados multiplos (Tabelas 9 a 12), vento oblíquo (45 graus), cpe medio de elementos de vedacao e coberturas isoladas (Tabelas 24 e 25).
 */
class WindShedService
{
public:
    using Vec3 = std::array<double, 3>;

    enum class RidgeAxis { X, Y };                       ///< direcao global da cumeeira (eixo horizontal)
    enum class ReferenceHeight { Ridge, Eave, Mean };    ///< cota em que se calcula S2
    enum class CpiMode {
        Sealed,               ///< edificacao estanque (6.3.2.2): Cpi = -0,2 ou 0
        FourFacesPermeable,   ///< quatro faces igualmente permeaveis (6.3.2.1-b): Cpi = -0,3 ou 0
        TwoOppositeFaces,     ///< duas faces opostas igualmente permeaveis, as outras impermeaveis (6.3.2.1-a)
        DominantWindward,     ///< abertura dominante na face de barlavento (6.3.2.1-c): Cpi pela razao de areas
        DominantLeeward,      ///< abertura dominante na face de sotavento: Cpi = Ce da parede de sotavento
        DominantHighSuction,  ///< abertura dominante em zona de alta succao externa: Cpi pela razao de areas
        Custom                ///< valores informados
    };

    struct Parameters {
        WindStaticService::Parameters site;   ///< V0, S1, S3, categoria
        RidgeAxis ridgeAxis {RidgeAxis::Y};
        ReferenceHeight reference {ReferenceHeight::Ridge};
        CpiMode cpiMode {CpiMode::FourFacesPermeable};
        double openingRatio {1.0};            ///< razao de areas das aberturas (modos de abertura dominante)
        bool transversePermeable {true};      ///< duas faces opostas permeaveis: true = as paredes laterais (perpendiculares ao vento transversal)
        double cpiFirst {-0.3};               ///< valores do modo Custom
        double cpiSecond {0.0};
        double overhang {0.0};                ///< prolongamento da faixa de influencia alem das barras extremas (m)
        bool transverse {true};               ///< gera T+ e T-
        bool longitudinal {true};             ///< gera L+ e L-
    };

    struct Bar {
        int id {0};
        Vec3 a {};   ///< m
        Vec3 b {};
    };

    struct MemberLoad {
        int memberId {0};
        Vec3 q {};   ///< kN/m, eixos globais
    };

    struct Geometry {
        double a {0.0}, b {0.0};     ///< dimensoes em planta: a ao longo da cumeeira, b transversal (m)
        double h {0.0};              ///< altura do beiral (m)
        double ridgeHeight {0.0};    ///< altura da cumeeira acima da base (m)
        double theta {0.0};          ///< inclinacao da cobertura (graus)
        bool twoPitch {true};        ///< false: uma agua
        double uRidge {0.0};         ///< coordenada transversal da cumeeira (duas aguas) ou do lado alto (uma agua)
        bool highSideAtMinU {false}; ///< uma agua: o lado alto fica em u minimo
        double zBase {0.0};
    };

    struct Case {
        QString name;          ///< ex.: "VENTO T+ cpi=-0,30"
        QString direction;     ///< "T+", "T-", "L+", "L-"
        double q {0.0};        ///< pressao dinamica (kN/m2)
        double cpi {0.0};
        char windClass {'B'};
        QVector<MemberLoad> loads;
        QStringList notes;     ///< coeficientes aplicados (memorial)
    };

    struct Result {
        Geometry geometry;
        QVector<Case> cases;
        QStringList warnings;
        int columns {0}, rafters {0};
    };

    [[nodiscard]] static Result compute(const Parameters &parameters, const QVector<Bar> &bars);

    // --- Tabelas (publicas para teste) ---
    /// Tabela 7: Ce de telhados de duas aguas. `column`: 0 EFI (barlavento, alfa = 90), 1 GHJ (sotavento, alfa = 90), 2 EG, 3 FH (alfa = 0). Interpolacao linear em theta.
    [[nodiscard]] static double twoPitchCe(double heightOverB, double thetaDegrees, int column);
    /// Tabela 8: Ce de telhados de uma agua. `column`: 0 H e I (90), 1 L e J (90), 2 H (45), 3 L (45), 4 H e L ate b/2 (0), 5 H e L de b/2 a a/2 (0), 6 H (-45), 7 L (-45), 8 H e I (-90), 9 L e J (-90).
    [[nodiscard]] static double onePitchCe(double thetaDegrees, int column);
    /// Tabela 6: Ce das paredes. alfa = 0: {A1B1, A2B2, C, D}; alfa = 90: {A, B, C1D1, C2D2}.
    [[nodiscard]] static std::array<double, 4> wallCe(double heightOverB, double aOverB, bool alpha90);
    /// Cpi por abertura dominante na face de barlavento (6.3.2.1-c) pela razao entre a area das aberturas de barlavento e a total.
    [[nodiscard]] static double cpiDominantWindward(double ratio);
    /// Cpi por abertura dominante em zona de alta succao externa (6.3.2.1-c, ultima tabela).
    [[nodiscard]] static double cpiDominantHighSuction(double ratio);
};

} // namespace Structura::App::Services
