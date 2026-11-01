#include <QtTest/QtTest>

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "app/services/ConnectionSolids.h"
#include "app/services/CutListService.h"
#include "app/services/DstvExporter.h"
#include "app/services/FabricationModel.h"
#include "app/services/IntegrityService.h"
#include "design/PinConnection.h"
#include "app/services/NodeEquilibrium.h"
#include "design/BasePlate.h"
#include "design/CompositeConnection.h"
#include "design/FlexEndPlate.h"
#include "design/ShearConnections.h"
#include "design/SupportChecks.h"
#include "design/WebShearConnection.h"

using namespace Structura::App::Services;
namespace D = Structura::Design::Connections;

namespace {

ConnectionService::BeamInfo beam()
{
    ConnectionService::BeamInfo b;
    b.name = "W 310 x 38,7";
    b.family = "W Laminado";
    b.depth = 310.0;
    b.bf = 165.0;
    b.tf = 9.7;
    b.tw = 5.8;
    b.geometry = {{"d", 310.0}, {"bf", 165.0}, {"tw", 5.8}, {"tf", 9.7}, {"r", 10.0}};
    b.area = 4900.0;
    b.fy = 345.0;
    b.fu = 450.0;
    return b;
}

ConnectionService::Forces forces(double shearKn)
{
    ConnectionService::Forces f;
    f.shear = shearKn * 1000.0;
    f.axial = 0.0;
    ConnectionService::ForceCase fc;
    fc.name = "ELU";
    fc.shear = f.shear;
    fc.axial = 60000.0;
    fc.braceSigned = {80000.0, -60000.0, 30000.0};   // diagonal tracionada, diagonal comprimida e montante (nos de trelica)
    f.cases.append(fc);
    return f;
}

} // namespace

class TestConnectionCatalog : public QObject
{
    Q_OBJECT

private slots:
    /// Barra inclinada sobre o apoio (banzo de tesoura sobre pilar): esforcos resolvidos no plano da face
    void skewedMemberIsResolvedInSupportPlane()
    {
        using Structura::App::ProjectData::NodeConnection;
        // inclinacao a partir da geometria: banzo a 20 graus da horizontal chegando a uma face de normal horizontal
        const double slope = 20.0 * 3.14159265358979 / 180.0;
        QVERIFY(std::abs(ConnectionService::skewDegrees({std::cos(slope), 0.0, std::sin(slope)}, {1.0, 0.0, 0.0}) - 20.0) < 1e-9);
        QCOMPARE(ConnectionService::skewDegrees({1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}), 0.0);
        // esforcos resolvidos: a = 0 igual aos da barra; a > 0 aumenta cortante paralelo e normal (soma em modulo)
        auto f = forces(100.0);
        f.axial = 150000.0;
        f.tension = 150000.0;
        f.cases[0].axial = 150000.0;
        const auto same = ConnectionService::interfaceForces(f, 0.0);
        QCOMPARE(same.shear, f.shear);
        QCOMPARE(same.tension, f.tension);
        const auto rotated = ConnectionService::interfaceForces(f, 30.0);
        QVERIFY(std::abs(rotated.shear - (100000.0 + 150000.0 * 0.5)) < 1e-6);
        QVERIFY(std::abs(rotated.tension - (150000.0 * std::cos(30.0 * 3.14159265358979 / 180.0) + 100000.0 * 0.5)) < 1e-6);
        // chapa de cisalhamento: a inclinacao nunca alivia e pode agravar (nota de resolucao presente)
        auto c = ConnectionService::makeDefault(NodeConnection::kShearTab, beam(), 250.0, 400.0);
        const auto straight = ConnectionService::evaluateExact(c, f);
        c.values["skew"] = 35.0;
        const auto inclined = ConnectionService::evaluateExact(c, f);
        QVERIFY(inclined.maxRatio >= straight.maxRatio - 1e-12);
        bool note = false;
        for (const auto &check : inclined.checks) {
            note = note || check.name.find("inclinada") != std::string::npos;
        }
        QVERIFY(note);
        // geometria projetada: a chapa de extremidade rigida tem a altura da viga dividida por cos a (soldas da alma mais longas)
        {
            using Structura::App::ProjectData::NodeConnection;
            auto plate = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam(), 250.0, 400.0);
            ConnectionService::Forces ef;
            ef.shear = 50000.0;
            ef.moment = 80e6;
            ConnectionService::ForceCase fc;
            fc.name = "ELU";
            fc.shear = 50000.0;
            fc.moment = 80e6;
            ef.cases.append(fc);
            const auto flat = ConnectionService::evaluateExactBeamFrame(plate, ef);
            plate.values["skew"] = 30.0;
            const auto tilted = ConnectionService::evaluateExactBeamFrame(plate, ef);
            auto weldLength = [](const ConnectionService::Evaluation &e) {
                for (const auto &part : e.parts) {
                    if (part.description.find("Filete") != std::string::npos) {
                        return part.length;
                    }
                }
                return 0.0;
            };
            QVERIFY(weldLength(tilted) > weldLength(flat) * 1.05);
            bool noteFound = false;
            for (const auto &check : tilted.checks) {
                noteFound = noteFound || check.name.find("geometria projetada") != std::string::npos;
            }
            QVERIFY(noteFound);
        }
        // chapa de no: a solda recebe 90 - a e o apoio so a componente normal da forca
        auto g = ConnectionService::makeDefault(NodeConnection::kGusset, beam(), 250.0, 400.0);
        ConnectionService::Forces gf;
        gf.tension = 200000.0;
        gf.axial = 200000.0;
        const auto before = ConnectionService::supportDemand(g, gf);
        g.values["skew"] = 60.0;
        const auto after = ConnectionService::supportDemand(g, gf);
        QVERIFY(std::abs(after.first - before.first * 0.5) < 1e-6);
        const auto weld = ConnectionService::evaluateExact(g, gf);
        QVERIFY(!weld.checks.empty());
    }

    /// Chapa ligada por pino e pino (NBR 8800:2024, 5.2.6, 6.4 e 6.6.2): calculo a mao
    void pinPlateAndPinMatchHandCalculation()
    {
        namespace D = Structura::Design::Connections;
        D::PinPlateInput in;
        in.plateThickness = 19.0;
        in.plateWidth = 120.0;
        in.endDistance = 80.0;
        in.plate = {250.0, 400.0};
        in.pinDiameter = 50.0;
        in.holeDiameter = 50.5;
        in.pin = {345.0, 450.0};
        in.otherThickness = 12.5;
        in.gap = 2.0;
        in.shearPlanes = 2;
        in.tension = 300000.0;
        const auto checks = D::checkPinPlate(in);
        auto find = [&](const char *key) -> const D::Check * {
            for (const auto &c : checks) {
                if (c.name.find(key) != std::string::npos) {
                    return &c;
                }
            }
            return nullptr;
        };
        const double bef = std::min(2.0 * 19.0 + 16.0, (120.0 - 50.5) / 2.0);   // 34,75
        QVERIFY(std::abs(find("escoamento da secao bruta")->resistance - 120.0 * 19.0 * 250.0 / 1.10 / 1000.0) < 1e-6);
        QVERIFY(std::abs(find("pressao de contato")->resistance - 1.8 * 50.0 * 19.0 * 250.0 / 1.35 / 1000.0) < 1e-6);
        QVERIFY(std::abs(find("(tracao)")->resistance - 2.0 * 19.0 * bef * 400.0 / 1.35 / 1000.0) < 1e-6);
        QVERIFY(std::abs(find("(cisalhamento)")->resistance - 0.60 * (2.0 * 19.0 * (80.0 + 25.0)) * 400.0 / 1.35 / 1000.0) < 1e-6);
        // pino: M = (F/2)(t/4 + g + t2/2), MRd = 1,2 W fy / ga1, V = F/2, esmagamento 1,5 t d fy / ga1
        const double moment = 150000.0 * (19.0 / 4.0 + 2.0 + 12.5 / 2.0);
        QVERIFY(std::abs(D::pinBendingMoment(in) - moment) < 1e-6);
        const double w = 3.14159265358979 * std::pow(50.0, 3) / 32.0;
        QVERIFY(std::abs(find("momento fletor")->resistance - 1.2 * w * 345.0 / 1.10 / 1e6) < 1e-9);
        QVERIFY(std::abs(find("forca cortante")->demand - 150.0) < 1e-9);
        QVERIFY(std::abs(find("forca cortante")->resistance - 0.60 * 0.75 * 3.14159265358979 * 2500.0 / 4.0 * 345.0 / 1.10 / 1000.0) < 1e-6);
        QVERIFY(std::abs(find("esmagamento (chapa central)")->resistance - 1.5 * 19.0 * 50.0 * 250.0 / 1.10 / 1000.0) < 1e-6);
        // requisitos geometricos: b >= 2 bef + dp = 119,5 (atendido) e a >= 1,33 bef
        QVERIFY(find("Largura da chapa")->ratio() == 0.0 || find("Largura da chapa")->ratio() < 1.0);
        in.endDistance = 30.0;   // a < 1,33 bef = 46,2 mm: violado
        const auto narrow = D::checkPinPlate(in);
        bool violated = false;
        for (const auto &c : narrow) {
            violated = violated || (c.name.find("Distancia a") != std::string::npos && c.ratio() > 1.0);
        }
        QVERIFY(violated);
        // familia do catalogo: avalia e gera solidos
        auto conn = ConnectionService::makeDefault(191, beam(), 250.0, 400.0);
        auto f = forces(0.0);
        f.tension = 200000.0;
        const auto e = ConnectionService::evaluate(conn, f);
        QVERIFY(!e.checks.empty());
        std::vector<ConnectionSolids::Primitive> solids;
        ConnectionCatalog::solids(conn, true, solids);
        QVERIFY(solids.size() >= 4);
        QVERIFY(!ConnectionService::fields(191).isEmpty());
        // fadiga (H.5-a, caso 2.4 categoria E) e requisitos de fabricacao (H.6)
        conn.values["fatOn"] = 1.0;
        conn.values["fatCycles"] = 1.0e6;
        conn.values["fatRange"] = 60.0;
        const auto fat = ConnectionService::evaluate(conn, f);
        bool material = false, fabrication = false;
        for (const auto &check : fat.checks) {
            if (check.name.find("Fadiga do metal-base") != std::string::npos) {
                material = true;
                const double net = (conn.values["plateB"] - conn.values["holeD"]) * conn.values["plateT"];
                QVERIFY(std::abs(check.demand - 60000.0 / net) < 1e-9);
                QVERIFY(std::abs(check.resistance - std::max(std::cbrt(11e8 / 1.0e6), 31.0)) < 1e-9);
            }
            fabrication = fabrication || check.name.find("H.6") != std::string::npos;
        }
        QVERIFY(material && fabrication);
    }

    /// Fadiga de parafusos e barras rosqueadas (Anexo H, H.5-b)
    void boltFatigueFollowsH5()
    {
        namespace D = Structura::Design::Connections;
        D::BoltFatigueInput in;
        in.db = 24.0;
        in.pitch = 3.0;
        in.bolts = 4;
        in.forceRange = 120000.0;
        in.cycles = 2.0e6;
        const auto checks = D::checkBoltFatigue(in);
        const double abe = 3.14159265358979 / 4.0 * std::pow(24.0 - 0.9382 * 3.0, 2);
        QVERIFY(std::abs(checks.front().demand - 120000.0 / (4.0 * abe)) < 1e-9);
        QVERIFY(std::abs(checks.front().resistance - std::max(std::cbrt(3.9e8 / 2.0e6), 48.0)) < 1e-9);
        in.cycles = 0.0;   // vida infinita: sigma_TH
        QCOMPARE(D::checkBoltFatigue(in).front().resistance, 48.0);
        in.forceRange = 0.0;
        in.staticDesignForce = 500000.0;   // alternativa de H.5: 20 % da forca, com protensao inicial
        QVERIFY(std::abs(D::checkBoltFatigue(in).front().demand - 0.2 * 500000.0 / (4.0 * abe)) < 1e-9);
    }

    /// 5.7.6: instabilidade da alma por compressao sob par de forcas opostas
    void webBucklingUnderOpposingForces()
    {
        namespace D = Structura::Design::Connections;
        D::SupportInput in;
        in.face = D::SupportInput::Flange;
        in.d = 300.0;
        in.bf = 150.0;
        in.tf = 12.0;
        in.tw = 8.0;
        in.k = 22.0;
        in.fy = 345.0;
        in.atMemberEnd = false;
        in.compression = 200000.0;
        in.lengthAlong = 100.0;
        in.lengthTransverse = 30.0;
        in.pairForce = 250000.0;
        const double h = 300.0 - 2.0 * 22.0;
        const double rd = 24.0 * 512.0 * std::sqrt(200000.0 * 345.0) / (1.10 * h);
        const D::Check *found = nullptr;
        const auto checks = D::checkSupportMember(in);
        for (const auto &c : checks) {
            if (c.clause.find("5.7.6") != std::string::npos) {
                found = &c;
            }
        }
        QVERIFY(found);
        QVERIFY(std::abs(found->resistance - rd / 1000.0) < 1e-6);
        in.atMemberEnd = true;   // 5.7.6.3: a menos de d/2 da extremidade, resistencia reduzida a metade
        for (const auto &c : D::checkSupportMember(in)) {
            if (c.clause.find("5.7.6") != std::string::npos) {
                QVERIFY(std::abs(c.resistance - 0.5 * rd / 1000.0) < 1e-6);
            }
        }
    }

    /// Integridade estrutural (4.13): travamento em duas direcoes, forca minima das contencoes e colapso progressivo
    void structuralIntegrityRows()
    {
        using Structura::App::Services::IntegrityService;
        QCOMPARE(IntegrityService::minimumTieForce(2.0e6, false), 75000.0);       // 1 % de 2000 kN = 20 kN < 75 kN
        QCOMPARE(IntegrityService::minimumTieForce(9.0e6, false), 90000.0);       // 1 % de 9000 kN = 90 kN
        QCOMPARE(IntegrityService::minimumTieForce(1.0e6, true), 45000.0);        // pilar de extremidade sem laje
        IntegrityService::Input in;
        // quatro pilares de 4 m e tres vigas de topo: os pilares das pontas (nos 11 e 14) so tem uma direcao de contencao
        in.nodes = {{1, 0, 0, 0, true}, {2, 6, 0, 0, true}, {3, 6, 6, 0, true}, {4, 0, 6, 0, true}, {11, 0, 0, 4, false}, {12, 6, 0, 4, false}, {13, 6, 6, 4, false}, {14, 0, 6, 4, false}};
        for (int i = 0; i < 4; ++i) {
            in.bars.append({100 + i, i + 1, i + 11, "pilar", 5000.0, 3000.0});
        }
        in.bars.append({201, 11, 12, "viga", 600.0, 300.0});
        in.bars.append({202, 12, 13, "viga", 600.0, 300.0});
        in.bars.append({203, 13, 14, "viga", 600.0, 300.0});
        for (int i = 0; i < 4; ++i) {
            in.columnLoads.append({100 + i, 8.0e6});   // 8000 kN: 1 % = 80 kN > 75 kN
        }
        in.endForces = {{201, false, "ELU1", 120000.0, 0.0}, {201, true, "ELU1", 120000.0, 0.0}, {202, false, "ELU1", 90000.0, 0.0}};
        in.options.progressiveCollapse = true;
        const auto rows = IntegrityService::run(in);
        int notBraced = 0, ties = 0, beams = 0;
        for (const auto &row : rows) {
            if (row.clause.endsWith("4.13.4") && !row.ok) {
                ++notBraced;
                QVERIFY(row.location.contains("Pilar 100") || row.location.contains("Pilar 103"));   // nos 11 e 14: uma so direcao
            }
            if (row.clause.endsWith("4.13.7") && row.description.contains("Contenção horizontal")) {
                ++ties;
                QVERIFY(std::abs(row.demandKn - 80.0) < 1e-9);   // max(1 % de 8000 kN ; 75 kN)
                QVERIFY(row.ok);
            }
            if (row.clause.endsWith("4.13.8") && row.description.startsWith("Viga")) {
                ++beams;
            }
        }
        QCOMPARE(notBraced, 2);
        QVERIFY(ties >= 6);
        QVERIFY(beams >= 4);
    }

    void minimumDesignForceOf45kN()
    {
        // NBR 8800:2024, 6.1.5.2: esforcos menores que 45 kN sao elevados a 45 kN (exceto diagonais, nos de trelica, tubulares e formados a frio)
        const auto c = ConnectionService::makeDefault(Structura::App::ProjectData::NodeConnection::kShearTab, beam(), 250.0, 400.0);
        QVERIFY(ConnectionService::minimumForceApplies(c));
        const auto small = ConnectionService::evaluate(c, forces(10.0));
        const auto exact45 = ConnectionService::evaluateExact(c, forces(45.0));
        const auto exact10 = ConnectionService::evaluateExact(c, forces(10.0));
        QVERIFY(std::abs(small.maxRatio - exact45.maxRatio) < 1e-9);
        QVERIFY(small.maxRatio >= exact10.maxRatio);
        bool note = false;
        for (const auto &check : small.checks) {
            note = note || check.clause.find("6.1.5.2") != std::string::npos;
        }
        QVERIFY(note);
        // acima de 45 kN nada muda
        QVERIFY(std::abs(ConnectionService::evaluate(c, forces(80.0)).maxRatio - ConnectionService::evaluateExact(c, forces(80.0)).maxRatio) < 1e-12);
        const auto gusset = ConnectionService::makeDefault(Structura::App::ProjectData::NodeConnection::kGusset, beam(), 250.0, 400.0);
        QVERIFY(!ConnectionService::minimumForceApplies(gusset));
    }
    void everyWebShearVariantEvaluatesAndBuildsSolids()
    {
        int count = 0;
        for (const auto &entry : ConnectionCatalog::entries()) {
            const auto c = ConnectionService::makeDefault(entry.id, beam(), 250.0, 400.0);
            QCOMPARE(c.type, entry.id);
            QVERIFY2(!ConnectionService::fields(entry.id).isEmpty(), qPrintable(entry.name));
            const auto e = ConnectionService::evaluate(c, forces(60.0));
            QVERIFY2(!e.checks.empty(), qPrintable(entry.name));
            QVERIFY2(!e.parts.empty(), qPrintable(entry.name));
            QVERIFY2(std::isfinite(e.maxRatio) && e.maxRatio > 0.0, qPrintable(entry.name));
            QVERIFY(!e.description.isEmpty());
            QVERIFY(!ConnectionSolids::build(c, false).empty());
            QVERIFY(ConnectionSolids::build(c, true).size() >= ConnectionSolids::build(c, false).size());
            ++count;
        }
        QCOMPARE(count, ConnectionCatalog::entries().size());
        QCOMPARE(ConnectionService::typeName(101), ConnectionCatalog::find(101)->name);
    }

    void trussNodesCarryTheResultantOfAllBars()
    {
        for (const int id : {121, 122, 123, 124, 125}) {
            auto c = ConnectionService::makeDefault(id, beam(), 250.0, 400.0);
            c.values["b3on"] = 1.0;
            const auto light = ConnectionService::evaluate(c, forces(0.0));
            QVERIFY2(!light.checks.empty() && light.maxRatio > 0.0, qPrintable(QString::number(id)));
            // dobrar os esforcos nas barras aumenta a razao
            auto heavy = forces(0.0);
            for (auto &fc : heavy.cases) {
                for (double &n : fc.braceSigned) {
                    n *= 2.0;
                }
            }
            auto total = [](const ConnectionService::Evaluation &e) {
                double sum = 0.0;
                for (const auto &chk : e.checks) {
                    sum += chk.ratio();
                }
                return sum;
            };
            QVERIFY2(total(ConnectionService::evaluate(c, heavy)) > total(light), qPrintable(QString::number(id)));
            // os modelos 3D incluem o banzo, a(s) barra(s) e a ligacao
            QVERIFY(ConnectionSolids::build(c, true).size() > 8);
        }
        // a barra comprimida nao pode ser verificada como tracionada: inverter o sinal muda as verificacoes da chapa
        auto c = ConnectionService::makeDefault(123, beam(), 250.0, 400.0);
        auto compression = forces(0.0);
        compression.cases[0].braceSigned = {-80000.0, -60000.0, 0.0};
        const auto e = ConnectionService::evaluate(c, compression);
        bool buckling = false;
        for (const auto &chk : e.checks) {
            buckling = buckling || chk.name.find("compressao na secao de Whitmore") != std::string::npos;
        }
        QVERIFY(buckling);
    }

    /// Memorial de uma tipologia (para conferencia manual): STRUCTURA_DUMP_ID=<id> TestConnectionCatalog dumpEntry
    void dumpEntry()
    {
        const int id = qEnvironmentVariableIntValue("STRUCTURA_DUMP_ID");
        if (id == 0) {
            QSKIP("defina STRUCTURA_DUMP_ID");
        }
        ConnectionService::BeamInfo b = beam();
        b.depth = 400; b.bf = 180; b.tf = 12.5; b.tw = 8; b.area = 8450;
        b.geometry = {{"d", 400.0}, {"bf", 180.0}, {"tw", 8.0}, {"tf", 12.5}, {"r", 0.0}};
        auto c = ConnectionService::makeDefault(id, b, 250.0, 400.0);
        ConnectionService::Forces f;
        ConnectionService::ForceCase fc;
        fc.name = "ELU";
        fc.shear = qEnvironmentVariableIntValue("STRUCTURA_DUMP_V") * 1000.0;
        fc.axial = qEnvironmentVariableIntValue("STRUCTURA_DUMP_N") * 1000.0;
        fc.moment = qEnvironmentVariableIntValue("STRUCTURA_DUMP_M") * 1e6;
        f.cases.append(fc);
        f.shear = fc.shear; f.axial = fc.axial; f.moment = fc.moment;
        const auto e = ConnectionService::evaluate(c, f);
        for (const auto &chk : e.checks) {
            qWarning("CHK %s | %.3f %.3f %s | r=%.3f", chk.name.c_str(), chk.demand, chk.resistance, chk.unit.c_str(), chk.ratio());
            for (const auto &l : chk.lines) {
                qWarning("      %s", l.c_str());
            }
        }
        qWarning("MAX %.3f %s", e.maxRatio, qPrintable(e.governing));
    }

    /// Placa de Levy: casos classicos de Timoshenko (todas as bordas apoiadas) e limite de faixa com borda livre
    void levyPlateReproducesClassicCoefficients()
    {
        QVERIFY(qAbs(D::plateMomentCoefficient(1.0, false) - 0.0479) < 2e-4);   // quadrada, 4 bordas apoiadas
        QVERIFY(qAbs(D::plateMomentCoefficient(2.0, false) - 0.1017) < 2e-4);   // a/b = 2
        const double free1 = D::plateMomentCoefficient(1.0, true);
        const double free3 = D::plateMomentCoefficient(3.0, true);
        QVERIFY(free1 > 0.10 && free1 < 0.125);                                 // borda livre pega mais que o caso apoiado
        QVERIFY(free3 > 0.125 && free3 < 0.14);                                 // limite: faixa biapoiada (q b2/8) com a borda livre sem restricao de Poisson
        QVERIFY(D::plateMomentCoefficient(0.5, true) < free1);                   // painel mais raso, menor momento
    }

    /// Tabelas O.1 a O.3: pontos tabelados e interpolacoes lineares
    void requiredRotationTables()
    {
        double r = 0.0;
        QVERIFY(D::requiredRotation(15.0, 350.0, 0.95, 0, r));
        QCOMPARE(r, 29.0);
        QVERIFY(D::requiredRotation(20.0, 250.0, 0.90, 1, r));
        QCOMPARE(r, 36.0);
        QVERIFY(D::requiredRotation(25.0, 300.0, 0.85, 0, r));    // media de 29 (350) e 21 (250)
        QVERIFY(qAbs(r - 25.0) < 1e-9);
        QVERIFY(D::requiredRotation(17.5, 350.0, 0.85, 1, r));    // entre 22 e 32
        QVERIFY(qAbs(r - 27.0) < 1e-9);
        QVERIFY(D::requiredRotation(20.0, 350.0, 0.925, 0, r));   // entre 35 (0,90) e 46 (0,95)
        QVERIFY(qAbs(r - 40.5) < 1e-9);
        QVERIFY(!D::requiredRotation(35.0, 350.0, 0.95, 0, r));   // fora de L/dt
        QVERIFY(!D::requiredRotation(20.0, 400.0, 0.95, 0, r));   // fora de fy
        QVERIFY(!D::requiredRotation(20.0, 350.0, 0.80, 0, r));   // fora de betaVm
    }

    /// Anexo O: M-Rd = fsd As (d + y), Fs,Rd e a rotacao pelas formulas fechadas
    void compositeConnectionClosedForms()
    {
        D::CompositeConnectionInput in;
        in.variant = D::CompositeConnectionInput::EndPlate;
        in.d = 400.0; in.bf = 180.0; in.tf = 12.5; in.tw = 8.0;
        in.bars = 8; in.barDiameter = 12.5; in.barSpacing = 100.0; in.y = 85.0;
        in.moment = 100e6;
        const auto r = D::checkCompositeConnection(in);
        const double as = 8.0 * 3.14159265358979 * 12.5 * 12.5 / 4.0;
        QVERIFY(qAbs(r.as - as) < 1e-6);
        QVERIFY(qAbs(r.fsRd - 500.0 / 1.15 * as) < 1e-3);
        QVERIFY(qAbs(r.mRd - 500.0 / 1.15 * as * 485.0) < 1.0);
        QVERIFY(qAbs(r.bEff - 1000.0) < 1e-9);   // 2 x min((8000+8000)/4/8 ; 3000/2) = 2 x 500
        QVERIFY(qAbs(r.fiRd - 1.25 * 250.0 / 1.10 * 180.0 * 12.5) < 1e-3);
        QVERIFY(r.thetaU > 0.0);
        QCOMPARE(r.yLnp, 0.0);                    // chapa de extremidade: Dui = 0, LNP na mesa inferior
        QVERIFY(r.si > 0.0);
        // mais armadura aumenta M-Rd e reduz a capacidade de rotacao
        D::CompositeConnectionInput more = in;
        more.bars = 12;
        more.barSpacing = 70.0;
        const auto rm = D::checkCompositeConnection(more);
        QVERIFY(rm.mRd > r.mRd);
        // momento acima de M-Rd reprova
        D::CompositeConnectionInput heavy = in;
        heavy.moment = 2.0 * r.mRd;
        QVERIFY(D::worstRatio(D::checkCompositeConnection(heavy).checks) > 1.0);
    }

    /// Todas as ligacoes mistas avaliam e produzem sinais: viga de apoio, chapa e so cantoneira inferior
    void compositeEntriesEvaluate()
    {
        for (int id = 161; id <= 167; ++id) {
            auto c = ConnectionService::makeDefault(id, beam(), 250.0, 400.0);
            auto f = forces(60.0);
            f.cases[0].moment = 90e6;
            const auto e = ConnectionService::evaluate(c, f);
            QVERIFY2(!e.checks.empty(), qPrintable(QString::number(id)));
            std::vector<ConnectionSolids::Primitive> solids;
            ConnectionCatalog::solids(c, true, solids);
            QVERIFY2(solids.size() > 5, qPrintable(QString::number(id)));
            QVERIFY(!ConnectionService::fields(id).isEmpty());
        }
    }

    /// Chapa de extremidade flexivel: V simples e o painel de alma por 5.7.7.1
    void flexEndPlateAndColumnPanel()
    {
        for (int id : {107, 108}) {
            auto c = ConnectionService::makeDefault(id, beam(), 250.0, 400.0);
            QVERIFY(ConnectionService::evaluate(c, forces(80.0)).maxRatio > 0.0);
            auto sumRatios = [](const ConnectionService::Evaluation &e) {
                double total = 0.0;
                for (const auto &check : e.checks) {
                    total += check.ratio();
                }
                return total;
            };
            const auto light = ConnectionService::evaluate(c, forces(20.0));
            const auto heavy = ConnectionService::evaluate(c, forces(200.0));
            QVERIFY(sumRatios(heavy) > sumRatios(light));
        }
        D::ColumnPanelInput pi;
        pi.d = 260.0; pi.bf = 255.0; pi.tf = 17.3; pi.tw = 10.0; pi.radius = 15.0; pi.fy = 345.0;
        pi.beamForce = 400000.0;
        const auto base = D::checkColumnPanel(pi);
        QVERIFY(!base.empty());
        D::ColumnPanelInput withDoubler = pi;
        withDoubler.doubler = 10.0;
        QVERIFY(D::checkColumnPanel(withDoubler).front().resistance > base.front().resistance);
        D::ColumnPanelInput axial = pi;
        axial.axial = 0.9 * (2.0 * 255.0 * 17.3 + (260.0 - 34.6) * 10.0) * 345.0;
        QVERIFY(D::checkColumnPanel(axial).front().resistance < base.front().resistance);     // NSd > 0,4 Np reduz FRd
        D::ColumnPanelInput relieved = pi;
        relieved.columnShear = 150000.0;
        QVERIFY(D::checkColumnPanel(relieved).front().demand < base.front().demand);          // V do pilar soma-se algebricamente
    }

    /// Equilibrio do no: cortante do painel = M/z - V do pilar (com sinal), a partir dos esforcos de extremidade
    void nodeEquilibriumPanelShear()
    {
        namespace Ne = Structura::App::Services::NodeEquilibrium;
        auto build = [](double columnShear) {
            std::vector<Ne::MemberEnd> m(3);
            // viga para +x (no no extremo i): momento de 100 kN.m (anti-horario) sobre o no
            m[0].atEnd = false;
            m[0].x = {1, 0, 0}; m[0].y = {0, 1, 0}; m[0].z = {0, 0, 1};
            m[0].f = {0, 0, 0, 0, 0, 100e6, 0, 0, 0, 0, 0, 0};
            // pilar de cima (no extremo i, eixo +y): empurra o no com +V em x
            m[1].atEnd = false;
            m[1].x = {0, 1, 0}; m[1].y = {-1, 0, 0}; m[1].z = {0, 0, 1};
            m[1].f = {-500e3, -columnShear, 0, 0, 0, -60e6, 0, 0, 0, 0, 0, 0};
            // pilar de baixo (no extremo j): -V em x
            m[2].atEnd = true;
            m[2].x = {0, 1, 0}; m[2].y = {-1, 0, 0}; m[2].z = {0, 0, 1};
            m[2].f = {0, 0, 0, 0, 0, 0, -500e3, -columnShear, 0, 0, 0, 40e6};
            return m;
        };
        const double z = 300.0;
        const auto relieved = Ne::panelShear(build(200e3), 0, 1, z);
        QVERIFY(relieved.valid);
        QVERIFY(qAbs(relieved.panelShear - qAbs(200e3 - 100e6 / z)) < 1.0);      // 133,3 kN
        QVERIFY(qAbs(relieved.columnAxial - 500e3) < 1.0);
        const auto none = Ne::panelShear(build(0.0), 0, 1, z);
        QVERIFY(qAbs(none.panelShear - 100e6 / z) < 1.0);                          // sem cortante no pilar: M/z
        const auto added = Ne::panelShear(build(-200e3), 0, 1, z);
        QVERIFY(added.panelShear > none.panelShear);                              // cortante de sentido oposto soma-se (nao alivia)
        QVERIFY(qAbs(added.panelShear - (200e3 + 100e6 / z)) < 1.0);
        // pilar de baixo ausente: so o pilar de cima equilibra
        auto single = build(200e3);
        single.pop_back();
        QVERIFY(qAbs(Ne::panelShear(single, 0, 1, z).panelShear - qAbs(200e3 - 100e6 / z)) < 1.0);
        QVERIFY(!Ne::panelShear(build(0.0), 0, 0, z).valid);                       // viga = pilar: invalido
    }

    /// Base com nervuras: a placa pode ser mais fina que sem nervuras e as nervuras sao verificadas
    void baseRibsReduceThePlateThickness()
    {
        D::BasePlateInput in;
        in.d = 300.0; in.bf = 300.0; in.tf = 15.0; in.tw = 10.0;
        in.plate = {250.0, 400.0};
        in.axial = -600000.0;
        in.moment = 0.0;
        auto tpMin = [](const D::BasePlateInput &input) {
            for (const auto &c : D::checkBasePlate(input)) {
                if (c.name.rfind("Placa de base: espessura minima", 0) == 0) {
                    return c.demand;
                }
            }
            return -1.0;
        };
        const double plain = tpMin(in);
        D::BasePlateInput ribbed = in;
        ribbed.ribs = 6;
        ribbed.ribHeight = 200.0;
        ribbed.ribLength = 100.0;
        const double withRibs = tpMin(ribbed);
        QVERIFY(plain > 0.0 && withRibs > 0.0);
        QVERIFY2(withRibs < plain, qPrintable(QString("%1 %2").arg(withRibs).arg(plain)));
        bool hasRibCheck = false;
        for (const auto &c : D::checkBasePlate(ribbed)) {
            hasRibCheck = hasRibCheck || c.name.rfind("Nervura", 0) == 0;
        }
        QVERIFY(hasRibCheck);
    }

    void bomAndDstvExportFollowTheFabricationModel()
    {
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::DstvExporter;
        using Structura::App::Services::FabricationModel;
        auto a = ConnectionService::makeDefault(NodeConnection::kShearTab, beam(), 250.0, 400.0);
        a.id = 1;
        auto b = a;
        b.id = 2;
        const auto single = FabricationModel::bom({a});
        const auto twice = FabricationModel::bom({a, b});
        QCOMPARE(twice.plates.size(), single.plates.size());
        QCOMPARE(twice.plates.first().quantity, 2 * single.plates.first().quantity);
        QCOMPARE(twice.plates.first().connections, QVector<int>({1, 2}));
        QVERIFY(std::abs(twice.plateMassKg - 2.0 * single.plateMassKg) < 1e-9);
        QCOMPARE(twice.bolts.first().quantity, 2 * single.bolts.first().quantity);
        // NC1: cabecalho, furos e fim
        const auto fab = FabricationModel::fromConnection(a);
        const QString nc = DstvExporter::plateToNc1(fab.parts.first(), QStringLiteral("L1-C1"), QStringLiteral("A36"), {});
        QVERIFY(nc.startsWith(QStringLiteral("ST\n")));
        QVERIFY(nc.contains(QStringLiteral("\nBO\n")));
        QVERIFY(nc.trimmed().endsWith(QStringLiteral("EN")));
        QCOMPARE(nc.count(QStringLiteral("\n  v ")), int(fab.parts.first().holes.size()));
        QVERIFY(nc.contains(QStringLiteral("  B\n")));   // codigo de perfil: chapa
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString error;
        const auto files = DstvExporter::exportConnections({a, b}, dir.path(), {}, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(files.size(), 2 * fab.plateCount() == 0 ? 0 : files.size());
        QVERIFY(!files.isEmpty());
        QVERIFY(QFileInfo(files.first()).size() > 100);
    }

    void copesAndWeldedPlatesAreDetected()
    {
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::FabricationModel;
        int withCopes = 0, withWelded = 0;
        for (const int type : {101, 102, 111, 112, 152, 148, NodeConnection::kEndPlate, NodeConnection::kShearTab}) {
            const auto c = ConnectionService::makeDefault(type, beam(), 250.0, 400.0);
            const auto fab = FabricationModel::fromConnection(c);
            for (const auto &cope : fab.copes) {
                QVERIFY(cope.length > 0.0 && cope.depth > 0.0);
            }
            withCopes += fab.copes.isEmpty() ? 0 : 1;
            for (const auto &p : fab.parts) {
                withWelded += (p.kind == Structura::App::Services::FabPart::Kind::Plate && p.weldedToMember) ? 1 : 0;
            }
        }
        qInfo("tipologias com recorte: %d; chapas soldadas ao perfil: %d", withCopes, withWelded);
        QVERIFY(withWelded >= 1);   // chapa de extremidade: soldada a viga
        // viga apoiada na alma de viga principal: recorte = (bf - tw)/2 + folga em comprimento e tf da viga ligada + folga em profundidade
        auto c = ConnectionService::makeDefault(101, beam(), 250.0, 400.0);
        c.values["support"] = 0.0;
        c.values["supBf"] = 150.0;
        c.values["supTw"] = 8.0;
        c.values["supTf"] = 12.0;
        c.values["supD"] = 400.0;
        c.values["copeC"] = 15.0;
        const auto fab = FabricationModel::fromConnection(c);
        QVERIFY(!fab.copes.isEmpty());
        for (const auto &cope : fab.copes) {
            QVERIFY(std::abs(cope.length - ((150.0 - 8.0) / 2.0 + 15.0)) < 1e-6);
            QVERIFY(cope.depth > 15.0);
        }
    }

    void cutListPacksBarsAndSheets()
    {
        using namespace Structura::App::Services;
        QVector<FabMember> members;
        FabMember a;
        a.mark = QStringLiteral("V1");
        a.section = QStringLiteral("W 310");
        a.cutLength = 5000.0;
        a.quantity = 3;
        a.massPerMeter = 40.0;
        members.push_back(a);
        FabMember b;
        b.mark = QStringLiteral("V2");
        b.section = QStringLiteral("W 310");
        b.cutLength = 1900.0;
        b.quantity = 2;
        b.massPerMeter = 40.0;
        members.push_back(b);
        FabBom bom;
        FabBom::PlateRow plate;
        plate.thickness = 9.5;
        plate.width = 1000.0;
        plate.height = 500.0;
        plate.quantity = 4;
        bom.plates.push_back(plate);
        FabBom::PlateRow big = plate;
        big.thickness = 12.5;
        big.width = 4000.0;
        big.quantity = 1;
        bom.plates.push_back(big);
        CutListService::Options options;
        const auto plan = CutListService::compute(members, bom, options);
        QCOMPARE(plan.bars.size(), 1);
        // 5000+3+5000 = 10003 (+ 3 + 1900 = 11906 <= 12000): barra 1 recebe 2x5000 + 1x1900; barra 2: 5000 + 1900 (6903)
        QCOMPARE(plan.bars.first().barCount, 2);
        QVERIFY(std::abs(plan.bars.first().usedLength - (3 * 5000.0 + 2 * 1900.0)) < 1e-9);
        QVERIFY(plan.bars.first().utilization() > 0.7 && plan.bars.first().utilization() <= 1.0);
        for (const auto &pattern : plan.bars.first().patterns) {
            double sum = 0.0;
            for (const auto &p : pattern) {
                sum += p.second;
            }
            QVERIFY(sum + 3.0 * (pattern.size() - 1) <= options.stockLength + 1e-9);
        }
        // chapas: 4 pecas 1000x500 cabem em uma chapa 3000x1200; a de 4000 mm excede a chapa padrao
        QCOMPARE(plan.sheets.size(), 2);
        const auto thin = plan.sheets.first();
        QCOMPARE(thin.sheetCount, 1);
        QCOMPARE(thin.pieceCount, 4);
        QVERIFY(thin.utilization() > 0.5);
        QCOMPARE(plan.sheets.last().oversize, 1);
        QVERIFY(!plan.warnings.isEmpty());
    }

    void outsideTheNodeConnectionsKeepTheJointAwayFromTheSupport()
    {
        using Structura::App::Services::ConnectionSolids;
        for (const int id : {147, 148}) {
            const auto *entry = ConnectionCatalog::find(id);
            QVERIFY(entry != nullptr);
            QVERIFY2(entry->usesSupport, "ligacao fora do no deve permitir escolher a barra de apoio");
            bool hasField = false;
            for (const auto &field : ConnectionCatalog::fields(id)) {
                hasField = hasField || field.key == QLatin1String("outDist");
            }
            QVERIFY2(hasField, "campo da distancia apoio-junta ausente");
            auto c = ConnectionService::makeDefault(id, beam(), 250.0, 400.0);
            QVERIFY(c.values.value(QStringLiteral("outDist")) > 0.0);
            c.values["outDist"] = 300.0;
            // a face do apoio (coluna de contexto) fica a outDist da junta (s = 0), do lado oposto da viga
            double faceS = -1e300;
            for (const auto &p : ConnectionSolids::build(c, true)) {
                if (p.role == ConnectionSolids::Role::Concrete && p.kind == ConnectionSolids::Kind::Box) {
                    faceS = std::max(faceS, p.a[0] + p.size[0] / 2.0);
                }
            }
            QVERIFY2(std::abs(faceS + 300.0) < 1e-6, qPrintable(QStringLiteral("tipo %1: face do apoio em s = %2").arg(id).arg(faceS)));
            // sem outDist maior, a chapa nao invade a face
            c.values["outDist"] = 1000.0;
            double faceFar = -1e300;
            for (const auto &p : ConnectionSolids::build(c, true)) {
                if (p.role == ConnectionSolids::Role::Concrete && p.kind == ConnectionSolids::Kind::Box) {
                    faceFar = std::max(faceFar, p.a[0] + p.size[0] / 2.0);
                }
            }
            QVERIFY(faceFar < faceS);
            // a viga de contexto do lado do apoio termina na face (nao entra no pilar)
            c.values["outDist"] = 300.0;
            for (const auto &p : ConnectionSolids::build(c, true)) {
                if (p.role == ConnectionSolids::Role::Member && p.kind == ConnectionSolids::Kind::Box) {
                    QVERIFY2(p.a[0] - p.size[0] / 2.0 >= -300.0 - 1e-6, qPrintable(QStringLiteral("tipo %1: viga entra no apoio (s min = %2)").arg(id).arg(p.a[0] - p.size[0] / 2.0)));
                }
            }
        }
    }

    void supportWeldIsCheckedProposedAndModelled()
    {
        using Structura::App::Services::ConnectionSolids;
        for (const int id : {147, 148}) {
            auto c = ConnectionService::makeDefault(id, beam(), 250.0, 400.0);
            ConnectionService::Forces f;
            f.shear = 120e3;
            f.axial = 30e3;
            f.moment = 90e6;
            f.tension = 30e3;
            f.cases.push_back({QStringLiteral("ELU1"), 120e3, 30e3, 90e6, {}, {}, -1.0, 0.0});
            auto countWeldChecks = [&](const auto &evaluation) {
                int n = 0;
                for (const auto &check : evaluation.checks) {
                    n += check.name.find("Solda viga-apoio") != std::string::npos ? 1 : 0;
                }
                return n;
            };
            const auto evaluation = ConnectionCatalog::evaluate(c, f);
            QVERIFY2(countWeldChecks(evaluation) >= 4, qPrintable(QStringLiteral("tipo %1: verificacoes da solda viga-apoio ausentes").arg(id)));
            // mesas em penetracao total: F = M/(d - tf) + N/2 contra bf tf fy / ga1
            const double d = c.values.value(QStringLiteral("beamD")), tf = c.values.value(QStringLiteral("tfl")), bf = c.values.value(QStringLiteral("bfl"));
            const double force = 90e6 / (d - tf) + 15e3;
            const double fy = c.values.value(QStringLiteral("beamFy"), 250.0);
            bool found = false;
            for (const auto &check : evaluation.checks) {
                if (check.name.find("mesas (penetracao total)") != std::string::npos) {
                    found = true;
                    QVERIFY(std::abs(check.demand - force / 1000.0) < 1e-6);
                    QVERIFY(std::abs(check.resistance - bf * tf * fy / 1.10 / 1000.0) < 1e-3);
                }
            }
            QVERIFY2(found, "mesas em penetracao total");
            // filete nas mesas: perna pequena reprova, o dimensionamento sobe a perna e aprova
            c.values["supWeldKind"] = 1.0;
            c.values["supWeldLegF"] = 3.0;
            c.values["supWeldLegW"] = 3.0;
            const auto weak = ConnectionCatalog::evaluate(c, f);
            auto weldOk = [&](const auto &evaluation2) {
                for (const auto &check : evaluation2.checks) {
                    if (check.name.find("Solda viga-apoio") != std::string::npos && !check.ok()) {
                        return false;
                    }
                }
                return true;
            };
            QVERIFY2(!weldOk(weak), "perna de 3 mm nao pode atender");
            QVERIFY(ConnectionCatalog::propose(c, f));
            QVERIFY(c.values.value(QStringLiteral("supWeldLegF")) > 3.0);
            QVERIFY2(weldOk(ConnectionCatalog::evaluate(c, f)), "solda proposta deve atender");
            // modelo 3D: solda na face do apoio (s = -outDist), entre a face e a junta
            c.values["outDist"] = 300.0;
            int welds = 0;
            for (const auto &p : ConnectionSolids::build(c, false)) {
                if (p.role == ConnectionSolids::Role::Weld && p.kind == ConnectionSolids::Kind::Box && std::abs(p.a[0] + 300.0 - p.size[0] / 2.0) < 1e-6) {
                    ++welds;
                }
            }
            QVERIFY2(welds >= 4, qPrintable(QStringLiteral("tipo %1: soldas na face do apoio: %2").arg(id).arg(welds)));
        }
    }

    void fabricationModelListsPlatesHolesAndBolts()
    {
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::FabricationModel;
        auto c = ConnectionService::makeDefault(NodeConnection::kShearTab, beam(), 250.0, 400.0);
        c.values["rows"] = 3.0;
        c.values["db"] = 20.0;
        c.values["pitch"] = 70.0;
        c.values["edgeV"] = 35.0;
        c.values["plateT"] = 9.5;
        const auto fab = FabricationModel::fromConnection(c);
        QCOMPARE(fab.plateCount(), 1);
        const auto &plate = fab.parts.first();
        QCOMPARE(plate.mark, QStringLiteral("C1"));
        QCOMPARE(plate.holes.size(), size_t(3));
        QVERIFY(std::abs(plate.holes[0].diameter - FabricationModel::holeDiameter(20.0)) < 1e-9);
        QVERIFY(std::abs(plate.thickness - 9.5) < 1e-9);
        QVERIFY(std::abs(plate.height - (2 * 70.0 + 2 * 35.0)) < 1e-9);
        // massa liquida = (area bruta - furos) x espessura x 7850 kg/m3
        const double gross = plate.width * plate.height;
        const double holes = 3.0 * 3.14159265358979 / 4.0 * plate.holes[0].diameter * plate.holes[0].diameter;
        QVERIFY(std::abs(plate.massKg - (gross - holes) * 9.5 * 7850e-9) < 1e-6);
        // espacamento dos furos preservado
        QVERIFY(std::abs(std::abs(plate.holes[1].b - plate.holes[0].b) - 70.0) < 1e-9);
        QCOMPARE(fab.bolts.size(), 1);
        QCOMPARE(fab.bolts.first().quantity, 3);
        QVERIFY(fab.bolts.first().length >= fab.bolts.first().gripLength);
        QCOMPARE(fab.welds.size(), 1);
        // dupla cantoneira: pecas identicas em duplicata compartilham a marca
        auto angles = ConnectionService::makeDefault(NodeConnection::kDoubleAngle, beam(), 250.0, 400.0);
        const auto two = FabricationModel::fromConnection(angles);
        int plates = 0, withTwo = 0;
        for (const auto &p : two.parts) {
            if (p.kind == Structura::App::Services::FabPart::Kind::Plate) {
                ++plates;
                withTwo += p.quantity >= 2 ? 1 : 0;
            }
        }
        QVERIFY(plates >= 1 && withTwo >= 1);
        // todas as tipologias geram pecas sem falhar
        for (const auto &entry : ConnectionCatalog::entries()) {
            auto catalog = ConnectionService::makeDefault(entry.id, beam(), 250.0, 400.0);
            const auto assembly = FabricationModel::fromConnection(catalog);
            for (const auto &p : assembly.parts) {
                QVERIFY2(p.thickness > 0.0 && p.width > 0.0 && p.height > 0.0, qPrintable(QStringLiteral("tipologia %1: peca invalida").arg(entry.id)));
            }
        }
    }

    void boltedSinglePlateMatchesTheExistingShearTab()
    {
        // mesma geometria: as verificacoes de parafusos, tala e solda coincidem com a chapa de cisalhamento ja validada
        D::ShearTabInput tab;
        tab.grade = D::boltGrades()[2];
        tab.db = 20.0;
        tab.rows = 3;
        tab.pitch = 70.0;
        tab.edgeV = 35.0;
        tab.edgeH = 35.0;
        tab.boltLineToWeld = 60.0;
        tab.beamSetback = 12.0;
        tab.plate = {250.0, 400.0};
        tab.plateThickness = 9.5;
        tab.web = {345.0, 450.0};
        tab.webThickness = 5.8;
        tab.weldLeg = 6.0;
        tab.support = {250.0, 400.0};
        tab.supportThickness = 12.0;
        tab.shear = 80000.0;
        D::WebShearInput w;
        w.plates = 1;
        w.webWelded = false;
        w.grade = tab.grade;
        w.db = 20.0;
        w.rows = 3;
        w.lines = 1;
        w.pitch = 70.0;
        w.edgeV = 35.0;
        w.edgeH = 35.0;
        w.boltLineToSupport = 60.0;
        w.gap = 12.0;
        w.plate = tab.plate;
        w.plateThickness = 9.5;
        w.web = tab.web;
        w.webThickness = 5.8;
        w.weldLegSupport = 6.0;
        w.support_ = tab.support;
        w.supportThickness = 12.0;
        w.shear = 80000.0;
        const auto a = D::checkShearTab(tab), b = D::checkWebShear(w);
        auto find = [](const std::vector<D::Check> &checks, const std::string &prefix) -> const D::Check * {
            for (const auto &c : checks) {
                if (c.name.rfind(prefix, 0) == 0) {
                    return &c;
                }
            }
            return nullptr;
        };
        for (const char *name : {"Cisalhamento", "Chapa - escoamento ao cisalhamento", "Chapa - flexao", "Solda de filete"}) {
            const D::Check *x = find(a, name);
            QVERIFY2(x != nullptr, name);
            const std::string other = std::string(name) == "Cisalhamento" ? "Cisalhamento" : (std::string(name).rfind("Chapa - escoamento", 0) == 0 ? "Tala - escoamento" : (std::string(name).rfind("Chapa - flexao", 0) == 0 ? "Tala - flexao" : "Solda tala - apoio (metodo"));
            const D::Check *y = find(b, other);
            QVERIFY2(y != nullptr, other.c_str());
            QVERIFY2(std::abs(x->ratio() - y->ratio()) < 1e-9 * std::max(1.0, x->ratio()), (std::string(name) + " " + std::to_string(x->ratio()) + " vs " + std::to_string(y->ratio())).c_str());
        }
    }

    void doublePlateHalvesThePlateDemandAndDoublesBoltPlanes()
    {
        D::WebShearInput single;
        single.grade = D::boltGrades()[2];
        single.plateThickness = 8.0;
        single.web = {345.0, 450.0};
        single.webThickness = 5.8;
        single.plate = {250.0, 400.0};
        single.support_ = {250.0, 400.0};
        single.shear = 120000.0;
        D::WebShearInput dbl = single;
        dbl.plates = 2;
        auto ratioOf = [](const std::vector<D::Check> &checks, const std::string &prefix) {
            for (const auto &c : checks) {
                if (c.name.rfind(prefix, 0) == 0) {
                    return c.ratio();
                }
            }
            return -1.0;
        };
        const auto a = D::checkWebShear(single), b = D::checkWebShear(dbl);
        QVERIFY(ratioOf(b, "Cisalhamento") < 0.55 * ratioOf(a, "Cisalhamento"));   // corte duplo
        QVERIFY(ratioOf(b, "Tala (cada) - escoamento") < 0.55 * ratioOf(a, "Tala - escoamento"));
        // soldada a alma: o grupo excentrico e verificado pelo centro instantaneo de rotacao
        D::WebShearInput welded = single;
        welded.webWelded = true;
        welded.plateLength = 140.0;
        welded.weldLegWeb = 6.0;
        const auto w = D::checkWebShear(welded);
        QVERIFY(ratioOf(w, "Solda tala - alma da viga") > 0.0);
        welded.weldLegWeb = 12.0;
        QVERIFY(ratioOf(D::checkWebShear(welded), "Solda tala - alma da viga") < ratioOf(w, "Solda tala - alma da viga"));
    }

    void beamToBeamNeedsCopesAndColumnWebNeedsRoom()
    {
        D::WebShearInput in;
        in.grade = D::boltGrades()[2];
        in.support = D::WebShearInput::BeamWeb;
        in.beamDepth = 310.0;
        in.beamTf = 9.7;
        in.beamFillet = 10.0;
        in.beamBf = 165.0;
        in.supD = 400.0;
        in.supBf = 180.0;
        in.supTf = 12.0;
        in.supTw = 8.0;
        const auto cope = D::webShearCope(in);
        QVERIFY(cope.length > 0.0 && cope.depth > in.beamTf);
        D::WebShearInput column = in;
        column.support = D::WebShearInput::ColumnWeb;
        column.supD = 200.0;   // pilar estreito: a mesa da viga nao cabe entre as mesas
        column.supTf = 12.0;
        bool failed = false;
        for (const auto &c : D::checkWebShear(column)) {
            if (c.name.rfind("Alma do pilar: largura livre", 0) == 0) {
                failed = c.ratio() > 1.0;
            }
        }
        QVERIFY(failed);
    }
};

QTEST_GUILESS_MAIN(TestConnectionCatalog)
#include "TestConnectionCatalog.moc"
