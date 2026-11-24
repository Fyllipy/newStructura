#include <QtTest/QtTest>

#include "app/services/StairGenerator.h"

#include <cmath>
#include <set>

using Structura::App::Services::StairGenerator;

namespace {

StairGenerator::Params baseParams(StairGenerator::Type type, StairGenerator::Layout layout)
{
    StairGenerator::Params p;
    p.type = type;
    p.layout = layout;
    p.baseZ = 0.0;
    p.topZ = 3.0;
    p.riser = 0.18;
    p.tread = 0.28;
    p.width = 1.0;
    return p;
}

int count(const StairGenerator::Result &r, StairGenerator::Role role)
{
    int n = 0;
    for (const auto &b : r.bars) {
        n += b.role == role ? 1 : 0;
    }
    return n;
}

void checkIntegrity(const StairGenerator::Result &r)
{
    std::set<std::pair<int, int>> seen;
    for (const auto &b : r.bars) {
        QVERIFY(b.a >= 0 && b.b >= 0);
        QVERIFY(b.a < static_cast<int>(r.nodes.size()) && b.b < static_cast<int>(r.nodes.size()));
        QVERIFY(b.a != b.b);
        QVERIFY(seen.insert({std::min(b.a, b.b), std::max(b.a, b.b)}).second);   // sem barras repetidas
        const auto &n1 = r.nodes[static_cast<std::size_t>(b.a)];
        const auto &n2 = r.nodes[static_cast<std::size_t>(b.b)];
        QVERIFY(std::hypot(std::hypot(n1.x - n2.x, n1.y - n2.y), n1.z - n2.z) > 1e-6);
    }
}

} // namespace

class TestStairGenerator : public QObject
{
    Q_OBJECT

private slots:
    void straightTwoStringers()
    {
        const auto r = StairGenerator::generate(baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Two));
        QCOMPARE(r.risers, 17);
        QVERIFY(std::abs(r.riser - 3.0 / 17.0) < 1e-12);
        QCOMPARE(static_cast<int>(r.nodes.size()), 2 * 18);
        QCOMPARE(count(r, StairGenerator::Role::Stringer), 2 * 17);
        QCOMPARE(count(r, StairGenerator::Role::Tread), 16);
        QCOMPARE(static_cast<int>(r.baseNodes.size()), 2);
        QCOMPARE(static_cast<int>(r.topNodes.size()), 2);
        for (const int id : r.topNodes) {
            QVERIFY(std::abs(r.nodes[static_cast<std::size_t>(id)].z - 3.0) < 1e-9);
        }
        for (const int id : r.baseNodes) {
            QVERIFY(std::abs(r.nodes[static_cast<std::size_t>(id)].z) < 1e-9);
        }
        checkIntegrity(r);
    }

    void straightThreeAndCentre()
    {
        const auto three = StairGenerator::generate(baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Three));
        QCOMPARE(static_cast<int>(three.nodes.size()), 3 * 18);
        QCOMPARE(count(three, StairGenerator::Role::Stringer), 3 * 17);
        QCOMPARE(count(three, StairGenerator::Role::Tread), 2 * 16);
        checkIntegrity(three);

        const auto centre = StairGenerator::generate(baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Center));
        QCOMPARE(static_cast<int>(centre.nodes.size()), 18 + 2 * 16);   // longarina + 2 pontas de balanço por degrau
        QCOMPARE(count(centre, StairGenerator::Role::Stringer), 17);
        QCOMPARE(count(centre, StairGenerator::Role::Tread), 2 * 16);
        QCOMPARE(static_cast<int>(centre.baseNodes.size()), 1);
        checkIntegrity(centre);
    }

    void positionAndDirection()
    {
        auto p = baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Center);
        p.x0 = 2.0;
        p.y0 = 1.0;
        p.azimuthDeg = 90.0;
        p.baseZ = 3.0;
        p.topZ = 6.0;
        const auto r = StairGenerator::generate(p);
        const auto &first = r.nodes[static_cast<std::size_t>(r.baseNodes.front())];
        const auto &last = r.nodes[static_cast<std::size_t>(r.topNodes.front())];
        QVERIFY(std::abs(first.x - 2.0) < 1e-9 && std::abs(first.y - 1.0) < 1e-9 && std::abs(first.z - 3.0) < 1e-9);
        QVERIFY(std::abs(last.x - 2.0) < 1e-9);                       // sobe ao longo de +Y
        QVERIFY(std::abs(last.y - (1.0 + r.risers * r.tread)) < 1e-9);
        QVERIFY(std::abs(last.z - 6.0) < 1e-9);
    }

    void endByPoint()
    {
        auto p = baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Two);
        p.endByPoint = true;
        p.x1 = 3.0;
        p.y1 = 4.0;   // 5 m em planta
        const auto r = StairGenerator::generate(p);
        const auto &last = r.nodes[static_cast<std::size_t>(r.topNodes.front())];
        const auto &first = r.nodes[static_cast<std::size_t>(r.baseNodes.front())];
        const double cx = 0.5 * (r.nodes[static_cast<std::size_t>(r.topNodes[0])].x + r.nodes[static_cast<std::size_t>(r.topNodes[1])].x);
        const double cy = 0.5 * (r.nodes[static_cast<std::size_t>(r.topNodes[0])].y + r.nodes[static_cast<std::size_t>(r.topNodes[1])].y);
        QVERIFY(std::abs(cx - 3.0) < 1e-9 && std::abs(cy - 4.0) < 1e-9);
        QVERIFY(std::abs(r.tread - 5.0 / 17.0) < 1e-12);
        Q_UNUSED(first)
        Q_UNUSED(last)
    }

    void lShapedHasLanding()
    {
        auto p = baseParams(StairGenerator::Type::LShaped, StairGenerator::Layout::Two);
        p.topZ = 3.2;
        const auto r = StairGenerator::generate(p);
        QCOMPARE(r.risers, 18);
        QCOMPARE(r.firstFlight, 9);
        QVERIFY(count(r, StairGenerator::Role::LandingBeam) >= 4);
        QVERIFY(count(r, StairGenerator::Role::LandingTread) >= 1);
        for (const int id : r.topNodes) {
            QVERIFY(std::abs(r.nodes[static_cast<std::size_t>(id)].z - 3.2) < 1e-9);
        }
        // a segunda lance sai na direção +Y (curva à esquerda): o fim fica acima do início em Y
        QVERIFY(r.nodes[static_cast<std::size_t>(r.topNodes.front())].y > 1.0);
        checkIntegrity(r);

        // curva à direita espelha em Y
        p.turnLeft = false;
        const auto right = StairGenerator::generate(p);
        QVERIFY(right.nodes[static_cast<std::size_t>(right.topNodes.front())].y < -1.0);
        QCOMPARE(right.bars.size(), r.bars.size());
    }

    void uShapedReturns()
    {
        auto p = baseParams(StairGenerator::Type::UShaped, StairGenerator::Layout::Three);
        p.topZ = 3.0;
        p.firstFlightSteps = 8;
        const auto r = StairGenerator::generate(p);
        QCOMPARE(r.firstFlight, 8);
        // o 2º lance volta: o fim fica a menos de 1 profundidade de degrau do patamar e lateralmente deslocado
        const auto &top = r.nodes[static_cast<std::size_t>(r.topNodes.front())];
        QVERIFY(top.x < 8 * r.tread);
        QVERIFY(std::abs(std::abs(top.y) - 1.0) > 0.0 || std::abs(top.y) > 0.5);
        for (const int id : r.topNodes) {
            QVERIFY(std::abs(r.nodes[static_cast<std::size_t>(id)].z - 3.0) < 1e-9);
        }
        QVERIFY(count(r, StairGenerator::Role::LandingBeam) >= 4);
        checkIntegrity(r);
    }

    void spiralAngle()
    {
        auto p = baseParams(StairGenerator::Type::Spiral, StairGenerator::Layout::Two);
        p.totalAngleDeg = 270.0;
        p.innerRadius = 0.2;
        p.outerRadius = 1.2;
        p.x0 = 5.0;
        p.y0 = -2.0;
        const auto r = StairGenerator::generate(p);
        QCOMPARE(r.risers, 17);
        const auto &outerTop = r.nodes[static_cast<std::size_t>(r.topNodes.back())];
        const double angle = std::atan2(outerTop.y - p.y0, outerTop.x - p.x0) * 180.0 / 3.14159265358979323846;
        QVERIFY(std::abs(angle - (270.0 - 360.0)) < 1e-6);     // 270° anti-horários = -90°
        QVERIFY(std::abs(std::hypot(outerTop.x - p.x0, outerTop.y - p.y0) - 1.2) < 1e-9);
        checkIntegrity(r);

        p.turnLeft = false;
        const auto cw = StairGenerator::generate(p);
        const auto &cwTop = cw.nodes[static_cast<std::size_t>(cw.topNodes.back())];
        QVERIFY(std::abs(std::atan2(cwTop.y - p.y0, cwTop.x - p.x0) * 180.0 / 3.14159265358979323846 - 90.0) < 1e-6);
    }

    void invalidInput()
    {
        auto p = baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Two);
        p.topZ = p.baseZ;
        const auto r = StairGenerator::generate(p);
        QVERIFY(r.nodes.empty());
        QVERIFY(!r.warnings.empty());
    }

    void blondelWarning()
    {
        auto p = baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Two);
        QVERIFY(StairGenerator::generate(p).warnings.empty());
        p.tread = 0.45;
        QVERIFY(!StairGenerator::generate(p).warnings.empty());
    }

    void loadsPerCase()
    {
        auto p = baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Two);
        p.loads.push_back({StairGenerator::LoadKind::TreadArea, 3.0, "Sobrecarga"});
        p.loads.push_back({StairGenerator::LoadKind::TreadArea, 1.0, "Sobrecarga"});     // soma no mesmo caso
        p.loads.push_back({StairGenerator::LoadKind::TreadArea, 0.5, "Acidental"});      // outro caso, mesma barra
        p.loads.push_back({StairGenerator::LoadKind::HandrailLine, 0.8, "Permanente"});
        p.loads.push_back({StairGenerator::LoadKind::StringerLine, 0.2, "Permanente"});
        const auto r = StairGenerator::generate(p);
        const auto specs = StairGenerator::memberLoads(p, r);

        int treadSobrecarga = 0, treadAcidental = 0, stringerPerm = 0;
        for (const auto &s : specs) {
            const auto &bar = r.bars[static_cast<std::size_t>(s.bar)];
            if (bar.role == StairGenerator::Role::Tread && s.loadCase == "Sobrecarga") {
                ++treadSobrecarga;
                QVERIFY(std::abs(s.qz - (-4000.0 * 0.28)) < 1e-6);
            } else if (bar.role == StairGenerator::Role::Tread && s.loadCase == "Acidental") {
                ++treadAcidental;
                QVERIFY(std::abs(s.qz - (-500.0 * 0.28)) < 1e-6);
            } else if (bar.role == StairGenerator::Role::Stringer && s.loadCase == "Permanente") {
                ++stringerPerm;
                QVERIFY(bar.lateral);
                QVERIFY(std::abs(s.qz - (-1000.0)) < 1e-6);   // 0,8 + 0,2 kN/m
            }
        }
        QCOMPARE(treadSobrecarga, 16);
        QCOMPARE(treadAcidental, 16);
        QCOMPARE(stringerPerm, 2 * 17);
        QCOMPARE(static_cast<int>(specs.size()), 16 + 16 + 34);

        // com uma só longarina central não há guarda-corpo
        auto c = baseParams(StairGenerator::Type::Straight, StairGenerator::Layout::Center);
        c.loads.push_back({StairGenerator::LoadKind::HandrailLine, 0.8, "Permanente"});
        const auto centre = StairGenerator::generate(c);
        QVERIFY(StairGenerator::memberLoads(c, centre).empty());
        QVERIFY(!centre.warnings.empty());
    }

    void landingAreaLoadBalance()
    {
        // toda a sobrecarga do patamar (kN/m2 x area) é distribuída entre vigas secundárias e bordas paralelas a elas
        auto p = baseParams(StairGenerator::Type::LShaped, StairGenerator::Layout::Two);
        p.loads.push_back({StairGenerator::LoadKind::LandingArea, 4.0, "Sobrecarga"});
        const auto r = StairGenerator::generate(p);
        double total = 0.0;
        for (const auto &s : StairGenerator::memberLoads(p, r)) {
            const auto &bar = r.bars[static_cast<std::size_t>(s.bar)];
            const auto &a = r.nodes[static_cast<std::size_t>(bar.a)];
            const auto &b = r.nodes[static_cast<std::size_t>(bar.b)];
            total += -s.qz * std::hypot(std::hypot(a.x - b.x, a.y - b.y), a.z - b.z);
        }
        QVERIFY(std::abs(total - 4000.0 * 1.0 * 1.0) < 1e-3);   // patamar 1,00 x 1,00 m
    }
};

QTEST_APPLESS_MAIN(TestStairGenerator)
#include "TestStairGenerator.moc"
