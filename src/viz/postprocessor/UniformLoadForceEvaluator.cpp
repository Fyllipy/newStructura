#include "UniformLoadForceEvaluator.h"

#include <cassert>
#include <cmath>

namespace Structura::Visualization {

// ── Helpers ───────────────────────────────────────────────────────────────

double UniformLoadForceEvaluator::dot3(const std::array<double, 3>& a,
                                        const std::array<double, 3>& b) noexcept
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

// ── Constructor ───────────────────────────────────────────────────────────

UniformLoadForceEvaluator::UniformLoadForceEvaluator(const BeamElementResults& el)
    : m_ef(el.endForces)
    , m_xLocal(el.xLocal)
    , m_yLocal(el.yLocal)
    , m_zLocal(el.zLocal)
{
    // Comprimento do elemento
    const double dx = el.node2[0] - el.node1[0];
    const double dy = el.node2[1] - el.node1[1];
    const double dz = el.node2[2] - el.node1[2];
    m_length = std::sqrt(dx*dx + dy*dy + dz*dz);

    // Acumula cargas distribuídas uniformes para o sistema local
    for (const auto& load : el.distributedLoads) {
        m_hasLoad = true;
        if (load.isLocalSystem) {
            m_qx_local += load.qx;
            m_qy_local += load.qy;
            m_qz_local += load.qz;
        } else {
            // Transforma carga global → local projetando nos eixos locais
            const std::array<double, 3> qg{load.qx, load.qy, load.qz};
            m_qx_local += dot3(qg, el.xLocal);
            m_qy_local += dot3(qg, el.yLocal);
            m_qz_local += dot3(qg, el.zLocal);
        }
    }

    // Acumula momentos distribuídos uniformes no sistema local
    for (const auto& moment : el.distributedMoments) {
        m_hasLoad = true;
        if (moment.isLocalSystem) {
            m_my_local += moment.my;
            m_mz_local += moment.mz;
        } else {
            const std::array<double, 3> mg{moment.mx, moment.my, moment.mz};
            m_my_local += dot3(mg, el.yLocal);
            m_mz_local += dot3(mg, el.zLocal);
        }
    }
}

// ── evaluate ─────────────────────────────────────────────────────────────

InternalForces UniformLoadForceEvaluator::evaluate(double x) const
{
    const double L = m_length;
    if (L < 1e-12) {
        return {};
    }

    InternalForces f;

    if (!m_hasLoad) {
        // Caso 1: Interpolação linear entre esforços nodais
        const double t = x / L;
        f.N  = m_ef.N1  + (m_ef.N2  - m_ef.N1)  * t;
        f.Vy = m_ef.Vy1 + (m_ef.Vy2 - m_ef.Vy1) * t;
        f.Vz = m_ef.Vz1 + (m_ef.Vz2 - m_ef.Vz1) * t;
        f.Mt = m_ef.Mt1 + (m_ef.Mt2 - m_ef.Mt1) * t;
        f.My = m_ef.My1 + (m_ef.My2 - m_ef.My1) * t;
        f.Mz = m_ef.Mz1 + (m_ef.Mz2 - m_ef.Mz1) * t;
    } else {
        // Caso 2: Equilíbrio interno com carga distribuída uniforme
        const double qx = m_qx_local;
        const double qy = m_qy_local;
        const double qz = m_qz_local;

        f.N  = m_ef.N1  - qx * x;
        f.Vy = m_ef.Vy1 - qy * x;
        f.Vz = m_ef.Vz1 - qz * x;
        // Torção: interpolação linear (sem carga de torção distribuída no modelo atual)
        f.Mt = m_ef.Mt1 + (m_ef.Mt2 - m_ef.Mt1) * (x / L);
        // Momentos: parabólicos (integração do cortante)
        // Momento distribuído my/mz contribui com derivada constante (-m·x).
        f.My = m_ef.My1 + m_ef.Vz1 * x - 0.5 * qz * x * x - m_my_local * x;
        f.Mz = m_ef.Mz1 - m_ef.Vy1 * x + 0.5 * qy * x * x - m_mz_local * x;
    }

    // ── Verificações de consistência (apenas em builds Debug) ──────────────
    // Confirma que evaluate(L) reproduz os esforços nodais do solver.
    // Falha indica: erro de sinal, convenção local invertida,
    // transformação global/local incorreta.
#ifndef NDEBUG
    constexpr double kAtol = 1e-3; // tolerância absoluta
    constexpr double kRtol = 1e-3; // tolerância relativa
    const bool atEnd = (std::abs(x - L) < 1e-9 * L);
    if (atEnd) {
        auto check = [&](double computed, double expected, const char* name) {
            const double tol = kAtol + kRtol * std::abs(expected);
            (void)name; // suppress unused in release
            assert(std::abs(computed - expected) < tol &&
                   "InternalForceEvaluator: consistência com solver falhou."
                   " Verifique convencao de sinal ou transformacao local/global.");
        };
        check(f.N,  m_ef.N2,  "N");
        check(f.Vy, m_ef.Vy2, "Vy");
        check(f.Vz, m_ef.Vz2, "Vz");
        check(f.My, m_ef.My2, "My");
        check(f.Mz, m_ef.Mz2, "Mz");
        // Mt: não verificado — depende de convenção que pode variar por solver
    }
#endif

    return f;
}

} // namespace Structura::Visualization
