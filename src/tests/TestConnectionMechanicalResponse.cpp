// Incremento 3: resposta mecanica (curva M-theta) de ligacoes DEFINIDAS, pelo modelo mecanico existente (metodo dos componentes EN 1993-1-8). Comparacao
// independente: a curva e recalculada aqui pela eq. (6.28) a partir de Sj,ini e Mj,Rd; Sj,ini e recomputado a partir das fileiras (keq, zeq); keff,r a partir da
// tabela de componentes; as fracoes de flexibilidade devem somar 1. Onde nao ha modelo (perfis T, cisalhamento, pilar nao informado, E ausente) o resultado e
// INDISPONIVEL e nao contem numeros. Nao e validacao profissional nem validacao experimental.
#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionMechanicalResponseService.h"
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "design/JointComponents.h"

#include <QtTest/QtTest>

#include <cmath>
#include <cstring>
#include <limits>

namespace AS = Structura::App::Services;
namespace JE = Structura::Design::JointEngine;
using NC = Structura::App::ProjectData::NodeConnection;
using CS = AS::ConnectionService;

namespace {

CS::BeamInfo beamOf(const char *name, double d, double bf, double tw, double tf)
{
    CS::BeamInfo b;
    b.name = name;
    b.family = "W Laminado";
    b.geometry = {{"d", d}, {"bf", bf}, {"tw", tw}, {"tf", tf}};
    b.depth = d;
    b.bf = bf;
    b.tw = tw;
    b.tf = tf;
    return b;
}

void setColumn(NC &c, double d, double bf, double tf, double tw, double r)
{
    c.values["supD"] = d;
    c.values["supBf"] = bf;
    c.values["supTf"] = tf;
    c.values["supTw"] = tw;
    c.values["supR"] = r;
    c.values["supFy"] = 250.0;
    c.values["supFu"] = 400.0;
    c.values["supConfirmed"] = 1.0;
}

NC make(int type, int rows = 2)
{
    NC c = CS::makeDefault(type, beamOf("IPE300", 300.0, 150.0, 7.1, 10.7), 250.0, 400.0);
    c.id = 1;
    c.nodeId = 4;
    c.memberId = 5;
    setColumn(c, 260, 146, 12.7, 7.6, 10);
    if (type == NC::kEndPlate) {
        c.values["tRows"] = rows;
    }
    return c;
}

constexpr double kE = 200000.0;   // MPa (aco)

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

// eq. (6.28) EN 1993-1-8, 6.3.1: theta = M/Sj,ini (M <= 2/3 Mj,Rd); theta = M/Sj,ini (1,5 M/Mj,Rd)^psi acima, psi = 2,7 (Tabela 6.8)
double thetaEn(double m, double sIni, double mj)
{
    return m <= 2.0 / 3.0 * mj ? m / sIni : m / sIni * std::pow(1.5 * m / mj, 2.7);
}

} // namespace

class TestConnectionMechanicalResponse : public QObject
{
    Q_OBJECT
private slots:
    void endPlateGivesAFunctionalCurveWithTheDeclaredContent()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate), kE, 40);
        QVERIFY2(r.available, r.message.c_str());
        QVERIFY(!r.partial);
        QVERIFY(JE::allowsNumbers(r.status));
        QCOMPARE(r.response.kind, JE::ResponseKind::MomentRotation);
        QVERIFY(r.response.consistent());
        QCOMPARE(r.response.curves.size(), std::size_t(1));
        QCOMPARE(r.response.curves[0].points.size(), Structura::Design::JointComponents::momentRotationCurve(r.initialStiffnessKnm, r.resistanceKnm, Structura::Design::JointComponents::kPsiWeldedAndEndPlate, 40).size());   // 40 + os pontos de 2/3 Mj,Rd e Mj,Rd
        QVERIFY(r.initialStiffnessKnm > 0.0 && r.resistanceKnm > 0.0);
        QVERIFY(r.response.initialStiffness && r.response.secantStiffness && r.response.tangentStiffness);
        QVERIFY(!r.response.deformationCapacity);                 // capacidade de rotacao INDISPONIVEL: nao inventada
        QVERIFY(!r.response.convergence.applicable);              // forma fechada
        QVERIFY(!r.components.empty() && !r.rows.empty() && !r.levels.empty());
        QVERIFY(!r.hypotheses.empty() && !r.limitations.empty());
        bool rotation = false, component = false;
        for (const auto &l : r.limitations) {
            rotation = rotation || l.find("Capacidade de rotação INDISPONÍVEL") != std::string::npos;
            component = component || l.find("Estado individual dos componentes") != std::string::npos;
        }
        QVERIFY(rotation && component);                           // o que nao existe e declarado, nao inventado
        QCOMPARE(r.response.provenance.modelId, std::string("EnComponentMethod"));
        QCOMPARE(r.configurationFingerprint.size(), std::size_t(12));
        QCOMPARE(r.analysisFingerprint.size(), std::size_t(12));
    }

    void curveEqualsTheIndependentEn6_28Recomputation()
    {
        for (const int rows : {1, 2, 3}) {
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate, rows), kE, 40);
            QVERIFY(r.available);
            const auto &pts = r.response.curves[0].points;
            for (const auto &p : pts) {
                const double theta = thetaEn(p.second, r.initialStiffnessKnm, r.resistanceKnm);
                QVERIFY2(std::abs(p.first - theta) <= 1e-9 * std::max(std::abs(theta), 1e-12), "theta da curva difere da eq. 6.28 recalculada");
            }
            QVERIFY(std::abs(pts.back().second - r.resistanceKnm) <= 1e-9 * r.resistanceKnm);   // a curva termina em Mj,Rd
            for (std::size_t i = 1; i < pts.size(); ++i) {                                         // monotonica
                QVERIFY(pts[i].first > pts[i - 1].first && pts[i].second > pts[i - 1].second);
            }
        }
    }

    void initialStiffnessIsRecomputedIndependentlyFromRowsAndNodeComponents()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate, 2), kE, 40);
        QVERIFY(r.available);
        double sumKh = 0.0, sumKh2 = 0.0;
        for (const auto &row : r.rows) {
            sumKh += row.keffMm * row.leverMm;
            sumKh2 += row.keffMm * row.leverMm * row.leverMm;
        }
        const double zeq = sumKh2 / sumKh;
        const double keq = sumKh / zeq;
        double nodeFlex = 0.0;
        for (const auto &c : r.components) {
            if (c.row == 0) {
                nodeFlex += c.flexibilityPerMm;
            }
        }
        const double sjIni = kE * zeq * zeq / (nodeFlex + 1.0 / keq) * 1e-6;   // EN 6.3.1(4), mu = 1; N.mm/rad -> kN.m/rad
        QVERIFY2(std::abs(sjIni - r.initialStiffnessKnm) <= 1e-6 * r.initialStiffnessKnm, "Sj,ini recomputado (componentes do no + fileiras) difere");
        QVERIFY(std::abs(zeq - r.leverArmMm) <= 1e-6 * zeq);
        QVERIFY(std::abs(r.rowsShareOfTotal - (1.0 / keq) / (nodeFlex + 1.0 / keq)) < 1e-9);
    }

    void rowStiffnessAndFlexibilitySharesAreConsistentWithTheComponentTable()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate, 2), kE, 40);
        QVERIFY(r.available);
        for (const auto &row : r.rows) {
            double flex = 0.0, share = 0.0;
            for (const auto &c : r.components) {
                if (c.row == row.row) {
                    flex += c.flexibilityPerMm;
                    share += c.shareInRow;
                }
            }
            QVERIFY(flex > 0.0);
            QVERIFY2(std::abs(1.0 / flex - row.keffMm) <= 1e-9 * row.keffMm, "keff,r != 1/soma(1/ki) dos componentes da fileira");
            QVERIFY2(std::abs(share - 1.0) < 1e-9, "as fracoes de flexibilidade da fileira nao somam 1");
        }
        double total = r.rowsShareOfTotal;
        for (const auto &c : r.components) {
            total += c.row == 0 ? c.shareOfTotal : 0.0;
        }
        QVERIFY2(std::abs(total - 1.0) < 1e-9, "as fracoes da flexibilidade total (no + fileiras) nao somam 1");
        QVERIFY(!r.governingContribution.empty());
        int governing = 0;
        for (const auto &c : r.components) {
            governing += c.governingInRow ? 1 : 0;
        }
        QVERIFY(governing >= 1);
    }

    void levelsReportOnlyTheStateOfTheNodeAndMatchTheModel()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate), kE, 40);
        QVERIFY(r.available);
        QCOMPARE(r.levels.size(), std::size_t(5));
        QVERIFY(std::abs(r.levels[2].ratioToResistance - 2.0 / 3.0) < 1e-12);
        QVERIFY2(std::abs(r.levels[2].secantKnmPerRad - r.initialStiffnessKnm) <= 1e-9 * r.initialStiffnessKnm, "a secante em 2/3 Mj,Rd deve ser Sj,ini");
        QVERIFY(r.levels[0].state.find("elástico") != std::string::npos && r.levels[2].state.find("elástico") != std::string::npos);
        QVERIFY(r.levels[3].state.find("não linear") != std::string::npos && r.levels[4].state.find("não linear") != std::string::npos);
        QVERIFY(r.levels[4].secantKnmPerRad < r.levels[0].secantKnmPerRad);   // perda de rigidez
        QVERIFY(std::abs(r.levels[4].secantKnmPerRad - r.response.secantStiffness->value) <= 1e-9 * r.levels[4].secantKnmPerRad);
        for (const auto &l : r.levels) {
            QVERIFY(std::abs(thetaEn(l.momentKnm, r.initialStiffnessKnm, r.resistanceKnm) - l.rotationRad) <= 1e-9 * l.rotationRad);
        }
    }

    void responseReusesTheExistingModelWithoutAlteringIt()
    {
        const NC c = make(NC::kEndPlate);
        const auto direct = AS::JointComponentService::evaluate(c, kE);
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40);
        QVERIFY(direct.supported && r.available);
        QVERIFY(sameBits(r.initialStiffnessKnm, direct.stiffness.initialStiffnessKnm()));
        QVERIFY(sameBits(r.resistanceKnm, direct.momentResistanceKnm));
        QVERIFY(sameBits(r.response.secantStiffness->value, direct.metrics.secantAtResistance));
        QCOMPARE(r.memory, direct.memory);
    }

    void weldedFlangesFromTheCatalogAlsoHaveTheModel()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(144), kE, 40);
        QVERIFY2(r.available, r.message.c_str());
        QVERIFY(r.resistanceKnm > 0.0 && r.initialStiffnessKnm > 0.0);
        QCOMPARE(r.family.empty(), false);
    }

    void tStubFlangesShearAndOtherTypologiesAreUnavailableWithoutNumbers()
    {
        for (const int type : {NC::kShearTab, NC::kDoubleAngle, NC::kGusset, NC::kBasePlate, NC::kSplice, NC::kTubular}) {   // 143 (perfis T): so resistencia desde o Incremento 6 (ver TestBeamSplice)
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(type), kE, 40);
            QVERIFY2(!r.available && !r.partial, std::to_string(type).c_str());
            QVERIFY(!r.message.empty());
            QVERIFY(r.response.consistent());
            QVERIFY(r.response.curves.empty() && !r.response.initialStiffness && r.response.resistances.empty());
            QVERIFY(r.components.empty() && r.levels.empty());
            QVERIFY(!JE::allowsNumbers(r.status));
        }
        const auto tStub = AS::ConnectionMechanicalResponseService::evaluate(make(143), kE, 40);   // sem curva nem rigidez; apenas a resistencia, com a lacuna declarada
        QVERIFY(tStub.resistanceOnly && !tStub.available && tStub.response.curves.empty() && !tStub.response.initialStiffness);
        QVERIFY(QString::fromStdString(tStub.message).contains("perfis T"));
    }

    void missingColumnOrYoungModulusGivesNoResponseAndNoSilentDefault()
    {
        NC noColumn = CS::makeDefault(NC::kEndPlate, beamOf("IPE300", 300.0, 150.0, 7.1, 10.7), 250.0, 400.0);   // pilar NAO informado
        noColumn.nodeId = 4;
        noColumn.memberId = 5;
        const auto a = AS::ConnectionMechanicalResponseService::evaluate(noColumn, kE, 40);
        QVERIFY(!a.available && !a.message.empty() && a.response.curves.empty());
        for (const double e : {0.0, -1.0, std::nan(""), std::numeric_limits<double>::infinity()}) {
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate), e, 40);
            QVERIFY(!r.available);
            QCOMPARE(r.status, JE::ValidityStatus::InsufficientData);
            QVERIFY(r.response.curves.empty() && !r.response.initialStiffness);
            QVERIFY(r.message.find("sem E") != std::string::npos);   // o servico legado trocaria E <= 0 por 200000 em silencio; aqui nao
        }
        QVERIFY(!AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate), kE, 1).available);
    }

    void fingerprintsSeparateConfigurationFromAnalysis()
    {
        const NC c = make(NC::kEndPlate);
        const auto a = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40);
        const auto b = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40);
        QCOMPARE(a.configurationFingerprint, b.configurationFingerprint);   // reprodutivel
        QCOMPARE(a.analysisFingerprint, b.analysisFingerprint);
        NC changed = c;
        changed.values["plateT"] = 25.0;
        const auto d = AS::ConnectionMechanicalResponseService::evaluate(changed, kE, 40);
        QVERIFY(d.configurationFingerprint != a.configurationFingerprint);
        QVERIFY(d.analysisFingerprint != a.analysisFingerprint);
        const auto e = AS::ConnectionMechanicalResponseService::evaluate(c, 210000.0, 40);   // outro E: mesma configuracao fisica? o E entra na configuracao derivada
        QVERIFY(e.analysisFingerprint != a.analysisFingerprint);
    }

    void thickerPlateOrMoreRowsRaiseResistanceAsPhysicsExpects()
    {
        // sanidade fisica (nao e validacao): mais fileiras tracionadas e chapa mais espessa nao reduzem a resistencia nem a rigidez inicial
        const auto one = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate, 1), kE, 40);
        const auto two = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate, 2), kE, 40);
        QVERIFY(one.available && two.available);
        QVERIFY(two.resistanceKnm >= one.resistanceKnm - 1e-9);
        QVERIFY(two.initialStiffnessKnm >= one.initialStiffnessKnm - 1e-9);
    }

    void manualColumnGeometryNotConfirmedGivesNoNumbers()
    {
        NC c = make(NC::kEndPlate);
        c.values["supConfirmed"] = 0.0;   // geometria do pilar digitada a mao e NAO confirmada, sem barra de apoio vinculada
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40);
        QVERIFY(!r.available);
        QVERIFY(!r.message.empty());
        QVERIFY(r.response.curves.empty() && !r.response.initialStiffness);
        c.values["supMember"] = 18.0;
        c.values["supType"] = 1.0;        // perfil I/H do modelo; com barra de apoio vinculada a secao vem do modelo (supConfirmed nao se aplica)
        QVERIFY(AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40).available);
    }

    void nbrBelowCurveIsCarriedAsALimitation()
    {
        for (const int rows : {1, 2, 3}) {
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(make(NC::kEndPlate, rows), kE, 40);
            QVERIFY(r.available);
            bool warned = false;
            for (const auto &l : r.limitations) {
                warned = warned || l.find("NÃO sustenta o momento da curva") != std::string::npos;
            }
            QCOMPARE(warned, r.nbrBelowCurve);   // se a ligacao cadastrada nao sustenta Mj,Rd da curva, a ressalva acompanha o resultado
        }
    }
};

QTEST_MAIN(TestConnectionMechanicalResponse)
#include "TestConnectionMechanicalResponse.moc"
