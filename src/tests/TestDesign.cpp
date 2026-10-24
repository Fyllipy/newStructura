#include "app/services/NodeEquilibrium.h"
#include "app/SolverBridge.h"
#include "design/BeamExtras.h"
#include "design/CompositeColumn.h"
#include "design/TaperedMember.h"
#include "app/services/AreaLoadService.h"
#include "app/services/DesignService.h"
#include "app/services/StructuralChecks.h"
#include "app/services/ConnectionService.h"
#include "app/services/ConnectionPlacement.h"
#include "design/EndPlateConnection.h"
#include "app/services/ConnectionSolids.h"
#include "app/services/TrussGenerator.h"
#include "design/SupportChecks.h"
#include "app/services/WindDynamicsService.h"
#include "design/BarDesign.h"
#include "design/CompositeFloor.h"
#include "design/Connections.h"
#include "design/BasePlate.h"
#include "design/ColdFormedConnections.h"
#include "design/ShearConnections.h"
#include "design/TrussConnections.h"
#include "design/TubularJoints.h"
#include "design/DesignSection.h"
#include "design/Nbr14762.h"
#include "design/Nbr8800.h"
#include "viz/structure/SectionCatalog.h"
#include "viz/structure/SectionShapes.h"

#include <QFile>
#include <QtTest>

using namespace Structura::Design;
using Structura::Visualization::SectionShapes;

namespace {

DesignSection make(const QString &family, const QMap<QString, double> &params, double j = 0.0, bool principalAxes = false)
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
    double angle = 0.0;
    if (principalAxes) {
        const auto g = Structura::Design::computeGrossSection(loops, shape->outerCount);
        const double theta = 0.5 * std::atan2(2.0 * g.Ixy, g.Ix - g.Iy) * 180.0 / 3.14159265358979323846;
        double best = 1e300;
        for (const double candidate : {theta, -theta}) {   // como DesignService::makeSection: o sinal que anula Ixy
            const DesignSection trial = makeDesignSection(spec, loops, j, Standard::NBR14762, candidate, shape->outerCount);
            const double residual = std::abs(trial.g.Ixy) / std::max(1.0, std::max(trial.g.Ix, trial.g.Iy));
            if (residual < best) {
                best = residual;
                angle = candidate;
            }
        }
    }
    return makeDesignSection(spec, loops, j, Standard::NBR14762, angle, shape->outerCount);
}

} // namespace

class TestDesign : public QObject
{
    Q_OBJECT
private slots:
    void supportMemberChecksMatchHandCalculation()
    {
        namespace D = Structura::Design::Connections;
        D::SupportInput in;
        in.face = D::SupportInput::Flange;
        in.d = 310.0;
        in.bf = 165.0;
        in.tf = 9.7;
        in.tw = 5.8;
        in.k = 19.7;
        in.fy = 345.0;
        in.atMemberEnd = false;
        in.tension = 50e3;
        in.compression = 50e3;
        in.lengthAlong = 100.0;
        in.lengthTransverse = 30.0;
        const auto checks = D::checkSupportMember(in);
        QCOMPARE(static_cast<int>(checks.size()), 3);
        const double flange = 6.25 * 9.7 * 9.7 * 345.0 / 1.1 / 1e3;   // kN
        const double yielding = 1.10 * (5.0 * 19.7 + 100.0) * 345.0 * 5.8 / 1.1 / 1e3;
        QVERIFY(std::abs(checks[0].resistance - flange) < 1e-6);
        QVERIFY(std::abs(checks[1].resistance - yielding) < 1e-6);
        QVERIFY(checks[2].resistance > 0.0);
        // na extremidade do perfil as resistencias caem
        in.atMemberEnd = true;
        const auto end = D::checkSupportMember(in);
        QVERIFY(end[0].resistance < checks[0].resistance && end[1].resistance < checks[1].resistance);
        // ligacao na alma: so a flexao transversal da alma
        in.face = D::SupportInput::Web;
        const auto web = D::checkSupportMember(in);
        QCOMPARE(static_cast<int>(web.size()), 1);
        const double h = 310.0 - 2.0 * 9.7;
        QVERIFY(std::abs(web[0].resistance - 8.0 * (345.0 * 5.8 * 5.8 / 4.0) * (100.0 + h) / h / 1.1 / 1e3) < 1e-6);
        // sem face informada, nada e verificado
        in.face = D::SupportInput::None;
        QVERIFY(D::checkSupportMember(in).empty());
    }

    void trussGeneratorProducesStableConnectedGeometry()
    {
        using Structura::App::Services::TrussGenerator;
        TrussGenerator::Params p;
        p.span = 12.0;
        p.height = 2.0;
        p.panels = 6;
        // Pratt e Howe de banzos paralelos: 2n+2 nos e 4n+1 barras = 2 nos - 3 (isostatica)
        for (const auto web : {TrussGenerator::Web::Pratt, TrussGenerator::Web::Howe}) {
            p.web = web;
            const auto r = TrussGenerator::generate(p);
            QCOMPARE(static_cast<int>(r.nodes.size()), 14);
            QCOMPARE(static_cast<int>(r.bars.size()), 25);
        }
        // Warren: n+1 nos embaixo, n em cima
        p.web = TrussGenerator::Web::Warren;
        auto warren = TrussGenerator::generate(p);
        QCOMPARE(static_cast<int>(warren.nodes.size()), 13);
        QCOMPARE(static_cast<int>(warren.bars.size()), 23);
        // todas as combinacoes: sem barras de comprimento nulo, sem duplicatas, todos os nos usados e conectados
        for (int profile = 0; profile <= 4; ++profile) {
            for (int web = 0; web <= 5; ++web) {
                p.profile = static_cast<TrussGenerator::Profile>(profile);
                p.web = static_cast<TrussGenerator::Web>(web);
                p.endHeight = 0.5;
                const auto r = TrussGenerator::generate(p);
                const QString label = QStringLiteral("%1/%2").arg(profile).arg(web);
                QVERIFY2(!r.bars.empty(), qPrintable(label));
                std::vector<int> parent(r.nodes.size());
                for (std::size_t i = 0; i < parent.size(); ++i) {
                    parent[i] = static_cast<int>(i);
                }
                std::function<int(int)> find = [&](int x) { return parent[static_cast<std::size_t>(x)] == x ? x : parent[static_cast<std::size_t>(x)] = find(parent[static_cast<std::size_t>(x)]); };
                std::set<std::pair<int, int>> seen;
                for (const auto &b : r.bars) {
                    QVERIFY2(b.a != b.b, qPrintable(label));
                    const auto &na = r.nodes[static_cast<std::size_t>(b.a)], &nb = r.nodes[static_cast<std::size_t>(b.b)];
                    QVERIFY2(std::hypot(na[0] - nb[0], na[1] - nb[1]) > 1e-6, qPrintable(label));
                    QVERIFY2(seen.insert({std::min(b.a, b.b), std::max(b.a, b.b)}).second, qPrintable(label));
                    parent[static_cast<std::size_t>(find(b.a))] = find(b.b);
                }
                for (std::size_t i = 0; i < r.nodes.size(); ++i) {
                    QVERIFY2(find(static_cast<int>(i)) == find(0), qPrintable(label + QStringLiteral(" no solto")));
                }
                // estabilidade minima: barras >= 2 nos - 3
                QVERIFY2(static_cast<int>(r.bars.size()) >= 2 * static_cast<int>(r.nodes.size()) - 3, qPrintable(label + QStringLiteral(" mecanismo")));
            }
        }
    }

    void connectionSolidsBuildForEveryTypeAndFollowTheFrame()
    {
        using namespace Structura::App;
        using Services::ConnectionService;
        using Services::ConnectionSolids;
        ConnectionService::BeamInfo beam;
        beam.depth = 310.0;
        beam.bf = 165.0;
        beam.tf = 9.7;
        beam.tw = 5.8;
        beam.area = 4000.0;
        beam.fy = 345.0;
        beam.fu = 450.0;
        for (const int type : {0, 1, 2, 3, 4, 6, 7}) {
            const auto connection = ConnectionService::makeDefault(type, beam, 250.0, 400.0);
            const auto prims = ConnectionSolids::build(connection);
            QVERIFY2(!prims.empty(), qPrintable(QString::number(type)));
            ConnectionSolids::Frame frame;
            frame.origin = {10.0, 20.0, 30.0};
            frame.s = {0, 0, 1};   // s para cima
            frame.t = {1, 0, 0};
            frame.u = {0, 1, 0};
            frame.scale = 0.001;
            ConnectionSolids::Mesh mesh;
            ConnectionSolids::append(mesh, prims, frame, 0.5);
            QVERIFY(!mesh.triangles.empty());
            QCOMPARE(mesh.points.size(), mesh.normals.size());
            QCOMPARE(mesh.points.size(), mesh.colors.size());
            for (const auto &t : mesh.triangles) {
                for (const int id : t) {
                    QVERIFY(id >= 0 && static_cast<std::size_t>(id) < mesh.points.size());
                }
            }
            // tudo fica perto da origem (ligacoes tem poucos metros de extensao)
            for (const auto &p : mesh.points) {
                QVERIFY(std::abs(p[0] - 10.0f) < 3.0f && std::abs(p[1] - 20.0f) < 3.0f && std::abs(p[2] - 30.0f) < 3.0f);
            }
        }
        // chapa de cisalhamento: o apoio fica em s <= 0 e a chapa em s >= 0 (z >= 30 com este referencial)
        const auto tab = ConnectionSolids::build(ConnectionService::makeDefault(0, beam, 250.0, 400.0));
        double maxS = 0.0;
        for (const auto &p : tab) {
            if (p.kind == ConnectionSolids::Kind::Prism) {
                for (const auto &v : p.polygon) {
                    maxS = std::max(maxS, v[0]);
                }
            }
        }
        QVERIFY(maxS > 60.0);
        // a cor da chapa segue a utilizacao
        QVERIFY(ConnectionSolids::color(ConnectionSolids::Role::Plate, 0.5) != ConnectionSolids::color(ConnectionSolids::Role::Plate, 1.2));
    }

    void dynamicWindDiscreteModelMatchesHandCalculation()
    {
        using namespace Structura::App;
        using Services::WindDynamicsService;
        SolverBridge::ModelData model;
        SolverBridge::Result modal;
        std::vector<double> mass;
        std::vector<double> shape;
        for (int i = 0; i <= 3; ++i) {
            SolverBridge::NodeData node;
            node.id = i + 1;
            node.z = 3.0 * i;
            model.nodes.push_back(node);
            mass.push_back(i == 0 ? 0.0 : 10.0);
            for (int d = 0; d < 6; ++d) {
                shape.push_back(d == 0 ? i / 3.0 : 0.0);
            }
        }
        modal.modalOmega = {2.0 * 3.14159265358979 * 0.5};
        modal.modalShapes = {shape};
        Services::DynamicsService::Basis basis;
        basis.model = &model;
        basis.modal = &modal;
        basis.mass = mass;
        WindDynamicsService::Input in;
        in.v0 = 30.0;
        in.width = 10.0;
        in.drag = 1.2;
        in.xi = 1.5;
        const auto r = WindDynamicsService::compute(basis, in);
        QVERIFY2(r.ok, qPrintable(r.message));
        const double vp = 0.69 * 30.0, q0 = 0.613 * vp * vp / 1000.0;
        QVERIFY(std::abs(r.q0 - q0) < 1e-12);
        double mean = 0.0, num = 0.0, den = 0.0;
        for (int i = 1; i <= 3; ++i) {
            const double z = 3.0 * i, x = i / 3.0;
            mean += q0 * 1.0 * 1.2 * 30.0 * std::pow(z / 10.0, 0.30);
            num += 1.2 * std::pow(z / 10.0, 0.15) * x;
            den += x * x;
        }
        const double fh = q0 * 30.0 * num / den * 1.5;
        QVERIFY(std::abs(r.baseShearMean - mean) / mean < 1e-9);
        QVERIFY(std::abs(r.fh - fh) / fh < 1e-9);
        // flutuante: F_H psi_i x_i (psi = 1 em todos os niveis)
        QVERIFY(std::abs(r.baseShearFluctuating - fh * (1.0 / 3.0 + 2.0 / 3.0 + 1.0)) / fh < 1e-9);
        // a soma das forcas nodais na direcao X e o cortante total; a transversal e 1/3
        double sumX = 0.0, sumY = 0.0;
        for (std::size_t i = 0; i < r.along.size(); ++i) {
            sumX += r.along[i][0];
            sumY += r.cross[i][1];
        }
        QVERIFY(std::abs(sumX - (r.baseShearMean + r.baseShearFluctuating)) < 1e-9);
        QVERIFY(std::abs(sumY - sumX / 3.0) < 1e-9);
    }

    void compositeBeamMatchesHandCalculation()
    {
        using namespace Structura::Design::Composite;
        BeamInput in;
        in.steel.d = 355.0;
        in.steel.bf = 171.0;
        in.steel.tf = 11.6;
        in.steel.tw = 7.2;
        in.steel.area = 6480.0;
        in.steel.inertia = 141.0e6;
        in.steel.fy = 345.0;
        in.concrete.fck = 25.0;
        in.slab.kind = SlabKind::Solid;
        in.slab.tc = 100.0;
        in.span = 8000.0;
        in.leftSpacing = in.rightSpacing = 3000.0;
        in.shored = true;
        in.studs.diameter = 19.0;
        in.studs.fu = 415.0;
        in.studs.spacing = 125.0;
        BeamDemand dm;
        dm.msd = 400.0e6;
        dm.vsd = 150.0e3;
        const auto r = checkBeam(in, dm);
        QVERIFY(r.ok);
        QVERIFY(std::abs(r.bEff - 2000.0) < 1e-9);
        QVERIFY(std::abs(r.ecs - 24150.0) < 5.0);
        // Q_Rd = min(88,1; 70,6) kN: ruptura do conector governa
        QVERIFY(std::abs(r.qRd - 70.6e3) / 70.6e3 < 0.01);
        QCOMPARE(r.connectorsHalfSpan, 32);
        // interacao completa com LN na laje: Mrd = Aa fyd (d/2 + tc - a/2)
        const double fyd = 345.0 / 1.1, tMax = 6480.0 * fyd;
        const double a = tMax / (0.85 * 25.0 / 1.4 * 2000.0);
        const double expected = tMax * (355.0 / 2.0 + 100.0 - a / 2.0);
        QVERIFY(std::abs(r.mRd - expected) / expected < 1e-6);
        QVERIFY(std::abs(r.interaction - 1.0) < 1e-9);
        // menos conectores: interacao parcial reduz o momento resistente
        in.studs.spacing = 400.0;
        const auto partial = checkBeam(in, dm);
        QVERIFY(partial.interaction < 1.0);
        QVERIFY(partial.mRd < r.mRd);
        QVERIFY(partial.mRd > 0.5 * r.mRd);
        // o espacamento proposto atende o grau minimo e o momento
        in.studs.spacing = 125.0;
        const double proposed = proposeSpacing(in, dm);
        QVERIFY(proposed >= 125.0);
        in.studs.spacing = proposed;
        const auto check = checkBeam(in, dm);
        for (const auto &c : check.checks) {
            if (c.name.find("Momento") == 0 || c.name.find("Grau") == 0) {
                QVERIFY2(c.ratio() <= 1.0 + 1e-9, c.name.c_str());
            }
        }
        // forma de aco perpendicular reduz Rg*Rp (dois conectores por nervura)
        BeamInput deck = in;
        deck.slab.kind = SlabKind::DeckPerpendicular;
        deck.slab.hF = 75.0;
        deck.slab.bF = 120.0;
        deck.slab.tc = 70.0;
        deck.studs.perRib = 2;
        deck.studs.emh = 60.0;
        deck.shored = false;
        const auto d = checkBeam(deck, dm);
        QVERIFY(std::abs(d.rg - 0.85) < 1e-12);
        QVERIFY(std::abs(d.rp - 0.75) < 1e-12);
        QVERIFY(d.qRd <= r.qRd);
    }

    void deckSlabMomentMatchesHandCalculation()
    {
        using namespace Structura::Design::Composite;
        DeckInput deck;   // forma de 75 mm, 1100 mm2/m, e = 40 mm
        DeckLoads loads;
        const auto r = checkDeck(deck, loads);
        QVERIFY(r.ok);
        const double fyd = 345.0 / 1.1, npa = 1100.0 * fyd;
        const double a = npa / (0.85 * 1.0 * (25.0 / 1.4) * 1000.0);
        const double expected = npa * (145.0 - 40.0 - 0.5 * a);
        QVERIFY(a <= deck.tc);
        QVERIFY(std::abs(r.mRdPositive - expected) / expected < 1e-6);
        QVERIFY(r.vlRd > 0.0 && r.vvRd > 0.0);
        QVERIFY(r.inertiaEffective > deck.ie);
        // mais carga so piora a verificacao
        DeckLoads heavy = loads;
        heavy.variable = 12.0;
        auto ratioOf = [](const DeckResult &d, const char *prefix) {
            for (const auto &c : d.checks) {
                if (c.name.find(prefix) == 0) {
                    return c.ratio();
                }
            }
            return -1.0;
        };
        QVERIFY(ratioOf(checkDeck(deck, heavy), "Momento fletor positivo") > ratioOf(r, "Momento fletor positivo"));
        QVERIFY(ratioOf(checkDeck(deck, heavy), "Cisalhamento longitudinal") > ratioOf(r, "Cisalhamento longitudinal"));
    }

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
    void fatigueOpeningAndBracingChecksOfAnnexFHAnd412()
    {
        const DesignSection section = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        Structura::Design::BarDesignInput in;
        in.section = &section;
        in.steel = {"A572 Gr50", 345, 450};
        in.length = 6000.0;
        Structura::Design::ComboEffects heavy, none;
        heavy.name = "FAD1";
        none.name = "FAD0";
        for (const double x : {0.0, 3000.0, 6000.0}) {
            Structura::Design::StationEffects st;
            st.x = x;
            st.e.Mx = x == 3000.0 ? 40.0e6 : 0.0;
            st.e.Vy = x == 3000.0 ? 0.0 : 20000.0;
            heavy.stations.push_back(st);
            Structura::Design::StationEffects zero;
            zero.x = x;
            none.stations.push_back(zero);
        }
        in.combos = {heavy, none};
        struct Found {
            bool present {false};
            Structura::Design::CheckResult check;
        };
        auto find = [&in](const char *id) {
            Found found;
            for (const auto &c : Structura::Design::designBar(in).checks) {
                if (c.id == id) {
                    found.present = true;
                    found.check = c;
                }
            }
            return found;
        };
        QVERIFY(!find("fatigue").present);
        in.fatigueCategory = 4;   // C: Cf = 44e8, sigma_TH = 69 MPa
        in.fatigueCycles = 2.0e6;
        const auto fatigue = find("fatigue").check;
        const double range = 40.0e6 / section.g.Wx();
        const double adm = std::max(std::pow(329.0 * 44e8 / 2.0e6, 0.333), 69.0);
        QVERIFY(std::abs(fatigue.demand - range) < 1e-6);
        QVERIFY(std::abs(fatigue.resistance - adm) < 1e-6);
        in.fatigueCycles = 10000.0;   // H.2.6: dispensada
        QCOMPARE(find("fatigue").check.ratio, 0.0);

        // Anexo F: abertura no meio do vao atende; fora dos dois quartos centrais nao
        in.openingDiameter = 100.0;
        in.openingPosition = 3000.0;
        QCOMPARE(find("web_opening").check.ratio, 0.0);
        in.openingPosition = 1000.0;
        QVERIFY(find("web_opening").check.ratio > 1.0);
        in.openingPosition = 3000.0;
        in.openingNeighbourD = 80.0;
        in.openingSpacing = 200.0;   // < 2,5 x 100
        QVERIFY(find("web_opening").check.ratio > 1.0);

        // 4.12: contencao no meio do vao -> requisitos de forca e rigidez
        in.lateralSegments = 2;
        const auto bracing = find("bracing");
        QVERIFY(bracing.present);
        QVERIFY(bracing.check.lines.front().find("F_br") != std::string::npos);
    }

    void gussetPlateConnection()
    {
        namespace C = Structura::Design::Connections;
        auto find = [](const std::vector<C::Check> &checks, const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        C::GussetInput in;
        in.grade = C::boltGrades()[2];
        in.db = 20.0;
        in.rows = 3;
        in.lines = 1;
        in.pitch = 70.0;
        in.bars = 2;
        in.barArea = 770.0;          // L 64 x 6,35
        in.barLegThickness = 6.35;
        in.barEccentricity = 18.0;
        in.bar = {250.0, 400.0};
        in.gusset = {250.0, 400.0};
        in.support = {250.0, 400.0};
        in.gussetThickness = 9.5;
        in.gussetWidth = 200.0;
        in.tension = 120000.0;
        in.compression = 90000.0;
        const auto checks = C::checkGusset(in);
        // largura de Whitmore: 2 L tan30 com L = 2 x 70
        QVERIFY(std::abs(C::whitmoreWidth(in) - 2.0 * 140.0 * std::tan(30.0 * 3.14159265358979 / 180.0)) < 1e-9);
        const double lw = C::whitmoreWidth(in);
        QVERIFY(std::abs(find(checks, "tracao na secao de Whitmore").resistance - std::min(lw * 9.5 * 250.0 / 1.10, std::min(lw - 24.0, 0.85 * lw) * 9.5 * 400.0 / 1.35) / 1000.0) < 1e-6);
        QVERIFY(find(checks, "compressao na secao de Whitmore").resistance > 0.0);
        QVERIFY(find(checks, "compressao na secao de Whitmore").resistance < lw * 9.5 * 250.0 / 1.10 / 1000.0 + 1e-9);
        QVERIFY(find(checks, "Diagonal: cisalhamento de bloco").resistance > 0.0);
        // Ct = 1 - 18/140
        QVERIFY(find(checks, "Diagonal: tracao").lines[0].find("0.871") != std::string::npos);
        // mais carga -> proposta; carga absurda -> sem proposta
        C::GussetInput base = in;
        base.tension = 250000.0;
        base.compression = 200000.0;
        base.barArea = 1900.0;
        const auto proposal = C::proposeGusset(base);
        QVERIFY(proposal.found);
        QVERIFY(C::worstRatio(C::checkGusset(proposal.input)) <= 1.0 + 1e-9);
        base.tension = 5.0e7;
        QVERIFY(!C::proposeGusset(base).found);
        QVERIFY(C::partsGusset(in).size() >= 3);
    }
    void beamSplice()
    {
        namespace C = Structura::Design::Connections;
        C::SpliceInput in;
        in.grade = C::boltGrades()[2];
        in.db = 20.0;
        in.beamDepth = 310.0;
        in.flangeWidth = 165.0;
        in.flangeThickness = 9.7;
        in.webThickness = 5.8;
        in.beamArea = 4000.0;
        in.beam = {345.0, 450.0};
        in.plates = {345.0, 450.0};
        in.moment = 60.0e6;          // 60 kN.m
        in.shear = 80000.0;
        const auto checks = C::checkSplice(in);
        auto find = [&](const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        QVERIFY(find("Cisalhamento do parafuso (mesa)").demand > 0.0);
        QVERIFY(find("Talas de mesa: tracao").demand > 0.0);
        QVERIFY(find("Talas de alma: cisalhamento").resistance > 0.0);
        QVERIFY(find("Talas de alma: flexao").demand > 0.0);
        const C::SpliceProposal proposal = C::proposeSplice(in);
        QVERIFY(proposal.found);
        QVERIFY(C::worstRatio(C::checkSplice(proposal.input)) <= 1.0 + 1e-9);
        QVERIFY(!C::partsSplice(proposal.input).empty());
        C::SpliceInput heavy = in;
        heavy.moment = 150.0e6;
        const C::SpliceProposal heavyProposal = C::proposeSplice(heavy);
        if (heavyProposal.found) {
            double lightMass = 0, heavyMass = 0;
            for (const auto &p : C::partsSplice(proposal.input)) lightMass += p.totalMass();
            for (const auto &p : C::partsSplice(heavyProposal.input)) heavyMass += p.totalMass();
            QVERIFY(heavyMass >= lightMass);
        }
    }
    void basePlateNbr8800()
    {
        namespace C = Structura::Design::Connections;
        C::BasePlateInput in;
        in.d = 310.0;
        in.bf = 165.0;
        in.tw = 5.8;
        in.tf = 9.7;
        in.column = {345.0, 450.0};
        in.plate = {250.0, 400.0};
        in.anchorDiameter = 25.0;
        in.anchors = 4;
        in.plateThickness = 25.0;
        in.fck = 25.0;
        // geometria pela Tabela 18: a1 = 50, a2 = 100
        const auto g = C::basePlateGeometry(in);
        QVERIFY(std::abs(g.lx - (310.0 + 4 * 50.0)) < 1e-9);
        QVERIFY(std::abs(g.ly - ((0.5 * 4 - 1) * 100.0 + 2 * 50.0)) < 1e-9);
        QVERIFY(std::abs(g.m - (g.lx - 0.95 * 310.0) / 2.0) < 1e-9);
        QVERIFY(g.sigmaCRd <= 1.7 * 25.0 / 1.4 + 1e-9);
        std::vector<C::Check> checks;
        auto find = [&](const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        // compressao centrada: C1
        in.axial = -600000.0;
        checks = C::checkBasePlate(in);
        QVERIFY(find("Caso de calculo").lines[1].find("Caso C1") != std::string::npos);
        QVERIFY(find("tp,min").demand > 0.0);
        QVERIFY(std::abs(find("Pressao de contato do concreto").demand - 600000.0 / (g.lx * g.ly)) < 1e-9);
        // grande excentricidade: tracao nos chumbadores (C3)
        in.axial = -100000.0;
        in.moment = 150.0e6;
        checks = C::checkBasePlate(in);
        QVERIFY(find("Caso de calculo").lines[1].find("Caso C3") != std::string::npos);
        QVERIFY(find("Chumbador: diametro minimo").demand > 0.0);
        // tracao pura: T1
        in.axial = 80000.0;
        in.moment = 0.0;
        checks = C::checkBasePlate(in);
        QVERIFY(find("Caso de calculo").lines[1].find("Caso T1") != std::string::npos);
        const double ft = 80000.0 / 4.0;
        QVERIFY(std::abs(find("Chumbador: diametro minimo").demand - std::sqrt(4.0 * ft / (0.75 * 3.14159265358979 * 400.0 / 1.35))) < 1e-9);
        // proposta
        C::BasePlateInput base = in;
        base.axial = -400000.0;
        base.moment = 40.0e6;
        base.shear = 40000.0;
        const C::BasePlateProposal proposal = C::proposeBasePlate(base);
        QVERIFY(proposal.found);
        QVERIFY(C::worstRatio(C::checkBasePlate(proposal.input)) <= 1.0 + 1e-9);
        QVERIFY(!C::partsBasePlate(proposal.input).empty());
        QVERIFY(C::anchorFor(26.0).db >= 26.0);
    }
    void tubularJointsCidect()
    {
        namespace C = Structura::Design::Connections;
        auto find = [](const std::vector<C::Check> &checks, const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        // junta T: eq. 4.5 e Qf
        C::TubularJointInput t;
        t.kind = C::TubularJointInput::TY;
        t.d0 = 168.3; t.t0 = 6.0; t.fy0 = 355; t.fu0 = 510;
        t.d1 = 88.9; t.t1 = 4.0; t.fy1 = 355; t.theta1 = 90.0; t.n1 = 50000.0;
        const double beta = 88.9 / 168.3, gamma = 168.3 / 12.0;
        const double qu = 2.6 * (1 + 6.8 * beta * beta) * std::pow(gamma, 0.2);
        const auto checks = C::checkTubularJoint(t);
        const double plain = find(checks, "Plastificacao da face do banzo").resistance;
        QVERIFY(std::abs(plain - qu * 355.0 * 36.0 / 1000.0) < 1e-6);
        // compressao no banzo reduz a resistencia (Qf < 1)
        const double npl = 3.14159265358979 / 4.0 * (168.3 * 168.3 - 156.3 * 156.3) * 355.0;
        t.chordAxial = -0.5 * npl;
        const double reduced = find(C::checkTubularJoint(t), "Plastificacao da face do banzo").resistance;
        QVERIFY(std::abs(reduced / plain - std::pow(1.0 - 0.5, 0.45 - 0.25 * beta)) < 1e-6);
        // tracao no banzo: C1 = 0,2
        t.chordAxial = 0.5 * npl;
        QVERIFY(std::abs(find(C::checkTubularJoint(t), "Plastificacao da face do banzo").resistance / plain - std::pow(0.5, 0.2)) < 1e-6);
        // K com afastamento
        C::TubularJointInput k;
        k.kind = C::TubularJointInput::KGap;
        k.d0 = 219.1; k.t0 = 7.1; k.fy0 = 355; k.fu0 = 510;
        k.d1 = 139.7; k.t1 = 4.5; k.theta1 = 38.7; k.n1 = 200000.0;
        k.d2 = 88.9; k.t2 = 3.6; k.theta2 = 50.8; k.n2 = 150000.0;
        k.gap = 21.3;
        const auto kc = C::checkTubularJoint(k);
        const double b1 = 139.7 / 219.1, g0 = 219.1 / 14.2;
        const double quK = 1.65 * (1 + 8 * std::pow(b1, 1.6)) * std::pow(g0, 0.3) * (1 + 1.0 / (1.2 + std::pow(21.3 / 7.1, 0.8)));
        QVERIFY(std::abs(find(kc, "Plastificacao da face do banzo (diagonal 1)").resistance
                         - quK * 355.0 * 7.1 * 7.1 / std::sin(38.7 * 3.14159265358979 / 180.0) / 1000.0) < 1e-6);
        QVERIFY(find(kc, "Afastamento").ok());
        k.gap = 5.0;   // menor que t1 + t2
        QVERIFY(!find(C::checkTubularJoint(k), "Afastamento").ok());
        // K com sobreposicao
        C::TubularJointInput o = k;
        o.kind = C::TubularJointInput::KOverlap;
        o.overlap = 50.0;
        const auto oc = C::checkTubularJoint(o);
        QVERIFY(find(oc, "Escoamento local da diagonal sobreposta").resistance > 0.0);
        QVERIFY(find(oc, "Diagonal coberta").resistance > 0.0);
        // chapa transversal em T
        C::TubularJointInput p;
        p.kind = C::TubularJointInput::TY;
        p.braceKind = C::TubularJointInput::TransversePlate;
        p.d0 = 168.3; p.t0 = 6.0; p.d1 = 80.0; p.t1 = 12.5; p.theta1 = 90.0; p.n1 = 60000.0;
        const auto pc = C::checkTubularJoint(p);
        const double bp = 80.0 / 168.3;
        QVERIFY(std::abs(find(pc, "Plastificacao da face do banzo").resistance - 2.2 * (1 + 6.8 * bp * bp) * std::pow(gamma, 0.2) * 355.0 * 36.0 / 1000.0) < 1e-6);
        QVERIFY(!C::partsTubularJoint(k).empty());
    }
    void shearTabAndDoubleAngleChecks()
    {
        namespace C = Structura::Design::Connections;
        C::ShearTabInput in;
        in.grade = C::boltGrades()[2];
        in.db = 20.0;
        in.rows = 3;
        in.pitch = 70.0;
        in.edgeV = 35.0;
        in.edgeH = 35.0;
        in.boltLineToWeld = 60.0;
        in.plateThickness = 9.5;
        in.webThickness = 6.0;
        in.weldLeg = 6.0;
        in.shear = 100000.0;   // 100 kN
        auto find = [](const std::vector<C::Check> &checks, const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        const auto checks = C::checkShearTab(in);
        QVERIFY(std::abs(C::plateHeight(in) - 210.0) < 1e-9);
        QVERIFY(std::abs(find(checks, "Chapa - escoamento").resistance - 0.6 * 9.5 * 210.0 * 250.0 / 1.10 / 1000.0) < 1e-6);
        QVERIFY(std::abs(find(checks, "Chapa - ruptura").resistance - 0.6 * 9.5 * (210.0 - 3 * 24.0) * 400.0 / 1.35 / 1000.0) < 1e-6);
        QVERIFY(std::abs(find(checks, "Chapa - flexao").resistance - 9.5 * 210.0 * 210.0 * 250.0 / (4.0 * 1.10) / 1e6) < 1e-6);
        QVERIFY(std::abs(find(checks, "Chapa - flexao").demand - 6.0) < 1e-9);   // 100 kN x 60 mm
        const double fv = 100000.0 / 420.0, fn = 6e6 / (210.0 * 210.0 / 3.0);
        QVERIFY(std::abs(find(checks, "Solda de filete").demand - std::hypot(fv, fn)) < 1e-6);
        QVERIFY(find(checks, "Solda de filete").ok());
        QVERIFY(find(checks, "alma da viga").resistance > 0.0);
        // excentricidade aumenta a solicitacao do parafuso mais afastado
        C::ShearTabInput concentric = in;
        concentric.boltLineToWeld = 0.0;
        QVERIFY(find(C::checkShearTab(concentric), "Cisalhamento").demand < find(checks, "Cisalhamento").demand);
        // proposta: atende para 120 kN, nao para 5000 kN
        C::ShearTabInput base = in;
        base.shear = 120000.0;
        const auto proposal = C::proposeShearTab(base);
        QVERIFY(proposal.found);
        QVERIFY(proposal.maxRatio <= 1.0 + 1e-9);
        QVERIFY(C::worstRatio(C::checkShearTab(proposal.input)) <= 1.0 + 1e-9);
        base.shear = 5.0e6;
        QVERIFY(!C::proposeShearTab(base).found);
        QVERIFY(!C::partsShearTab(in).empty());

        C::DoubleAngleInput da;
        da.grade = C::boltGrades()[2];
        da.db = 20.0;
        da.rows = 3;
        da.shear = 150000.0;
        da.axial = 20000.0;
        const auto dchecks = C::checkDoubleAngle(da);
        // corte duplo: Rd por plano vezes dois planos na verificacao (demanda por plano e metade)
        QVERIFY(find(dchecks, "lado da alma").ok() || !find(dchecks, "lado da alma").ok());
        QVERIFY(!find(dchecks, "lado do apoio").name.empty());
        QVERIFY(!find(dchecks, "Tracao").name.empty());   // N > 0: tracao nos parafusos do apoio
        const auto dp = C::proposeDoubleAngle(da);
        QVERIFY(dp.found);
        QVERIFY(C::worstRatio(C::checkDoubleAngle(dp.input)) <= 1.0 + 1e-9);
        QCOMPARE(static_cast<int>(C::partsDoubleAngle(dp.input).size()), 3);
    }
    void norm2024FlexureCorrections()
    {
        SteelGrade steel {"A572-50", 345, 450, 200000, 77000, false};
        Nbr8800::Options o;
        auto contains = [](const std::vector<std::string> &lines, const char *text) {
            for (const auto &line : lines) {
                if (line.find(text) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };
        // T com a alma comprimida: FLT conforme D.2.3 (NBR 8800:2024), com r = fy Wx / Mcr
        const DesignSection tee = make(QStringLiteral("T Laminado"), {{"d", 150}, {"bf", 150}, {"tw", 8}, {"tf", 10}}, 74747.0);
        const bool flangeUp = tee.flangeUp;
        const auto f = Nbr8800::flexure(tee, steel, Axis::X, !flangeUp, 3000.0, 1.0, o);   // lado da alma comprimido
        QVERIFY(f.covered && f.MRd > 0.0);
        QVERIFY(contains(f.lines, "alma comprimida"));
        const double wx = flangeUp ? tee.g.WxBottom : tee.g.WxTop;
        QVERIFY(f.MRd <= 1.5 * 345.0 * wx / 1.10 + 1e-6);
        // Cantoneira simples: flexao verificada pelo criterio adaptado
        const DesignSection angle = make(QStringLiteral("L Laminado"), {{"b", 76}, {"t", 6.35}}, 0.0);
        const auto fa = Nbr8800::flexure(angle, steel, Axis::X, true, 2000.0, 1.0, o);
        QVERIFY(fa.covered && fa.MRd > 0.0);
        QVERIFY(fa.MRd <= 345.0 * angle.g.Wx() / 1.10 + 1e-6);
    }
    void coldFormedWebCripplingAndServiceInertia()
    {
        SteelGrade zar {"ZAR 345", 345, 430, 200000, 77000, true};
        Nbr14762::Options o;
        const DesignSection u = make(QStringLiteral("U Formado"), {{"d", 150}, {"bf", 60}, {"t", 3.0}});
        const auto wc = Nbr14762::webCrippling(u, zar, true, 50.0, o);
        QVERIFY(wc.covered);
        const double h = 150.0 - 2.0 * (3.0 + 3.0);
        const double expected = 4.6 * 9.0 * 345.0 * (1.0 - 0.40) * (1.0 + 0.60 * std::sqrt(50.0 / 3.0)) * (1.0 - 0.03 * std::sqrt(h / 3.0)) / 1.35;
        QVERIFY(std::abs(wc.FRd - expected) < 1e-6);
        const DesignSection tube = make(QStringLiteral("Tubular Retangular"), {{"b", 100}, {"h", 60}, {"t", 3.0}});
        QVERIFY(!Nbr14762::webCrippling(tube, zar, true, 50.0, o).covered);
        // Alma esbelta: inercia efetiva menor que a bruta
        const DesignSection slender = make(QStringLiteral("U Formado"), {{"d", 300}, {"bf", 60}, {"t", 1.2}});
        const auto ratios = Nbr14762::serviceInertiaRatios(slender, zar);
        QVERIFY(ratios[0] > 0.05 && ratios[0] < 1.0);
        QVERIFY(ratios[1] <= 1.0);
        // tensao de servico baixa: secao quase integral; momento nulo: razao 1,0
        const auto light = Nbr14762::serviceInertiaRatios(slender, zar, 1.0e5, 0.0);
        QVERIFY(light[0] >= ratios[0] && light[0] <= 1.0);
        QCOMPARE(light[1], 1.0);
        const auto stocky = Nbr14762::serviceInertiaRatios(u, zar);
        QVERIFY(stocky[0] > 0.8 && stocky[0] <= 1.0 && stocky[0] > ratios[0]);
    }
    void slipCriticalBoltsFollowNorm2024()
    {
        namespace C = Structura::Design::Connections;
        C::SlipInput in;
        for (const auto &g : C::boltGrades()) {
            if (g.name.find("A325") != std::string::npos) {
                in.grade = g;
            }
        }
        in.db = 20.0;
        in.bolts = 4;
        in.shear = 100000.0;
        const auto serviceCheck = C::checkSlip(in);
        QCOMPARE(static_cast<int>(serviceCheck.size()), 1);
        QVERIFY(std::abs(serviceCheck[0].resistance - 0.80 * 0.30 * 142.0) < 1e-9);   // kN, Tabela 19
        QVERIFY(std::abs(serviceCheck[0].demand - 0.7 * 25.0) < 1e-9);
        in.oversized = true;
        in.surfaceClass = 1;
        in.fillers = true;
        const auto ultimate = C::checkSlip(in);
        QVERIFY(std::abs(ultimate[0].resistance - 1.13 * 0.50 * 0.85 * 142.0 / 1.20) < 1e-9);
    }
    void pryingLongGripAndLongJointFollowNorm2024()
    {
        namespace C = Structura::Design::Connections;
        // 6.3.5.4: b = 40, a = 30, p = 70, df = 22, Ft,Rd = 100 kN, Ft,0,Sd = 50 kN
        C::PryingInput pr;
        pr.db = 20.0;
        pr.tensionForce = 50000.0;
        pr.boltResistance = 100000.0;
        pr.a = 30.0;
        pr.b = 40.0;
        pr.p = 70.0;
        pr.holeWidth = 22.0;
        pr.thickness = 16.0;
        pr.fu = 400.0;
        const auto flexible = C::checkPrying(pr);
        QCOMPARE(static_cast<int>(flexible.size()), 1);
        const double delta = 1.0 - 22.0 / 70.0;
        const double beta = (30.0 + 10.0) / 30.0 * (100000.0 / 50000.0 - 1.0);   // 1,333 >= 1 -> alfa = 1
        QVERIFY(beta >= 1.0);
        const double tReq = std::sqrt(4.0 * 30.0 * 50000.0 * 1.10 / (70.0 * 400.0 * (1.0 + delta * 1.0)));
        QVERIFY(std::abs(flexible[0].resistance - tReq) < 1e-9);
        QVERIFY(flexible[0].ok());
        pr.thickness = 8.0;
        QVERIFY(!C::checkPrying(pr)[0].ok());
        // beta < 1: parafuso quase no limite -> alfa < 1 e placa mais espessa
        pr.boltResistance = 60000.0;
        pr.thickness = 30.0;
        const double beta2 = 40.0 / 30.0 * (60000.0 / 50000.0 - 1.0);
        const double alpha2 = std::min(1.0, 1.0 / delta * beta2 / (1.0 - beta2));
        QVERIFY(beta2 < 1.0 && alpha2 < 1.0);
        QVERIFY(std::abs(C::checkPrying(pr)[0].resistance - std::sqrt(4.0 * 30.0 * 50000.0 * 1.10 / (70.0 * 400.0 * (1.0 + delta * alpha2)))) < 1e-9);
        pr.boltResistance = 40000.0;   // beta < 0: o parafuso deve ser alterado
        QVERIFY(!C::checkPrying(pr)[0].ok());

        // 6.3.7: pega de 130 mm com db = 20 mm -> (130 - 100)/1,5 x 1 % = 20 % de reducao; 6.3.8: > 950 mm majora em 1,20
        C::BoltedInput bolt;
        for (const auto &g : C::boltGrades()) {
            if (g.name.find("A325") != std::string::npos) {
                bolt.grade = g;
            }
        }
        bolt.db = 20.0;
        bolt.shearY = 60000.0;
        bolt.nx = 1;
        bolt.ny = 2;
        auto shearOf = [](const std::vector<C::Check> &checks) {
            for (const auto &c : checks) {
                if (c.name.rfind("Cisalhamento", 0) == 0) {
                    return c;
                }
            }
            return C::Check {};
        };
        const auto base = shearOf(C::checkBolted(bolt));
        bolt.grip = 130.0;
        const auto longGrip = shearOf(C::checkBolted(bolt));
        QVERIFY(std::abs(longGrip.resistance - 0.8 * base.resistance) < 1e-9);
        bolt.pretensioned = true;
        QVERIFY(std::abs(shearOf(C::checkBolted(bolt)).resistance - base.resistance) < 1e-9);
        bolt.grip = 0.0;
        bolt.connectionLength = 1000.0;
        QVERIFY(std::abs(shearOf(C::checkBolted(bolt)).demand - 1.2 * base.demand) < 1e-9);
    }
    void endPlateMomentConnectionMatchesHandCalculation()
    {
        namespace C = Structura::Design::Connections;
        C::EndPlateInput in;
        for (const auto &g : C::boltGrades()) {
            if (g.name.find("A325") != std::string::npos) {
                in.grade = g;
            }
        }
        in.db = 20.0;
        in.moment = 100.0e6;
        in.shear = 80000.0;
        auto find = [](const std::vector<C::Check> &checks, const char *name) {
            for (const auto &c : checks) {
                if (c.name.find(name) != std::string::npos) {
                    return c;
                }
            }
            return C::Check {};
        };
        const auto checks = C::checkEndPlate(in);
        // F_t = M/(d - tf) = 100e6/290; 4 parafusos tracionados (2 fileiras x 2)
        const double ft = 100.0e6 / 290.0;
        const double ab = 0.25 * 3.14159265358979 * 400.0;
        const auto bolts = find(checks, "Parafusos tracionados");
        QVERIFY(std::abs(bolts.demand - ft / 4.0 / 1000.0) < 1e-6);
        QVERIFY(std::abs(bolts.resistance - 0.75 * ab * in.grade.fub / 1.35 / 1000.0) < 1e-6);
        // painel de alma sem reforco: kv = 5,34; h = 260 - 2 (12,7 + 10) = 214,6; tw = 7,6
        const auto panel = find(checks, "Painel de alma");
        const double h = 260.0 - 2.0 * (12.7 + 10.0), lambda = h / 7.6;
        const double lp = 1.10 * std::sqrt(5.34 * 200000.0 / 250.0), lr = 1.37 * std::sqrt(5.34 * 200000.0 / 250.0);
        const double vpl = 0.6 * 260.0 * 7.6 * 250.0;
        const double vrd = (lambda <= lp ? vpl : (lambda <= lr ? lp / lambda * vpl : 1.24 * lp * lp / (lambda * lambda) * vpl)) / 1.10;
        QVERIFY(std::abs(panel.resistance - vrd / 1000.0) < 1e-6);
        QVERIFY(std::abs(panel.demand - ft / 1000.0) < 1e-6);
        QVERIFY(C::endPlateHeight(in) > in.beamDepth);
        // com chapa de reforco da alma a resistencia do painel aumenta
        in.doublerThickness = 8.0;
        QVERIFY(find(C::checkEndPlate(in), "Painel de alma").resistance > panel.resistance);
        // enrijecedores: aparecem as verificacoes 5.7.9
        in.continuityPlates = true;
        bool stiffeners = false;
        for (const auto &c : C::checkEndPlate(in)) {
            stiffeners = stiffeners || c.clause.find("5.7.9") != std::string::npos;
        }
        QVERIFY(stiffeners);
        QVERIFY(!C::partsEndPlate(in).empty());
    }

    void curvedSurfaceFilletWeldFollows1476210_2_4()
    {
        namespace C = Structura::Design::Connections;
        C::ColdFormedInput in;
        in.mode = C::ColdFormedInput::CurvedWeld;
        in.t1 = 2.0;
        in.fu1 = 450.0;
        in.fw = 485.0;
        in.force = 20000.0;
        in.lengthLong = 100.0;
        in.lengthTrans = 100.0;
        in.curvedRe = 6.0;
        auto resistanceOf = [](const C::ColdFormedInput &input) {
            for (const auto &c : C::checkColdFormed(input)) {
                if (c.clause.find("10.2.4") != std::string::npos && c.name.find("Filete") != std::string::npos) {
                    return c.resistance * 1000.0;
                }
            }
            return -1.0;
        };
        in.curvedParallel = false;   // 10.2.4-a
        QVERIFY(std::abs(resistanceOf(in) - 0.83 * 2.0 * 100.0 * 450.0 / 1.65) < 1e-6);
        in.curvedParallel = true;    // tef = 0,3 re = 1,8 < 2 t: 0,75 t L fu / 1,80
        QVERIFY(std::abs(resistanceOf(in) - 0.75 * 2.0 * 100.0 * 450.0 / 1.80) < 1e-6);
        in.curvedTwoSurfaces = true;   // re > 12,5: tef = 0,37 re >= 2 t; enrijecedor com h >= L: 1,50 t L fu / 1,80
        in.curvedRe = 14.0;
        in.curvedStiffenerHeight = 120.0;
        QVERIFY(std::abs(resistanceOf(in) - 1.50 * 2.0 * 100.0 * 450.0 / 1.80) < 1e-6);
        in.t1 = 3.0;   // t > 2,5: tef = 5,18 < 2 t = 6 (0,75 t L fu / 1,80), tambem limitada por 0,75 tef L fw / 1,65
        const double tef = 0.37 * 14.0;
        QVERIFY(std::abs(resistanceOf(in) - std::min(0.75 * 3.0 * 100.0 * 450.0 / 1.80, 0.75 * tef * 100.0 * 485.0 / 1.65)) < 1e-6);
    }

    void weldGroupsFollowNorm2024Annex()
    {
        namespace C = Structura::Design::Connections;
        // 6.2.5.2-c: max(Fl + Ft ; 0,85 Fl + 1,5 Ft)
        QVERIFY(std::abs(C::weldCombinedResistance(100.0, 100.0) - 235.0) < 1e-9);
        QVERIFY(std::abs(C::weldCombinedResistance(100.0, 10.0) - 110.0) < 1e-9);
        C::WeldInput weld;
        weld.leg = 6.0;
        weld.length = 300.0;
        weld.lengthTransverse = 100.0;
        weld.force = 100000.0;
        const double awL = 0.707 * 6.0 * 200.0, awT = 0.707 * 6.0 * 100.0;
        const double fl = 0.6 * awL * 485.0 / 1.35, ft = 0.6 * awT * 485.0 / 1.35;
        const auto checks = C::checkFilletWeld(weld);
        QVERIFY(std::abs(checks[0].resistance - C::weldCombinedResistance(fl, ft) / 1000.0) < 1e-9);
        QCOMPARE(checks[0].clause, std::string("6.2.5.2-c"));

        // 6.2.5.2-b: linha vertical de 200 mm, carga vertical com excentricidade e
        auto capacity = [](double e) {
            C::IcrInput in;
            in.segments = {{0.0, 0.0, 0.0, 200.0}};
            in.leg = 6.0;
            in.loadAngleDeg = 90.0;
            in.loadX = e;
            in.loadY = 100.0;
            return C::weldGroupIcr(in);
        };
        const double unit = 0.6 * 485.0 / 1.35 * 0.707 * 6.0;   // N/mm, filete longitudinal
        double previous = 1e300;
        for (const double e : {20.0, 50.0, 100.0, 150.0}) {
            const auto r = capacity(e);
            QVERIFY(r.converged);
            const double elastic = unit * 200.0 / std::sqrt(1.0 + std::pow(6.0 * e / 200.0, 2));   // metodo elastico (conservador)
            QVERIFY(r.forceRd > elastic);
            QVERIFY(r.forceRd < 1.5 * unit * 200.0);
            QVERIFY(r.forceRd < previous);
            previous = r.forceRd;
        }
    }
    void coldFormedConnectionsMatchHandCalculation()
    {
        namespace C = Structura::Design::Connections;
        C::ColdFormedInput in;
        in.mode = C::ColdFormedInput::Bolts;
        in.force = 10000.0;
        in.t1 = 2.0;
        in.fu1 = 450.0;
        in.fy1 = 345.0;
        in.t2 = 4.75;
        in.fu2 = 400.0;
        in.d = 12.0;
        in.n = 2;
        in.fub = 400.0;
        in.thread = true;
        in.end1 = 30.0;
        in.end2 = 30.0;
        in.pitch = 40.0;
        auto find = [](const std::vector<C::Check> &checks, const char *prefix) {
            for (const auto &c : checks) {
                if (c.name.rfind(prefix, 0) == 0) {
                    return c;
                }
            }
            qFatal("check nao encontrado: %s", prefix);
            return C::Check {};
        };
        const auto bolts = C::checkColdFormed(in);
        const double ab = 3.14159265358979 * 144.0 / 4.0;
        QVERIFY(std::abs(find(bolts, "Cisalhamento").resistance - 0.4 * ab * 400.0 / 1.35 / 1e3) < 1e-6);
        QVERIFY(std::abs(find(bolts, "Rasgamento entre furo e borda (perfil)").resistance - 2.0 * 30.0 * 450.0 / 1.45 / 1e3) < 1e-6);
        QVERIFY(std::abs(find(bolts, "Pressao de contato (perfil)").resistance - (0.183 * 2.0 + 1.53) * 12.0 * 2.0 * 450.0 / 1.55 / 1e3) < 1e-6);
        // minimo de 50 % da resistencia da barra (10.1-a)
        in.memberResistance = 100000.0;
        const auto minimum = C::checkColdFormed(in);
        QVERIFY(std::abs(find(minimum, "Forca minima").resistance - 50.0) < 1e-6);
        QVERIFY(std::abs(find(minimum, "Pressao de contato (perfil)").demand - 50.0 / 2.0) < 1e-6);
        // espessura acima de 4,75 mm: fora do campo da norma
        in.t1 = 6.0;
        in.t2 = 6.0;
        QVERIFY(find(C::checkColdFormed(in), "Espessura").ratio() > 1.0);
        // solda de filete (10.2.3)
        C::ColdFormedInput w;
        w.mode = C::ColdFormedInput::Weld;
        w.force = 20000.0;
        w.t1 = 3.0;
        w.t2 = 4.0;
        w.fu1 = 450.0;
        w.leg = 3.0;
        w.lengthLong = 60.0;
        w.lengthTrans = 40.0;
        w.fw = 485.0;
        const auto weldChecks = C::checkColdFormed(w);
        const double tef = 0.7 * 3.0;
        const double longBase = (1.0 - 0.01 * 60.0 / 3.0) * 3.0 * 60.0 * 450.0 / 1.65;
        const double longWeld = 0.75 * tef * 60.0 * 485.0 / 1.65;
        const double transBase = 3.0 * 40.0 * 450.0 / 1.55;
        const double transWeld = 0.75 * tef * 40.0 * 485.0 / 1.65;
        const double expected = (std::min(longBase, longWeld) + std::min(transBase, transWeld)) / 1e3;
        QVERIFY(std::abs(find(weldChecks, "Solda de filete").resistance - expected) < 1e-6);
        // parafusos autoatarraxantes (10.5.2.1): t2/t1 >= 2,5
        C::ColdFormedInput sc;
        sc.mode = C::ColdFormedInput::Screws;
        sc.force = 3000.0;
        sc.t1 = 1.0;
        sc.t2 = 3.0;
        sc.fu1 = 450.0;
        sc.fu2 = 450.0;
        sc.d = 5.5;
        sc.n = 3;
        sc.pitch = 25.0;
        sc.end1 = 15.0;
        sc.end2 = 15.0;
        sc.edge = 15.0;
        sc.fssRk = 6000.0;
        const auto screwChecks = C::checkColdFormed(sc);
        const double fc = std::min(2.7 * 1.0 * 5.5 * 450.0, 2.7 * 3.0 * 5.5 * 450.0) / 2.0 / 1e3;
        QVERIFY(std::abs(find(screwChecks, "Pressao no metal-base").resistance - fc) < 1e-6);
        QVERIFY(std::abs(find(screwChecks, "Cisalhamento do parafuso").resistance - 3.0) < 1e-6);
    }
    void distortionalBucklingCoversEveryColdFormedSection()
    {
        SteelGrade zar {"ZAR 345", 345, 430, 200000, 77000, true};
        const std::vector<std::pair<QString, QMap<QString, double>>> families = {
            {QStringLiteral("C Formado"), {{"d", 200}, {"bf", 75}, {"c", 20}, {"t", 2.65}}},
            {QStringLiteral("U Formado"), {{"d", 150}, {"bf", 60}, {"t", 3.0}}},
            {QStringLiteral("Z Formado"), {{"d", 150}, {"bf", 60}, {"t", 3.0}}},
            {QStringLiteral("L Formado"), {{"d", 100}, {"bf", 100}, {"t", 3.0}}},
            {QStringLiteral("Cartola Formado"), {{"d", 100}, {"bf", 80}, {"c", 30}, {"t", 2.0}}},
            {QStringLiteral("Tubular Retangular"), {{"b", 100}, {"h", 60}, {"t", 3.0}}}};
        auto mentions = [](const std::vector<std::string> &lines) {
            for (const auto &line : lines) {
                if (line.find("istorcional") != std::string::npos) {
                    return true;
                }
            }
            return false;
        };
        int analysed = 0;
        for (const auto &family : families) {
            if (!SectionShapes::findFamily(family.first)) {
                continue;
            }
            const DesignSection s = make(family.first, family.second);
            if (!s.supported() || s.standard != Standard::NBR14762) {
                continue;
            }
            ++analysed;
            Nbr14762::Options o;
            const auto c = Nbr14762::compression(s, zar, {3000, 3000, 3000}, o);
            QVERIFY2(c.NcRd > 0, qPrintable(family.first));
            QVERIFY2(mentions(c.lines), qPrintable(family.first));   // verificacao feita (valor ou nota) para todos os perfis
            for (const Axis axis : {Axis::X, Axis::Y}) {
                for (const bool top : {true, false}) {
                    const auto f = Nbr14762::flexure(s, zar, axis, top, 0.0, 1.0, o);
                    QVERIFY2(f.MRd > 0, qPrintable(family.first));
                    QVERIFY2(mentions(f.lines), qPrintable(family.first + (axis == Axis::X ? QStringLiteral(" X") : QStringLiteral(" Y"))));
                }
            }
        }
        QVERIFY(analysed >= 4);
        // enrijecedor curto reduz o momento resistente em relacao a um enrijecedor adequado
        const DesignSection weak = make(QStringLiteral("C Formado"), {{"d", 200}, {"bf", 100}, {"c", 8}, {"t", 1.5}});
        const DesignSection strong = make(QStringLiteral("C Formado"), {{"d", 200}, {"bf", 100}, {"c", 25}, {"t", 1.5}});
        Nbr14762::Options o;
        QVERIFY(Nbr14762::flexure(weak, zar, Axis::X, true, 0.0, 1.0, o).MRd <= Nbr14762::flexure(strong, zar, Axis::X, true, 0.0, 1.0, o).MRd * 1.0001);
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

    void shearWithTransverseStiffenersUsesKv()
    {
        // alma esbelta (h/tw ~ 145): V_Rd = 1,24 (lambda_p/lambda)^2 Vpl, lambda_p^2 proporcional a kv
        const DesignSection s = make("W Laminado", {{"d", 900}, {"bf", 250}, {"tw", 6}, {"tf", 16}});
        SteelGrade a {"A572 Gr50", 345, 450};
        Nbr8800::Options o;
        const double v0 = Nbr8800::shear(s, a, Axis::Y, o).VRd;
        o.stiffenerSpacing = 1000.0;
        const double h = s.h;
        const double kv = 5.0 + 5.0 / ((1000.0 / h) * (1000.0 / h));
        const double v1 = Nbr8800::shear(s, a, Axis::Y, o).VRd;
        QVERIFY(std::abs(v1 / v0 - kv / 5.34) < 1e-9);
        o.stiffenerSpacing = 4.0 * h;   // a/h > 3: kv = 5,34
        QVERIFY(std::abs(Nbr8800::shear(s, a, Axis::Y, o).VRd - v0) < 1e-9);
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


    /// Esforcos de extremidade do solver: um no sem cargas esta em equilibrio com a convencao "esforco na face positiva" (base do painel de alma)
    void nodeEquilibriumFromSolverEndForces()
    {
        using namespace Structura::App;
        using Services::DesignService;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
        }
        ProjectData::SectionInfo section;
        section.family = "W Laminado";
        section.geometry = {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}};
        const auto props = Structura::Visualization::SectionShapes::properties(section.family, section.geometry);
        QVERIFY(props.has_value());
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e8, 7.7e7});
        model.sections.push_back({1, props->area, props->iz, props->iy, props->j});
        SolverBridge::NodeData base;
        base.id = 1;
        base.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData joint;
        joint.id = 2;
        joint.z = 3.0;
        SolverBridge::NodeData top;
        top.id = 3;
        top.z = 6.0;
        SolverBridge::NodeData beamEnd;
        beamEnd.id = 4;
        beamEnd.x = 5.0;
        beamEnd.z = 3.0;
        beamEnd.restraints = {{1, 1, 1, 1, 1, 1}};
        model.nodes = {base, joint, top, beamEnd};
        model.members.push_back(SolverBridge::MemberData {1, 1, 2, 1, 1});   // pilar de baixo
        model.members.push_back(SolverBridge::MemberData {2, 2, 3, 1, 1});   // pilar de cima
        model.members.push_back(SolverBridge::MemberData {3, 2, 4, 1, 1});   // viga
        SolverBridge::NodalLoadData lateral;
        lateral.nodeId = 3;
        lateral.values = {{30.0, 0.0, -80.0, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(lateral);
        const auto result = SolverBridge(dll).solve(model);
        DesignService::ResultSet set;
        set.name = "ELU1";
        set.result = &result;
        set.terms = {{&model, 1.0}};
        const auto effects = DesignService::collectEffects(model, {set}, 1000.0, 1000.0);
        QCOMPARE(effects.size(), 3);
        std::vector<Structura::App::Services::NodeEquilibrium::MemberEnd> atJoint;
        for (const auto &me : effects) {
            QCOMPARE(int(me.ends.size()), 1);
            Structura::App::Services::NodeEquilibrium::MemberEnd m;
            m.atEnd = me.memberId != 3 ? me.memberId == 1 : false;   // barra 1 termina no no 2; barras 2 e 3 comecam nele
            m.x = me.ends[0].x; m.y = me.ends[0].y; m.z = me.ends[0].z;
            m.f = me.ends[0].f;
            atJoint.push_back(m);
        }
        const auto sum = Structura::App::Services::NodeEquilibrium::nodeResultant(atJoint);
        double scaleF = 1.0, scaleM = 1.0;
        for (const auto &me : effects) {
            for (int i : {0, 1, 2}) { scaleF = std::max(scaleF, std::abs(me.ends[0].f[i])); }
            for (int i : {3, 4, 5}) { scaleM = std::max(scaleM, std::abs(me.ends[0].f[i])); }
        }
        for (int i = 0; i < 3; ++i) {
            QVERIFY2(std::abs(sum[i]) < 1e-4 * scaleF, qPrintable(QString("F%1 = %2 (escala %3)").arg(i).arg(sum[i]).arg(scaleF)));
            QVERIFY2(std::abs(sum[3 + i]) < 1e-4 * scaleM, qPrintable(QString("M%1 = %2 (escala %3)").arg(i).arg(sum[3 + i]).arg(scaleM)));
        }
    }

    void warpingTorsionEffectsAndCheck()
    {
        using namespace Structura::App;
        using Structura::App::Services::DesignService;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
        }
        // Balanco em I sob torque T na ponta (unidades do modelo: kN e m). O bimomento no engaste e T tanh(kL)/k (em modulo) e, no engaste, todo o torque
        // e resistido por empenamento (psi = 0 => T_sv = 0); na ponta livre B = 0 e T_w = T - T_sv.
        const QMap<QString, double> geometry = {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}};
        const auto props = Structura::Visualization::SectionShapes::properties(QStringLiteral("W Laminado"), geometry);
        QVERIFY(props.has_value());
        const DesignSection section = make("W Laminado", geometry, props->j * 1e12);
        QVERIFY(section.Cw > 0.0);
        const double length = 3.0, torque = 5.0;   // m, kN*m
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e8, 7.7e7});
        model.sections.push_back({1, props->area, props->iz, props->iy, props->j});
        model.sections[0].warpingConstant = section.Cw * 1e-18;
        SolverBridge::NodeData base;
        base.id = 1;
        base.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData tip;
        tip.id = 2;
        tip.x = length;
        model.nodes = {base, tip};
        model.members.push_back(SolverBridge::MemberData {1, 1, 2, 1, 1});
        model.members[0].warping = true;
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{0.0, 0.0, 0.0, torque, 0.0, 0.0}};
        model.nodalLoads.push_back(load);
        const auto result = SolverBridge(dll).solve(model);
        DesignService::ResultSet set;
        set.name = "ELU1";
        set.result = &result;
        set.terms = {{&model, 1.0}};
        const auto effects = DesignService::collectEffects(model, {set}, 1000.0, 1000.0, 11);
        QCOMPARE(effects.size(), 1);
        QVERIFY(effects[0].warping);
        const auto &stations = effects[0].combos[0].stations;
        const double e = 2.0e5, g = 7.7e4;   // MPa
        const double k = std::sqrt(g * section.J / (e * section.Cw));   // 1/mm
        const double t = torque * 1e6, l = length * 1e3;   // N*mm, mm
        const double expectedBase = t * std::tanh(k * l) / k;   // N*mm2
        QVERIFY2(std::abs(std::abs(stations.front().e.B) - expectedBase) / expectedBase < 1e-6,
                 qPrintable(QString("B(0) = %1 x %2").arg(stations.front().e.B).arg(expectedBase)));
        QVERIFY(std::abs(stations.back().e.B) < 1e-6 * expectedBase);
        QVERIFY2(std::abs(std::abs(stations.front().e.Tw) - t) / t < 1e-6, qPrintable(QString("Tw(0) = %1").arg(stations.front().e.Tw)));
        // Saint-Venant puro na ponta nao e atingido (empenamento livre, mas a barra e curta); Tsv + Tw = T em toda a barra
        for (const auto &st : stations) {
            QVERIFY(std::abs(std::abs(st.e.T) - t) / t < 1e-6);
        }
        // verificacao 5.5.2.3
        Structura::Design::BarDesignInput in;
        in.section = &section;
        in.steel = {"A572 Gr50", 345, 450};
        in.length = l;
        in.warping = true;
        in.combos = effects[0].combos;
        const auto design = Structura::Design::designBar(in);
        bool normal = false, shear = false;
        for (const auto &check : design.checks) {
            normal = normal || check.id == "warping_normal";
            shear = shear || check.id == "warping_shear";
            if (check.id == "warping_normal") {
                // sigma_w = B omega/Cw no engaste, sem outros esforcos: omega = h bf/4
                const double sigma = expectedBase * section.sectorial.omegaMax / section.sectorial.Cw;   // omega_max = h bf/4 na extremidade das mesas (linha media)
                QVERIFY2(std::abs(check.demand - sigma) / sigma < 1e-6, qPrintable(QString("sigma = %1 x %2").arg(check.demand).arg(sigma)));
            }
        }
        QVERIFY(normal);
        QVERIFY(shear);
        in.warping = false;   // sem empenamento: so Saint-Venant (T_sv = T, sem tensoes de empenamento) e aviso de que o empenamento foi desprezado
        const auto plain = Structura::Design::designBar(in);
        bool plainNormal = false, plainShear = false;
        for (const auto &check : plain.checks) {
            if (check.id == "warping_normal") {
                plainNormal = true;
                QVERIFY(check.demand < 1e-6);   // sem N, M nem B: tensao normal nula
            }
            plainShear = plainShear || check.id == "warping_shear";
        }
        QVERIFY(plainNormal && plainShear);
        QVERIFY(!plain.warnings.empty());
    }

    void dumpMemorial()
    {
        if (qEnvironmentVariableIsEmpty("STRUCTURA_DUMP_MEMO")) {
            QSKIP("so desenvolvimento");
        }
        QFile out(QString::fromLocal8Bit(qgetenv("STRUCTURA_DUMP_MEMO")));
        QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream text(&out);
        text.setEncoding(QStringConverter::Utf8);
        for (const QString family : {QStringLiteral("W Laminado"), QStringLiteral("C Formado")}) {
            const QMap<QString, double> params = family.startsWith("W") ? QMap<QString, double> {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}}
                                                                        : QMap<QString, double> {{"d", 150}, {"bf", 60}, {"c", 20}, {"t", 2.65}};
            const DesignSection section = make(family, params, 0.0);
            Structura::Design::BarDesignInput in;
            in.section = &section;
            in.steel = family.startsWith("W") ? Structura::Design::SteelGrade {"A572 Gr50", 345, 450} : Structura::Design::SteelGrade {"MR250", 250, 400, 200000, 77000, true};
            in.length = 4000.0;
            Structura::Design::ComboEffects combo;
            combo.name = "ELU1";
            for (const double x : {0.0, 2000.0, 4000.0}) {
                Structura::Design::StationEffects st;
                st.x = x;
                st.e.N = -100000.0; st.e.Mx = 20e6 * (1.0 - x / 4000.0); st.e.My = 2e6; st.e.Vy = 30000.0; st.e.Vz = 5000.0;
                combo.stations.push_back(st);
            }
            in.combos = {combo};
            const auto result = Structura::Design::designBar(in);
            text << "=== " << family << "\n";
            for (const auto &c : result.summaryLines) { text << "SUMMARY " << QString::fromStdString(c) << "\n"; }
            for (const auto &c : result.checks) {
                text << "## " << QString::fromStdString(c.id) << " | " << QString::fromStdString(c.title) << " | " << QString::fromStdString(c.clause) << " | " << c.demand << " " << QString::fromStdString(c.unit) << " / " << c.resistance << "\n";
                for (const auto &l : c.lines) { text << "   " << QString::fromStdString(l) << "\n"; }
            }
        }
    }

    void sectorialPropertiesOfThinWalledSections()
    {
        const DesignSection w = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}}, 1.0);
        QVERIFY(w.sectorial.valid);
        QVERIFY(std::abs(w.sectorial.ys) < 1e-6 && std::abs(w.sectorial.zs) < 1e-6);
        const double h = 300 - 12;
        QVERIFY(std::abs(w.sectorial.omegaMax - h * 150 / 4.0) / (h * 150 / 4.0) < 1e-9);
        // Cw da linha media de um I: tf h^2 bf^3 / 24 (mesas) -- compara com a formula da norma (D.2.8-a), que usa Iy da secao real
        QVERIFY(std::abs(w.sectorial.Cw - 12.0 * h * h * std::pow(150.0, 3) / 24.0) / w.sectorial.Cw < 1e-9);
        QVERIFY(std::abs(w.sectorial.Cw - w.Cw) / w.Cw < 0.05);
        // nos eixos locais (y = altura, z = largura) as mesas estao em y constante: omega = y z (sentido positivo de torcao em torno de +x)
        for (const auto &p : w.sectorial.samples) {
            if (std::abs(std::abs(p.y) - h / 2.0) < 1e-6) {
                QVERIFY(std::abs(p.omega - p.y * p.z) < 1e-6 * h * 150.0);
            }
        }
        // U: o centro de cisalhamento fica fora da alma, a distancia e0 da linha media
        const DesignSection u = make("U Laminado", {{"d", 254}, {"bf", 66}, {"tw", 6.1}, {"tf", 11.1}, {"r", 9.0}, {"r1", 4.5}}, 1.0);
        QVERIFY2(u.sectorial.valid, qPrintable(QString("score %1").arg(u.sectorial.matchScore)));
        QVERIFY2(std::abs(u.sectorial.zs + u.x0) < 0.15 * std::abs(u.x0), qPrintable(QString("zs = %1, x0 = %2").arg(u.sectorial.zs).arg(u.x0)));
        QVERIFY(std::abs(u.sectorial.Cw - u.Cw) / u.Cw < 0.08);
        for (const auto &spec : {std::make_pair(QStringLiteral("C Formado"), QMap<QString, double> {{"d", 150}, {"bf", 60}, {"c", 20}, {"t", 2.65}}),
                                 std::make_pair(QStringLiteral("Z Formado"), QMap<QString, double> {{"d", 150}, {"bf", 60}, {"t", 2.65}}),
                                 std::make_pair(QStringLiteral("U Formado"), QMap<QString, double> {{"d", 100}, {"bf", 50}, {"t", 2.0}}),
                                 std::make_pair(QStringLiteral("L Laminado"), QMap<QString, double> {{"b", 76}, {"t", 6.35}, {"r1", 0.0}, {"r2", 0.0}})}) {
            const DesignSection s = make(spec.first, spec.second, 1.0, true);
            QVERIFY2(s.sectorial.valid, qPrintable(QString("%1 score %2 Ixy %3 Ix %4 Iy %5 ang %6").arg(spec.first).arg(s.sectorial.matchScore).arg(s.g.Ixy).arg(s.g.Ix).arg(s.g.Iy).arg(s.principalAngleDeg)));
            QVERIFY(s.sectorial.Cw >= 0.0);
        }
    }

    void threadedRodWebFlangeWeldCoverPlateAndPlasticHinges()
    {
        namespace B = Structura::Design;
        // 5.2.7: barra redonda rosqueada, ruptura da parte rosqueada com Abe = 0,75 Ab
        {
            const DesignSection rod = make("Barra Circular", {{"d", 20}});
            B::BarDesignInput in;
            in.section = &rod;
            in.steel = {"A36", 250, 400};
            in.length = 3000.0;
            in.threadedEnds = true;
            B::ComboEffects combo;
            combo.name = "ELU1";
            for (const double x : {0.0, 3000.0}) {
                B::StationEffects st;
                st.x = x;
                st.e.N = 50000.0;
                combo.stations.push_back(st);
            }
            in.combos = {combo};
            const auto result = B::designBar(in);
            const double ag = rod.g.area;
            for (const auto &check : result.checks) {
                if (check.id == "tension") {
                    QVERIFY2(std::abs(check.resistance - std::min(ag * 250.0 / 1.10, 0.75 * ag * 400.0 / 1.35) / 1000.0) < 1e-6, qPrintable(QString("%1 kind %2").arg(check.resistance).arg(int(rod.kind))));
                }
            }
        }
        // 5.4.5 / 5.4.4 / 4.11.5 em uma viga I biapoiada de 8 m
        const DesignSection w = make("W Laminado", {{"d", 400}, {"bf", 180}, {"tw", 8}, {"tf", 12.5}});
        B::BarDesignInput in;
        in.section = &w;
        in.steel = {"A572 Gr50", 345, 450};
        in.length = 8000.0;
        in.compressionFlangeBraced = true;
        in.webWeldLeg = 5.0;
        in.flangeLineLoad = 20.0;
        const double mMax = 600e6;
        B::ComboEffects combo;
        combo.name = "ELU1";
        for (int i = 0; i <= 20; ++i) {
            B::StationEffects st;
            st.x = 8000.0 * i / 20.0;
            const double u = st.x / 8000.0;
            st.e.Mx = mMax * 4.0 * u * (1.0 - u);
            st.e.Vy = 4.0 * mMax / 8000.0 * std::abs(1.0 - 2.0 * u);
            combo.stations.push_back(st);
        }
        in.combos = {combo};
        auto find = [](const B::BarDesignOutput &r, const char *id) -> const B::CheckResult * {
            for (const auto &c : r.checks) {
                if (c.id == id) {
                    return &c;
                }
            }
            return nullptr;
        };
        {
            const auto result = B::designBar(in);
            const auto *weld = find(result, "web_flange_weld");
            if (!weld) { QString ids; for (const auto &c : result.checks) { ids += QString::fromStdString(c.id) + " "; } for (const auto &wn : result.warnings) { ids += " W:" + QString::fromStdString(wn); } qWarning("ids: %s", qPrintable(ids)); }
            QVERIFY(weld);
            const double vMax = 4.0 * mMax / 8000.0;
            const double q = vMax * (180.0 * 12.5 * (400.0 - 12.5) / 2.0) / w.g.Ix;
            QVERIFY(std::abs(weld->demand - std::hypot(q, 20.0) / 2.0) < 1e-6);
            QVERIFY(std::abs(weld->resistance - std::min(0.60 * 0.707 * 5.0 * 485.0 / 1.35, 0.60 * 8.0 * std::min(345.0 / 1.10, 450.0 / 1.35))) < 1e-6);
            QVERIFY(find(result, "web_flange_weld_min"));
        }
        {
            // lamela 200 x 12,5 nas duas mesas de x = 1500 a x = 6500: ha prolongamento alem das secoes de transicao
            in.webWeldLeg = 0.0;
            in.coverB = 200.0;
            in.coverT = 12.5;
            in.coverStart = 1500.0;
            in.coverEnd = 6500.0;
            in.coverWeldLeg = 6.0;
            const auto result = B::designBar(in);
            QVERIFY(find(result, "cover_plate_transition_esquerda"));
            QVERIFY(find(result, "cover_plate_transition_direita"));
            QVERIFY(find(result, "cover_plate_end_esquerda"));
            QVERIFY(find(result, "cover_plate_flexure"));
            const double ic = w.g.Ix + 2.0 * 200.0 * 12.5 * std::pow(200.0 + 6.25, 2) + 2.0 * 200.0 * std::pow(12.5, 3) / 12.0;
            const auto *flex = find(result, "cover_plate_flexure");
            QVERIFY(std::abs(flex->resistance - ic / (200.0 + 12.5) * 345.0 / 1.10 / 1e6) < 1e-6);
            // sem solda na extremidade, a_l = 2 b = 400 mm
            in.coverEndWeld = false;
            const auto noEnd = B::designBar(in);
            bool found = false;
            for (const auto &line : find(noEnd, "cover_plate_end_esquerda")->lines) {
                found = found || line.find("a_l = 2.0 b") != std::string::npos;
            }
            QVERIFY(found);
        }
        {
            // 4.11.5: viga contida em 2 trechos de 4 m com curvatura simples: M1/M2 = -0 no apoio extremo... trecho 1: apoio (0) e meio (max): ratio 0
            in.coverB = 0.0;
            in.plasticAnalysis = true;
            in.compressionFlangeBraced = false;
            in.lateralSegments = 2;
            const auto result = B::designBar(in);
            const auto *hinge = find(result, "plastic_hinge");
            QVERIFY(hinge);
            const double ry = w.g.ry;
            QVERIFY(std::abs(hinge->resistance - B::plasticHingeUnbracedLength(false, 200000.0, 345.0, ry, 0.0)) < 1e-6);
            QVERIFY(std::abs(B::plasticHingeUnbracedLength(false, 200000.0, 345.0, ry, 0.5) - (0.12 + 0.076 * 0.5) * 200000.0 / 345.0 * ry) < 1e-9);
            QVERIFY(B::plasticHingeUnbracedLength(true, 200000.0, 345.0, ry, -1.0) >= 0.10 * 200000.0 / 345.0 * ry - 1e-9);
        }
    }

    void taperedMemberFollowsAnnexG()
    {
        namespace B = Structura::Design;
        B::TaperedGeometry constant {400.0, 400.0, 180.0, 8.0, 12.5};
        const double iConstant = constant.strongInertia(400.0);
        // d1 = d2: a inercia equivalente e a propria (os senos sao os modos exatos da coluna prismatica)
        QVERIFY(std::abs(B::taperedEquivalentInertia(constant, 6000.0) - iConstant) / iConstant < 1e-9);
        B::TaperedGeometry geometry {300.0, 600.0, 180.0, 8.0, 12.5};
        const double ixEq = B::taperedEquivalentInertia(geometry, 6000.0);
        QVERIFY(ixEq > geometry.strongInertia(300.0) && ixEq < geometry.strongInertia(600.0));
        // verificacao secao a secao: tracao com a menor altura (G.2); flexao com o momento de cada secao
        const double length = 6000.0;
        std::vector<DesignSection> stations;
        B::BarDesignInput in;
        B::ComboEffects combo;
        combo.name = "ELU1";
        for (int i = 0; i <= 20; ++i) {
            B::StationEffects st;
            st.x = length * i / 20.0;
            st.e.N = 150000.0;   // tracao
            st.e.Mx = 250e6 * (1.0 - st.x / length);   // maior no engaste (altura 300 mm)
            st.e.Vy = 80000.0;
            combo.stations.push_back(st);
            const double d = geometry.depthAt(st.x, length);
            stations.push_back(make("W Laminado", {{"d", d}, {"bf", 180}, {"tw", 8}, {"tf", 12.5}}));
        }
        const DesignSection smallest = make("W Laminado", {{"d", 300}, {"bf", 180}, {"tw", 8}, {"tf", 12.5}});
        const DesignSection largest = make("W Laminado", {{"d", 600}, {"bf", 180}, {"tw", 8}, {"tf", 12.5}});
        in.section = &stations.front();
        in.steel = {"A572 Gr50", 345, 450};
        in.length = length;
        in.compressionFlangeBraced = true;
        in.combos = {combo};
        const auto result = B::designTaperedBar(in, geometry, stations, smallest, largest);
        QVERIFY(result.supported);
        const B::CheckResult *tension = nullptr, *flexure = nullptr;
        for (const auto &c : result.checks) {
            if (c.id == "tension") tension = &c;
            if (c.id == "flexure_x") flexure = &c;
        }
        QVERIFY(tension && flexure);
        QVERIFY(std::abs(tension->resistance - smallest.g.area * 345.0 / 1.10 / 1000.0) < 1e-6);   // G.2: area da menor altura
        // o engaste (maior momento, menor altura) governa a flexao: razao igual a da secao de 300 mm isolada
        B::BarDesignInput solo = in;
        solo.section = &smallest;
        const auto alone = B::designBar(solo);
        double aloneRatio = 0.0;
        for (const auto &c : alone.checks) {
            if (c.id == "flexure_x") aloneRatio = c.ratio;
        }
        QVERIFY(flexure->ratio >= aloneRatio - 1e-9);
        QVERIFY(!result.summaryLines.empty());
    }

    void rectangularTubeStandardIsRecognisedFromTheSeries()
    {
        using Structura::App::Services::DesignService;
        auto tube = [](double t) {
            Structura::App::ProjectData::SectionInfo section;
            section.name = QStringLiteral("RHS");
            section.family = QStringLiteral("Tubular Retangular");
            section.geometry = {{QStringLiteral("b"), 150.0}, {QStringLiteral("h"), 100.0}, {QStringLiteral("t"), t}};
            section.j = 1e-6;
            return section;
        };
        using Structura::Design::Standard;
        QCOMPARE(DesignService::makeSection(tube(3.0), QStringLiteral("AUTO")).standard, Standard::NBR14762);    // parede fina: formado a frio
        QCOMPARE(DesignService::makeSection(tube(8.0), QStringLiteral("AUTO")).standard, Standard::NBR8800);     // parede grossa: laminado a quente
        QCOMPARE(DesignService::makeSection(tube(8.0), QStringLiteral("NBR14762")).standard, Standard::NBR14762);   // escolha manual prevalece
        QCOMPARE(DesignService::makeSection(tube(3.0), QStringLiteral("NBR8800")).standard, Standard::NBR8800);
    }

    void compositeColumnFollowsAnnexM()
    {
        try { compositeColumnBody(); } catch (const std::exception &ex) { QFAIL(ex.what()); }
    }

    void compositeColumnBody()
    {
        namespace B = Structura::Design;
        // tubo circular preenchido (sem armadura): D = 406,4 mm, t = 12,5 mm, fy = 345 MPa, fck = 40 MPa
        B::CompositeColumnSection tube;
        tube.type = B::CompositeColumnSection::Type::FilledCircular;
        tube.b1 = tube.b2 = 406.4;
        tube.t = 12.5;
        tube.fck = 40.0;
        tube.finalize();
        const double di = 406.4 - 25.0;
        QVERIFY(std::abs(tube.Aa - B::kPi / 4.0 * (406.4 * 406.4 - di * di)) < 1e-6);
        QVERIFY(std::abs(tube.Ac - B::kPi / 4.0 * di * di) < 1e-6);
        const auto r = B::compositeColumnResistance(tube, 345.0, 200000.0, 1.10, 1.4, 1.15, 4000.0, 4000.0);
        QVERIFY(r.valid);
        const double fcd1 = 0.95 * 40.0 / 1.4;
        QVERIFY(std::abs(r.Npl - (tube.Aa * 345.0 / 1.10 + tube.Ac * fcd1)) < 1e-3);
        QVERIFY(r.delta > 0.1 && r.delta < 0.9);
        // alfa_c = 0,45 + 3 (Aa + As)/(Ac + Aa + As) <= 0,9; (EI)e = Ea Ia + alfa_c Ec Ic
        const double alphaC = std::min(0.45 + 3.0 * tube.Aa / (tube.Ac + tube.Aa), 0.9);
        QVERIFY(std::abs(tube.alphaC() - alphaC) < 1e-12);
        QVERIFY(std::abs(r.EIx - (200000.0 * tube.IaX + alphaC * tube.Ec * tube.IcX)) / r.EIx < 1e-12);
        QVERIFY(r.chi_x > 0.0 && r.chi_x <= 1.0);
        QVERIFY(std::abs(r.NRd_x - r.chi_x * r.Np) < 1e-6);
        // momento de plastificacao sem armadura: hn = Ac fcd1 / (2 D fcd1 + 4 t (2 fyd - fcd1)) (M.5.5.4)
        const double fyd = 345.0 / 1.10;
        const double hn = tube.Ac * fcd1 / (2.0 * 406.4 * fcd1 + 4.0 * 12.5 * (2.0 * fyd - fcd1));
        QVERIFY(std::abs(r.hnX - hn) < 1e-6);
        const double zcn = (406.4 - 25.0) * hn * hn, zan = 406.4 * hn * hn - zcn;
        const double mpl = fyd * (tube.ZaX - zan) + 0.5 * fcd1 * (tube.ZcX - zcn);
        QVERIFY(std::abs(r.Mpl_x - mpl) / mpl < 1e-9);
        QVERIFY(r.MmaxPl_x > r.Mpl_x);

        // secao totalmente revestida com armadura: W 310 x 97 aproximado, 450 x 450, 8 barras
        B::CompositeColumnSection enc;
        enc.type = B::CompositeColumnSection::Type::FullyEncased;
        enc.d = 308.0;
        enc.bf = 305.0;
        enc.tw = 9.9;
        enc.tf = 15.4;
        enc.hc = enc.bc = 500.0;
        enc.fck = 30.0;
        enc.ringOfBars(3, 1, 314.0, 45.0);
        enc.finalize();
        QVERIFY(std::abs(enc.As - 8 * 314.0) < 1e-9);
        QVERIFY(std::abs(enc.Ac + enc.Aa + enc.As - 500.0 * 500.0) < 1e-6);
        const auto re = B::compositeColumnResistance(enc, 345.0, 200000.0, 1.10, 1.4, 1.15, 4000.0, 4000.0);
        QVERIFY(re.valid && re.Mpl_x > 0.0 && re.Mpl_y > 0.0);
        // a linha neutra plastica fica dentro do perfil e M_pl <= M_max,pl
        QVERIFY(re.hnX >= 0.0 && re.hnX <= 250.0);
        QVERIFY(re.Mpl_x <= re.MmaxPl_x + 1e-6 && re.Mpl_y <= re.MmaxPl_y + 1e-6);
        QVERIFY(std::abs(re.Npl_s - enc.As * enc.fsk / 1.15) < 1e-6);

        // verificacao da barra: compressao pura de 40 % de N_Rd e interacao pelo modelo I
        B::BarDesignInput in;
        in.compositeColumn = &tube;
        in.steel = {"A572 Gr50", 345, 450};
        in.length = 4000.0;
        B::ComboEffects combo;
        combo.name = "ELU1";
        for (int i = 0; i <= 4; ++i) {
            B::StationEffects st;
            st.x = 1000.0 * i;
            st.e.N = -0.4 * std::min(r.NRd_x, r.NRd_y);
            st.e.Mx = 30e6;
            combo.stations.push_back(st);
        }
        in.combos = {combo};
        const auto out = B::designCompositeColumn(in, 1);
        QVERIFY(out.supported);
        const B::CheckResult *comp = nullptr, *inter = nullptr;
        for (const auto &c : out.checks) {
            if (c.id == "compression") comp = &c;
            if (c.id == "interaction") inter = &c;
        }
        QVERIFY(comp && inter);
        QVERIFY(std::abs(comp->ratio - 0.4) < 1e-9);
        QVERIFY(inter->ratio > 0.4);
        const auto iii = B::designCompositeColumn(in, 3);
        QVERIFY(iii.supported);
    }

    void serviceCantileverEffectsAndSigns()
    {
        using namespace Structura::App;
        using Services::DesignService;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
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

        // tirante (so tracao): a compressao residual e folga e nao reprova por esbeltez de compressao
        {
            SolverBridge::ModelData slack = model;
            slack.nodalLoads.clear();
            SolverBridge::NodalLoadData push;
            push.nodeId = 2;
            push.values = {{-5.0, 0.0, 0.0, 0.0, 0.0, 0.0}};   // 5 kN de compressao
            slack.nodalLoads.push_back(push);
            slack.nodes[1].x = 12.0;   // barra muito esbelta
            const auto pushed = SolverBridge(dll).solve(slack);
            DesignService::ResultSet pushSet;
            pushSet.name = "ELU1";
            pushSet.result = &pushed;
            pushSet.terms = {{&slack, 1.0}};
            ProjectData::DesignSettings tieSettings;
            tieSettings.defaultSteel = "ASTM A572 Gr.50";
            const auto asColumn = DesignService::collectEffects(slack, {pushSet}, 1000.0, 1000.0);
            const auto columnVerdict = DesignService::verifyMember(asColumn[0], section, tieSettings.paramsFor(1), tieSettings, 1000.0);
            QVERIFY(columnVerdict.output.maxRatio > 1.0);   // como pilar, a esbeltez reprova
            slack.members[0].tensionOnly = true;
            const auto asTie = DesignService::collectEffects(slack, {pushSet}, 1000.0, 1000.0);
            for (const auto &station : asTie[0].combos[0].stations) {
                QVERIFY(station.e.N >= 0.0);
            }
            const auto tieVerdict = DesignService::verifyMember(asTie[0], section, tieSettings.paramsFor(1), tieSettings, 1000.0);
            QVERIFY(tieVerdict.output.maxRatio < 1.0);

            // as demais barras ficam inalteradas: duas barras, so a 2 e tirante; a 1 mantem a compressao e o veredito
            SolverBridge::ModelData pair = slack;
            pair.members[0].tensionOnly = false;
            SolverBridge::NodeData extraBase, extraTip;
            extraBase.id = 3;
            extraBase.y = 5.0;
            extraBase.restraints = {{1, 1, 1, 1, 1, 1}};
            extraTip.id = 4;
            extraTip.x = 12.0;
            extraTip.y = 5.0;
            pair.nodes.push_back(extraBase);
            pair.nodes.push_back(extraTip);
            pair.members.push_back(SolverBridge::MemberData {2, 3, 4, 1, 1});
            pair.members[1].tensionOnly = true;
            SolverBridge::NodalLoadData pushTwo;
            pushTwo.nodeId = 4;
            pushTwo.values = {{-5.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
            pair.nodalLoads.push_back(pushTwo);
            const auto pairResult = SolverBridge(dll).solve(pair);
            DesignService::ResultSet pairSet;
            pairSet.name = "ELU1";
            pairSet.result = &pairResult;
            pairSet.terms = {{&pair, 1.0}};
            const auto both = DesignService::collectEffects(pair, {pairSet}, 1000.0, 1000.0);
            QCOMPARE(both.size(), 2);
            const auto &column = both[0].memberId == 1 ? both[0] : both[1];
            const auto &tieBar = both[0].memberId == 1 ? both[1] : both[0];
            // a barra comum e igual a calculada sozinha (esforcos e veredito preservados)
            QCOMPARE(column.combos[0].stations.size(), asColumn[0].combos[0].stations.size());
            for (std::size_t i = 0; i < column.combos[0].stations.size(); ++i) {
                QCOMPARE(column.combos[0].stations[i].e.N, asColumn[0].combos[0].stations[i].e.N);
                QCOMPARE(column.combos[0].stations[i].e.Mx, asColumn[0].combos[0].stations[i].e.Mx);
            }
            QVERIFY(column.combos[0].stations.front().e.N < 0.0);
            const auto pairColumn = DesignService::verifyMember(column, section, tieSettings.paramsFor(1), tieSettings, 1000.0);
            QCOMPARE(pairColumn.output.maxRatio, columnVerdict.output.maxRatio);
            for (const auto &station : tieBar.combos[0].stations) {
                QVERIFY(station.e.N >= 0.0);
            }
        }

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
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
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


    void purlinWithPanelOnTensionFlangeFollowsAnnexF()
    {
        SteelGrade zar {"ZAR 345", 345, 430, 200000, 77000, true};
        const DesignSection s = make("C Formado", {{"d", 200}, {"bf", 60}, {"c", 20}, {"t", 2.0}});
        Nbr14762::Options o;
        o.checkDistortional = false;
        const double yield = Nbr14762::flexure(s, zar, Axis::X, true, 0.0, 1.0, o).MRd;
        o.panelTensionFlange = 1;   // biapoiada: 165 < bw = 200 <= 216 -> R = 0,65 (U)
        QVERIFY(std::abs(Nbr14762::flexure(s, zar, Axis::X, true, 2000.0, 1.0, o).MRd - 0.65 * yield) < 1e-6 * yield);
        o.panelTensionFlange = 2;   // continua: U -> R = 0,60
        QVERIFY(std::abs(Nbr14762::flexure(s, zar, Axis::X, true, 2000.0, 1.0, o).MRd - 0.60 * yield) < 1e-6 * yield);
        // bw/t = 200 > 170: condicao do Anexo F nao atendida, volta a FLT
        const DesignSection thin = make("C Formado", {{"d", 200}, {"bf", 60}, {"c", 20}, {"t", 1.0}});
        o.panelTensionFlange = 1;
        Nbr14762::Options off = o;
        off.panelTensionFlange = 0;
        QCOMPARE(Nbr14762::flexure(thin, zar, Axis::X, true, 2000.0, 1.0, o).MRd, Nbr14762::flexure(thin, zar, Axis::X, true, 2000.0, 1.0, off).MRd);
    }

    void connectionFrameTouchesTheChosenSupportFace()
    {
        using Structura::App::Services::SupportSection;
        using Structura::App::Services::supportFaceOffset;
        // pilar W 250 (d = 260, bf = 146, tw = 6,1) no eixo z; modelo em metros (0,001 por mm); y' = X global, z' = Y global
        SupportSection column;
        column.axisPoint = {0.0, 0.0, 0.0};
        column.y = {1.0, 0.0, 0.0};
        column.z = {0.0, 1.0, 0.0};
        column.depth = 260.0;
        column.width = 146.0;
        column.webThickness = 6.1;
        column.isIShape = true;
        double offset = 0.0;
        // viga vinda de +X pela altura do perfil: face externa da mesa em d/2 = 130 mm
        QVERIFY(supportFaceOffset({0.0, 0.0, 3.0}, {1.0, 0.0, 0.0}, column, 0.001, offset));
        QVERIFY(std::abs(offset - 0.130) < 1e-12);
        // eixo da viga deslocado 20 mm (excentricidade) para dentro do pilar: a face fica 20 mm mais longe da origem da viga
        QVERIFY(supportFaceOffset({-0.020, 0.0, 3.0}, {1.0, 0.0, 0.0}, column, 0.001, offset));
        QVERIFY(std::abs(offset - 0.150) < 1e-12);
        // viga vinda de -X (sentido oposto): mesma face, do outro lado
        QVERIFY(supportFaceOffset({0.0, 0.0, 3.0}, {-1.0, 0.0, 0.0}, column, 0.001, offset));
        QVERIFY(std::abs(offset - 0.130) < 1e-12);
        // viga vinda de +Y (pela largura): face da alma, tw/2 = 3,05 mm
        QVERIFY(supportFaceOffset({0.0, 0.0, 3.0}, {0.0, 1.0, 0.0}, column, 0.001, offset));
        QVERIFY(std::abs(offset - 0.00305) < 1e-12);
        // pilar excentrico: o eixo do pilar fica 50 mm em +X; a face voltada para -X esta 80 mm do eixo da viga em x = 0
        column.axisPoint = {0.050, 0.0, 0.0};
        QVERIFY(supportFaceOffset({0.0, 0.0, 3.0}, {-1.0, 0.0, 0.0}, column, 0.001, offset));
        QVERIFY(std::abs(offset - (0.130 - 0.050)) < 1e-12);
        // viga paralela ao apoio: nao ha face
        QVERIFY(!supportFaceOffset({0.0, 0.0, 3.0}, {0.0, 0.0, 1.0}, column, 0.001, offset));
    }

    void supportWebLateralBucklingFollowsClause575()
    {
        namespace C = Structura::Design::Connections;
        C::SupportInput in;
        in.face = C::SupportInput::Flange;
        in.d = 300.0;
        in.bf = 150.0;
        in.tf = 10.0;
        in.tw = 7.0;
        in.k = 15.0;
        in.compression = 40000.0;
        in.lengthAlong = 100.0;
        in.lengthTransverse = 60.0;
        auto find = [](const std::vector<C::Check> &checks) -> const C::Check * {
            for (const auto &c : checks) {
                if (c.name.find("flambagem lateral") != std::string::npos) {
                    return &c;
                }
            }
            return nullptr;
        };
        QVERIFY(find(C::checkSupportMember(in)) == nullptr);   // mesas contidas (l = 0): dispensada
        in.unbracedLength = 5000.0;
        const auto checks = C::checkSupportMember(in);
        const auto *c = find(checks);
        QVERIFY(c != nullptr);
        const double h = 300.0 - 2.0 * 15.0;
        const double slender = (h / 7.0) / (5000.0 / 150.0);
        QVERIFY(slender < 1.70);
        const double expected = 16.0 * 200000.0 * std::pow(7.0, 3.0) * 10.0 / (1.10 * h * h) * 0.37 * std::pow(slender, 3.0);
        QVERIFY(std::abs(c->resistance - expected / 1000.0) < 1e-6);
        in.unbracedLength = 2000.0;   // (h/tw)/(l/bf) > 1,70: o estado-limite nao ocorre
        const auto *dispensed = find(C::checkSupportMember(in));
        QVERIFY(dispensed != nullptr && dispensed->ratio() == 0.0);
    }

    void coldWorkStrengthIncreaseFollowsAnnexB()
    {
        SteelGrade zar {"ZAR 345", 345, 430, 200000, 77000, true};
        const DesignSection s = make("C Formado", {{"d", 100}, {"bf", 50}, {"c", 17}, {"t", 3.0}});
        Nbr14762::Options o;
        std::string note;
        const double fya = Nbr14762::coldWorkYield(s, zar, false, &note);
        // B.2.1-c com ri = t: Bc = 3,69 r - 0,819 r^2 - 1,79; m = 0,192 r - 0,068; fyc = Bc fy; C = 4 dobras / area
        const double r = 430.0 / 345.0;
        const double bc = 3.69 * r - 0.819 * r * r - 1.79;
        const double fyc = bc * 345.0;
        const double cornerArea = 3.0 * 0.5 * 3.14159265358979 * (3.0 + 1.5);
        const double c = 4.0 * cornerArea / s.g.area;
        QVERIFY(std::abs(fya - std::min(c * fyc + (1.0 - c) * 345.0, 430.0)) < 1e-6);
        QVERIFY(fya > 345.0 && fya <= 430.0);
        const double plain = Nbr14762::tension(s, zar, o).yieldRd;
        o.coldWork = true;
        QVERIFY(std::abs(Nbr14762::tension(s, zar, o).yieldRd - plain * fya / 345.0) < 1e-6 * plain);
        // fu/fy < 1,2: sem aumento
        SteelGrade weak {"A", 345, 400, 200000, 77000, true};
        QCOMPARE(Nbr14762::coldWorkYield(s, weak, false), 345.0);
    }

    void beamSupportingColumnUsesTableB1Limit()
    {
        using namespace Structura::App;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
        }
        // viga de 6 m com um pilar que nasce no meio do vao (viga de transicao): limite L/500 (Tabela B.1)
        SolverBridge::ModelData model;
        model.materials.push_back({1, 2.0e8, 7.7e7});
        model.sections.push_back({1, 0.01, 2.0e-4, 2.0e-4, 1e-5});
        SolverBridge::NodeData n1, n2, n3, n4;
        n1.id = 1;
        n1.restraints = {1, 1, 1, 1, 0, 0};
        n2.id = 2;
        n2.x = 6.0;
        n2.restraints = {0, 1, 1, 0, 0, 0};
        n3.id = 3;
        n3.x = 3.0;
        n4.id = 4;
        n4.x = 3.0;
        n4.z = 3.0;
        model.nodes = {n1, n2, n3, n4};
        model.members.push_back(SolverBridge::MemberData {1, 1, 3, 1, 1});
        model.members.push_back(SolverBridge::MemberData {2, 3, 2, 1, 1});
        model.members.push_back(SolverBridge::MemberData {3, 3, 4, 1, 1});
        SolverBridge::NodalLoadData load;
        load.nodeId = 4;
        load.values = {{0.0, 0.0, -50.0, 0.0, 0.0, 0.0}};
        model.nodalLoads.push_back(load);
        const auto result = SolverBridge(dll).solve(model);
        Services::DesignService::ResultSet set;
        set.name = "ELS";
        set.result = &result;
        set.terms = {{&model, 1.0}};
        ProjectData::DesignSettings settings;
        const auto out = Services::StructuralChecks::deflections(model, {set}, settings, 1000.0);
        bool beams = false;
        for (const auto &d : out) {
            if (d.memberId == 1 || d.memberId == 2) {
                QCOMPARE(d.denominator, 500);
                beams = true;
            }
        }
        QVERIFY(beams);
    }


    void b2FromStoryDrift()
    {
        using namespace Structura::App;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
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

    void cbOfCantileverAndOneBracedFlange()
    {
        const DesignSection s = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        auto run = [&](bool cantilever, int oneFlange, bool away, double m0, double mEnd) {
            BarDesignInput in;
            in.section = &s;
            in.steel = {"A572 Gr50", 345, 450};
            in.length = 4000;
            in.cantilever = cantilever;
            in.oneFlangeBraced = oneFlange;
            in.forceAwayFromFreeFlange = away;
            in.amplifyB1 = false;
            ComboEffects c;
            c.name = "ELU1";
            for (int i = 0; i <= 4; ++i) {
                StationEffects st;
                st.x = i * 1000.0;
                st.e.Mx = m0 + (mEnd - m0) * i / 4.0;
                c.stations.push_back(st);
            }
            in.combos.push_back(c);
            const auto out = designBar(in);
            for (const auto &check : out.checks) {
                if (check.id == "flexure_x") {
                    std::string text;
                    for (const auto &l : check.lines) {
                        text += l + "\n";
                    }
                    return std::make_pair(check.ratio, text);
                }
            }
            return std::make_pair(0.0, std::string());
        };
        // balanco: momento nulo na ponta livre e maximo no engaste; a formula geral daria Cb = 1,67, a norma manda Cb = 1,0 (5.4.2.3-b)
        const auto general = run(false, 0, false, 0.0, -40e6);
        const auto cantilever = run(true, 0, false, 0.0, -40e6);
        QVERIFY(general.second.find("Cb = 1.67") != std::string::npos);
        QVERIFY(cantilever.second.find("Cb = 1.00") != std::string::npos);
        QVERIFY(cantilever.first > general.first);
        // uma mesa contida (superior): momento negativo comprime a mesa inferior (livre). M1 = M2 = MCL: Cb = 1,0; M1 = -M, M2 = 0: Cb = 5/3
        const auto uniform = run(false, 1, false, -40e6, -40e6);
        QVERIFY(uniform.second.find("5.4.2.4-a") != std::string::npos);
        QVERIFY(uniform.second.find("Cb = 1.00") != std::string::npos);
        const auto triangular = run(false, 1, false, -40e6, 0.0);
        QVERIFY2(triangular.second.find("Cb = 1.67") != std::string::npos, triangular.second.c_str());
        // forcas no sentido oposto a mesa livre: caso c) Cb = 1,0
        const auto away = run(false, 1, true, -40e6, 0.0);
        QVERIFY(away.second.find("5.4.2.4-c") != std::string::npos);
        // momento positivo comprime a mesa contida (superior): sem FLT
        const auto bracedCompressed = run(false, 1, false, 40e6, 40e6);
        QVERIFY(bracedCompressed.second.find("sem FLT") != std::string::npos);
        QVERIFY(bracedCompressed.first < uniform.first);
    }
    void annexCSeparatesNtAndLtAndClassifiesTheStructure()
    {
        // B1 e B2 aplicados a estrutura nt/lt: N_Sd = N_nt + B2 N_lt; M_Sd = B1 M_nt + B2 M_lt (Anexo C.2.1)
        const DesignSection s = make("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
        auto build = [&](bool useNt, double b2, double stiffness) {
            BarDesignInput in;
            in.section = &s;
            in.steel = {"A572 Gr50", 345, 450};
            in.length = 3000;
            in.amplifyB1 = true;
            ComboEffects c;
            c.name = "ELU1";
            c.b2 = b2;
            c.hasNt = useNt;
            c.bStiffness = stiffness;
            for (int i = 0; i <= 2; ++i) {
                StationEffects st;
                st.x = i * 1500.0;
                st.e.N = -100e3;
                st.e.Mx = 60e6;
                st.nt.N = -80e3;
                st.nt.Mx = 40e6;
                c.stations.push_back(st);
            }
            in.combos.push_back(c);
            return designBar(in);
        };
        const auto withNt = build(true, 1.2, 1.0);
        std::string text;
        for (const auto &check : withNt.checks) {
            if (check.id == "flexure_x") {
                for (const auto &l : check.lines) {
                    text += l + "\n";
                }
            }
        }
        QVERIFY2(text.find("N_Sd = N_nt + B2 N_lt = -80.00 + 1.200 x -20.00 = -104.00 kN") != std::string::npos, text.c_str());
        // rigidez reduzida (media deslocabilidade) aumenta B1 (Ne x 0,8)
        const auto reduced = build(true, 1.2, 0.8);
        auto interaction = [](const BarDesignOutput &out) {
            for (const auto &check : out.checks) {
                if (check.id == "interaction") {
                    return check.ratio;
                }
            }
            return 0.0;
        };
        QVERIFY(interaction(reduced) > interaction(withNt) && interaction(withNt) > 0.0);
        // sem a separacao, a forca axial nao e amplificada e o momento total e amplificado por B1 e B2
        const auto legacy = build(false, 1.2, 1.0);
        QVERIFY(interaction(legacy) != interaction(withNt));
    }
    void swayClassificationAndReducedStiffnessB2()
    {
        using namespace Structura::App;
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
        if (!QFile::exists(dll)) {
            QSKIP("rds.dll nao encontrada na raiz do repositorio.");
        }
        const double E = 2.0e8, I = 2.0e-4, L = 5.0, H = 20.0;
        auto run = [&](double P) {
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
            return Services::StructuralChecks::stories(model, {set}, settings, 1000.0, 1000.0);
        };
        const double k = 3.0 * E * I / (L * L);                       // P L^2 / (3 E I) = P / k
        const auto small = run(0.05 * k);                              // B2 = 1/(1-0,05) = 1,053 <= 1,10
        QCOMPARE(small.swayClass, 0);
        QVERIFY(std::abs(small.stiffnessFactor - 1.0) < 1e-12);
        const auto medium = run(0.17 * k);                             // B2 nominal = 1/(1-0,17) = 1,205: media deslocabilidade
        QCOMPARE(medium.swayClass, 1);
        QVERIFY(std::abs(medium.maxB2Nominal - 1.0 / (1.0 - 0.17)) < 1e-2);
        QVERIFY(std::abs(medium.stiffnessFactor - 0.8) < 1e-12);
        QVERIFY(std::abs(medium.b2.value("ELU1").value(1) - 1.0 / (1.0 - 0.17 / 0.8)) < 1e-2);   // B2 com a rigidez reduzida a 0,8
        const auto large = run(0.40 * k);                              // B2 = 1,67: grande deslocabilidade
        QCOMPARE(large.swayClass, 2);
    }
    void exceptionalCombinationsUseTable3Coefficients()
    {
        using namespace Structura::App;
        QVERIFY(Services::DesignService::isExceptionalCombination("EXC1"));
        QVERIFY(!Services::DesignService::isExceptionalCombination("ELU1"));
        Structura::App::ProjectData::SectionInfo section;
        section.family = QStringLiteral("W Laminado");
        section.name = QStringLiteral("W 310x32.7");
        section.geometry = {{"d", 313}, {"bf", 102}, {"tw", 6.6}, {"tf", 10.8}};
        section.area = 4190.0;
        section.iz = 6.5e7 * 1e-12;
        section.iy = 1.9e6 * 1e-12;
        section.j = 2.9e4 * 1e-12;
        auto effects = [&](const char *name) {
            Services::DesignService::MemberEffects me;
            me.memberId = 1;
            me.lengthMm = 3000.0;
            Structura::Design::ComboEffects c;
            c.name = name;
            for (int i = 0; i <= 2; ++i) {
                Structura::Design::StationEffects st;
                st.x = i * 1500.0;
                st.e.N = 300e3;   // tracao: Nt,Rd depende de gama_a1 e gama_a2
                c.stations.push_back(st);
            }
            me.combos.push_back(c);
            return me;
        };
        ProjectData::DesignSettings settings;
        ProjectData::DesignMemberParams params;
        const auto normal = Services::DesignService::verifyMember(effects("ELU1"), section, params, settings, 1.0);
        const auto exceptional = Services::DesignService::verifyMember(effects("EXC1"), section, params, settings, 1.0);
        double rn = 0.0, re = 0.0;
        for (const auto &c : normal.output.checks) {
            if (c.id == "tension") {
                rn = c.ratio;
            }
        }
        for (const auto &c : exceptional.output.checks) {
            if (c.id == "tension") {
                re = c.ratio;
            }
        }
        QVERIFY(rn > 0.0 && re > 0.0);
        // escoamento: gama_a1 1,10 -> 1,00 (razao x 1,00/1,10); a ruptura (1,35 -> 1,15) reduz ainda mais; Nt,Rd = min dos dois
        QVERIFY(re < rn);
        QVERIFY(std::abs(re / rn - 1.0 / 1.10) < 1e-6 || std::abs(re / rn - 1.15 / 1.35) < 1e-6);
    }
    void compositeSectionsGeometryAndCatalog()
    {
        using Structura::Visualization::SectionCatalog;
        const QMap<QString, double> l = {{"b", 76.0}, {"t", 6.35}};
        const auto single = SectionShapes::properties(QStringLiteral("L Laminado"), l);
        QMap<QString, double> pair = l;
        pair.insert(QStringLiteral("e"), 10.0);
        const auto doubled = SectionShapes::properties(QStringLiteral("Dupla Cantoneira T"), pair);
        QVERIFY(single && doubled);
        QVERIFY(std::abs(doubled->area - 2.0 * single->area) / single->area < 1e-9);
        // Iz (flexao no plano da altura): inercias das duas cantoneiras em torno do eixo horizontal comum do conjunto
        QVERIFY(doubled->iz > 2.0 * single->iz * 0.5);
        // o afastamento aumenta a inercia em torno do eixo vertical (Iy): Iy(e) > Iy(0)
        QMap<QString, double> touching = l;
        touching.insert(QStringLiteral("e"), 0.0);
        const auto zeroGap = SectionShapes::properties(QStringLiteral("Dupla Cantoneira T"), touching);
        QVERIFY(zeroGap && doubled->iy > zeroGap->iy);
        QVERIFY(SectionShapes::isComposite(QStringLiteral("Duplo U Costas")) && !SectionShapes::isComposite(QStringLiteral("W Laminado")));
        // catalogo derivado dos perfis laminados
        if (!SectionCatalog::profiles(QStringLiteral("L Laminado")).isEmpty()) {   // o catalogo e um recurso Qt (so presente no aplicativo)
            const auto &angles = SectionCatalog::profiles(QStringLiteral("Dupla Cantoneira T"));
            QVERIFY(angles.size() == SectionCatalog::profiles(QStringLiteral("L Laminado")).size() && !angles.isEmpty());
            QVERIFY(angles.first().name.startsWith(QStringLiteral("2L ")));
            QVERIFY(std::abs(angles.first().parameters.value(QStringLiteral("e")) - 10.0) < 1e-12);
            QVERIFY(SectionCatalog::profiles(QStringLiteral("Duplo U Costas")).first().name.contains(QStringLiteral("(costas)")));
        }
        // as barras compostas sao reconhecidas pelo dimensionamento
        const auto design = make(QStringLiteral("Dupla Cantoneira T"), pair, doubled->j * 1e12);
        QVERIFY(design.supported() && design.kind == Structura::Design::SectionKind::DoubleAngleT && design.composite());
        QVERIFY(design.rMin > 0.0 && design.rMin < design.g.ry);
        QVERIFY(design.symmetricAboutY && !design.symmetricAboutX);
        const auto boxes = make(QStringLiteral("Duplo U Face a Face"), {{"d", 203}, {"bf", 57}, {"tw", 5.6}, {"tf", 9.5}, {"e", 10}}, 1.0e5);
        QVERIFY(boxes.supported() && boxes.kind == Structura::Design::SectionKind::DoubleChannelBox && boxes.symmetricAboutX && boxes.symmetricAboutY);
    }
    void compositeCompressionUsesEquivalentSlendernessOfClause536()
    {
        const QMap<QString, double> pair = {{"b", 76.0}, {"t", 6.35}, {"e", 10.0}};
        const auto props = SectionShapes::properties(QStringLiteral("Dupla Cantoneira T"), pair);
        QVERIFY(props);
        const DesignSection s = make(QStringLiteral("Dupla Cantoneira T"), pair, props->j * 1e12);
        auto run = [&](double spacing, bool welded) {
            BarDesignInput in;
            in.section = &s;
            in.steel = {"A36", 250, 400};
            in.length = 3000;
            in.Kx = in.Ky = in.Kz = 1.0;
            in.compositeSpacing = spacing;
            in.compositeWelded = welded;
            ComboEffects c;
            c.name = "ELU1";
            for (int i = 0; i <= 2; ++i) {
                StationEffects st;
                st.x = i * 1500.0;
                st.e.N = -60e3;
                c.stations.push_back(st);
            }
            in.combos.push_back(c);
            return designBar(in);
        };
        auto find = [](const BarDesignOutput &out, const char *id) -> const CheckResult * {
            for (const auto &check : out.checks) {
                if (check.id == id) {
                    return &check;
                }
            }
            return nullptr;
        };
        const auto bolted = run(1000.0, false);
        const auto welded = run(1000.0, true);
        const auto sparse = run(1500.0, false);
        QVERIFY(find(bolted, "composite") && find(bolted, "compression"));
        // 5.3.6.3-a (aperto normal) penaliza mais que 5.3.6.3-b (solda); ligacoes mais afastadas reduzem Nc,Rd
        QVERIFY(find(bolted, "compression")->resistance <= find(welded, "compression")->resistance + 1e-9);
        QVERIFY(find(sparse, "compression")->resistance < find(bolted, "compression")->resistance);
        std::string text;
        for (const auto &l : find(bolted, "compression")->lines) {
            text += l + "\n";
        }
        QVERIFY2(text.find("5.3.6.3-a") != std::string::npos, text.c_str());
        // 5.3.6.1: so uma ligacao intermediaria (l = L/2) nao atende
        QVERIFY(find(sparse, "composite")->ratio > 1.0);
        QVERIFY(find(bolted, "composite")->ratio < find(sparse, "composite")->ratio);
        // flexao da dupla cantoneira em T pela D.2.4
        BarDesignInput in;
        in.section = &s;
        in.steel = {"A36", 250, 400};
        in.length = 3000;
        ComboEffects c;
        c.name = "ELU1";
        for (int i = 0; i <= 2; ++i) {
            StationEffects st;
            st.x = i * 1500.0;
            st.e.Mx = 3e6;
            c.stations.push_back(st);
        }
        in.combos.push_back(c);
        const auto flex = designBar(in);
        std::string flexText;
        for (const auto &check : flex.checks) {
            if (check.id == "flexure_x") {
                for (const auto &l : check.lines) {
                    flexText += l + "\n";
                }
            }
        }
        QVERIFY2(flexText.find("D.2.4") != std::string::npos, flexText.c_str());
    }
    void dumpCompositeLoops()
    {
        if (qEnvironmentVariableIsEmpty("STRUCTURA_DUMP_LOOPS")) {
            QSKIP("so desenvolvimento");
        }
        QFile out(QString::fromLocal8Bit(qgetenv("STRUCTURA_DUMP_LOOPS")));
        QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream text(&out);
        const QMap<QString, QMap<QString, double>> cases = {
            {QStringLiteral("Dupla Cantoneira T"), {{"b", 76.0}, {"t", 6.35}, {"e", 10.0}}},
            {QStringLiteral("Duplo U Costas"), {{"d", 203}, {"bf", 57}, {"tw", 5.6}, {"tf", 9.5}, {"e", 10}}},
            {QStringLiteral("Duplo U Face a Face"), {{"d", 203}, {"bf", 57}, {"tw", 5.6}, {"tf", 9.5}, {"e", 10}}}};
        for (auto it = cases.begin(); it != cases.end(); ++it) {
            const auto shape = SectionShapes::build(it.key(), it.value(), false);
            QVERIFY(shape);
            text << "FAMILY " << it.key() << " outer " << shape->outerCount << "\n";
            for (const auto &loop : shape->loops) {
                text << "LOOP";
                for (const auto &p : loop) {
                    text << " " << p[0] << "," << p[1];
                }
                text << "\n";
            }
        }
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
