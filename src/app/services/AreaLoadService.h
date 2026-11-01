#pragma once

#include <QString>
#include <QVector>

#include <array>

namespace Structura::App::Services {

/**
 * @brief Cargas de area (pressoes sobre superficies) distribuidas nas barras que as recebem.
 *
 * O vento de edificios e gerado por WindStaticService.
 */
class AreaLoadService
{
public:
    using Vec3 = std::array<double, 3>;

    // ── Cargas de area ────────────────────────────────────────────────────
    struct BarSegment {
        int id {0};
        Vec3 a {};
        Vec3 b {};
    };

    struct Options {
        double pressure {0.0};         ///< kN/m2 (positiva no sentido de `direction`)
        Vec3 direction {0.0, 0.0, -1.0};   ///< sentido da forca (nao precisa ser unitario)
        bool projected {false};        ///< pressao sobre a projecao da superficie ortogonal a `direction`
        double manualWidth {0.0};      ///< largura de influencia fixa (m); 0 = automatica
        double overhang {0.0};         ///< prolongamento da superficie alem das barras externas (m)
    };

    struct BarLoad {
        int id {0};
        Vec3 q {};          ///< kN/m, eixos globais
        double width {0.0}; ///< largura de influencia (m)
    };

    /// Distribui a pressao nas barras. Barras paralelas sao agrupadas e a largura de influencia de cada
    /// uma e metade da distancia as vizinhas (mais `overhang` nas extremidades).
    static QVector<BarLoad> distribute(const QVector<BarSegment> &bars, const Options &options);

    /// Pano (superficie carregada) apoiado em um grupo de barras paralelas.
    struct Panel {
        QVector<int> members;
        std::vector<Vec3> polygon;   ///< quadrilatero no espaco (m)
    };

    /// Panos carregados: um por grupo de barras paralelas (contorno entre a primeira e a ultima barra, com prolongamento).
    static QVector<Panel> panels(const QVector<BarSegment> &bars, const Options &options);

    /// Normal da superficie definida pelas barras (sinal arbitrario); {0,0,0} se indefinida.
    static Vec3 surfaceNormal(const QVector<BarSegment> &bars);
};

} // namespace Structura::App::Services
