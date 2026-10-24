#include "app/services/IntegrityService.h"

#include "app/services/ConnectionService.h"

#include <QHash>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Structura::App::Services {

namespace {

using Row = IntegrityService::Row;

struct Vec {
    double x {0.0}, y {0.0}, z {0.0};
};

double cross2(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }

/// Pontos (indices) na fronteira do envoltorio convexo, inclusive os colineares sobre as arestas.
QSet<int> hullBoundary(const std::vector<std::pair<double, double>> &pts)
{
    QSet<int> boundary;
    const int n = static_cast<int>(pts.size());
    if (n <= 3) {
        for (int i = 0; i < n; ++i) {
            boundary.insert(i);
        }
        return boundary;
    }
    std::vector<int> order(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        order[static_cast<std::size_t>(i)] = i;
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pts[static_cast<std::size_t>(a)] < pts[static_cast<std::size_t>(b)]; });
    std::vector<int> hull;
    auto turn = [&](int a, int b, int c) {
        return cross2(pts[static_cast<std::size_t>(b)].first - pts[static_cast<std::size_t>(a)].first, pts[static_cast<std::size_t>(b)].second - pts[static_cast<std::size_t>(a)].second,
                      pts[static_cast<std::size_t>(c)].first - pts[static_cast<std::size_t>(a)].first, pts[static_cast<std::size_t>(c)].second - pts[static_cast<std::size_t>(a)].second);
    };
    for (int pass = 0; pass < 2; ++pass) {
        const std::size_t start = hull.size();
        for (int k = 0; k < n; ++k) {
            const int i = pass == 0 ? order[static_cast<std::size_t>(k)] : order[static_cast<std::size_t>(n - 1 - k)];
            while (hull.size() >= start + 2 && turn(hull[hull.size() - 2], hull.back(), i) <= 1e-12) {
                hull.pop_back();
            }
            hull.push_back(i);
        }
        hull.pop_back();
    }
    for (const int i : hull) {
        boundary.insert(i);
    }
    // pontos colineares sobre as arestas
    for (int i = 0; i < n; ++i) {
        if (boundary.contains(i)) {
            continue;
        }
        for (std::size_t e = 0; e < hull.size(); ++e) {
            const int a = hull[e], b = hull[(e + 1) % hull.size()];
            const auto &pa = pts[static_cast<std::size_t>(a)], &pb = pts[static_cast<std::size_t>(b)], &pi = pts[static_cast<std::size_t>(i)];
            const double length = std::hypot(pb.first - pa.first, pb.second - pa.second);
            if (length < 1e-9) {
                continue;
            }
            const double distance = std::abs(cross2(pb.first - pa.first, pb.second - pa.second, pi.first - pa.first, pi.second - pa.second)) / length;
            const double along = ((pi.first - pa.first) * (pb.first - pa.first) + (pi.second - pa.second) * (pb.second - pa.second)) / length;
            if (distance < 1e-6 && along > -1e-9 && along < length + 1e-9) {
                boundary.insert(i);
                break;
            }
        }
    }
    return boundary;
}

QString atLevel(int member, double z)
{
    return QStringLiteral("Pilar %1, nível z = %2 m").arg(member).arg(z, 0, 'f', 2);
}

} // namespace

double IntegrityService::minimumTieForce(double columnForceN, bool edgeWithoutSlab)
{
    return std::max(0.01 * columnForceN, (edgeWithoutSlab ? 45.0 : 75.0) * 1000.0);
}

QVector<IntegrityService::Row> IntegrityService::run(const Input &in)
{
    QVector<Row> rows;
    QHash<int, Node> nodeById;
    for (const Node &n : in.nodes) {
        nodeById.insert(n.id, n);
    }
    auto dirOf = [&](const Bar &b, int fromNode) -> Vec {
        const auto a = nodeById.value(b.nodeI), c = nodeById.value(b.nodeJ);
        Vec v {c.x - a.x, c.y - a.y, c.z - a.z};
        if (fromNode == b.nodeJ) {
            v = {-v.x, -v.y, -v.z};
        }
        return v;
    };
    auto lengthOf = [](const Vec &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };

    QHash<int, QVector<int>> barsAtNode;   // indices em in.bars
    for (int i = 0; i < in.bars.size(); ++i) {
        barsAtNode[in.bars[i].nodeI].append(i);
        barsAtNode[in.bars[i].nodeJ].append(i);
    }
    auto isVertical = [&](const Bar &b) {
        const Vec v = dirOf(b, b.nodeI);
        const double l = lengthOf(v);
        return l > 1e-9 && std::abs(v.z) / l >= in.options.verticalTolerance;
    };
    auto isHorizontal = [&](const Bar &b) {
        const Vec v = dirOf(b, b.nodeI);
        const double l = lengthOf(v);
        return l > 1e-9 && std::abs(v.z) / l <= in.options.horizontalTolerance && std::hypot(v.x, v.y) / l > 0.5;
    };
    QHash<int, double> compressionOf;
    for (const ColumnLoad &c : in.columnLoads) {
        compressionOf[c.memberId] = std::max(compressionOf.value(c.memberId, 0.0), c.compression);
    }
    auto connectionOf = [&](int memberId, int nodeId) -> const ProjectData::NodeConnection * {
        for (const auto &c : in.connections) {
            if (c.memberId == memberId && c.nodeId == nodeId) {
                return &c;
            }
        }
        return nullptr;
    };

    // nos de nivel: extremidades de pilares fora dos apoios
    struct Level {
        int node {0};
        int column {0};
        double z {0.0};
        double columnForce {0.0};
    };
    std::vector<Level> levels;
    for (int i = 0; i < in.bars.size(); ++i) {
        const Bar &col = in.bars[i];
        if (!isVertical(col)) {
            continue;
        }
        for (const int nodeId : {col.nodeI, col.nodeJ}) {
            const Node node = nodeById.value(nodeId);
            if (node.supported) {
                continue;
            }
            double force = compressionOf.value(col.id, 0.0);
            // o pilar de cima tambem chega ao no: vale a maior forca entre os pilares que o tocam
            for (const int k : barsAtNode.value(nodeId)) {
                if (isVertical(in.bars[k])) {
                    force = std::max(force, compressionOf.value(in.bars[k].id, 0.0));
                }
            }
            if (std::none_of(levels.begin(), levels.end(), [&](const Level &l) { return l.node == nodeId; })) {
                levels.push_back({nodeId, col.id, node.z, force});
            }
        }
    }

    // pilares de extremidade de cada nivel (fronteira do envoltorio convexo em planta)
    QSet<int> edgeNodes;
    {
        QHash<long long, std::vector<int>> byZ;
        for (std::size_t i = 0; i < levels.size(); ++i) {
            byZ[std::llround(levels[i].z * 1000.0)].push_back(static_cast<int>(i));
        }
        for (auto it = byZ.constBegin(); it != byZ.constEnd(); ++it) {
            std::vector<std::pair<double, double>> pts;
            for (const int idx : it.value()) {
                const Node n = nodeById.value(levels[static_cast<std::size_t>(idx)].node);
                pts.emplace_back(n.x, n.y);
            }
            const QSet<int> boundary = hullBoundary(pts);
            for (int k = 0; k < static_cast<int>(it.value().size()); ++k) {
                if (boundary.contains(k)) {
                    edgeNodes.insert(levels[static_cast<std::size_t>(it.value()[static_cast<std::size_t>(k)])].node);
                }
            }
        }
    }

    // ---- 4.13.4 e 4.13.7: contencoes horizontais dos pilares -------------------------------------------------------------------------------
    for (const Level &level : levels) {
        std::vector<double> angles;   // direcoes das contencoes em planta, graus em [0, 180)
        std::vector<int> restraints;
        for (const int k : barsAtNode.value(level.node)) {
            const Bar &b = in.bars[k];
            if (!isHorizontal(b)) {
                continue;
            }
            const Vec v = dirOf(b, level.node);
            double a = std::atan2(v.y, v.x) * 180.0 / 3.14159265358979;
            a = std::fmod(a + 360.0, 180.0);
            restraints.push_back(k);
            if (std::none_of(angles.begin(), angles.end(), [&](double other) { const double d = std::abs(other - a); return std::min(d, 180.0 - d) < 15.0; })) {
                angles.push_back(a);
            }
        }
        Row travamento;
        travamento.clause = QStringLiteral("NBR 8800:2024, 4.13.4");
        travamento.description = QObject::tr("Pilar travado por contenções horizontais em pelo menos duas direções, em cada nível suportado (preferencialmente)");
        travamento.location = atLevel(level.column, level.z);
        travamento.demandKn = 2.0;
        travamento.resistanceKn = static_cast<double>(angles.size());
        travamento.eta = angles.size() >= 2 ? 0.0 : 1.0e3;
        travamento.ok = angles.size() >= 2;
        travamento.advisory = true;
        travamento.note = QObject::tr("%1 direção(ões) de contenção horizontal no nó (barras horizontais ou pouco inclinadas, como vigas, escoras e tesouras).").arg(static_cast<int>(angles.size()));
        rows.append(travamento);

        const bool edge = in.options.roofWithoutSlab && edgeNodes.contains(level.node);
        const double required = minimumTieForce(level.columnForce, edge);
        for (const int k : restraints) {
            const Bar &b = in.bars[k];
            Row tie;
            tie.clause = QStringLiteral("NBR 8800:2024, 4.13.7");
            tie.description = QObject::tr("Contenção horizontal %1: tração de cálculo ≥ max(1 %% da força no pilar ; %2 kN)").arg(b.id).arg(edge ? 45 : 75);
            tie.location = atLevel(level.column, level.z);
            tie.demandKn = required / 1000.0;
            tie.resistanceKn = b.ntRdKn;
            tie.eta = b.ntRdKn > 0.0 ? tie.demandKn / b.ntRdKn : -1.0;
            tie.ok = b.ntRdKn > 0.0 && tie.demandKn <= b.ntRdKn + 1e-9;
            tie.note = QObject::tr("Força do pilar N_Sd = %1 kN; força de tração exigida %2 kN (não precisa ser somada às demais ações).").arg(level.columnForce / 1000.0, 0, 'f', 1).arg(required / 1000.0, 0, 'f', 1);
            rows.append(tie);
            if (edge) {
                Row compression = tie;
                compression.description = QObject::tr("Contenção horizontal %1 (pilar de extremidade, piso ou cobertura sem laje): compressão de cálculo ≥ max(1 %% da força no pilar ; 45 kN)").arg(b.id);
                compression.resistanceKn = b.ncRdKn;
                compression.eta = b.ncRdKn > 0.0 ? compression.demandKn / b.ncRdKn : -1.0;
                compression.ok = b.ncRdKn > 0.0 && compression.demandKn <= b.ncRdKn + 1e-9;
                rows.append(compression);
            }
            // ligacao da contencao ao pilar
            const int nodeForConnection = level.node;
            if (const auto *connection = connectionOf(b.id, nodeForConnection)) {
                ConnectionService::Forces forces;
                forces.tension = required;
                forces.axial = required;
                const auto evaluation = ConnectionService::evaluateExact(*connection, forces);
                Row link;
                link.clause = QStringLiteral("NBR 8800:2024, 4.13.7");
                link.description = QObject::tr("Ligação da contenção %1: resistência à força de tração exigida").arg(b.id);
                link.location = atLevel(level.column, level.z);
                link.demandKn = required / 1000.0;
                link.resistanceKn = evaluation.maxRatio > 1e-9 ? required / 1000.0 / evaluation.maxRatio : -1.0;
                link.eta = evaluation.maxRatio;
                link.ok = evaluation.maxRatio <= 1.0 + 1e-9;
                link.note = QObject::tr("Verificação mais desfavorável: %1.").arg(evaluation.governing);
                rows.append(link);
            } else {
                Row missing;
                missing.clause = QStringLiteral("NBR 8800:2024, 4.13.7");
                missing.description = QObject::tr("Ligação da contenção %1 não definida: a resistência à tração da ligação não foi verificada").arg(b.id);
                missing.location = atLevel(level.column, level.z);
                missing.demandKn = required / 1000.0;
                missing.resistanceKn = -1.0;
                missing.eta = -1.0;
                missing.ok = false;
                missing.advisory = true;
                rows.append(missing);
            }
        }
    }

    // ---- 4.13.8: colapso progressivo --------------------------------------------------------------------------------------------------------
    if (in.options.progressiveCollapse) {
        // reacao vertical de cada viga em cada pilar: viga com a extremidade no no do pilar
        QHash<int, double> reactionOf;   // chave: indice da barra * 2 + (extremidade final)
        for (const EndForce &f : in.endForces) {
            if (!in.options.combination.isEmpty() && f.combo != in.options.combination) {
                continue;
            }
            const int key = f.memberId * 2 + (f.atEnd ? 1 : 0);
            reactionOf[key] = std::max(reactionOf.value(key, 0.0), std::abs(f.vertical));
        }
        QHash<long long, double> maxFloorReaction;   // (coluna x,y; nivel) -> maior reacao aplicada ao pilar no nivel
        for (const Level &level : levels) {
            const Node node = nodeById.value(level.node);
            double floorMax = 0.0;
            for (const int k : barsAtNode.value(level.node)) {
                const Bar &b = in.bars[k];
                if (!isHorizontal(b)) {
                    continue;
                }
                const double reaction = reactionOf.value(b.id * 2 + (b.nodeJ == level.node ? 1 : 0), 0.0);
                floorMax = std::max(floorMax, reaction);
                Row beam;
                beam.clause = QStringLiteral("NBR 8800:2024, 4.13.8");
                beam.description = QObject::tr("Viga %1: tração igual à reação vertical de cálculo (falha acidental do pilar)").arg(b.id);
                beam.location = atLevel(level.column, level.z);
                beam.demandKn = reaction / 1000.0;
                beam.resistanceKn = b.ntRdKn;
                beam.eta = b.ntRdKn > 0.0 ? beam.demandKn / b.ntRdKn : -1.0;
                beam.ok = b.ntRdKn > 0.0 && beam.demandKn <= b.ntRdKn + 1e-9;
                beam.note = in.options.combination.isEmpty() ? QObject::tr("Envoltória das combinações (adotar a de ações permanentes diretas e de uso e ocupação).")
                                                             : QObject::tr("Combinação %1.").arg(in.options.combination);
                rows.append(beam);
                if (const auto *connection = connectionOf(b.id, level.node)) {
                    ConnectionService::Forces forces;
                    forces.tension = reaction;
                    forces.axial = reaction;
                    const auto evaluation = ConnectionService::evaluateExact(*connection, forces);
                    Row link;
                    link.clause = QStringLiteral("NBR 8800:2024, 4.13.8");
                    link.description = QObject::tr("Ligação da viga %1 ao pilar: tração igual à reação vertical").arg(b.id);
                    link.location = atLevel(level.column, level.z);
                    link.demandKn = reaction / 1000.0;
                    link.resistanceKn = evaluation.maxRatio > 1e-9 ? reaction / 1000.0 / evaluation.maxRatio : -1.0;
                    link.eta = evaluation.maxRatio;
                    link.ok = evaluation.maxRatio <= 1.0 + 1e-9;
                    link.note = QObject::tr("Verificação mais desfavorável: %1.").arg(evaluation.governing);
                    rows.append(link);
                }
            }
            maxFloorReaction[std::llround(node.x * 1000.0) * 1000003LL + std::llround(node.y * 1000.0) * 1009LL + std::llround(node.z * 1000.0)] = floorMax;
        }
        // emendas de pilares: maior reacao aplicada ao pilar por um pavimento entre a emenda e a emenda imediatamente abaixo
        for (const auto &connection : in.connections) {
            const bool splice = connection.type == ProjectData::NodeConnection::kSplice || connection.type == 136;
            if (!splice) {
                continue;
            }
            const auto bar = std::find_if(in.bars.begin(), in.bars.end(), [&](const Bar &b) { return b.id == connection.memberId; });
            if (bar == in.bars.end() || !isVertical(*bar)) {
                continue;
            }
            const Node spliceNode = nodeById.value(connection.nodeId);
            // linha do pilar e emendas abaixo na mesma linha
            double zBelow = -1e300;
            for (const auto &other : in.connections) {
                const bool otherSplice = other.type == ProjectData::NodeConnection::kSplice || other.type == 136;
                if (!otherSplice || other.nodeId == connection.nodeId) {
                    continue;
                }
                const Node n = nodeById.value(other.nodeId);
                if (std::hypot(n.x - spliceNode.x, n.y - spliceNode.y) < 1e-6 && n.z < spliceNode.z - 1e-6) {
                    zBelow = std::max(zBelow, n.z);
                }
            }
            double maxReaction = 0.0;
            for (const Level &level : levels) {
                const Node n = nodeById.value(level.node);
                if (std::hypot(n.x - spliceNode.x, n.y - spliceNode.y) < 1e-6 && n.z <= spliceNode.z + 1e-6 && n.z > zBelow + 1e-6) {
                    maxReaction = std::max(maxReaction, maxFloorReaction.value(std::llround(n.x * 1000.0) * 1000003LL + std::llround(n.y * 1000.0) * 1009LL + std::llround(n.z * 1000.0), 0.0));
                }
            }
            ConnectionService::Forces forces;
            forces.tension = maxReaction;
            forces.axial = maxReaction;
            const auto evaluation = ConnectionService::evaluateExact(connection, forces);
            Row row;
            row.clause = QStringLiteral("NBR 8800:2024, 4.13.8");
            row.description = QObject::tr("Emenda do pilar %1: tração igual à maior reação de cálculo aplicada por um pavimento entre a emenda e a emenda abaixo").arg(connection.memberId);
            row.location = QObject::tr("Pilar %1, emenda em z = %2 m").arg(connection.memberId).arg(spliceNode.z, 0, 'f', 2);
            row.demandKn = maxReaction / 1000.0;
            row.resistanceKn = evaluation.maxRatio > 1e-9 ? maxReaction / 1000.0 / evaluation.maxRatio : -1.0;
            row.eta = evaluation.maxRatio;
            row.ok = evaluation.maxRatio <= 1.0 + 1e-9;
            row.note = QObject::tr("Verificação mais desfavorável: %1.").arg(evaluation.governing);
            rows.append(row);
        }
    }
    return rows;
}

} // namespace Structura::App::Services
