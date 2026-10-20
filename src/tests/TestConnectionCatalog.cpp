#include <QtTest/QtTest>

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
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
