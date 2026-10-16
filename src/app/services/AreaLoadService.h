#pragma once

#include <QString>
#include <QVector>

#include <array>

namespace Structura::App::Services {

/**
 * @brief Cargas de area (pressoes sobre superficies) distribuidas nas barras que as recebem e
 *        pressao dinamica do vento conforme a ABNT NBR 6123.
 *
 * O vento segue o roteiro da norma: Vk = V0 S1 S2 S3 e q = 0,613 Vk^2 (N/m2). Os parametros
 * meteorologicos (b, p, Fr) usados no S2 sao os tabelados na NBR 6123:1988; a edicao 2023 pode
 * alterar valores, por isso o chamador deve permitir conferi-los/edita-los.
 */
class AreaLoadService
{
public:
    using Vec3 = std::array<double, 3>;

    // ── Vento ─────────────────────────────────────────────────────────────
    struct WindParameters {
        double v0 {35.0};     ///< velocidade basica (m/s)
        double s1 {1.0};      ///< fator topografico
        double s3 {1.0};      ///< fator estatistico
        int category {2};     ///< categoria de rugosidade, 1 a 5
        char cls {'B'};       ///< classe da edificacao: 'A', 'B' ou 'C'
        double height {10.0}; ///< altura z acima do terreno (m)
    };

    /// Parametros meteorologicos da categoria/classe: b, Fr (sempre da categoria II) e p.
    static void meteorologicalParameters(int category, char cls, double &b, double &fr, double &p);
    static double s2(const WindParameters &w);
    static double characteristicSpeed(const WindParameters &w);   ///< Vk (m/s)
    /// Pressao dinamica q em kN/m2.
    static double dynamicPressure(const WindParameters &w);

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
