#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <functional>

namespace Structura::App::Services {

/**
 * @brief Envoltorias de esforcos por posicao ao longo da barra, com a combinacao governante em cada ponto.
 *
 * O servico nao conhece o solver nem a visualizacao: recebe um provedor que devolve os esforcos internos
 * {N, Vy, Vz, T, My, Mz} da combinacao `combo` na barra `member` na abscissa x (m, a partir do no I).
 */
class ResultsEnvelopeService
{
public:
    using Forces = std::array<double, 6>;
    using Provider = std::function<Forces(int combo, int member, double x)>;

    struct Point {
        double x {0.0};
        Forces max {};
        Forces min {};
        std::array<int, 6> maxCombo {};   ///< indice da combinacao que gera o maximo de cada componente
        std::array<int, 6> minCombo {};
    };

    struct MemberEnvelope {
        int memberId {0};
        double length {0.0};
        QVector<Point> points;
        /// Extremos absolutos de cada componente ao longo da barra.
        Forces peak {};
        std::array<double, 6> peakX {};
        std::array<int, 6> peakCombo {};
    };

    static MemberEnvelope compute(int member, double length, int comboCount, const Provider &provider, int samples = 21);

    static const char *componentName(int component);
};

} // namespace Structura::App::Services
