#include "shell/ShellDesign.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::Shell {

namespace {

struct Tab17_3 {
    double fck, rho;
};
// Tabela 17.3 (CA-50, d/h = 0,8, gamma_c = 1,4 e gamma_s = 1,15), rho_min em %
constexpr Tab17_3 kRhoMin[] = {{20, 0.150}, {25, 0.150}, {30, 0.150}, {35, 0.164}, {40, 0.179}, {45, 0.194}, {50, 0.208}, {55, 0.211},
                               {60, 0.219}, {65, 0.226}, {70, 0.233}, {75, 0.239}, {80, 0.245}, {85, 0.251}, {90, 0.256}};

struct WoodArmer {
    double a {0.0}, b {0.0};   ///< momentos de projeto (>= 0) nas direcoes a e b, para tracao na face considerada
};

/// Momentos de Wood-Armer para tracao na face em que `ma`, `mb` (positivos = tracao nessa face) atuam.
WoodArmer woodArmer(double ma, double mb, double mab)
{
    const double t = std::abs(mab);
    WoodArmer w;
    w.a = ma + t;
    w.b = mb + t;
    if (w.a < 0.0) {
        w.a = 0.0;
        w.b = (ma != 0.0) ? mb + mab * mab / std::abs(ma) : mb;
    } else if (w.b < 0.0) {
        w.b = 0.0;
        w.a = (mb != 0.0) ? ma + mab * mab / std::abs(mb) : ma;
    }
    w.a = std::max(0.0, w.a);
    w.b = std::max(0.0, w.b);
    return w;
}

double tresca(double s1, double s2)
{
    return std::max({std::abs(s1), std::abs(s2), std::abs(s1 - s2)});
}

struct ZoneAccumulator {
    std::string name;
    std::string material;
    bool concrete {false};
    bool vessel {false};
    double fy {0.0}, fu {0.0}, fck {0.0};
    double thickness {0.0};
    double maxVonMises {0.0};
    double maxPm {0.0};
    double maxTotal {0.0};
    double maxAs[4] {0.0, 0.0, 0.0, 0.0};
    double maxXd {0.0};
    double maxShearRatio {0.0};
    double maxShear {0.0};
    double vrd {0.0};
    bool overXd {false};
};

} // namespace

double minimumFlexureRatio(double fckMPa)
{
    const std::size_t n = sizeof(kRhoMin) / sizeof(kRhoMin[0]);
    if (fckMPa <= kRhoMin[0].fck) {
        return kRhoMin[0].rho / 100.0;
    }
    for (std::size_t i = 1; i < n; ++i) {
        if (fckMPa <= kRhoMin[i].fck) {
            const double t = (fckMPa - kRhoMin[i - 1].fck) / (kRhoMin[i].fck - kRhoMin[i - 1].fck);
            return (kRhoMin[i - 1].rho + t * (kRhoMin[i].rho - kRhoMin[i - 1].rho)) / 100.0;
        }
    }
    return kRhoMin[n - 1].rho / 100.0;
}

bool flexureReinforcement(double md, double h, double d, double fck, double fyk, double gammaC, double gammaS, double &as, double &xd)
{
    (void)h;
    as = 0.0;
    xd = 0.0;
    if (md <= 0.0) {
        return true;
    }
    const double fcd = fck / gammaC, fyd = fyk / gammaS;
    // Md = 0,68 b x fcd (d - 0,4 x), b = 1 m  ->  x = 1,25 d [1 - sqrt(1 - Md / (0,425 b d2 fcd))]
    const double k = md / (0.425 * d * d * fcd);
    if (k > 1.0) {
        xd = 1e9;
        as = 0.0;
        return false;
    }
    const double x = 1.25 * d * (1.0 - std::sqrt(1.0 - k));
    xd = x / d;
    as = md / (fyd * (d - 0.4 * x));
    const double limit = fck <= 50.0e6 ? 0.45 : 0.35;
    return xd <= limit + 1e-12;
}

double slabShearResistance(double fck, double d, double rho1, double gammaC)
{
    const double fckMPa = fck / 1.0e6;
    const double fctm = fckMPa <= 50.0 ? 0.3 * std::pow(fckMPa, 2.0 / 3.0) : 2.12 * std::log(1.0 + 0.11 * fckMPa);
    const double fctd = 0.7 * fctm / gammaC * 1.0e6;   // Pa
    const double tauRd = 0.25 * fctd;
    const double k = std::max(1.0, 1.6 - d);
    return tauRd * k * (1.2 + 40.0 * std::min(rho1, 0.02)) * 1.0 * d;
}

DesignOutput designShells(const Mesh &mesh, const Model &model, const std::vector<CaseResult> &ultimate, const CaseResult *service)
{
    DesignOutput out;
    out.elementRatio.assign(mesh.quads.size(), 0.0);
    out.reinforcement.assign(mesh.quads.size(), std::array<double, 4> {0.0, 0.0, 0.0, 0.0});
    if (ultimate.empty()) {
        out.notes.push_back("Sem combinacoes ultimas selecionadas: nada a verificar.");
        return out;
    }
    std::map<int, ZoneAccumulator> zones;
    auto zoneOf = [&](const MeshQuad &q) -> ZoneAccumulator & {
        ZoneAccumulator &z = zones[q.zone];
        if (z.name.empty()) {
            const Material &m = model.materials[static_cast<std::size_t>(q.material)];
            const Object &o = model.objects[static_cast<std::size_t>(q.object)];
            z.name = mesh.zones[static_cast<std::size_t>(q.zone)];
            z.material = m.name;
            z.concrete = m.concrete;
            z.vessel = o.kind == Kind::Vessel;
            z.fy = m.fy;
            z.fu = m.fu;
            z.fck = m.fy;
            z.thickness = q.thickness;
        }
        return z;
    };

    for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
        const MeshQuad &q = mesh.quads[e];
        ZoneAccumulator &z = zoneOf(q);
        const Object &o = model.objects[static_cast<std::size_t>(q.object)];
        const Vec3 normal = quadNormal(mesh, q);
        const double t = q.thickness;
        for (const CaseResult &c : ultimate) {
            if (e >= c.elements.size()) {
                continue;
            }
            const ElementState &state = c.elements[e];
            if (!z.concrete) {
                double ratio = 0.0;
                for (int p = 0; p < 5; ++p) {
                    for (const int side : {+1, -1}) {
                        const FaceStress f = faceStress(q, state, p, side);
                        z.maxVonMises = std::max(z.maxVonMises, f.vonMises());
                        const double s1 = f.principalMax(), s2 = f.principalMin();
                        z.maxTotal = std::max(z.maxTotal, tresca(s1, s2));
                        ratio = std::max(ratio, f.vonMises() / (z.fy / 1.10));
                    }
                    const double *v = &state.v[static_cast<std::size_t>(p) * 8];
                    const double n1 = 0.5 * (v[0] + v[1]) + std::hypot(0.5 * (v[0] - v[1]), v[2]);
                    const double n2 = 0.5 * (v[0] + v[1]) - std::hypot(0.5 * (v[0] - v[1]), v[2]);
                    z.maxPm = std::max(z.maxPm, tresca(n1 / t, n2 / t));
                }
                out.elementRatio[e] = std::max(out.elementRatio[e], ratio);
            } else {
                out.hasConcrete = true;
                const double fck = z.fck, fyk = o.param("fyk", 500.0e6);
                const double gc = o.param("gammaC", 1.4), gs = o.param("gammaS", 1.15);
                const double cover = o.param("cover", 0.03), bar = o.param("bar", 0.01);
                const double dA = t - cover - bar / 2.0;   // camada externa (dir. a)
                const double dB = dA - bar;                // camada interna (dir. b)
                double ratioE = 0.0;
                double asE[4] = {0.0, 0.0, 0.0, 0.0};
                for (int p = 0; p < 5; ++p) {
                    const double ma = elementQuantity(mesh, model, c, e, p, Quantity::MomentA);
                    const double mb = elementQuantity(mesh, model, c, e, p, Quantity::MomentB);
                    const double mab = elementQuantity(mesh, model, c, e, p, Quantity::MomentAB);
                    // face + traciona com m > 0; face - com m < 0
                    const WoodArmer plus = woodArmer(ma, mb, mab), minus = woodArmer(-ma, -mb, mab);
                    const double md[4] = {plus.a, plus.b, minus.a, minus.b};
                    const double d[4] = {dA, dB, dA, dB};
                    for (int k = 0; k < 4; ++k) {
                        double as = 0.0, xd = 0.0;
                        const bool ok = flexureReinforcement(md[k], t, d[k], fck, fyk, gc, gs, as, xd);
                        asE[k] = std::max(asE[k], as);
                        z.maxXd = std::max(z.maxXd, std::min(xd, 9.0));
                        if (!ok) {
                            z.overXd = true;
                            ratioE = std::max(ratioE, 2.0);
                        } else {
                            ratioE = std::max(ratioE, xd / 0.45);
                        }
                    }
                }
                // armadura minima (Tabelas 17.3 e 19.1): faces com normal para cima = armadura negativa (rho_min); para baixo = positiva (0,67 rho_min)
                const double rho = minimumFlexureRatio(fck / 1.0e6);
                const bool plusIsTop = normal[2] > 0.5, plusIsBottom = normal[2] < -0.5;
                const double asMinPlus = rho * t * (plusIsBottom ? 0.67 : 1.0), asMinMinus = rho * t * (plusIsTop ? 0.67 : 1.0);
                asE[0] = std::max(asE[0], asMinPlus);
                asE[1] = std::max(asE[1], asMinPlus);
                asE[2] = std::max(asE[2], asMinMinus);
                asE[3] = std::max(asE[3], asMinMinus);
                // cisalhamento sem armadura transversal (centro do elemento)
                const double *vc = &state.v[4 * 8];
                const double vsd = std::hypot(vc[6], vc[7]);
                const double rho1 = std::min(0.02, std::max({asE[0], asE[1], asE[2], asE[3]}) / dA);
                const double vrd = slabShearResistance(fck, dA, rho1, gc);
                z.vrd = z.vrd == 0.0 ? vrd : std::min(z.vrd, vrd);
                z.maxShear = std::max(z.maxShear, vsd);
                const double shearRatio = vrd > 0.0 ? vsd / vrd : 0.0;
                z.maxShearRatio = std::max(z.maxShearRatio, shearRatio);
                ratioE = std::max(ratioE, shearRatio);
                for (int k = 0; k < 4; ++k) {
                    const double cm2 = asE[k] * 1.0e4;
                    out.reinforcement[e][static_cast<std::size_t>(k)] = std::max(out.reinforcement[e][static_cast<std::size_t>(k)], cm2);
                    z.maxAs[k] = std::max(z.maxAs[k], cm2);
                }
                out.elementRatio[e] = std::max(out.elementRatio[e], ratioE);
            }
        }
    }

    for (auto &[index, z] : zones) {
        (void)index;
        if (!z.concrete) {
            ZoneCheck vm;
            vm.zone = z.name;
            vm.criterion = "Tensao equivalente de von Mises (faces)";
            vm.demand = z.maxVonMises / 1.0e6;
            vm.limit = z.fy / 1.10 / 1.0e6;
            vm.ratio = vm.limit > 0.0 ? vm.demand / vm.limit : 0.0;
            vm.unit = "MPa";
            vm.reference = "NBR 8800:2024, 4.9 e Tabela 3 (fy/ga1)";
            out.checks.push_back(vm);
            if (z.vessel) {
                const double s = std::min(z.fu / 3.5, z.fy / 1.5);
                ZoneCheck pm;
                pm.zone = z.name;
                pm.criterion = "Membrana primaria Pm (intensidade de tensao de Tresca)";
                pm.demand = z.maxPm / 1.0e6;
                pm.limit = s / 1.0e6;
                pm.ratio = pm.limit > 0.0 ? pm.demand / pm.limit : 0.0;
                pm.unit = "MPa";
                pm.reference = "Criterio tipo ASME VIII Div. 1 (S = min(fu/3,5; fy/1,5))";
                pm.note = "Pm calculada com os esforcos de membrana de cada elemento; nao separa tensoes locais de descontinuidade.";
                out.checks.push_back(pm);
                ZoneCheck tot;
                tot.zone = z.name;
                tot.criterion = "Membrana + flexao + secundarias (intensidade de tensao nas faces)";
                tot.demand = z.maxTotal / 1.0e6;
                tot.limit = 3.0 * s / 1.0e6;
                tot.ratio = tot.limit > 0.0 ? tot.demand / tot.limit : 0.0;
                tot.unit = "MPa";
                tot.reference = "Criterio tipo ASME VIII Div. 2 (PL + Pb + Q <= 3 S)";
                out.checks.push_back(tot);
            }
        } else {
            ZoneCheck xd;
            xd.zone = z.name;
            xd.criterion = "Posicao da linha neutra x/d";
            xd.demand = z.maxXd;
            xd.limit = z.fck <= 50.0e6 ? 0.45 : 0.35;
            xd.ratio = z.overXd ? 2.0 : xd.demand / xd.limit;
            xd.unit = "-";
            xd.reference = "NBR 6118:2023, 14.6.4.3";
            xd.note = z.overXd ? "Secao insuficiente a flexao (armadura dupla ou maior espessura)." : "";
            out.checks.push_back(xd);
            ZoneCheck sh;
            sh.zone = z.name;
            sh.criterion = "Cisalhamento sem armadura transversal";
            sh.demand = z.maxShear / 1.0e3;
            sh.limit = z.vrd / 1.0e3;
            sh.ratio = z.maxShearRatio;
            sh.unit = "kN/m";
            sh.reference = "NBR 6118:2023, 19.4.1";
            out.checks.push_back(sh);
            for (int k = 0; k < 4; ++k) {
                ZoneCheck as;
                as.zone = z.name;
                static const char *names[4] = {"face + direcao a", "face + direcao b", "face - direcao a", "face - direcao b"};
                as.criterion = std::string("Armadura de flexao requerida, ") + names[k];
                as.demand = z.maxAs[k];
                as.limit = 0.0;
                as.ratio = 0.0;
                as.unit = "cm2/m";
                as.reference = "NBR 6118:2023, 17.2, 17.3.5 e Tabela 19.1";
                out.checks.push_back(as);
            }
        }
    }
    if (service && !service->displacement.empty()) {
        double w = 0.0;
        for (const auto &d : service->displacement) {
            w = std::max(w, std::abs(d[2]));
        }
        ZoneCheck defl;
        defl.zone = "Modelo";
        defl.criterion = "Deslocamento vertical maximo (servico, sem fissuracao nem fluencia)";
        defl.demand = w * 1000.0;
        defl.limit = 0.0;
        defl.unit = "mm";
        defl.reference = "NBR 6118:2023, 13.3 (compare com L/250)";
        defl.note = "Estadio I: lajes de concreto fissuram; use rigidez reduzida (alpha_f) para o limite.";
        out.checks.push_back(defl);
    }
    for (const ZoneCheck &c : out.checks) {
        if (c.limit > 0.0) {
            out.maxRatio = std::max(out.maxRatio, c.ratio);
        }
    }
    for (const double r : out.elementRatio) {
        out.maxRatio = std::max(out.maxRatio, r);
    }
    out.notes.push_back("Nao verificados: puncionamento (6118, 19.5), fissuracao e flechas diferidas do concreto; flambagem de cascas cilindricas (vento, vacuo); fadiga; ligacoes (bocais, selas).");
    return out;
}

} // namespace Structura::Shell
