#include "ModelCheckService.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::App::Services {

using Severity = ModelCheckService::Severity;
using Kind = ModelCheckService::TargetKind;

namespace {

struct Context {
    QVector<ModelCheckService::Issue> issues;
    void add(Severity s, const QString &code, const QString &message, Kind kind = Kind::None, int id = 0)
    {
        issues.append({s, code, message, kind, id});
    }
};

double distance(const ModelCheckService::Node &a, const ModelCheckService::Node &b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

} // namespace

int ModelCheckService::count(const QVector<Issue> &issues, Severity severity)
{
    return static_cast<int>(std::count_if(issues.begin(), issues.end(), [severity](const Issue &i) { return i.severity == severity; }));
}

QVector<ModelCheckService::Issue> ModelCheckService::run(const Input &in)
{
    Context ctx;
    QHash<int, Node> nodeById;
    for (const Node &n : in.nodes) {
        nodeById.insert(n.id, n);
    }
    if (in.nodes.isEmpty() || in.bars.isEmpty()) {
        ctx.add(Severity::Error, QStringLiteral("EMPTY"), QStringLiteral("O modelo não tem nós ou barras."));
        return ctx.issues;
    }

    // ---- barras ---------------------------------------------------------------------------------------------
    QHash<int, int> degree;   // barras por no
    std::map<std::pair<int, int>, int> pairs;
    QHash<int, QSet<int>> adjacency;
    for (const Bar &b : in.bars) {
        const auto ni = nodeById.constFind(b.nodeI);
        const auto nj = nodeById.constFind(b.nodeJ);
        if (ni == nodeById.constEnd() || nj == nodeById.constEnd()) {
            ctx.add(Severity::Error, QStringLiteral("BAR_NODE_MISSING"), QStringLiteral("Barra %1 referencia nó inexistente.").arg(b.id), Kind::Bar, b.id);
            continue;
        }
        ++degree[b.nodeI];
        ++degree[b.nodeJ];
        adjacency[b.nodeI].insert(b.nodeJ);
        adjacency[b.nodeJ].insert(b.nodeI);
        const double length = distance(*ni, *nj);
        if (length < in.tolerance) {
            ctx.add(Severity::Error, QStringLiteral("BAR_ZERO_LENGTH"), QStringLiteral("Barra %1 tem comprimento nulo.").arg(b.id), Kind::Bar, b.id);
        } else if (length < 0.01) {
            ctx.add(Severity::Warning, QStringLiteral("BAR_VERY_SHORT"), QStringLiteral("Barra %1 muito curta (%2 mm).").arg(b.id).arg(length * 1000.0, 0, 'f', 1), Kind::Bar, b.id);
        }
        const auto key = std::make_pair(std::min(b.nodeI, b.nodeJ), std::max(b.nodeI, b.nodeJ));
        if (++pairs[key] == 2) {
            ctx.add(Severity::Warning, QStringLiteral("BAR_DUPLICATE"), QStringLiteral("Existe mais de uma barra entre os nós %1 e %2 (duplicada?).").arg(key.first).arg(key.second), Kind::Bar, b.id);
        }
        if (!b.hasMaterial) {
            ctx.add(Severity::Error, QStringLiteral("BAR_NO_MATERIAL"), QStringLiteral("Barra %1 sem material.").arg(b.id), Kind::Bar, b.id);
        }
        if (!b.hasSection) {
            ctx.add(Severity::Error, QStringLiteral("BAR_NO_SECTION"), QStringLiteral("Barra %1 sem seção.").arg(b.id), Kind::Bar, b.id);
        } else if (b.family.isEmpty()) {
            ctx.add(Severity::Info, QStringLiteral("BAR_NO_GEOMETRY"),
                    QStringLiteral("Barra %1: a seção \"%2\" só tem propriedades (sem geometria): não será dimensionada nem desenhada.").arg(b.id).arg(b.sectionName), Kind::Bar, b.id);
        }
    }

    // ---- nos ------------------------------------------------------------------------------------------------
    for (const Node &n : in.nodes) {
        if (degree.value(n.id, 0) == 0) {
            ctx.add(Severity::Warning, QStringLiteral("NODE_ORPHAN"), QStringLiteral("Nó %1 não está ligado a nenhuma barra.").arg(n.id), Kind::Node, n.id);
        }
    }
    // nos coincidentes
    {
        QHash<QString, int> seen;
        for (const Node &n : in.nodes) {
            const QString key = QStringLiteral("%1|%2|%3").arg(std::llround(n.x / in.tolerance)).arg(std::llround(n.y / in.tolerance)).arg(std::llround(n.z / in.tolerance));
            const auto it = seen.constFind(key);
            if (it != seen.constEnd()) {
                ctx.add(Severity::Warning, QStringLiteral("NODE_DUPLICATE"), QStringLiteral("Nós %1 e %2 estão na mesma posição.").arg(it.value()).arg(n.id), Kind::Node, n.id);
            } else {
                seen.insert(key, n.id);
            }
        }
    }
    // no sobre o interior de uma barra, sem ligacao a ela
    for (const Bar &b : in.bars) {
        const auto ni = nodeById.constFind(b.nodeI);
        const auto nj = nodeById.constFind(b.nodeJ);
        if (ni == nodeById.constEnd() || nj == nodeById.constEnd()) {
            continue;
        }
        const double dx = nj->x - ni->x, dy = nj->y - ni->y, dz = nj->z - ni->z;
        const double len2 = dx * dx + dy * dy + dz * dz;
        if (len2 < 1e-12) {
            continue;
        }
        for (const Node &n : in.nodes) {
            if (n.id == b.nodeI || n.id == b.nodeJ) {
                continue;
            }
            const double t = ((n.x - ni->x) * dx + (n.y - ni->y) * dy + (n.z - ni->z) * dz) / len2;
            if (t <= 1e-6 || t >= 1.0 - 1e-6) {
                continue;
            }
            const double px = ni->x + t * dx - n.x, py = ni->y + t * dy - n.y, pz = ni->z + t * dz - n.z;
            if (std::sqrt(px * px + py * py + pz * pz) < 1e-4) {
                ctx.add(Severity::Warning, QStringLiteral("NODE_ON_BAR"), QStringLiteral("O nó %1 está sobre a barra %2 sem estar ligado a ela (divida a barra).").arg(n.id).arg(b.id), Kind::Node, n.id);
                break;
            }
        }
    }

    // ---- apoios / mecanismos --------------------------------------------------------------------------------
    QHash<int, int> restrained;   // nos -> graus restringidos
    int totalRestrained = 0;
    for (const auto &s : in.supports) {
        int c = 0;
        for (int i = 0; i < 6; ++i) {
            c += s.restraints[i] ? 1 : 0;
        }
        if (c > 0) {
            restrained[s.nodeId] += c;
            totalRestrained += c;
        }
    }
    if (totalRestrained == 0) {
        ctx.add(Severity::Error, QStringLiteral("NO_SUPPORTS"), QStringLiteral("O modelo não tem apoios: a estrutura é instável."));
    } else if (totalRestrained < 6) {
        ctx.add(Severity::Error, QStringLiteral("FEW_SUPPORTS"), QStringLiteral("Há apenas %1 grau(s) de liberdade restringido(s) no modelo; são necessários ao menos 6 para impedir movimento de corpo rígido.").arg(totalRestrained));
    }
    for (const auto &s : in.supports) {
        if (!nodeById.contains(s.nodeId)) {
            ctx.add(Severity::Warning, QStringLiteral("SUPPORT_NODE_MISSING"), QStringLiteral("Apoio em nó inexistente (%1).").arg(s.nodeId));
        }
    }
    // componentes conexos sem restricao suficiente
    {
        QSet<int> visited;
        for (const Node &start : in.nodes) {
            if (visited.contains(start.id) || degree.value(start.id, 0) == 0) {
                continue;
            }
            QVector<int> stack {start.id};
            QVector<int> component;
            visited.insert(start.id);
            while (!stack.isEmpty()) {
                const int current = stack.takeLast();
                component.append(current);
                for (const int next : adjacency.value(current)) {
                    if (!visited.contains(next)) {
                        visited.insert(next);
                        stack.append(next);
                    }
                }
            }
            int dofs = 0;
            for (const int id : component) {
                dofs += restrained.value(id, 0);
            }
            if (dofs < 6) {
                ctx.add(Severity::Error, QStringLiteral("COMPONENT_UNSUPPORTED"),
                        QStringLiteral("Trecho do modelo com o nó %1 (e %2 outro(s)) tem só %3 grau(s) de liberdade restringido(s): mecanismo / estrutura solta.").arg(start.id).arg(component.size() - 1).arg(dofs),
                        Kind::Node, start.id);
            }
        }
    }

    // ---- secoes e materiais ---------------------------------------------------------------------------------
    for (const auto &s : in.sections) {
        if (s.area <= 0.0 || s.iz <= 0.0 || s.iy <= 0.0) {
            ctx.add(Severity::Error, QStringLiteral("SECTION_PROPS"), QStringLiteral("Seção \"%1\" com área ou inércias nulas.").arg(s.name));
        } else if (s.j <= 0.0) {
            ctx.add(Severity::Warning, QStringLiteral("SECTION_J"), QStringLiteral("Seção \"%1\" sem constante de torção (J = 0).").arg(s.name));
        }
    }
    double maxE = 0.0;
    for (const auto &m : in.materials) {
        maxE = std::max(maxE, m.youngModulus);
        if (m.youngModulus <= 0.0 || m.shearModulus <= 0.0) {
            ctx.add(Severity::Error, QStringLiteral("MATERIAL_PROPS"), QStringLiteral("Material \"%1\" com E ou G nulo.").arg(m.name));
        }
    }

    // ---- cargas ---------------------------------------------------------------------------------------------
    QSet<int> barIds;
    for (const Bar &b : in.bars) {
        barIds.insert(b.id);
    }
    QSet<QString> caseNames;
    for (const auto &c : in.extras.loadCases) {
        caseNames.insert(c.name);
    }
    const bool anyLoad = !in.nodalLoads.isEmpty() || !in.memberLoads.isEmpty() || !in.extras.memberMoments.isEmpty()
                         || std::any_of(in.extras.loadCases.begin(), in.extras.loadCases.end(), [](const auto &c) { return c.selfWeight; });
    if (!anyLoad) {
        ctx.add(Severity::Warning, QStringLiteral("NO_LOADS"), QStringLiteral("Não há cargas no modelo (nem peso próprio)."));
    }
    for (const auto &l : in.nodalLoads) {
        if (!nodeById.contains(l.nodeId)) {
            ctx.add(Severity::Warning, QStringLiteral("LOAD_NODE_MISSING"), QStringLiteral("Carga nodal em nó inexistente (%1).").arg(l.nodeId));
        }
        if (!l.loadCase.isEmpty() && !caseNames.contains(l.loadCase)) {
            ctx.add(Severity::Warning, QStringLiteral("LOAD_CASE_MISSING"), QStringLiteral("Carga nodal no nó %1 usa o caso inexistente \"%2\".").arg(l.nodeId).arg(l.loadCase), Kind::Node, l.nodeId);
        }
    }
    for (const auto &l : in.memberLoads) {
        if (!barIds.contains(l.memberId)) {
            ctx.add(Severity::Warning, QStringLiteral("LOAD_BAR_MISSING"), QStringLiteral("Carga distribuída em barra inexistente (%1).").arg(l.memberId));
        }
        if (!l.loadCase.isEmpty() && !caseNames.contains(l.loadCase)) {
            ctx.add(Severity::Warning, QStringLiteral("LOAD_CASE_MISSING"), QStringLiteral("Carga na barra %1 usa o caso inexistente \"%2\".").arg(l.memberId).arg(l.loadCase), Kind::Bar, l.memberId);
        }
    }
    for (const auto &c : in.extras.combinations) {
        for (const auto &term : c.factors) {
            if (!caseNames.contains(term.first)) {
                ctx.add(Severity::Error, QStringLiteral("COMBO_CASE_MISSING"), QStringLiteral("A combinação \"%1\" usa o caso inexistente \"%2\".").arg(c.name).arg(term.first));
            }
        }
    }
    const bool hasUltimate = std::any_of(in.extras.combinations.begin(), in.extras.combinations.end(), [](const auto &c) { return !c.name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive); });
    if (in.extras.combinations.isEmpty()) {
        ctx.add(Severity::Info, QStringLiteral("NO_COMBINATIONS"), QStringLiteral("Não há combinações definidas: o dimensionamento usa somente a combinação implícita de todos os casos."));
    } else if (!hasUltimate) {
        ctx.add(Severity::Warning, QStringLiteral("NO_ULTIMATE"), QStringLiteral("Todas as combinações são de serviço (ELS): não há combinação última para dimensionar."));
    }

    // ---- unidades -------------------------------------------------------------------------------------------
    if (maxE > 1e10) {
        double maxLoad = 0.0;
        for (const auto &l : in.nodalLoads) {
            maxLoad = std::max({maxLoad, std::abs(l.fx), std::abs(l.fy), std::abs(l.fz)});
        }
        if (maxLoad > 0.0 && maxLoad < 200.0) {
            ctx.add(Severity::Warning, QStringLiteral("UNITS"), QStringLiteral("E está em Pa (N) mas as maiores cargas nodais são de poucas unidades (%1): confirme se as cargas estão em N ou em kN.").arg(maxLoad, 0, 'f', 1));
        }
    }
    return ctx.issues;
}

} // namespace Structura::App::Services
