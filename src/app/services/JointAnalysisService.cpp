#include "app/services/JointAnalysisService.h"

#include "app/BarFrame.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::App::Services {

using namespace Structura::Design::JointClass;
using SB = Structura::App::SolverBridge;

namespace {

using Vec3 = JointAnalysisService::Vec3;

Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const Vec3 &a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3 &a)
{
    const double n = norm(a);
    return n > 1e-15 ? Vec3 {a[0] / n, a[1] / n, a[2] / n} : Vec3 {0, 0, 0};
}

struct Axes {
    Vec3 y {{0, 1, 0}}, z {{0, 0, 1}};
};

/// Eixos locais y e z da barra (referencial do programa, com o giro da secao), a partir do eixo x unitario.
Axes localAxes(const Vec3 &x, double rotationDeg)
{
    const Vec3 aux = Structura::App::BarFrame::referenceAux(x);
    Vec3 z = unit(cross(x, aux));
    Vec3 y = cross(z, x);
    const double angle = rotationDeg * 3.14159265358979323846 / 180.0;
    const double c = std::cos(angle), s = std::sin(angle);
    Axes a;
    for (int i = 0; i < 3; ++i) {
        a.y[static_cast<std::size_t>(i)] = c * y[static_cast<std::size_t>(i)] + s * z[static_cast<std::size_t>(i)];
        a.z[static_cast<std::size_t>(i)] = -s * y[static_cast<std::size_t>(i)] + c * z[static_cast<std::size_t>(i)];
    }
    return a;
}

/// Inercia da secao (eixos principais) em relacao ao eixo de flexao `n` (normal ao plano de analise).
double inertiaAbout(const SB::SectionData &section, const Axes &axes, const Vec3 &n)
{
    const double nz = dot(n, axes.z), ny = dot(n, axes.y);
    return section.iz * nz * nz + section.iy * ny * ny;
}

} // namespace

std::vector<int> JointAnalysisService::sharedNodes(const SB::ModelData &model, int memberA, int memberB)
{
    const SB::MemberData *a = nullptr, *b = nullptr;
    for (const auto &m : model.members) {
        if (m.id == memberA) {
            a = &m;
        } else if (m.id == memberB) {
            b = &m;
        }
    }
    std::vector<int> out;
    if (!a || !b || memberA == memberB) {
        return out;
    }
    for (const int n : {a->nodeI, a->nodeJ}) {
        if ((n == b->nodeI || n == b->nodeJ) && std::find(out.begin(), out.end(), n) == out.end()) {
            out.push_back(n);
        }
    }
    return out;
}

JointAnalysisService::Context JointAnalysisService::analyze(const SB::ModelData &model, int nodeId, int memberA, int memberB, double forceToKn)
{
    Context ctx;
    ctx.nodeId = nodeId;
    ctx.selectedMembers = {memberA, memberB};
    std::map<int, const SB::NodeData *> nodes;
    for (const auto &n : model.nodes) {
        nodes[n.id] = &n;
    }
    std::map<int, const SB::SectionData *> sections;
    for (const auto &s : model.sections) {
        sections[s.id] = &s;
    }
    std::map<int, const SB::MaterialData *> materials;
    for (const auto &m : model.materials) {
        materials[m.id] = &m;
    }
    const auto at = nodes.find(nodeId);
    if (at == nodes.end()) {
        ctx.error = "Nó não encontrado no modelo.";
        return ctx;
    }
    const Vec3 p0 {at->second->x, at->second->y, at->second->z};
    ctx.position = p0;
    auto position = [&](int id) -> std::optional<Vec3> {
        const auto it = nodes.find(id);
        if (it == nodes.end()) {
            return std::nullopt;
        }
        return Vec3 {it->second->x, it->second->y, it->second->z};
    };

    // direcoes das duas barras selecionadas
    std::array<Vec3, 2> selDir {};
    for (int k = 0; k < 2; ++k) {
        const int id = k == 0 ? memberA : memberB;
        const SB::MemberData *m = nullptr;
        for (const auto &mm : model.members) {
            if (mm.id == id) {
                m = &mm;
            }
        }
        if (!m || (m->nodeI != nodeId && m->nodeJ != nodeId)) {
            ctx.error = "A barra " + std::to_string(id) + " não chega ao nó escolhido.";
            return ctx;
        }
        const auto other = position(m->nodeI == nodeId ? m->nodeJ : m->nodeI);
        if (!other) {
            ctx.error = "Nó da barra " + std::to_string(id) + " ausente do modelo.";
            return ctx;
        }
        selDir[static_cast<std::size_t>(k)] = unit(sub(*other, p0));
    }
    // plano de analise: o das duas barras; se colineares, plano vertical que contem a barra (hipotese declarada)
    Vec3 n = unit(cross(selDir[0], selDir[1]));
    if (norm(n) < 1e-6) {
        ctx.planeFromSelection = false;
        n = unit(cross(selDir[0], {0, 0, 1}));
        if (norm(n) < 1e-6) {
            n = {0, 1, 0};
        }
        ctx.notes.push_back("As duas barras são colineares: o plano de análise é indefinido pela seleção; adotado o plano vertical que contém as barras (normal " +
                            std::to_string(n[0]).substr(0, 5) + ", " + std::to_string(n[1]).substr(0, 5) + ", " + std::to_string(n[2]).substr(0, 5) + ").");
    }
    ctx.planeNormal = n;

    // todas as barras do no
    for (const auto &m : model.members) {
        if (m.nodeI != nodeId && m.nodeJ != nodeId) {
            continue;
        }
        Bar bar;
        bar.memberId = m.id;
        bar.nodeIsStart = m.nodeI == nodeId;
        bar.otherNode = bar.nodeIsStart ? m.nodeJ : m.nodeI;
        const auto other = position(bar.otherNode);
        if (!other) {
            continue;
        }
        const Vec3 d = sub(*other, p0);
        bar.length = norm(d);
        bar.direction = unit(d);
        bar.role = roleOfDirection(bar.direction);
        bar.selected = m.id == memberA || m.id == memberB;
        const auto sec = sections.find(m.sectionId);
        const auto mat = materials.find(m.materialId);
        const Vec3 x = bar.nodeIsStart ? bar.direction : Vec3 {-bar.direction[0], -bar.direction[1], -bar.direction[2]};
        const Axes axes = localAxes(x, m.sectionRotationDeg);
        if (sec != sections.end()) {
            bar.inertia = inertiaAbout(*sec->second, axes, n);
        }
        if (mat != materials.end()) {
            bar.youngModulus = mat->second->e * forceToKn;
        }
        const bool aboutZ = std::abs(dot(n, axes.z)) >= std::abs(dot(n, axes.y));
        const std::size_t base = bar.nodeIsStart ? 0 : 6;
        const std::size_t dof = base + (aboutZ ? 5 : 4);
        bar.rotationDof = static_cast<int>(dof);
        bar.rotationAlignment = std::max(std::abs(dot(n, axes.z)), std::abs(dot(n, axes.y)));
        bar.hasEccentricity = bar.nodeIsStart ? (m.offsetY1 != 0.0 || m.offsetZ1 != 0.0 || m.rigidStart > 0.0) : (m.offsetY2 != 0.0 || m.offsetZ2 != 0.0 || m.rigidEnd > 0.0);
        bar.rotationalRelease = m.releases[dof] != 0;
        for (const auto &spring : model.endSprings) {
            if (spring.memberId == m.id && spring.k[dof] >= 0.0) {
                bar.springKnm = spring.k[dof] * forceToKn;
            }
        }
        ctx.bars.push_back(bar);
    }
    if (ctx.bars.size() < 2) {
        ctx.error = "O nó tem menos de duas barras.";
        return ctx;
    }
    // pares (cada par e uma ligacao distinta: as rigidezes nao sao somadas)
    for (std::size_t i = 0; i < ctx.bars.size(); ++i) {
        for (std::size_t j = i + 1; j < ctx.bars.size(); ++j) {
            PairInfo pair;
            pair.memberA = ctx.bars[i].memberId;
            pair.memberB = ctx.bars[j].memberId;
            pair.kind = kindOf(ctx.bars[i].role, ctx.bars[j].role);
            pair.angleDeg = std::acos(std::clamp(dot(ctx.bars[i].direction, ctx.bars[j].direction), -1.0, 1.0)) * 180.0 / 3.14159265358979323846;
            pair.selected = (pair.memberA == memberA && pair.memberB == memberB) || (pair.memberA == memberB && pair.memberB == memberA);
            ctx.pairs.push_back(pair);
        }
    }
    const Bar *barA = nullptr, *barB = nullptr;
    for (const auto &b : ctx.bars) {
        if (b.memberId == memberA) {
            barA = &b;
        } else if (b.memberId == memberB) {
            barB = &b;
        }
    }
    if (!barA || !barB) {
        ctx.error = "Barras selecionadas não encontradas no nó.";
        return ctx;
    }
    ctx.kind = kindOf(barA->role, barB->role);
    // viga de referencia: a viga da selecao; senao a barra mais proxima da horizontal
    const Bar *ref = nullptr;
    for (const Bar *b : {barA, barB}) {
        if (b->role == BarRole::Beam) {
            ref = b;
            break;
        }
    }
    if (!ref) {
        ref = std::abs(barA->direction[2]) <= std::abs(barB->direction[2]) ? barA : barB;
        ctx.notes.push_back("Nenhuma das barras selecionadas é uma viga (horizontal): os limites usam Iv/Lv da barra mais próxima da horizontal, apenas como referência.");
    }
    ctx.referenceBar = static_cast<int>(ref - ctx.bars.data());
    ctx.beamInertia = ref->inertia;
    ctx.beamLength = ref->length;
    ctx.youngModulus = ref->youngModulus;
    ctx.limits = limitsFor(ctx.youngModulus, ctx.beamInertia, ctx.beamLength);
    // limitacoes do modelo equivalente (mola de extremidade da viga) neste no
    if (ref->rotationAlignment < 0.98) {
        ctx.notes.push_back("O plano de análise não coincide com um eixo principal da seção da viga: a mola/rótula de extremidade age em torno do eixo local mais próximo e só aproxima a ligação.");
    }
    for (const Bar &b : ctx.bars) {
        if (b.hasEccentricity) {
            ctx.notes.push_back("Barra " + std::to_string(b.memberId) + " tem excentricidade de eixo ou zona rígida neste nó: o modelo da ligação NÃO considera excentricidades (o momento é medido no nó).");
        }
    }
    int oppositeBeams = 0;
    for (const Bar &b : ctx.bars) {
        if (b.memberId != ref->memberId && b.role == BarRole::Beam && dot(b.direction, ref->direction) < -0.9) {
            ++oppositeBeams;
        }
    }
    if (oppositeBeams > 0) {
        ctx.notes.push_back("Há viga oposta no nó: a interação entre as duas ligações passa pelo painel de alma (parâmetro beta). O cadastro da ligação assume beta = 1 (viga de um lado) ou beta = 0 (vigas dos dois lados, momentos iguais); momentos desiguais exigem beta específico.");
    }
    ctx.notes.push_back("A rigidez do painel de alma (k1) está incluída em Sj,ini; no solver global, a rotação do nó é a do pilar e a mola de extremidade da viga representa a rotação relativa viga-pilar, painel incluído.");
    ctx.notes.push_back("Lv e Lp são os comprimentos das barras do modelo (eixo a eixo dos nós); barras subdivididas no modelo dão comprimentos parciais.");
    ctx.notes.push_back("Iv e Ip: momento de inércia da seção em relação ao eixo normal ao plano de análise, com os eixos principais girados pela posição da seção.");

    // pavimento: barras no plano de analise, na cota do no (vigas) e pilares que chegam a essa cota por baixo
    const double tol = 1e-3;
    auto inPlane = [&](const Vec3 &p) { return std::abs(dot(sub(p, p0), n)) <= tol; };
    for (const auto &m : model.members) {
        const auto a = position(m.nodeI), b = position(m.nodeJ);
        if (!a || !b || !inPlane(*a) || !inPlane(*b)) {
            continue;
        }
        const Vec3 d = sub(*b, *a);
        const double len = norm(d);
        if (len < 1e-9) {
            continue;
        }
        const BarRole role = roleOfDirection(unit(d));
        const auto sec = sections.find(m.sectionId);
        if (sec == sections.end()) {
            continue;
        }
        const Axes axes = localAxes(unit(d), m.sectionRotationDeg);
        const double inertia = inertiaAbout(*sec->second, axes, n);
        if (role == BarRole::Beam && std::abs((*a)[2] - p0[2]) <= tol && std::abs((*b)[2] - p0[2]) <= tol) {
            ctx.storeyBeams.push_back({m.id, inertia, len});
        } else if (role == BarRole::Column) {
            const double top = std::max((*a)[2], (*b)[2]);
            if (std::abs(top - p0[2]) <= tol) {
                ctx.storeyColumns.push_back({m.id, inertia, len});
            }
        }
    }
    ctx.storey = storeyStiffness(ctx.storeyBeams, ctx.storeyColumns);
    ctx.notes.push_back("Pavimento: vigas no plano de análise e na cota do nó; pilares do plano que chegam à cota do nó por baixo (Kv = média de Iv/Lv, Kp = média de Ip/Lp).");
    ctx.valid = true;
    return ctx;
}

void JointAnalysisService::idealize(Structura::App::ProjectData::MemberConnectionSpec &spec, int dof, int kind, double siKnm, double yieldKnm, double hardening)
{
    if (dof < 0 || dof > 11) {
        return;
    }
    // a idealizacao substitui qualquer rotula plastica anterior desse grau de liberdade
    for (int i = spec.hinges.size() - 1; i >= 0; --i) {
        if (spec.hinges[i].dof == dof) {
            spec.hinges.removeAt(i);
        }
    }
    spec.springK[static_cast<std::size_t>(dof)] = kind == 0 ? 0.0 : kind == 1 ? std::max(siKnm, 0.0) : -1.0;
    if (kind == 3 && siKnm > 0.0 && yieldKnm > 0.0) {
        Structura::App::ProjectData::PlasticHingeSpec hinge;
        hinge.dof = dof;
        hinge.kElastic = siKnm;
        hinge.yieldMoment = yieldKnm;
        hinge.hardeningRatio = std::max(hardening, 0.0);
        hinge.axialYield = 0.0;
        spec.hinges.append(hinge);
    }
}

std::optional<JointBehavior> JointAnalysisService::behaviorFromModel(const Context &ctx)
{
    if (!ctx.valid || ctx.referenceBar < 0) {
        return std::nullopt;
    }
    const Bar &beam = ctx.bars[static_cast<std::size_t>(ctx.referenceBar)];
    JointBehavior b;
    b.origin = DataOrigin::ModelSpring;
    if (beam.springKnm) {
        b.description = "Mola rotacional de extremidade da barra " + std::to_string(beam.memberId) + " no modelo (kN·m/rad); Mj,Rd não faz parte do modelo.";
        b.positive.stiffness = *beam.springKnm;
        b.negative.stiffness = *beam.springKnm;
        return b;
    }
    if (beam.rotationalRelease) {
        b.description = "Extremidade da barra " + std::to_string(beam.memberId) + " liberada à rotação no modelo (articulação: Si = 0 por hipótese de modelagem).";
        b.positive.stiffness = 0.0;
        b.negative.stiffness = 0.0;
        return b;
    }
    return std::nullopt;
}

} // namespace Structura::App::Services
