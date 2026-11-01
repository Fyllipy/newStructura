#include "app/services/MemberFabrication.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace Structura::App::Services {

namespace {

constexpr double kTol = 1e-6;

struct EndData {
    double gap {0.0};
    QVector<FabMember::Hole> holes;
    QVector<FabMember::Attached> attached;
    QVector<FabMember::Cope> copes;
    double angle {0.0};
};

/// Furacao e afastamento do perfil de uma ligacao, em coordenadas da peca (x desde a extremidade de corte).
EndData endOf(const SheetBuilder::ConnectionDetail &detail, char end)
{
    EndData out;
    const auto &connection = detail.connection;
    const FabAssembly fab = FabricationModel::fromConnection(connection);
    // afastamento: menor coordenada s das pecas do perfil que ficam do lado positivo (o perfil ligado ao nó); a outra metade (emenda) e outro perfil
    double gap = 1e300;
    for (const auto &part : fab.parts) {
        if (part.kind != FabPart::Kind::MemberPlate || part.origin[0] < -kTol) {
            continue;
        }
        gap = std::min(gap, part.origin[0]);
    }
    if (gap > 1e299) {
        gap = 0.0;
    }
    out.gap = gap;
    out.angle = fab.endAngleDeg;
    for (const auto &c : fab.copes) {
        out.copes.push_back({end, c.top, c.length, c.depth});
    }
    for (const auto &part : fab.parts) {
        if (part.kind == FabPart::Kind::Plate) {
            FabMember::Attached a;
            a.plateMark = QStringLiteral("L%1-%2").arg(connection.id).arg(part.mark);
            a.name = part.name;
            a.end = end;
            a.connectionId = connection.id;
            a.quantity = part.quantity;
            a.welded = part.weldedToMember;
            a.massKg = part.massKg;
            out.attached.push_back(a);
            continue;
        }
        if (part.origin[0] < -kTol) {
            continue;
        }
        // a coordenada s do plano e o primeiro eixo do plano (0) em todas as pecas planas do perfil; a outra e t (alma) ou u (mesa)
        const bool web = part.thicknessAxis == 2;
        for (const auto &hole : part.holes) {
            FabMember::Hole h;
            h.end = end;
            h.face = web ? QStringLiteral("alma") : QStringLiteral("mesa");
            h.x = hole.a + part.origin[0] - gap;
            h.y = hole.b + part.origin[1];
            h.diameter = hole.diameter;
            h.connectionId = connection.id;
            out.holes.push_back(h);
        }
    }
    return out;
}

QString signature(const QVector<FabMember::Hole> &holes, const QVector<FabMember::Attached> &attached, const QVector<FabMember::Cope> &copes = {}, double angleI = 0.0, double angleJ = 0.0)
{
    std::vector<std::tuple<char, QString, long, long, long>> items;
    for (const auto &h : holes) {
        items.emplace_back(h.end, h.face, std::lround(h.x * 10.0), std::lround(h.y * 10.0), std::lround(h.diameter * 10.0));
    }
    std::sort(items.begin(), items.end());
    QString key;
    for (const auto &i : items) {
        key += QStringLiteral("%1%2:%3,%4,%5;").arg(QLatin1Char(std::get<0>(i)), std::get<1>(i)).arg(std::get<2>(i)).arg(std::get<3>(i)).arg(std::get<4>(i));
    }
    QStringList names;
    for (const auto &a : attached) {
        names << QStringLiteral("%1%2x%3").arg(QLatin1Char(a.end), a.name).arg(a.quantity);
    }
    names.sort();
    QString cutKey;
    for (const auto &c : copes) {
        cutKey += QStringLiteral("%1%2%3x%4;").arg(QLatin1Char(c.end), c.top ? QLatin1String("t") : QLatin1String("b")).arg(std::lround(c.length * 10.0)).arg(std::lround(c.depth * 10.0));
    }
    return key + QLatin1Char('|') + names.join(QLatin1Char(',')) + QLatin1Char('|') + cutKey + QStringLiteral("@%1,%2").arg(std::lround(angleI * 10.0)).arg(std::lround(angleJ * 10.0));
}

} // namespace

QVector<FabMember> MemberFabrication::build(const SheetBuilder::Input &input)
{
    QMap<int, SheetBuilder::Node> nodes;
    for (const auto &n : input.nodes) {
        nodes.insert(n.id, n);
    }
    auto isVertical = [&](const SheetBuilder::Node &a, const SheetBuilder::Node &b) { return std::abs(a.x - b.x) < 1e-3 && std::abs(a.y - b.y) < 1e-3 && std::abs(a.z - b.z) > 1e-3; };
    auto isHorizontal = [&](const SheetBuilder::Node &a, const SheetBuilder::Node &b) { return std::abs(a.z - b.z) < 1e-3; };

    struct Group {
        FabMember member;
        QString prefix;
    };
    std::map<std::tuple<QString, QString, long, QString>, int> index;
    QVector<Group> groups;
    for (const auto &bar : input.bars) {
        const auto a = nodes.value(bar.nodeI), b = nodes.value(bar.nodeJ);
        const double nodeLength = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)) * 1000.0;
        EndData endI, endJ;
        for (const auto &detail : input.connections) {
            if (detail.connection.memberId != bar.id) {
                continue;
            }
            const bool atEnd = detail.connection.atEnd;
            EndData data = endOf(detail, atEnd ? 'J' : 'I');
            EndData &target = atEnd ? endJ : endI;
            target.gap = std::max(target.gap, data.gap);
            target.holes += data.holes;
            target.attached += data.attached;
            target.copes += data.copes;
            target.angle = std::max(target.angle, data.angle);
        }
        QString prefix = QStringLiteral("D");
        if (bar.memberType == 2) {
            prefix = QStringLiteral("T");
        } else if (bar.memberType == 1) {
            prefix = QStringLiteral("R");
        } else if (isVertical(a, b)) {
            prefix = QStringLiteral("P");
        } else if (isHorizontal(a, b)) {
            prefix = QStringLiteral("V");
        }
        FabMember m;
        m.section = bar.section;
        m.nodeLength = nodeLength;
        m.faceLength = nodeLength - bar.trimI - bar.trimJ;
        m.gapI = endI.gap;
        m.gapJ = endJ.gap;
        m.cutLength = m.faceLength - endI.gap - endJ.gap;
        m.massPerMeter = bar.massPerMeter;
        m.holes = endI.holes + endJ.holes;
        m.attached = endI.attached + endJ.attached;
        m.copes = endI.copes + endJ.copes;
        m.endAngleI = endI.angle;
        m.endAngleJ = endJ.angle;
        m.profile = bar;
        QVector<FabMember::Attached> welded;
        for (const auto &a : m.attached) {
            if (a.welded) {
                welded.push_back(a);
            }
        }
        const QString sig = signature(m.holes, welded, m.copes, m.endAngleI, m.endAngleJ);   // corte, furacao e chapas soldadas ao perfil definem a marca
        const auto key = std::make_tuple(prefix, bar.section, std::lround(m.cutLength), sig);
        const auto it = index.find(key);
        if (it == index.end()) {
            index[key] = groups.size();
            m.bars = {bar.id};
            m.quantity = 1;
            groups.push_back({m, prefix});
        } else {
            groups[it->second].member.bars.push_back(bar.id);
            ++groups[it->second].member.quantity;
        }
    }
    // marcas em ordem estavel (prefixo, perfil, comprimento)
    std::vector<int> order(static_cast<std::size_t>(groups.size()));
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    std::sort(order.begin(), order.end(), [&](int x, int y) {
        const auto &a = groups[x], &b = groups[y];
        return std::make_tuple(a.prefix, a.member.section, std::lround(a.member.cutLength), signature(a.member.holes, {}, a.member.copes, a.member.endAngleI, a.member.endAngleJ))
               < std::make_tuple(b.prefix, b.member.section, std::lround(b.member.cutLength), signature(b.member.holes, {}, b.member.copes, b.member.endAngleI, b.member.endAngleJ));
    });
    QMap<QString, int> counters;
    QVector<FabMember> result;
    for (const int i : order) {
        FabMember m = groups[i].member;
        m.mark = QStringLiteral("%1%2").arg(groups[i].prefix).arg(++counters[groups[i].prefix]);
        std::sort(m.bars.begin(), m.bars.end());
        m.unitMassKg = m.massPerMeter * m.cutLength / 1000.0;
        m.assemblyMassKg = m.unitMassKg;
        for (const auto &a : m.attached) {
            if (a.welded) {
                m.assemblyMassKg += a.massKg * a.quantity;
            }
        }
        result.push_back(std::move(m));
    }
    return result;
}

} // namespace Structura::App::Services
