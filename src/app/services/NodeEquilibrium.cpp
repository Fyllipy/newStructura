#include "core/math/Vec3Ops.h"
#include "app/services/NodeEquilibrium.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::NodeEquilibrium {

namespace {

using Vec = std::array<double, 3>;

using Structura::Math::cross;
using Structura::Math::dot;



Vec unit(const Vec &a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 1e-12 ? Vec {a[0] / l, a[1] / l, a[2] / l} : Vec {0.0, 0.0, 0.0};
}

/// Direcao da barra saindo do no.
Vec direction(const MemberEnd &m)
{
    return m.atEnd ? Vec {-m.x[0], -m.x[1], -m.x[2]} : m.x;
}

/// Forca e momento que a barra exerce sobre o no, no sistema global. A face positiva da secao recebe o esforco interno: no extremo i a barra
/// e puxada pelo no com -(esforco) e o no recebe +(esforco); no extremo j, o contrario.
void onNode(const MemberEnd &m, Vec &force, Vec &moment)
{
    const double sign = m.atEnd ? -1.0 : 1.0;
    const std::size_t o = m.atEnd ? 6 : 0;
    for (std::size_t i = 0; i < 3; ++i) {
        force[i] = sign * (m.f[o + 0] * m.x[i] + m.f[o + 1] * m.y[i] + m.f[o + 2] * m.z[i]);
        moment[i] = sign * (m.f[o + 3] * m.x[i] + m.f[o + 4] * m.y[i] + m.f[o + 5] * m.z[i]);
    }
}

} // namespace

std::array<double, 6> nodeResultant(const std::vector<MemberEnd> &members)
{
    std::array<double, 6> total {};
    for (const auto &m : members) {
        Vec force {}, moment {};
        onNode(m, force, moment);
        for (std::size_t i = 0; i < 3; ++i) {
            total[i] += force[i];
            total[3 + i] += moment[i];
        }
    }
    return total;
}

PanelResult panelShear(const std::vector<MemberEnd> &members, int beamIndex, int hostIndex, double lever)
{
    PanelResult result;
    const int count = static_cast<int>(members.size());
    if (beamIndex < 0 || hostIndex < 0 || beamIndex >= count || hostIndex >= count || beamIndex == hostIndex || lever <= 0.0) {
        return result;
    }
    const Vec s = unit(direction(members[beamIndex]));
    const Vec c = unit(direction(members[hostIndex]));
    const Vec nRaw = cross(s, c);
    if (std::sqrt(dot(nRaw, nRaw)) < 0.2) {
        return result;   // viga paralela ao pilar: nao e uma ligacao viga-pilar
    }
    const Vec n = unit(nRaw);
    double fsBeams = 0.0, mnBeams = 0.0;
    double vAbove = 0.0, vBelow = 0.0, nAbove = 0.0, nBelow = 0.0;
    bool hasBelow = false;
    for (int i = 0; i < count; ++i) {
        Vec force {}, moment {};
        onNode(members[i], force, moment);
        const double along = dot(direction(members[i]), c);
        if (along > 0.985) {            // trecho de pilar do mesmo lado do apoio (acima)
            vAbove += dot(force, s);
            nAbove = std::max(nAbove, std::abs(dot(force, c)));
        } else if (along < -0.985) {    // continuidade do pilar (abaixo)
            vBelow += dot(force, s);
            nBelow = std::max(nBelow, std::abs(dot(force, c)));
            hasBelow = true;
        } else {                        // viga
            fsBeams += dot(force, s);
            mnBeams += dot(moment, n);
        }
    }
    // binario das mesas das vigas: Fbot - Ftop = 2 Mn / z e Ftop + Fbot = soma dos axiais das vigas (na direcao s)
    const double fTop = 0.5 * (fsBeams - 2.0 * mnBeams / lever);
    const double fBot = 0.5 * (fsBeams + 2.0 * mnBeams / lever);
    // meia-alma superior: mesa superior + pilar de cima; meia-alma inferior: mesa inferior + pilar de baixo (sentido oposto)
    const double topHalf = fTop + vAbove;
    const double bottomHalf = -(fBot + vBelow);
    result.panelShear = std::abs(hasBelow ? 0.5 * (topHalf + bottomHalf) : topHalf);
    result.columnAxial = std::max(nAbove, nBelow);
    result.beamFlangeForce = std::abs(mnBeams) / lever;
    result.valid = true;
    return result;
}

} // namespace Structura::App::Services::NodeEquilibrium
