#include "UniformLoadForceEvaluator.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace Structura::Visualization {

// ── Helpers ───────────────────────────────────────────────────────────────

double UniformLoadForceEvaluator::dot3(const std::array<double, 3>& a,
                                        const std::array<double, 3>& b) noexcept
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

namespace {

/// Integrais de uma carga trapezoidal q(s) (q1 em a, q2 em b) ate a abscissa x:
/// F = ∫0^x q ds (resultante) e G = ∫0^x (x - s) q ds (momento da carga em relacao a x).
void trapezoidIntegrals(double q1, double q2, double a, double b, double x, double& F, double& G)
{
    F = 0.0;
    G = 0.0;
    if (x <= a) {
        return;
    }
    const double span = b - a;
    const double k = span > 0.0 ? (q2 - q1) / span : 0.0;
    if (x < b) {
        const double u = x - a;
        F = q1 * u + 0.5 * k * u * u;
        G = 0.5 * q1 * u * u + k * u * u * u / 6.0;
    } else {
        const double w = span;
        F = q1 * w + 0.5 * k * w * w;
        const double firstMoment = a * F + 0.5 * q1 * w * w + k * w * w * w / 3.0;
        G = x * F - firstMoment;
    }
}

} // namespace

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

    // Cargas com posição: converte para o sistema local; forças e momentos pontuais nos nós já foram para o nó (não entram na barra)
    const double tol = 1e-9 * std::max(m_length, 1.0);
    for (const auto& pl : el.positionedLoads) {
        LocalPositioned lp;
        lp.kind = pl.kind;
        lp.a = pl.a;
        lp.b = pl.b;
        auto toLocal = [&](const std::array<double, 3>& v) -> std::array<double, 3> {
            if (pl.isLocalSystem) {
                return v;
            }
            return {dot3(v, el.xLocal), dot3(v, el.yLocal), dot3(v, el.zLocal)};
        };
        lp.v1 = toLocal(pl.v1);
        lp.v2 = toLocal(pl.v2);
        if (pl.kind != PositionedLoad::Kind::Trapezoid && (pl.a <= tol || pl.a >= m_length - tol)) {
            continue;
        }
        if (pl.kind == PositionedLoad::Kind::PointMoment && lp.v1[0] != 0.0) {
            m_hasTorque = true;
        }
        m_positioned.push_back(lp);
        m_hasLoad = true;
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

        // Cargas com posição: o mesmo equilíbrio de corpo livre do trecho [0, x]
        for (const auto& lp : m_positioned) {
            switch (lp.kind) {
            case PositionedLoad::Kind::Trapezoid: {
                double fx, gx, fy, gy, fz, gz;
                trapezoidIntegrals(lp.v1[0], lp.v2[0], lp.a, lp.b, x, fx, gx);
                trapezoidIntegrals(lp.v1[1], lp.v2[1], lp.a, lp.b, x, fy, gy);
                trapezoidIntegrals(lp.v1[2], lp.v2[2], lp.a, lp.b, x, fz, gz);
                f.N -= fx;
                f.Vy -= fy;
                f.Vz -= fz;
                f.My -= gz;
                f.Mz += gy;
                break;
            }
            case PositionedLoad::Kind::PointForce:
                if (x > lp.a) {
                    f.N -= lp.v1[0];
                    f.Vy -= lp.v1[1];
                    f.Vz -= lp.v1[2];
                    f.My -= lp.v1[2] * (x - lp.a);
                    f.Mz += lp.v1[1] * (x - lp.a);
                }
                break;
            case PositionedLoad::Kind::PointMoment:
                if (x > lp.a) {
                    f.My -= lp.v1[1];
                    f.Mz -= lp.v1[2];
                }
                break;
            }
        }
        if (m_hasTorque) {
            // torção: equilíbrio com os torques pontuais (o momento distribuído em x não é considerado aqui, como na interpolação)
            f.Mt = m_ef.Mt1;
            for (const auto& lp : m_positioned) {
                if (lp.kind == PositionedLoad::Kind::PointMoment && x > lp.a) {
                    f.Mt -= lp.v1[0];
                }
            }
        }
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
