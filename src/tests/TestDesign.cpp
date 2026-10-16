#include "app/SolverBridge.h"
#include "app/services/AreaLoadService.h"
#include "app/services/DesignService.h"
#include "app/services/StructuralChecks.h"
#include "design/BarDesign.h"
#include "design/Connections.h"
#include "design/DesignSection.h"
#include "design/Nbr14762.h"
#include "design/Nbr8800.h"
#include "viz/structure/SectionShapes.h"

#include <QFile>
#include <QtTest>

using namespace Structura::Design;
using Structura::Visualization::SectionShapes;

namespace {

DesignSection make(const QString &family, const QMap<QString, double> &params, double j = 0.0)
{
    QString err;
    const auto shape = SectionShapes::build(family, params, false, &err);
    if (!shape) {
        qFatal("build failed: %s", qPrintable(err));
    }
    ProfileSpec spec;
    spec.family = family.toStdString();
    for (auto it = params.begin(); it != params.end(); ++it) {
        spec.params[it.key().toStdString()] = it.value();
    }
    std::vector<Loop> loops;
    for (const auto &l : shape->loops) {
        loops.emplace_back(l.begin(), l.end());
    }
    return makeDesignSection(spec, loops, j);
}

} // namespace

class TestDesign : public QObject
{
    Q_OBJECT
private slots:
    void connectionsBoltsAndWelds()
    {
        namespace C = Structura::Design::Connections;
        C::BoltedInput in;
        in.grade = C::boltGrades()[2];   // A325
        in.db = 20.0;
        in.nx = 2;
        in.ny = 2;
        in.sx = in.sy = 70.0;
        in.plateThickness = 12.5;
        in.plateFu = 400.0;
        in.endDistance = in.edgeDistance = 40.0;
        in.shearY = 200000.0;   // 200 kN
        auto find = [](const std::vector<C::Check> &checks, const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        auto checks = C::checkBolted(in);
        const double ab = 0.25 * 3.14159265358979 * 400.0;
        QVERIFY(std::abs(find(checks, "Cisalhamento").resistance - 0.45 * ab * 830.0 / 1.35 / 1000.0) < 1e-6);   // rosca no plano
        QVERIFY(std::abs(find(checks, "Cisalhamento").demand - 50.0) < 1e-6);                                  // 200 kN / 4 parafusos
        in.threadInShearPlane = false;
        QVERIFY(std::abs(find(C::checkBolted(in), "Cisalhamento").resistance - 0.56 * ab * 830.0 / 1.35 / 1000.0) < 1e-6);
        // contato: lf = min(40 - 11, 70 - 22) = 29 mm -> 1,2 lf t fu = 174 kN < 2,4 db t fu = 240 kN
        QVERIFY(std::abs(find(checks, "contato").resistance - 1.2 * 29.0 * 12.5 * 400.0 / 1.35 / 1000.0) < 1e-6);
        // momento no plano: o parafuso mais afastado recebe mais que o cortante direto
        in.momentZ = 20e6;
        QVERIFY(find(C::checkBolted(in), "Cisalhamento").demand > 50.0);
        // tracao + cisalhamento: interacao quadratica
        in.momentZ = 0.0;
        in.tension = 120000.0;
        const auto inter = find(C::checkBolted(in), "combinados");
        const double ft = 30000.0 / (0.75 * ab * 830.0 / 1.35);
        const double fv = 50000.0 / (0.56 * ab * 830.0 / 1.35);
        QVERIFY(std::abs(inter.demand - (ft * ft + fv * fv)) < 1e-9);

        C::WeldInput weld;
        weld.leg = 6.0;
        weld.length = 200.0;
        weld.fw = 485.0;
        weld.force = 150000.0;
        const auto wchecks = C::checkFilletWeld(weld);
        QVERIFY(std::abs(find(wchecks, "Metal da solda").resistance - 0.6 * 0.707 * 6.0 * 200.0 * 485.0 / 1.35 / 1000.0) < 1e-6);
        QVERIFY(find(wchecks, "Perna minima").ok());                   // 6 mm >= 5 mm (t = 10 mm)
        weld.leg = 3.0;
        QVERIFY(!find(C::checkFilletWeld(weld), "Perna minima").ok());   // 3 mm < 5 mm
    }
    void informedBucklingLengthReplacesK()
    {
        const DesignSection section = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        Structura::Design::BarDesignInput in;
        in.section = &section;
        in.steel = {"A572 Gr50", 345, 450};
        in.length = 6000.0;
        Structura::Design::ComboEffects combo;
        combo.name = "ELU1";
        for (const double x : {0.0, 3000.0, 6000.0}) {
            Structura::Design::StationEffects st;
            st.x = x;
            st.e.N = -300000.0;   // 300 kN de compressao
            combo.stations.push_back(st);
        }
        in.combos = {combo};
        const double base = Structura::Design::designBar(in).maxRatio;
        in.lyOverride = 3000.0;   // travamento no meio do vao no eixo fraco
        const double braced = Structura::Design::designBar(in).maxRatio;
        QVERIFY(base > 0.0);
        QVERIFY(braced < base);
        in.lyOverride = 0.0;
        in.Ky = 0.5;              // K = 0,5 equivale ao comprimento informado L/2 (para o eixo fraco governante)
        const double kBased = Structura::Design::designBar(in).maxRatio;
        QVERIFY(std::abs(kBased - braced) < 1e-9);
    }
    void wSectionGrossProperties()
    {
        const DesignSection s = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        QVERIFY(s.supported());
        const double h = 300 - 24;
        const double area = 2 * 150 * 12 + h * 8;
        const double ix = 150.0 * 300 * 300 * 300 / 12 - (150 - 8.0) * h * h * h / 12;
        QVERIFY(std::abs(s.g.area - area) / area < 4e-2);
        QVERIFY(std::abs(s.g.Ix - ix) / ix < 4e-2);
        QCOMPARE(s.standard, Standard::NBR8800);
    }

    void wSectionTensionAndFlexure()
    {
        const DesignSection s = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        SteelGrade a {"A572 Gr50", 345, 450};
        Nbr8800::Options o;
        const auto t = Nbr8800::tension(s, a, o);
        QVERIFY(std::abs(t.yieldRd - s.g.area * 345 / 1.10) < 1.0);
        // Mesa e alma compactas? lambda_f = 75/12=6.25 < 0.38 sqrt(E/fy)=9.15 ; alma 276/8=34.5 < 3.76 sqrt(E/fy)=90
        const auto f = Nbr8800::flexure(s, a, Axis::X, true, 0.0, 1.0, o);
        const double zx = 150.0 * 12 * (300 - 12) + 8.0 * 276 * 276 / 4;
        QVERIFY(std::abs(f.MRd - zx * 345 / 1.10) / f.MRd < 0.04);
        const auto v = Nbr8800::shear(s, a, Axis::Y, o, 3000);
        // alma: lambda=34.5 <= lambdap=1.10 sqrt(5.34*200000/345)=61.2 -> Vpl = 0.6 d tw fy/1.1
        QVERIFY(std::abs(v.VRd - 0.6 * 300 * 8 * 345 / 1.10) / v.VRd < 1e-6);
    }

    void wSectionCompressionEuler()
    {
        const DesignSection s = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        SteelGrade a {"A572 Gr50", 345, 450};
        Nbr8800::Options o;
        const auto c = Nbr8800::compression(s, a, {3000, 3000, 3000}, o);
        const double ney = kPi * kPi * 200000.0 * s.g.Iy / (3000.0 * 3000.0);
        QVERIFY(c.Ne > 0 && c.Ne <= ney * 1.0001);
        const double l0 = std::sqrt(s.g.area * 345 / c.Ne);
        const double chi = l0 <= 1.5 ? std::pow(0.658, l0 * l0) : 0.877 / (l0 * l0);
        QVERIFY(c.NcRd <= chi * s.g.area * 345 / 1.10 * 1.0001);
        QVERIFY(c.NcRd > 0.5 * chi * s.g.area * 345 / 1.10);
    }

    void coldLippedChannel()
    {
        const DesignSection s = make("C Formado", {{"d", 200}, {"bf", 75}, {"c", 20}, {"t", 2.65}});
        QVERIFY(s.supported());
        QCOMPARE(s.standard, Standard::NBR14762);
        SteelGrade a {"ZAR 345", 345, 430, 200000, 77000, true};
        Nbr14762::Options o;
        const auto c = Nbr14762::compression(s, a, {2000, 2000, 2000}, o);
        const auto f = Nbr14762::flexure(s, a, Axis::X, true, 0.0, 1.0, o);
        const auto v = Nbr14762::shear(s, a, Axis::Y, o);
        qInfo() << "A" << s.g.area << "Ix" << s.g.Ix << "Ne" << c.Ne << "NcRd" << c.NcRd << "MRd" << f.MRd << "VRd" << v.VRd;
        for (const auto &l : c.lines) qInfo() << QString::fromStdString(l);
        for (const auto &l : f.lines) qInfo() << QString::fromStdString(l);
        QVERIFY(c.NcRd > 0 && c.NcRd < s.g.area * 345 / 1.2);
        QVERIFY(f.MRd > 0 && f.MRd <= s.g.Wx() * 345 / 1.10 * 1.001);
        QVERIFY(v.VRd > 0);
    }


    void serviceCantileverEffectsAndSigns()
    {
        using namespace Structura::App;
        using Services::DesignService;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        ProjectData::SectionInfo section;
        section.name = "W teste";
        section.family = "W Laminado";
        section.geometry = {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}};
        const auto props = Structura::Visualization::SectionShapes::properties(section.family, section.geometry);
        QVERIFY(props.has_value());
        section.area = props->area;
        section.iz = props->iz;
        section.iy = props->iy;
        section.j = props->j;

        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e8, 7.7e7});   // kN/m2
        model.sections.push_back({1, section.area, section.iz, section.iy, section.j});
        SolverBridge::NodeData fixed;
        fixed.id = 1;
        fixed.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData tip;
        tip.id = 2;
        tip.x = 3.0;
        model.nodes = {fixed, tip};
        model.members.push_back(SolverBridge::MemberData {1, 1, 2, 1, 1});
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{0.0, 0.0, -20.0, 0.0, 0.0, 0.0}};   // 20 kN para baixo
        model.nodalLoads.push_back(load);
        const auto result = SolverBridge(dll).solve(model);

        DesignService::ResultSet set;
        set.name = "ELU1";
        set.result = &result;
        set.terms = {{&model, 1.0}};
        const auto effects = DesignService::collectEffects(model, {set}, 1000.0, 1000.0);
        QCOMPARE(effects.size(), 1);
        const auto &st = effects[0].combos[0].stations;
        QVERIFY(std::abs(std::abs(st.front().e.Mx) - 60e6) < 1e3);
        QVERIFY(std::abs(st.back().e.Mx) < 1e3);
        QVERIFY(std::abs(std::abs(st.front().e.Vy) - 20e3) < 1.0);
        QVERIFY(st.front().e.Mx < 0.0);   // balanco com carga para baixo: topo tracionado, base (-b) comprimida

        ProjectData::DesignSettings settings;
        settings.defaultSteel = "ASTM A572 Gr.50";
        const auto verdict = DesignService::verifyMember(effects[0], section, settings.paramsFor(1), settings, 1000.0);
        QVERIFY(verdict.supported);
        QVERIFY(verdict.output.maxRatio > 0.2 && verdict.output.maxRatio < 1.5);
        // Serie parametrica de formados a frio (nao depende do catalogo em recursos do executavel)
        ProjectData::SectionInfo cold;
        cold.name = "Ue";
        cold.family = "C Formado";
        cold.geometry = {{"d", 200}, {"bf", 75}, {"c", 25}, {"t", 3.0}};
        const auto series = DesignService::candidateSeries(cold);
        QVERIFY(series.size() > 20);
        for (int i = 1; i < series.size(); ++i) {
            QVERIFY(series[i].area >= series[i - 1].area);
        }
        DesignService::MemberEffects light = effects[0];
        for (auto &combo : light.combos) {
            for (auto &st : combo.stations) {
                st.e.Mx *= 0.1;
                st.e.Vy *= 0.1;
            }
        }
        ProjectData::DesignSettings coldSettings;
        coldSettings.defaultSteel = "ZAR 345";
        const auto best = DesignService::chooseBest({light}, {1}, cold, coldSettings, 1000.0);
        QVERIFY2(best.found, qPrintable(best.note));
        QVERIFY(best.maxRatio <= 1.0);
        // O perfil imediatamente mais leve nao pode atender
        int idx = -1;
        for (int i = 0; i < series.size(); ++i) {
            if (series[i].name == best.section.name) {
                idx = i;
            }
        }
        QVERIFY(idx >= 0);
        if (idx > 0) {
            const auto lighter = DesignService::verifyMember(light, series[idx - 1], coldSettings.paramsFor(1), coldSettings, 1000.0);
            QVERIFY(lighter.output.maxRatio > 1.0);
        }
        qInfo() << "melhor formado:" << best.section.name << best.maxRatio;
    }


    void deflectionMatchesClosedForm()
    {
        using namespace Structura::App;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        const double E = 2.0e8, I = 2.0e-4, L = 6.0, q = 10.0;
        auto run = [&](bool cantilever) {
            SolverBridge::ModelData model;
            model.materials.push_back({1, E, 7.7e7});
            model.sections.push_back({1, 0.01, I, I, 1e-5});
            SolverBridge::NodeData a;
            a.id = 1;
            a.restraints = cantilever ? std::array<int, 6> {1, 1, 1, 1, 1, 1} : std::array<int, 6> {1, 1, 1, 1, 0, 0};
            SolverBridge::NodeData b;
            b.id = 2;
            b.x = L;
            b.restraints = cantilever ? std::array<int, 6> {0, 0, 0, 0, 0, 0} : std::array<int, 6> {0, 1, 1, 0, 0, 0};
            model.nodes = {a, b};
            model.members.push_back(SolverBridge::MemberData {1, 1, 2, 1, 1});
            SolverBridge::MemberLoadData load;
            load.memberId = 1;
            load.coordSystem = SolverBridge::MemberLoadCoordSystem::Global;
            load.qz = -q;
            model.memberLoads.push_back(load);
            const auto result = SolverBridge(dll).solve(model);
            Services::DesignService::ResultSet set;
            set.name = "ELS";
            set.result = &result;
            set.terms = {{&model, 1.0}};
            ProjectData::DesignSettings settings;
            return Services::StructuralChecks::deflections(model, {set}, settings, 1000.0);
        };
        const auto ss = run(false);
        QCOMPARE(ss.size(), 1);
        const double expected = 5.0 * q * std::pow(L, 4) / (384.0 * E * I) * 1000.0;
        QVERIFY(std::abs(ss[0].deflectionMm - expected) / expected < 0.01);
        QCOMPARE(ss[0].denominator, 350);
        const auto cant = run(true);
        QCOMPARE(cant.size(), 1);
        QVERIFY(cant[0].cantilever);
        const double tip = q * std::pow(L, 4) / (8.0 * E * I) * 1000.0;
        QVERIFY(std::abs(cant[0].deflectionMm - tip) / tip < 0.01);
    }


    void b2FromStoryDrift()
    {
        using namespace Structura::App;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/solver_dll.dll");
        if (!QFile::exists(dll)) {
            QSKIP("solver_dll.dll nao encontrada na raiz do repositorio.");
        }
        const double E = 2.0e8, I = 2.0e-4, L = 5.0, P = 300.0, H = 20.0;
        SolverBridge::ModelData model;
        model.materials.push_back({1, E, 7.7e7});
        model.sections.push_back({1, 0.01, I, I, 1e-5});
        SolverBridge::NodeData a;
        a.id = 1;
        a.restraints = {1, 1, 1, 1, 1, 1};
        SolverBridge::NodeData b;
        b.id = 2;
        b.z = L;
        model.nodes = {a, b};
        model.members.push_back(SolverBridge::MemberData {1, 1, 2, 1, 1});
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{H, 0.0, -P, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(load);
        const auto result = SolverBridge(dll).solve(model);
        Services::DesignService::ResultSet set;
        set.name = "ELU1";
        set.result = &result;
        set.terms = {{&model, 1.0}};
        ProjectData::DesignSettings settings;
        settings.rs = 1.0;
        const auto out = Services::StructuralChecks::stories(model, {set}, settings, 1000.0, 1000.0);
        QCOMPARE(out.stories.size(), 1);
        const double expected = 1.0 / (1.0 - P * L * L / (3.0 * E * I));
        QVERIFY(std::abs(out.b2.value("ELU1").value(1) - expected) / expected < 1e-3);
    }


    void windAndAreaLoads()
    {
        using Structura::App::Services::AreaLoadService;
        AreaLoadService::WindParameters w;
        w.v0 = 35.0;
        w.category = 2;
        w.cls = 'B';
        w.height = 10.0;
        QVERIFY(std::abs(AreaLoadService::s2(w) - 0.98) < 1e-9);
        const double vk = 35.0 * 0.98;
        QVERIFY(std::abs(AreaLoadService::dynamicPressure(w) - 0.613 * vk * vk / 1000.0) < 1e-9);
        w.height = 20.0;
        QVERIFY(std::abs(AreaLoadService::s2(w) - 0.98 * std::pow(2.0, 0.09)) < 1e-9);

        QVector<AreaLoadService::BarSegment> bars;
        for (int i = 0; i < 3; ++i) {
            bars.append({i + 1, {0.0, 1.5 * i, 0.0}, {6.0, 1.5 * i, 0.0}});
        }
        AreaLoadService::Options o;
        o.pressure = 2.0;
        o.direction = {0, 0, -1};
        const auto loads = AreaLoadService::distribute(bars, o);
        QCOMPARE(loads.size(), 3);
        QVERIFY(std::abs(loads[0].q[2] + 1.5) < 1e-9);
        QVERIFY(std::abs(loads[1].q[2] + 3.0) < 1e-9);
        QVERIFY(std::abs(loads[2].q[2] + 1.5) < 1e-9);
    }

    void designBarOrchestration()
    {
        const DesignSection s = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        BarDesignInput in;
        in.section = &s;
        in.steel = {"A572 Gr50", 345, 450};
        in.length = 4000;
        ComboEffects c;
        c.name = "ELU1";
        for (int i = 0; i <= 4; ++i) {
            StationEffects st;
            st.x = i * 1000.0;
            st.e.N = -100e3;
            st.e.Mx = 80e6 * std::sin(kPi * i / 4.0);
            st.e.Vy = 40e3;
            c.stations.push_back(st);
        }
        in.combos.push_back(c);
        BarDesignOutput out;
        try {
            out = designBar(in);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
        QVERIFY(out.supported);
        QVERIFY(out.maxRatio > 0.1 && out.maxRatio < 2.0);
        QVERIFY(!out.checks.empty());
        qInfo() << "eta" << out.maxRatio << QString::fromStdString(out.governingCheck);
    }
};

int main(int argc, char **argv)
{
    try {
        TestDesign t;
        return QTest::qExec(&t, argc, argv);
    } catch (const std::exception &e) {
        qFatal("exception: %s", e.what());
    }
}
#include "TestDesign.moc"
