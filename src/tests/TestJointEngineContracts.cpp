// J0 do Joint Mechanical Engine: contratos e interfaces. Testa invariantes dos tipos (sem defaults silenciosos, estados que autorizam numeros,
// materiais de concreto/graute sem propriedades inventadas, descritor completo, parametros com origem, componente com estado). Sem modelo real.
// NAO e validacao de nenhum modelo mecanico.
#include "design/jointengine/Component.h"
#include "design/jointengine/Descriptor.h"
#include "design/jointengine/Materials.h"
#include "design/jointengine/Response.h"
#include "design/jointengine/Types.h"

#include <QtTest/QtTest>

#include <cmath>

namespace JE = Structura::Design::JointEngine;

namespace {

JE::ModelDescriptor completeDescriptor()
{
    JE::ModelDescriptor d;
    d.id = "ToyBilinear";
    d.version = "0.0";
    d.formulation = "bilinear de teste";
    d.references = {"teste"};
    d.supportedDofs.dofs = {{"N", JE::DofKind::Force, "x", "kN", "mm"}};
    d.parameters = {{"k", "kN/mm", {JE::Origin::UserProvided}, true, "rigidez"}, {"Fy", "kN", {}, true, "escoamento"}};
    d.limitations = {"somente teste"};
    d.nonlinear = true;
    d.hasInternalState = true;
    return d;
}

// componente de TESTE com estado (maxima deformacao ja atingida): prova que a interface comporta nao linearidade com historia
class ToyBilinear : public JE::IDeformableComponent
{
public:
    explicit ToyBilinear(double k, double fy) : m_k(k), m_fy(fy), m_d(completeDescriptor()) {}
    const JE::ModelDescriptor &descriptor() const override { return m_d; }
    JE::Applicability applicable(const JE::ParameterSet &p) const override
    {
        JE::Applicability a;
        a.messages = JE::checkParameters(m_d, p);
        a.status = a.messages.empty() ? JE::ValidityStatus::Valid : JE::ValidityStatus::InsufficientData;
        return a;
    }
    JE::ComponentResponse evaluate(const JE::Vector &u, JE::ComponentState &s) const override
    {
        if (u.size() != 1 || !std::isfinite(u[0])) {
            return JE::ComponentResponse::unavailable(JE::ValidityStatus::OutOfDomain, {{JE::Severity::Error, "BAD_INPUT", "deslocamento invalido"}});
        }
        if (s.internal.empty()) {
            s.internal = {0.0};
        }
        s.internal[0] = std::max(s.internal[0], std::abs(u[0]));
        JE::ComponentResponse r;
        r.status = JE::ValidityStatus::Valid;
        const double f = std::min(m_k * std::abs(u[0]), m_fy);
        r.force = {u[0] < 0 ? -f : f};
        return r;
    }

private:
    double m_k, m_fy;
    JE::ModelDescriptor m_d;
};

class ToyStrategy : public JE::IAssemblyStrategy
{
public:
    std::string id() const override { return "ToyStrategy"; }
    JE::Applicability applicable(const JE::Assembly &) const override { return {JE::ValidityStatus::NotYetAvailable, {{JE::Severity::Info, "NO_MODEL", "sem modelo"}}}; }
    JE::MechanicalResponse solve(const JE::Assembly &, const JE::ActionSet &, const JE::SolveControls &) const override
    {
        JE::MechanicalResponse r;
        r.status = JE::ValidityStatus::NotYetAvailable;
        return r;
    }
};

} // namespace

class TestJointEngineContracts : public QObject
{
    Q_OBJECT
private slots:
    void quantityDefaultIsAbsentNotZero()
    {
        const JE::Quantity q;
        QVERIFY(std::isnan(q.value));
        QVERIFY(!q.provided());
        QVERIFY(!q.origin.has_value());
        JE::Quantity noOrigin;
        noOrigin.value = 3.0;
        noOrigin.unit = "mm";
        QVERIFY2(!noOrigin.provided(), "valor sem origem nao conta como informado");
        QVERIFY(JE::Quantity::make(3.0, "mm", JE::Origin::UserProvided).provided());
        QVERIFY(!JE::Quantity::make(std::nan(""), "mm", JE::Origin::UserProvided).provided());
        QVERIFY(!JE::Quantity::make(3.0, "", JE::Origin::UserProvided).provided());
    }

    void onlyValidStatesAllowNumbers()
    {
        QVERIFY(JE::allowsNumbers(JE::ValidityStatus::Valid));
        QVERIFY(JE::allowsNumbers(JE::ValidityStatus::ValidWithCaveats));
        for (const auto s : {JE::ValidityStatus::OutOfDomain, JE::ValidityStatus::InsufficientData, JE::ValidityStatus::InvalidGeometry,
                             JE::ValidityStatus::MissingMaterialProperties, JE::ValidityStatus::Unsupported, JE::ValidityStatus::NotYetAvailable}) {
            QVERIFY2(!JE::allowsNumbers(s), JE::statusName(s).c_str());
        }
    }

    void onlyNormativeResistanceEntersApproval()
    {
        QVERIFY(JE::mayEnterApproval(JE::ResultKind::NormativeResistance));
        QVERIFY(!JE::mayEnterApproval(JE::ResultKind::ModelUltimate));
        QVERIFY(!JE::mayEnterApproval(JE::ResultKind::ModelEstimate));
        QVERIFY(!JE::mayEnterApproval(JE::ResultKind::Reference));
    }

    void concreteAndGroutAreFirstClassWithoutInventedProperties()
    {
        JE::MaterialSpec concrete;
        concrete.kind = JE::MaterialKind::Concrete;
        JE::MaterialSpec grout;
        grout.kind = JE::MaterialKind::Grout;
        QCOMPARE(JE::materialKindName(concrete.kind), std::string("Concrete"));
        QCOMPARE(JE::materialKindName(grout.kind), std::string("Grout"));
        QVERIFY(concrete.properties.empty());   // nasce vazio: nenhum fck presumido
        const std::vector<std::string> need = {"fck", "E"};
        QCOMPARE(concrete.missing(need).size(), std::size_t(2));
        QVERIFY(concrete.find("fck") == nullptr);
        concrete.properties["fck"] = JE::Quantity::make(30.0, "MPa", JE::Origin::UserProvided, "projeto");
        QVERIFY(concrete.find("fck") != nullptr);
        QCOMPARE(concrete.find("fck")->value, 30.0);
        const auto miss = concrete.missing(need);
        QCOMPARE(miss.size(), std::size_t(1));
        QCOMPARE(miss[0], std::string("E"));
        concrete.properties["E"] = JE::Quantity{};   // chave presente, mas ausente de valor
        QCOMPARE(concrete.missing(need).size(), std::size_t(1));
    }

    void descriptorCompleteness()
    {
        QVERIFY(JE::validateDescriptor(completeDescriptor()).empty());
        JE::ModelDescriptor bad;
        const auto issues = JE::validateDescriptor(bad);
        QVERIFY(issues.size() >= 5);
        JE::ModelDescriptor stateful = completeDescriptor();
        stateful.nonlinear = false;
        QVERIFY(!JE::validateDescriptor(stateful).empty());
        JE::ModelDescriptor dup = completeDescriptor();
        dup.parameters.push_back(dup.parameters[0]);
        QVERIFY(!JE::validateDescriptor(dup).empty());
    }

    void parameterChecksNeedPresenceOriginUnitAndAllowedOrigin()
    {
        const JE::ModelDescriptor d = completeDescriptor();
        JE::ParameterSet p;
        QCOMPARE(JE::checkParameters(d, p).size(), std::size_t(2));   // k e Fy ausentes
        p["k"] = JE::Quantity::make(100.0, "kN/mm", JE::Origin::Test);          // origem nao admitida
        p["Fy"] = JE::Quantity::make(50.0, "kN", JE::Origin::UserProvided);
        auto msgs = JE::checkParameters(d, p);
        QCOMPARE(msgs.size(), std::size_t(1));
        QCOMPARE(msgs[0].code, std::string("ORIGIN_NOT_ALLOWED"));
        p["k"] = JE::Quantity::make(100.0, "N/mm", JE::Origin::UserProvided);   // unidade errada
        msgs = JE::checkParameters(d, p);
        QCOMPARE(msgs.size(), std::size_t(1));
        QCOMPARE(msgs[0].code, std::string("UNIT_MISMATCH"));
        p["k"].unit = "kN/mm";
        QVERIFY(JE::checkParameters(d, p).empty());
        JE::Quantity noOrigin;
        noOrigin.value = 1.0;
        noOrigin.unit = "kN";
        p["Fy"] = noOrigin;
        QCOMPARE(JE::checkParameters(d, p)[0].code, std::string("MISSING_ORIGIN"));
    }

    void unavailableResponsesNeverCarryNumbers()
    {
        const auto r = JE::ComponentResponse::unavailable(JE::ValidityStatus::OutOfDomain, {{JE::Severity::Warning, "X", "fora"}});
        QVERIFY(r.consistent());
        QVERIFY(r.force.empty());
        JE::ComponentResponse bad;
        bad.status = JE::ValidityStatus::OutOfDomain;
        bad.force = {1.0};
        QVERIFY2(!bad.consistent(), "numero com estado nao autorizado deve ser detectado");
    }

    void mechanicalResponseInvariant()
    {
        JE::MechanicalResponse r;
        r.status = JE::ValidityStatus::InsufficientData;
        QVERIFY(r.consistent());
        r.resistances.push_back({"x", 1.0, "kN", JE::ResultKind::NormativeResistance, true});
        QVERIFY(!r.consistent());
        r.status = JE::ValidityStatus::Valid;
        QVERIFY(r.consistent());
    }

    void componentInterfaceSupportsStateAndNonlinearity()
    {
        ToyBilinear c(100.0, 50.0);
        JE::ComponentState s;
        auto r1 = c.evaluate({0.2}, s);
        QCOMPARE(r1.force[0], 20.0);
        auto r2 = c.evaluate({2.0}, s);       // plastifica
        QCOMPARE(r2.force[0], 50.0);
        auto r3 = c.evaluate({0.1}, s);       // estado guarda o maximo
        QCOMPARE(r3.force[0], 10.0);
        QCOMPARE(s.internal[0], 2.0);
        const auto bad = c.evaluate({std::nan("")}, s);
        QCOMPARE(bad.status, JE::ValidityStatus::OutOfDomain);
        QVERIFY(bad.consistent() && bad.force.empty());
        QVERIFY(c.descriptor().nonlinear && c.descriptor().hasInternalState);
    }

    void componentIsDeterministic()
    {
        ToyBilinear c(100.0, 50.0);
        JE::ComponentState a, b;
        for (const double u : {0.1, 0.3, 1.0, -0.5}) {
            QCOMPARE(c.evaluate({u}, a).force[0], c.evaluate({u}, b).force[0]);
        }
    }

    void strategyInterfaceDeclaresUnavailabilityWithoutNumbers()
    {
        ToyStrategy s;
        const JE::Assembly assembly;
        QCOMPARE(s.applicable(assembly).status, JE::ValidityStatus::NotYetAvailable);
        const auto r = s.solve(assembly, {}, {});
        QVERIFY(r.consistent());
        QVERIFY(r.curves.empty());
        const JE::SolveControls controls;   // sem defaults
        QVERIFY(!controls.maxIterations && !controls.tolerance && !controls.incrementSteps);
    }

    void desiredComputedClassificationAreSeparateObjects()
    {
        static_assert(!std::is_same_v<JE::DesiredBehavior, JE::ClassificationResult>);
        static_assert(!std::is_same_v<JE::ClassificationResult, JE::MechanicalResponse>);
        const JE::ClassificationResult res;   // default = nao verificavel (dado ausente nao vira aprovacao)
        QCOMPARE(res.verdict, JE::Verdict::NaoVerificavel);
        const JE::BehaviorComparison cmp;
        QCOMPARE(cmp.verdict, JE::Verdict::NaoVerificavel);
    }
};

QTEST_APPLESS_MAIN(TestJointEngineContracts)
#include "TestJointEngineContracts.moc"
