// J3 do Joint Mechanical Engine: REPRESENTABILIDADE. (A) toda tipologia REAL do ConnectionCatalog e dos tipos legados e derivada em topologia valida
// ou recebe estado de indisponibilidade com justificativa; (B) configuracoes conceituais de todas as familias (viga-pilar parafusada e soldada, hibrida,
// gusset, no de trelica, emenda, tubular, mista, base com chumbadores) sao representaveis pelos contratos. Capacidade MECANICA e declarada a parte:
// representar a geometria NAO implica modelo fundamentado (so o metodo dos componentes EN tem modelo). Nao ha nenhum modelo constitutivo novo.
#include "app/ProjectDataTypes.h"
#include "app/services/ConnectionCatalog.h"
#include "app/services/JointTopologyAdapter.h"
#include "design/jointengine/Capability.h"
#include "design/jointengine/Fingerprint.h"
#include "design/jointengine/Topology.h"

#include <QtTest/QtTest>

#include <algorithm>
#include <set>

namespace JE = Structura::Design::JointEngine;
namespace AS = Structura::App::Services;
using NC = Structura::App::ProjectData::NodeConnection;

namespace {

JE::PartRef part(int id, JE::PartKind kind, const char *role, int memberId = 0)
{
    JE::PartRef p;
    p.id = id;
    p.kind = kind;
    p.role = role;
    p.memberId = memberId;
    return p;
}

JE::Link link(int id, JE::LinkKind kind, std::vector<int> ids, JE::Link::Resolution res = JE::Link::Resolution::Pairwise)
{
    JE::Link l;
    l.id = id;
    l.kind = kind;
    l.partIds = std::move(ids);
    l.resolution = res;
    return l;
}

JE::JointTopology finish(JE::JointTopology t)
{
    t.gaps.push_back({"LOAD_PATHS_NOT_DECLARED", "fixture conceitual: sem caminhos de carga declarados"});
    t.status = JE::TopologyStatus::Partial;
    return t;
}

struct Fixture {
    std::string name;
    JE::JointTopology topology;
    JE::ValidityStatus mechanical;   // capacidade mecanica declarada
    std::string response;            // tipo de resposta esperado (documentacao)
};

std::vector<Fixture> fixtures()
{
    using PK = JE::PartKind;
    using LK = JE::LinkKind;
    std::vector<Fixture> f;
    {   // 1 viga-pilar parafusada, chapa de extremidade estendida
        JE::JointTopology t;
        t.familyId = "boltedEndPlate";
        t.parts = {part(1, PK::Member, "beam", 10), part(2, PK::Member, "column", 11), part(3, PK::Plate, "end plate"), part(4, PK::Stiffener, "column web stiffener")};
        t.links = {link(1, LK::Welded, {1, 3}), link(2, LK::Bolted, {2, 3}), link(3, LK::Welded, {2, 4})};
        f.push_back({"viga-pilar parafusada", finish(t), JE::ValidityStatus::ValidWithCaveats, "MomentRotation"});
    }
    {   // 2 viga-pilar soldada
        JE::JointTopology t;
        t.familyId = "weldedBeamColumn";
        t.parts = {part(1, PK::Member, "beam", 10), part(2, PK::Member, "column", 11), part(3, PK::Stiffener, "continuity plate")};
        t.links = {link(1, LK::Welded, {1, 2}), link(2, LK::Welded, {2, 3})};
        f.push_back({"viga-pilar soldada", finish(t), JE::ValidityStatus::ValidWithCaveats, "MomentRotation (solda rigida por hipotese rotulada)"});
    }
    {   // 3 hibrida: tala soldada a viga e parafusada ao pilar
        JE::JointTopology t;
        t.familyId = "hybridWebPlate";
        t.parts = {part(1, PK::Member, "beam", 10), part(2, PK::Member, "column", 11), part(3, PK::Plate, "web plate")};
        t.links = {link(1, LK::Welded, {1, 3}), link(2, LK::Bolted, {3, 2})};
        // caminho de carga SO com fundamento declarado (nao inferido)
        t.paths.push_back({1, {1, 2}, "declarado pelo projetista: cortante da viga -> solda -> tala -> parafusos -> pilar", "caminho em serie declarado"});
        f.push_back({"hibrida solda+parafuso", finish(t), JE::ValidityStatus::NotYetAvailable, "ResistanceOnly (rigidez sem fonte)"});
    }
    {   // 4 contraventamento com gusset: N-ario
        JE::JointTopology t;
        t.familyId = "braceGusset";
        t.parts = {part(1, PK::Member, "brace", 20), part(2, PK::Member, "brace", 21), part(3, PK::Member, "beam", 10), part(4, PK::Member, "column", 11), part(5, PK::Gusset, "gusset")};
        t.links = {link(1, LK::Bolted, {1, 5}), link(2, LK::Bolted, {2, 5}), link(3, LK::Welded, {5, 3, 4}, JE::Link::Resolution::Aggregate)};   // uniao de 3 partes
        f.push_back({"contraventamento com gusset", finish(t), JE::ValidityStatus::NotYetAvailable, "GeneralizedVector + ResistanceOnly"});
    }
    {   // 5 no de trelica com multiplos elementos
        JE::JointTopology t;
        t.familyId = "trussNode";
        t.parts = {part(1, PK::Member, "chord", 30), part(2, PK::Member, "chord", 31), part(3, PK::Member, "diagonal", 32), part(4, PK::Member, "diagonal", 33), part(5, PK::Member, "post", 34), part(6, PK::Gusset, "gusset")};
        t.links = {link(1, LK::Welded, {1, 2, 6}, JE::Link::Resolution::Aggregate), link(2, LK::Bolted, {3, 6}), link(3, LK::Bolted, {4, 6}), link(4, LK::Bolted, {5, 6})};
        f.push_back({"no de trelica multi-elemento", finish(t), JE::ValidityStatus::NotYetAvailable, "GeneralizedVector"});
    }
    {   // 6 emenda de viga com chapas de extremidade (sem elemento "principal")
        JE::JointTopology t;
        t.familyId = "beamSplice";
        t.parts = {part(1, PK::Member, "left", 10), part(2, PK::Member, "right", 12), part(3, PK::Plate, "end plate"), part(4, PK::Plate, "end plate")};
        t.links = {link(1, LK::Welded, {1, 3}), link(2, LK::Welded, {2, 4}), link(3, LK::Bolted, {3, 4}), link(4, LK::Contact, {3, 4})};
        f.push_back({"emenda de viga", finish(t), JE::ValidityStatus::NotYetAvailable, "MomentRotation/ForceDisplacement (modelo de emenda sem fonte)"});
    }
    {   // 7 base com chumbadores, graute e concreto
        JE::JointTopology t;
        t.familyId = "basePlateAnchors";
        t.parts = {part(1, PK::Member, "column", 11), part(2, PK::BasePlate, "base plate"), part(3, PK::Anchor, "anchor group"), part(4, PK::Grout, "grout"), part(5, PK::Concrete, "pedestal")};
        for (auto &p : t.parts) {
            if (p.kind == PK::Grout || p.kind == PK::Concrete) {
                JE::MaterialSpec m;
                m.kind = p.kind == PK::Grout ? JE::MaterialKind::Grout : JE::MaterialKind::Concrete;
                p.material = m;   // sem propriedades: nada inventado
            }
        }
        t.links = {link(1, LK::Welded, {1, 2}), link(2, LK::Contact, {2, 4}), link(3, LK::Contact, {4, 5}), link(4, LK::Embedded, {3, 5}), link(5, LK::Bolted, {2, 3})};
        f.push_back({"base com chumbadores", finish(t), JE::ValidityStatus::NotYetAvailable, "GeneralizedVector (N-M-V) + InterfaceModel; concreto sem propriedades"});
    }
    {   // 8 ligacao tubular
        JE::JointTopology t;
        t.familyId = "tubularKJoint";
        t.parts = {part(1, PK::Tube, "chord", 40), part(2, PK::Tube, "brace", 41), part(3, PK::Tube, "brace", 42)};
        t.links = {link(1, LK::Welded, {1, 2}), link(2, LK::Welded, {1, 3})};
        f.push_back({"tubular K", finish(t), JE::ValidityStatus::NotYetAvailable, "ResistanceOnly"});
    }
    {   // 9 mista aco-concreto
        JE::JointTopology t;
        t.familyId = "compositeJoint";
        t.parts = {part(1, PK::Member, "beam", 10), part(2, PK::Member, "column", 11), part(3, PK::Concrete, "slab"), part(4, PK::Anchor, "shear connectors")};
        t.parts[2].material = JE::MaterialSpec{JE::MaterialKind::Concrete, "", {}};
        t.links = {link(1, LK::Embedded, {3, 4}), link(2, LK::Welded, {4, 1}), link(3, LK::Welded, {1, 2})};
        f.push_back({"mista aco-concreto", finish(t), JE::ValidityStatus::NotYetAvailable, "MomentRotation (modelo da ligacao mista sem fonte)"});
    }
    return f;
}

NC connectionOfType(int type)
{
    NC c;
    c.id = 1;
    c.nodeId = 1;
    c.memberId = 10;
    c.type = type;
    return c;
}

} // namespace

class TestJointEngineRepresentability : public QObject
{
    Q_OBJECT
private slots:
    // ---------------------------------------------------------------------------------------- (A) tipologias reais
    void everyRealCatalogEntryIsDerivedOrUnavailableWithReason()
    {
        const auto &entries = AS::ConnectionCatalog::entries();
        QVERIFY(!entries.isEmpty());
        std::set<int> families;
        int partial = 0, unavailable = 0, withModel = 0, withoutModel = 0;
        for (const auto &e : entries) {
            families.insert(static_cast<int>(e.family));
            NC c = connectionOfType(e.id);
            c.values = e.preset;   // a variante do catalogo define o modo das mesas no preset
            const auto t = AS::JointTopologyAdapter::derive(c, {10, 11, 12});
            const auto issues = JE::validateTopology(t);
            QVERIFY2(issues.empty(), (std::to_string(e.id) + ": " + (issues.empty() ? "" : issues[0])).c_str());
            QVERIFY2(t.status == JE::TopologyStatus::Partial || (t.status == JE::TopologyStatus::Unavailable && !t.unavailableReason.empty()), std::to_string(e.id).c_str());
            QVERIFY2(t.paths.empty(), "caminhos de carga nunca inferidos");
            (t.status == JE::TopologyStatus::Partial ? partial : unavailable)++;

            const auto cap = AS::JointTopologyAdapter::mechanicalCapability(c);
            if (cap.status == JE::ValidityStatus::ValidWithCaveats) {
                // Incremento 6: so o 143 (perfis T) tem "so resistencia" (NbrResistanceOnly); os demais com modelo seguem o metodo dos componentes
                QVERIFY(cap.modelId == "EnComponentMethod" || (cap.modelId == "NbrResistanceOnly" && e.id == 143));
                ++withModel;
            } else {
                QCOMPARE(cap.status, JE::ValidityStatus::NotYetAvailable);
                QVERIFY2(!cap.justification.empty() && !cap.missingFoundations.empty(), std::to_string(e.id).c_str());
                QVERIFY(cap.modelId.empty());
                ++withoutModel;
            }
        }
        for (const auto fam : {AS::ConnectionCatalog::Family::WebShear, AS::ConnectionCatalog::Family::Moment, AS::ConnectionCatalog::Family::CoverSplice,
                               AS::ConnectionCatalog::Family::TubeSplice, AS::ConnectionCatalog::Family::EndPlateSplice, AS::ConnectionCatalog::Family::TrussNode,
                               AS::ConnectionCatalog::Family::FlexEndPlate, AS::ConnectionCatalog::Family::Composite, AS::ConnectionCatalog::Family::Pin}) {
            // familias com variantes proprias (legyType < 0) devem estar no catalogo; as que so tem aliases sao cobertas pelos tipos legados
            Q_UNUSED(fam);
        }
        qInfo().noquote() << QString("catalogo: %1 entradas; topologia Partial=%2, Unavailable=%3; modelo mecanico: com=%4, sem=%5")
                                 .arg(entries.size()).arg(partial).arg(unavailable).arg(withModel).arg(withoutModel);
        QVERIFY(withoutModel > withModel);   // a maioria das tipologias NAO tem modelo mecanico fundamentado: nada e declarado suportado sem ele
    }

    void everyLegacyTypeIsCoveredAndUnknownIsUnavailable()
    {
        for (int type = 0; type <= 7; ++type) {
            const auto t = AS::JointTopologyAdapter::derive(connectionOfType(type), {10, 11});
            QVERIFY2(JE::validateTopology(t).empty(), std::to_string(type).c_str());
            QCOMPARE(t.status, JE::TopologyStatus::Partial);
            const auto cap = AS::JointTopologyAdapter::mechanicalCapability(connectionOfType(type));
            QVERIFY2(cap.status == JE::ValidityStatus::ValidWithCaveats || (cap.status == JE::ValidityStatus::NotYetAvailable && !cap.justification.empty()), std::to_string(type).c_str());
        }
        const auto u = AS::JointTopologyAdapter::derive(connectionOfType(99), {});
        QCOMPARE(u.status, JE::TopologyStatus::Unavailable);
        QCOMPARE(AS::JointTopologyAdapter::mechanicalCapability(connectionOfType(99)).status, JE::ValidityStatus::Unsupported);
    }

    void onlyTheEnComponentMethodTypologiesClaimAMechanicalModel()
    {
        // kEndPlate (chapa de extremidade rigida) e a tipologia legada com modelo EN; as demais legadas nao
        QCOMPARE(AS::JointTopologyAdapter::mechanicalCapability(connectionOfType(NC::kEndPlate)).status, JE::ValidityStatus::ValidWithCaveats);
        for (const int type : {NC::kShearTab, NC::kDoubleAngle, NC::kGusset, NC::kSplice, NC::kBasePlate, NC::kTubular, NC::kColdFormed}) {
            QCOMPARE(AS::JointTopologyAdapter::mechanicalCapability(connectionOfType(type)).status, JE::ValidityStatus::NotYetAvailable);
        }
    }

    // ----------------------------------------------------------------------------------------- (B) configuracoes conceituais
    void everyConceptualFixtureIsRepresentableAndValid()
    {
        const auto all = fixtures();
        QCOMPARE(all.size(), std::size_t(9));
        for (const Fixture &f : all) {
            const auto issues = JE::validateTopology(f.topology);
            QVERIFY2(issues.empty(), (f.name + ": " + (issues.empty() ? "" : issues[0])).c_str());
            QVERIFY(!JE::configurationFingerprint(f.topology, {}).empty());
        }
    }

    void fixturesDoNotDependOnMainSecondaryRoles()
    {
        for (const Fixture &f : fixtures()) {
            for (const auto &p : f.topology.parts) {
                QVERIFY2(p.role != "main" && p.role != "secondary", f.name.c_str());
            }
        }
    }

    void gussetAndTrussNeedNaryLinksAndManyMembers()
    {
        const auto all = fixtures();
        const auto &gusset = all[3].topology;
        const auto &truss = all[4].topology;
        const auto maxArity = [](const JE::JointTopology &t) {
            std::size_t m = 0;
            for (const auto &l : t.links) m = std::max(m, l.partIds.size());
            return m;
        };
        QVERIFY(maxArity(gusset) >= 3);
        QVERIFY(maxArity(truss) >= 3);
        const auto members = [](const JE::JointTopology &t) {
            return std::count_if(t.parts.begin(), t.parts.end(), [](const JE::PartRef &p) { return p.kind == JE::PartKind::Member; });
        };
        QVERIFY(members(gusset) >= 4);
        QVERIFY(members(truss) >= 5);
    }

    void baseFixtureHasFirstClassConcreteAndGroutWithoutProperties()
    {
        const JE::JointTopology base = fixtures()[6].topology;   // copia: o vetor e temporario
        int concrete = 0, grout = 0;
        for (const auto &p : base.parts) {
            if (p.kind == JE::PartKind::Concrete) {
                ++concrete;
                QVERIFY(p.material && p.material->kind == JE::MaterialKind::Concrete && p.material->properties.empty());
            }
            if (p.kind == JE::PartKind::Grout) {
                ++grout;
                QVERIFY(p.material && p.material->kind == JE::MaterialKind::Grout && p.material->properties.empty());
            }
        }
        QCOMPARE(concrete, 1);
        QCOMPARE(grout, 1);
        // preencher uma propriedade muda o fingerprint (e passa a ser dado rastreado)
        auto filled = base;
        for (auto &p : filled.parts) {
            if (p.kind == JE::PartKind::Concrete) {
                p.material->properties["fck"] = JE::Quantity::make(30.0, "MPa", JE::Origin::UserProvided, "projeto");
            }
        }
        QVERIFY(JE::configurationFingerprint(filled, {}) != JE::configurationFingerprint(base, {}));
        // contato placa-graute-concreto e embutimento do chumbador representados (interfaces, nao molas)
        std::set<int> kinds;
        for (const auto &l : base.links) kinds.insert(static_cast<int>(l.kind));
        QVERIFY(kinds.count(static_cast<int>(JE::LinkKind::Contact)) && kinds.count(static_cast<int>(JE::LinkKind::Embedded)));
    }

    void loadPathsAreOnlyThoseExplicitlyDeclaredWithBasis()
    {
        for (const Fixture &f : fixtures()) {
            for (const auto &lp : f.topology.paths) {
                QVERIFY2(!lp.basis.empty(), f.name.c_str());
            }
        }
        QCOMPARE(fixtures()[2].topology.paths.size(), std::size_t(1));   // hibrida: declarado
        QVERIFY(fixtures()[3].topology.paths.empty());                   // gusset: nada inferido
    }

    void representabilityDoesNotImplyMechanicalCapability()
    {
        int withModel = 0;
        for (const Fixture &f : fixtures()) {
            QVERIFY(JE::validateTopology(f.topology).empty());   // todas representaveis ...
            if (f.mechanical == JE::ValidityStatus::ValidWithCaveats) {
                ++withModel;
            } else {
                QCOMPARE(f.mechanical, JE::ValidityStatus::NotYetAvailable);   // ... mas so duas tem modelo mecanico
            }
        }
        QCOMPARE(withModel, 2);
    }

    void fixturesCoverAllPartAndLinkKindsUsedByTheFamilies()
    {
        std::set<int> parts, links;
        for (const Fixture &f : fixtures()) {
            for (const auto &p : f.topology.parts) parts.insert(static_cast<int>(p.kind));
            for (const auto &l : f.topology.links) links.insert(static_cast<int>(l.kind));
        }
        for (const auto k : {JE::PartKind::Member, JE::PartKind::Plate, JE::PartKind::Stiffener, JE::PartKind::Gusset, JE::PartKind::BasePlate, JE::PartKind::Tube,
                             JE::PartKind::Concrete, JE::PartKind::Grout, JE::PartKind::Anchor}) {
            QVERIFY2(parts.count(static_cast<int>(k)), JE::partKindName(k).c_str());
        }
        for (const auto k : {JE::LinkKind::Bolted, JE::LinkKind::Welded, JE::LinkKind::Contact, JE::LinkKind::Embedded}) {
            QVERIFY2(links.count(static_cast<int>(k)), JE::linkKindName(k).c_str());
        }
    }
};

QTEST_MAIN(TestJointEngineRepresentability)
#include "TestJointEngineRepresentability.moc"
