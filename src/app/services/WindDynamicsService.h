#pragma once

#include "app/services/DynamicsService.h"

#include <QString>

#include <array>
#include <vector>

namespace Structura::App::Services {

/**
 * Resposta dinamica ao vento na direcao do vento pelo modelo discreto da NBR 6123:2023, 9.4.
 * Os niveis sao os planos de nos de mesma cota; o fator dinamico xi vem dos abacos (Figuras 20 a 24) e e informado pelo usuario.
 */
class WindDynamicsService
{
public:
    struct Input {
        int mode {0};                 ///< indice do modo de flexao na direcao do vento
        int direction {0};            ///< 0 = X, 1 = Y (direcao do vento)
        double v0 {35.0};             ///< m/s
        double s1 {1.0};
        double s3 {1.0};
        int category {1};             ///< categoria de rugosidade 0..4 (I a V)
        double width {20.0};          ///< dimensao da fachada normal ao vento (m)
        double drag {1.2};            ///< Ca
        double xi {1.5};              ///< fator dinamico da Figura 20-24 (C2 incluso no 2o modo)
        bool fluctuatingOnly {false}; ///< so a parcela flutuante (2o modo: a media ja esta no 1o)
        double crossFactor {1.0 / 3.0};   ///< Y_i = X_i / 3 (9.5)
    };

    struct Level {
        double z {0.0};          ///< cota acima do nivel do terreno (m)
        double mass {0.0};       ///< t
        double shape {0.0};      ///< x_i (normalizado, topo positivo)
        double area {0.0};       ///< A_i (m2)
        double mean {0.0};       ///< kN
        double fluctuating {0.0};   ///< kN
        std::vector<std::size_t> nodes;   ///< indices em model.nodes
    };

    struct Result {
        bool ok {false};
        QString message;
        double frequency {0.0};
        double vp {0.0};         ///< m/s
        double q0 {0.0};         ///< kN/m2
        double p {0.0}, bm {0.0};
        double fh {0.0};         ///< kN
        double baseShearMean {0.0}, baseShearFluctuating {0.0};   ///< kN
        std::vector<Level> levels;
        /// Forca total por no (X, Y, Z em kN) na direcao do vento; a parcela transversal fica em `cross`.
        std::vector<std::array<double, 3>> along;
        std::vector<std::array<double, 3>> cross;
    };

    [[nodiscard]] static Result compute(const DynamicsService::Basis &basis, const Input &input);
    [[nodiscard]] static double exponentP(int category);
    [[nodiscard]] static double factorBm(int category);
};

} // namespace Structura::App::Services
