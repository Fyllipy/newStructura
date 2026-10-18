#include "shell/ShellResults.h"

#include <algorithm>
#include <cmath>

namespace Structura::Shell {

namespace {

struct Rotated {
    double aa {0.0}, bb {0.0}, ab {0.0};
};

Rotated rotate(double n11, double n22, double n12, double phi)
{
    const double c = std::cos(phi), s = std::sin(phi);
    Rotated r;
    r.aa = n11 * c * c + n22 * s * s + 2.0 * n12 * s * c;
    r.bb = n11 * s * s + n22 * c * c - 2.0 * n12 * s * c;
    r.ab = (n22 - n11) * s * c + n12 * (c * c - s * s);
    return r;
}

/// Angulo de e1 para a direcao de referencia: nas chapas, a projecao de X (ou Y se a chapa for vertical em x); nas cascas de revolucao, o proprio e1
/// (circunferencial).
double referenceAngle(const Model &model, const MeshQuad &q, const ElementState &s)
{
    const Object &o = model.objects[static_cast<std::size_t>(q.object)];
    if (o.kind != Kind::Plate && o.kind != Kind::RectTank) {
        return 0.0;
    }
    const std::array<double, 3> e1 {s.axes[0], s.axes[1], s.axes[2]}, e2 {s.axes[3], s.axes[4], s.axes[5]}, e3 {s.axes[6], s.axes[7], s.axes[8]};
    std::array<double, 3> ref {1.0, 0.0, 0.0};
    auto projected = [&](std::array<double, 3> r) {
        const double d = r[0] * e3[0] + r[1] * e3[1] + r[2] * e3[2];
        return std::array<double, 3> {r[0] - d * e3[0], r[1] - d * e3[1], r[2] - d * e3[2]};
    };
    std::array<double, 3> a = projected(ref);
    double len = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if (len < 0.3) {
        a = projected({0.0, 0.0, 1.0});   // chapa vertical em x: usa Z
        len = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    }
    if (len < 1e-9) {
        return 0.0;
    }
    return std::atan2((a[0] * e2[0] + a[1] * e2[1] + a[2] * e2[2]) / len, (a[0] * e1[0] + a[1] * e1[1] + a[2] * e1[2]) / len);
}

} // namespace

double FaceStress::vonMises() const
{
    return std::sqrt(std::max(0.0, sa * sa - sa * sb + sb * sb + 3.0 * tab * tab));
}

double FaceStress::principalMax() const
{
    return 0.5 * (sa + sb) + std::hypot(0.5 * (sa - sb), tab);
}

double FaceStress::principalMin() const
{
    return 0.5 * (sa + sb) - std::hypot(0.5 * (sa - sb), tab);
}

CaseResult combine(const Results &results, const std::vector<std::pair<std::string, double>> &factors, const std::string &name)
{
    CaseResult out;
    out.name = name;
    bool first = true;
    for (const auto &[caseName, factor] : factors) {
        const CaseResult *source = nullptr;
        for (const CaseResult &c : results.cases) {
            if (c.name == caseName) {
                source = &c;
                break;
            }
        }
        if (!source) {
            continue;
        }
        if (first) {
            out.displacement.assign(source->displacement.size(), std::array<double, 6> {});
            out.reaction.assign(source->reaction.size(), std::array<double, 6> {});
            out.elements.assign(source->elements.size(), ElementState {});
            for (std::size_t e = 0; e < source->elements.size(); ++e) {
                out.elements[e].axes = source->elements[e].axes;
            }
            first = false;
        }
        for (std::size_t i = 0; i < source->displacement.size(); ++i) {
            for (int c = 0; c < 6; ++c) {
                out.displacement[i][static_cast<std::size_t>(c)] += factor * source->displacement[i][static_cast<std::size_t>(c)];
                out.reaction[i][static_cast<std::size_t>(c)] += factor * source->reaction[i][static_cast<std::size_t>(c)];
            }
        }
        for (std::size_t e = 0; e < source->elements.size(); ++e) {
            for (std::size_t k = 0; k < 40; ++k) {
                out.elements[e].v[k] += factor * source->elements[e].v[k];
            }
        }
    }
    return out;
}

const std::vector<QuantityInfo> &quantityList()
{
    static const std::vector<QuantityInfo> list = {
        {Quantity::DisplacementMagnitude, "Deslocamento total", "mm", 1000.0, true},
        {Quantity::Ux, "Deslocamento X", "mm", 1000.0, true},
        {Quantity::Uy, "Deslocamento Y", "mm", 1000.0, true},
        {Quantity::Uz, "Deslocamento Z", "mm", 1000.0, true},
        {Quantity::MembraneA, "Esforco de membrana n_a (circunferencial / X)", "kN/m", 1e-3, false},
        {Quantity::MembraneB, "Esforco de membrana n_b (meridional / Y)", "kN/m", 1e-3, false},
        {Quantity::MembraneAB, "Esforco de membrana n_ab", "kN/m", 1e-3, false},
        {Quantity::MembranePrincipalMax, "Membrana principal maxima", "kN/m", 1e-3, false},
        {Quantity::MembranePrincipalMin, "Membrana principal minima", "kN/m", 1e-3, false},
        {Quantity::MomentA, "Momento m_a", "kN.m/m", 1e-3, false},
        {Quantity::MomentB, "Momento m_b", "kN.m/m", 1e-3, false},
        {Quantity::MomentAB, "Momento de torcao m_ab", "kN.m/m", 1e-3, false},
        {Quantity::MomentPrincipalMax, "Momento principal maximo", "kN.m/m", 1e-3, false},
        {Quantity::MomentPrincipalMin, "Momento principal minimo", "kN.m/m", 1e-3, false},
        {Quantity::ShearMax, "Cortante transversal", "kN/m", 1e-3, false},
        {Quantity::StressVonMisesPlus, "Tensao de von Mises (face +)", "MPa", 1e-6, false},
        {Quantity::StressVonMisesMinus, "Tensao de von Mises (face -)", "MPa", 1e-6, false},
        {Quantity::StressVonMisesMax, "Tensao de von Mises (maior face)", "MPa", 1e-6, false},
        {Quantity::StressMaxPrincipal, "Tensao principal maxima", "MPa", 1e-6, false},
        {Quantity::StressMinPrincipal, "Tensao principal minima", "MPa", 1e-6, false},
    };
    return list;
}

const QuantityInfo &quantityInfo(Quantity q)
{
    for (const QuantityInfo &i : quantityList()) {
        if (i.id == q) {
            return i;
        }
    }
    return quantityList().front();
}

FaceStress faceStress(const MeshQuad &quad, const ElementState &state, int point, int side)
{
    const double *v = &state.v[static_cast<std::size_t>(point) * 8];
    const double t = quad.thickness;
    FaceStress f;
    // direcao de referencia ja aplicada pelo chamador via angulo 0: o chamador usa faceStressRotated; aqui sigma em eixos locais
    const double s11 = v[0] / t + side * 6.0 * v[3] / (t * t);
    const double s22 = v[1] / t + side * 6.0 * v[4] / (t * t);
    const double s12 = v[2] / t + side * 6.0 * v[5] / (t * t);
    f.sa = s11;
    f.sb = s22;
    f.tab = s12;
    return f;
}

double elementQuantity(const Mesh &mesh, const Model &model, const CaseResult &result, std::size_t element, int point, Quantity q)
{
    const MeshQuad &quad = mesh.quads[element];
    const ElementState &state = result.elements[element];
    const double *v = &state.v[static_cast<std::size_t>(point) * 8];
    const double phi = referenceAngle(model, quad, state);
    switch (q) {
    case Quantity::MembraneA:
        return rotate(v[0], v[1], v[2], phi).aa;
    case Quantity::MembraneB:
        return rotate(v[0], v[1], v[2], phi).bb;
    case Quantity::MembraneAB:
        return rotate(v[0], v[1], v[2], phi).ab;
    case Quantity::MembranePrincipalMax:
        return 0.5 * (v[0] + v[1]) + std::hypot(0.5 * (v[0] - v[1]), v[2]);
    case Quantity::MembranePrincipalMin:
        return 0.5 * (v[0] + v[1]) - std::hypot(0.5 * (v[0] - v[1]), v[2]);
    case Quantity::MomentA:
        return rotate(v[3], v[4], v[5], phi).aa;
    case Quantity::MomentB:
        return rotate(v[3], v[4], v[5], phi).bb;
    case Quantity::MomentAB:
        return rotate(v[3], v[4], v[5], phi).ab;
    case Quantity::MomentPrincipalMax:
        return 0.5 * (v[3] + v[4]) + std::hypot(0.5 * (v[3] - v[4]), v[5]);
    case Quantity::MomentPrincipalMin:
        return 0.5 * (v[3] + v[4]) - std::hypot(0.5 * (v[3] - v[4]), v[5]);
    case Quantity::ShearMax:
        return std::hypot(v[6], v[7]);
    case Quantity::StressVonMisesPlus:
        return faceStress(quad, state, point, +1).vonMises();
    case Quantity::StressVonMisesMinus:
        return faceStress(quad, state, point, -1).vonMises();
    case Quantity::StressVonMisesMax:
        return std::max(faceStress(quad, state, point, +1).vonMises(), faceStress(quad, state, point, -1).vonMises());
    case Quantity::StressMaxPrincipal:
        return std::max(faceStress(quad, state, point, +1).principalMax(), faceStress(quad, state, point, -1).principalMax());
    case Quantity::StressMinPrincipal:
        return std::min(faceStress(quad, state, point, +1).principalMin(), faceStress(quad, state, point, -1).principalMin());
    default: {
        // deslocamentos: media dos nos do elemento (ou valor do canto)
        if (point < 4) {
            const auto &d = result.displacement[static_cast<std::size_t>(quad.n[static_cast<std::size_t>(point)])];
            return q == Quantity::Ux ? d[0] : q == Quantity::Uy ? d[1] : q == Quantity::Uz ? d[2] : std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        }
        double sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            sum += elementQuantity(mesh, model, result, element, i, q) * 0.25;
        }
        return sum;
    }
    }
}

} // namespace Structura::Shell
