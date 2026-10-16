#pragma once

#include "InternalForceEvaluator.h"
#include "PostProcessorDataTypes.h"

#include <array>

namespace Structura::Visualization {

/**
 * @brief Avaliador de esforços internos para cargas distribuídas uniformes.
 *
 * Suporta os casos de carregamento disponíveis no modelo atual:
 *
 * Caso 1 — Sem cargas distribuídas:
 *   Interpolação linear entre os esforços nodais retornados pelo solver.
 *
 * Caso 2 — Carga distribuída uniforme (qx, qy, qz):
 *   Reconstrução por equilíbrio interno, produzindo:
 *   - N(x)  = N1 - qx·x          (linear)
 *   - Vy(x) = Vy1 - qy·x          (linear)
 *   - Vz(x) = Vz1 - qz·x          (linear)
 *   - Mz(x) = Mz1 + Vy1·x - qy·x²/2  (parabólico)
 *   - My(x) = My1 + Vz1·x - qz·x²/2  (parabólico)
 *   - Mt(x) = interpolação linear (torção uniforme)
 *
 * Cargas globais são transformadas para o sistema local antes de aplicar
 * as fórmulas acima.
 *
 * DEBUG: Assertivas verificam consistência numérica com o solver em x=L.
 */
class UniformLoadForceEvaluator final : public InternalForceEvaluator {
public:
    explicit UniformLoadForceEvaluator(const BeamElementResults& element);

    InternalForces evaluate(double x) const override;
    double length() const override { return m_length; }

private:
    /// Produto escalar de dois vetores 3D.
    static double dot3(const std::array<double, 3>& a,
                       const std::array<double, 3>& b) noexcept;

    double m_length{0.0};
    EndForces m_ef;

    // Cargas resultantes no sistema local (soma de todas as cargas)
    double m_qx_local{0.0};
    double m_qy_local{0.0};
    double m_qz_local{0.0};
    // Momentos distribuídos resultantes no sistema local (por unidade de comprimento)
    double m_my_local{0.0};
    double m_mz_local{0.0};
    bool   m_hasLoad{false};

    std::array<double, 3> m_xLocal{};
    std::array<double, 3> m_yLocal{};
    std::array<double, 3> m_zLocal{};
};

} // namespace Structura::Visualization
