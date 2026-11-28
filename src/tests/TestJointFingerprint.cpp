// J1 do Joint Mechanical Engine: topologia, fingerprints em tres niveis e derivacao da topologia a partir da ligacao legada.
// SHA-256 verificado por vetores publicados (FIPS 180-4). Nao valida nenhum modelo mecanico.
#include "app/ProjectDataTypes.h"
#include "app/services/ConnectionCatalog.h"
#include "app/services/JointTopologyAdapter.h"
#include "design/jointengine/Fingerprint.h"
#include "design/jointengine/Topology.h"

#include <QtTest/QtTest>

namespace JE = Structura::Design::JointEngine;
namespace AS = Structura::App::Services;
using NC = Structura::App::ProjectData::NodeConnection;

namespace {

JE::JointTopology sampleTopology()
{
    JE::JointTopology t;
    t.familyId = "sample";
    t.status = JE::TopologyStatus::Complete;
    t.parts = {{1, JE::PartKind::Member, 10, "beam", std::nullopt, ""}, {2, JE::PartKind::Plate, 0, "plate", std::nullopt, ""}, {3, JE::PartKind::Member, 11, "column", std::nullopt, ""}};
    JE::Link a;
    a.id = 1;
    a.kind = JE::LinkKind::Welded;
    a.partIds = {1, 2};
    JE::Link b;
    b.id = 2;
    b.kind = JE::LinkKind::Bolted;
    b.partIds = {2, 3};
    t.links = {a, b};
    return t;
}

JE::ConfigurationData sampleData()
{
    JE::ConfigurationData d;
    d.quantities["tf"] = JE::Quantity::make(12.0, "mm", JE::Origin::UserProvided);
    d.legacyValues["plateThickness"] = 12.5;
    d.texts["grade"] = "A325";
    return d;
}

JE::AnalysisData sampleAnalysis()
{
    JE::AnalysisData a;
    a.actions.actions["M"] = JE::Quantity::make(100.0, "kN.m", JE::Origin::UserProvided);
    a.models = {{"EnComponentMethod", "1"}, {"Other", "2"}};
    a.strategy = "S1";
    a.assumptions = {"h1", "h2"};
    return a;
}

NC legacy(int type, int memberId = 7)
{
    NC c;
    c.id = 5;
    c.nodeId = 3;
    c.memberId = memberId;
    c.type = type;
    c.values.insert(QStringLiteral("tp"), 10.0);
    return c;
}

} // namespace

class TestJointFingerprint : public QObject
{
    Q_OBJECT
private slots:
    // ----------------------------------------------------------------------------------------------------------------- SHA-256
    void sha256PublishedVectors()
    {
        QCOMPARE(JE::sha256Hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
        QCOMPARE(JE::sha256Hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        QCOMPARE(JE::sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                 std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
        QCOMPARE(JE::sha256Hex(std::string(1000000, 'a')), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    }

    // -------------------------------------------------------------------------------------------------------------- topologia
    void validTopologyHasNoIssues() { QVERIFY(JE::validateTopology(sampleTopology()).empty()); }

    void topologyIntegrityViolations()
    {
        auto t = sampleTopology();
        t.links[0].partIds = {1};
        QVERIFY(!JE::validateTopology(t).empty());
        t = sampleTopology();
        t.links[0].partIds = {1, 99};
        QVERIFY(!JE::validateTopology(t).empty());
        t = sampleTopology();
        t.parts.push_back(t.parts[0]);   // id duplicado
        QVERIFY(!JE::validateTopology(t).empty());
        t = sampleTopology();
        t.links[0].partIds = {1, 1};
        QVERIFY(!JE::validateTopology(t).empty());
    }

    void loadPathRequiresBasisAndExistingLinks()
    {
        auto t = sampleTopology();
        t.paths.push_back({1, {1, 2}, "", "serie"});
        QVERIFY2(!JE::validateTopology(t).empty(), "caminho sem fundamento nao e aceito (nada de serie/paralelo presumido)");
        t.paths[0].basis = "declarado pelo usuario";
        QVERIFY(JE::validateTopology(t).empty());
        t.paths[0].linkIds = {1, 77};
        QVERIFY(!JE::validateTopology(t).empty());
    }

    void statusRulesCompletePartialUnavailable()
    {
        auto t = sampleTopology();
        t.gaps.push_back({"X", "lacuna"});
        QVERIFY(!JE::validateTopology(t).empty());   // Complete com lacuna
        t.status = JE::TopologyStatus::Partial;
        QVERIFY(JE::validateTopology(t).empty());
        t.gaps.clear();
        QVERIFY(!JE::validateTopology(t).empty());   // Partial sem lacuna
        JE::JointTopology u;
        u.status = JE::TopologyStatus::Unavailable;
        QVERIFY(!JE::validateTopology(u).empty());   // sem justificativa
        u.unavailableReason = "sem fonte";
        QVERIFY(JE::validateTopology(u).empty());
    }

    // ---------------------------------------------------------------------------------------------------------- fingerprints
    void fingerprintIsDeterministicAnd64Hex()
    {
        const auto a = JE::configurationFingerprint(sampleTopology(), sampleData());
        const auto b = JE::configurationFingerprint(sampleTopology(), sampleData());
        QCOMPARE(a, b);
        QCOMPARE(a.size(), std::size_t(64));
    }

    void permutingPartsLinksAndMapsDoesNotChangeCfp()
    {
        auto t = sampleTopology();
        auto p = sampleTopology();
        std::reverse(p.parts.begin(), p.parts.end());
        std::reverse(p.links.begin(), p.links.end());
        p.links[0].partIds = {3, 2};   // ordem interna das partes de uma uniao
        QCOMPARE(JE::configurationFingerprint(t, sampleData()), JE::configurationFingerprint(p, sampleData()));
    }

    void anyConfigurationChangeChangesCfp()
    {
        const auto base = JE::configurationFingerprint(sampleTopology(), sampleData());
        auto d = sampleData();
        d.legacyValues["plateThickness"] = 12.6;
        QVERIFY(JE::configurationFingerprint(sampleTopology(), d) != base);
        d = sampleData();
        d.quantities["tf"].origin = JE::Origin::Test;   // origem faz parte da identidade
        QVERIFY(JE::configurationFingerprint(sampleTopology(), d) != base);
        d = sampleData();
        d.texts["grade"] = "A490";
        QVERIFY(JE::configurationFingerprint(sampleTopology(), d) != base);
        auto t = sampleTopology();
        t.links[1].kind = JE::LinkKind::Welded;
        QVERIFY(JE::configurationFingerprint(t, sampleData()) != base);
        t = sampleTopology();
        t.parts[1].material = JE::MaterialSpec{};
        QVERIFY(JE::configurationFingerprint(t, sampleData()) != base);
    }

    void fillingAMissingValueChangesTheHash()
    {
        auto d = sampleData();
        d.quantities["fck"] = JE::Quantity{};   // ausente (NaN) ...
        const auto absent = JE::configurationFingerprint(sampleTopology(), d);
        d.quantities["fck"] = JE::Quantity::make(0.0, "MPa", JE::Origin::UserProvided);   // ... nao e o mesmo que zero informado
        QVERIFY(JE::configurationFingerprint(sampleTopology(), d) != absent);
        d.quantities["fck"] = JE::Quantity::make(30.0, "MPa", JE::Origin::UserProvided);
        QVERIFY(JE::configurationFingerprint(sampleTopology(), d) != absent);
    }

    void analysisDependsOnConfigurationAndAnalysisData()
    {
        const auto cfp = JE::configurationFingerprint(sampleTopology(), sampleData());
        const auto a0 = JE::analysisFingerprint(cfp, sampleAnalysis());
        QCOMPARE(a0, JE::analysisFingerprint(cfp, sampleAnalysis()));
        auto a = sampleAnalysis();
        a.actions.actions["M"].value = 101.0;
        QVERIFY(JE::analysisFingerprint(cfp, a) != a0);
        a = sampleAnalysis();
        a.models[0].second = "2";                       // versao de modelo
        QVERIFY(JE::analysisFingerprint(cfp, a) != a0);
        a = sampleAnalysis();
        a.controls.tolerance = 1e-6;
        QVERIFY(JE::analysisFingerprint(cfp, a) != a0);
        a = sampleAnalysis();
        std::swap(a.models[0], a.models[1]);            // ordem dos modelos nao importa
        a.assumptions = {"h2", "h1"};
        QCOMPARE(JE::analysisFingerprint(cfp, a), a0);
        auto d = sampleData();
        d.legacyValues["plateThickness"] = 13.0;
        QVERIFY(JE::analysisFingerprint(JE::configurationFingerprint(sampleTopology(), d), sampleAnalysis()) != a0);
    }

    void classificationChangeInvalidatesOnlyClassification()
    {
        JE::MechanicalResponse r;
        r.status = JE::ValidityStatus::Valid;
        r.kind = JE::ResponseKind::MomentRotation;
        r.curves.push_back({"M", "kN.m", "rad", {{0.001, 10.0}, {0.002, 18.0}}});
        r.initialStiffness = JE::Stiffness{10000.0, "kN.m/rad"};
        const auto rfp = JE::responseFingerprint(r);
        JE::ClassificationKey nbr{"NBR 8800", "2024", "6.1.2", {}};
        JE::ClassificationKey en{"EN 1993-1-8", "2005", "5.2.2", {}};
        JE::FingerprintSet stored;
        stored.configuration = JE::configurationFingerprint(sampleTopology(), sampleData());
        stored.analysis = JE::analysisFingerprint(stored.configuration, sampleAnalysis());
        stored.response = rfp;
        stored.classification = JE::classificationFingerprint(rfp, nbr);
        QVERIFY(JE::compare(stored, stored).current());
        JE::FingerprintSet now = stored;
        now.classification = JE::classificationFingerprint(rfp, en);   // trocou a norma
        const auto s = JE::compare(stored, now);
        QVERIFY2(!s.analysis, "mudar a norma NAO invalida a analise mecanica");
        QVERIFY(s.classification);
        QVERIFY(JE::classificationFingerprint(rfp, nbr) != JE::classificationFingerprint(rfp, en));
    }

    void analysisChangeMarksAnalysisAndPendingClassification()
    {
        JE::FingerprintSet stored{"c", "a", "r", "n"};
        JE::FingerprintSet now = stored;
        now.analysis = "a2";
        const auto s = JE::compare(stored, now);
        QVERIFY(s.analysis && s.classification);
        now = stored;
        now.configuration = "c2";
        const auto s2 = JE::compare(stored, now);
        QVERIFY(s2.analysis && s2.classification);
    }

    void sameResponseAfterRerunKeepsTheClassificationHash()
    {
        JE::MechanicalResponse r;
        r.status = JE::ValidityStatus::Valid;
        r.resistances.push_back({"Mj,Rd", 100.0, "kN.m", JE::ResultKind::NormativeResistance, true});
        JE::ClassificationKey k{"NBR 8800", "2024", "6.1.2", {}};
        r.provenance.analysisFingerprint = "irrelevante-1";   // proveniencia NAO entra no hash da resposta
        const auto n1 = JE::classificationFingerprint(JE::responseFingerprint(r), k);
        r.provenance.analysisFingerprint = "irrelevante-2";
        QCOMPARE(JE::classificationFingerprint(JE::responseFingerprint(r), k), n1);
        r.resistances[0].value = 100.0000001;
        QVERIFY(JE::classificationFingerprint(JE::responseFingerprint(r), k) != n1);
    }

    // ---------------------------------------------------------------------------------------------------- derivacao (adaptador)
    void legacyTypesAreDerivedWithoutInferringLoadPaths()
    {
        for (const int type : {NC::kShearTab, NC::kDoubleAngle, NC::kGusset, NC::kSplice, NC::kBasePlate, NC::kTubular, NC::kColdFormed, NC::kEndPlate}) {
            const auto t = AS::JointTopologyAdapter::derive(legacy(type), {7, 8});
            const auto issues = JE::validateTopology(t);
            QVERIFY2(issues.empty(), (t.familyId + ": " + (issues.empty() ? "" : issues[0])).c_str());
            QCOMPARE(t.status, JE::TopologyStatus::Partial);
            QVERIFY(t.paths.empty());   // nunca inferido
            const bool hasGap = std::any_of(t.gaps.begin(), t.gaps.end(), [](const JE::DataGap &g) { return g.code == "LOAD_PATHS_NOT_DERIVED"; });
            QVERIFY(hasGap);
        }
    }

    void unknownTypeIsUnavailableWithReason()
    {
        const auto t = AS::JointTopologyAdapter::derive(legacy(99), {});
        QCOMPARE(t.status, JE::TopologyStatus::Unavailable);
        QVERIFY(!t.unavailableReason.empty());
        QVERIFY(JE::validateTopology(t).empty());
    }

    void derivationDoesNotAssignRolesToUnknownBars()
    {
        const auto t = AS::JointTopologyAdapter::derive(legacy(NC::kShearTab, 7), {7, 8, 9});
        int unclassified = 0;
        for (const auto &p : t.parts) {
            if (p.kind == JE::PartKind::Member && p.role == "unclassified") {
                ++unclassified;
            }
        }
        QCOMPARE(unclassified, 2);
        QVERIFY(std::any_of(t.gaps.begin(), t.gaps.end(), [](const JE::DataGap &g) { return g.code == "MEMBER_ROLES_NOT_STORED"; }));
    }

    void missingSupportIsAGapNotAnInventedBar()
    {
        const auto t = AS::JointTopologyAdapter::derive(legacy(NC::kShearTab, 7), {7});
        QVERIFY(std::any_of(t.gaps.begin(), t.gaps.end(), [](const JE::DataGap &g) { return g.code == "SUPPORT_MEMBER_UNKNOWN"; }));
        for (const auto &p : t.parts) {
            if (p.role == "support") {
                QCOMPARE(p.memberId, 0);   // marcador sem id real
            }
        }
    }

    void basePlateCarriesConcreteWithoutProperties()
    {
        const auto t = AS::JointTopologyAdapter::derive(legacy(NC::kBasePlate), {7});
        bool found = false;
        for (const auto &p : t.parts) {
            if (p.kind == JE::PartKind::Concrete) {
                found = true;
                QVERIFY(p.material.has_value());
                QCOMPARE(p.material->kind, JE::MaterialKind::Concrete);
                QVERIFY(p.material->properties.empty());   // nenhuma propriedade inventada
            }
        }
        QVERIFY(found);
        QVERIFY(std::any_of(t.gaps.begin(), t.gaps.end(), [](const JE::DataGap &g) { return g.code == "CONCRETE_PROPERTIES_NOT_AVAILABLE"; }));
    }

    void cfpOfLegacyChangesWithAnyValue()
    {
        NC c = legacy(NC::kEndPlate);
        const auto f0 = AS::JointTopologyAdapter::configurationFingerprint(c, {7, 8}, 200000.0);
        QCOMPARE(f0, AS::JointTopologyAdapter::configurationFingerprint(c, {7, 8}, 200000.0));
        NC c2 = c;
        c2.values[QStringLiteral("tp")] = 10.1;
        QVERIFY(AS::JointTopologyAdapter::configurationFingerprint(c2, {7, 8}, 200000.0) != f0);
        QVERIFY(AS::JointTopologyAdapter::configurationFingerprint(c, {7, 8}, 210000.0) != f0);
        QVERIFY(AS::JointTopologyAdapter::configurationFingerprint(c, {7, 8, 9}, 200000.0) != f0);
        QVERIFY(AS::JointTopologyAdapter::configurationFingerprint(c, {7, 8}, std::nan("")) != f0);   // E ausente != E informado
    }

    void derivationDoesNotModifyTheConnection()
    {
        const NC c = legacy(NC::kGusset);
        const NC copy = c;
        (void)AS::JointTopologyAdapter::derive(c, {7, 8});
        QCOMPARE(c.values, copy.values);
        QCOMPARE(c.type, copy.type);
        QCOMPARE(c.memberId, copy.memberId);
    }
};

QTEST_MAIN(TestJointFingerprint)
#include "TestJointFingerprint.moc"
