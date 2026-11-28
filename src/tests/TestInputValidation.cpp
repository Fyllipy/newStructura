#include <QtTest/QtTest>
#include <QApplication>

#include "app/services/ConnectionService.h"
#include "design/Connections.h"
#include "design/EndPlateConnection.h"
#include "design/InputValidation.h"
#include "design/ShearConnections.h"
#include "design/SupportChecks.h"

#include <cmath>
#include <limits>

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Structura::App::Services::ConnectionService;

// Correcao A1 (geometria do pilar sem valor padrao) e M2 (verificacoes nunca somem; resultado global so aprova com tudo executado).
class TestInputValidation : public QObject
{
    Q_OBJECT

    static ConnectionService::BeamInfo beam()
    {
        ConnectionService::BeamInfo b;
        b.name = "IPE300";
        b.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
        b.depth = 300.0;
        b.bf = 150.0;
        b.tw = 7.1;
        b.tf = 10.7;
        b.fy = 250.0;
        b.fu = 400.0;
        return b;
    }
    static ConnectionService::Forces loads()
    {
        ConnectionService::Forces f;
        f.moment = 30.0e6;
        f.shear = 40.0e3;
        return f;
    }
    static bool hasUnverifiable(const std::vector<D::Check> &c)
    {
        for (const auto &k : c) {
            if (k.status == D::CheckStatus::Unverifiable) {
                return true;
            }
        }
        return false;
    }

private slots:
    // 1) chapa de extremidade sem barra de apoio e sem geometria manual: NAO pode aprovar nem usar 260/146/12,7/7,6
    void noSupportAndNoManualGeometry()
    {
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam(), 250.0, 400.0);
        const auto e = ConnectionService::evaluate(c, loads());
        QVERIFY(e.incomplete());
        QVERIFY(!e.approved());
        QVERIFY(e.maxRatio >= D::kUnverifiableRatio);
        QVERIFY(hasUnverifiable(e.checks));
        QVERIFY(e.statusText().contains(QStringLiteral("NÃO VERIFICÁVEL")));
        const D::EndPlateInput in = ConnectionService::endPlateInput(c);
        QCOMPARE(in.colDepth, 0.0);            // nenhum valor padrao embutido
        QCOMPARE(in.colWebThickness, 0.0);
        QVERIFY(!e.problems().isEmpty());
    }

    // 2) geometria manual valida: so conta se CONFIRMADA; confirmada, a verificacao roda e a secao usada fica na memoria
    void manualGeometryNeedsConfirmation()
    {
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam(), 250.0, 400.0);
        c.values["supD"] = 260.0;
        c.values["supBf"] = 146.0;
        c.values["supTf"] = 12.7;
        c.values["supTw"] = 7.6;
        c.values["supR"] = 10.0;
        c.values["supFy"] = 250.0;
        c.values["supFu"] = 400.0;
        c.values["supType"] = 1.0;
        c.values["tRows"] = 1.0;   // A2: a alavanca da mesa do pilar so e verificada (NBR) com 1 fileira; com mais, a ligacao e NAO VERIFICAVEL
        auto e = ConnectionService::evaluate(c, loads());
        QVERIFY(e.incomplete());   // informada mas nao confirmada
        c.values["supConfirmed"] = 1.0;
        e = ConnectionService::evaluate(c, loads());
        QVERIFY(!e.incomplete());
        QVERIFY(e.maxRatio < D::kUnverifiableRatio);
        bool shown = false;
        for (const auto &k : e.checks) {
            if (k.status == D::CheckStatus::Information && k.name.find("Seção do pilar utilizada") != std::string::npos) {
                shown = true;
                QVERIFY(k.lines.size() >= 3);
                QVERIFY(k.lines[0].find("confirmada") != std::string::npos);
            }
        }
        QVERIFY(shown);
    }

    // confirmacao manual vale so para as dimensoes confirmadas: alterou uma dimensao, volta a NAO VERIFICAVEL
    void confirmationIsInvalidatedWhenDimensionsChange()
    {
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam(), 250.0, 400.0);
        c.values["supD"] = 200.0;
        c.values["supBf"] = 200.0;
        c.values["supTf"] = 15.0;
        c.values["supTw"] = 9.0;
        c.values["supR"] = 18.0;
        c.values["supFy"] = 345.0;
        c.values["supFu"] = 450.0;
        c.values["supConfirmed"] = 1.0;
        c.values["tRows"] = 1.0;
        c.values["supConfirmedSig"] = ConnectionService::columnSignature(c);
        QVERIFY(!ConnectionService::evaluate(c, loads()).incomplete());
        c.values["supTf"] = 6.0;   // dimensao alterada depois da confirmacao
        const auto e = ConnectionService::evaluate(c, loads());
        QVERIFY(e.incomplete());
        QVERIFY(e.problems().join(QLatin1Char(' ')).contains(QStringLiteral("ALTERADA")));
    }

    // 3) espessura de alma igual a zero: nenhuma verificacao desaparece
    void zeroWebThickness()
    {
        D::ColumnPanelInput panel;
        panel.d = 200.0;
        panel.bf = 150.0;
        panel.tf = 15.0;
        panel.tw = 0.0;
        panel.fy = 250.0;
        panel.beamForce = 500e3;
        const auto p = D::checkColumnPanel(panel);
        QVERIFY(!p.empty());
        QVERIFY(hasUnverifiable(p));
        D::SupportInput sup;
        sup.face = D::SupportInput::Flange;
        sup.d = 200.0;
        sup.bf = 150.0;
        sup.tf = 15.0;
        sup.tw = 0.0;
        sup.fy = 250.0;
        sup.fu = 400.0;
        sup.tension = 500e3;
        const auto s = D::checkSupportMember(sup);
        QVERIFY(!s.empty());
        QVERIFY(hasUnverifiable(s));
        QVERIFY(D::summarize(s).status == D::Summary::Incomplete);
    }

    // 4) dimensoes negativas
    void negativeDimensions()
    {
        D::BoltedInput in;
        in.db = -20.0;
        in.plateThickness = 10.0;
        const auto checks = D::checkBolted(in);
        QVERIFY(!checks.empty());
        QVERIFY(hasUnverifiable(checks));
        D::WeldInput w;
        w.leg = -6.0;
        QVERIFY(hasUnverifiable(D::checkFilletWeld(w)));
    }

    // 5) NaN e infinito
    void nanAndInfinity()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        D::EndPlateInput in;
        in.colDepth = nan;
        auto checks = D::checkEndPlate(in);
        QVERIFY(hasUnverifiable(checks));
        QVERIFY(D::summarize(checks).status == D::Summary::Incomplete);
        D::EndPlateInput in2;
        in2.moment = inf;
        checks = D::checkEndPlate(in2);
        QVERIFY(hasUnverifiable(checks));
        QVERIFY(D::summarize(checks).worstRatio >= D::kUnverifiableRatio);
    }

    // 6) lista vazia de verificacoes nunca aprova
    void emptyListNeverApproves()
    {
        const std::vector<D::Check> none;
        const auto s = D::summarize(none);
        QVERIFY(s.status == D::Summary::Incomplete);
        QVERIFY(D::worstRatio(none) >= D::kUnverifiableRatio);
        QVERIFY(!s.problems.empty());
        // lista so com "nao aplicavel" e informacao tambem nao aprova
        std::vector<D::Check> only {D::notApplicableCheck("x", "y", "porque sim"), D::informationCheck("i", "c", {"linha"})};
        QVERIFY(D::summarize(only).status == D::Summary::Incomplete);
    }

    // 7) todas aprovadas, incluindo nao aplicavel com justificativa => APROVADA
    void allApproved()
    {
        D::Check ok;
        ok.name = "a";
        ok.demand = 5.0;
        ok.resistance = 10.0;
        std::vector<D::Check> list {ok, D::notApplicableCheck("n", "c", "sem apoio I/H")};
        const auto s = D::summarize(list);
        QVERIFY(s.status == D::Summary::Approved);
        QCOMPARE(s.executed, 1);
        QCOMPARE(s.notApplicable, 1);
        QVERIFY(std::abs(D::worstRatio(list) - 0.5) < 1e-12);
    }

    // 8) pelo menos uma reprovada
    void atLeastOneFailed()
    {
        D::Check ok;
        ok.demand = 5.0;
        ok.resistance = 10.0;
        D::Check bad;
        bad.demand = 12.0;
        bad.resistance = 10.0;
        const auto s = D::summarize({ok, bad});
        QVERIFY(s.status == D::Summary::Failed);
        QCOMPARE(s.failed, 1);
        QCOMPARE(QString::fromStdString(s.label()), QStringLiteral("REPROVADA"));
    }

    // 9) pelo menos uma nao verificavel: aprovada + nao verificavel nunca e APROVADA
    void atLeastOneUnverifiable()
    {
        D::Check ok;
        ok.demand = 5.0;
        ok.resistance = 10.0;
        const auto s = D::summarize({ok, D::unverifiableCheck("v", "c", "falta tw")});
        QVERIFY(s.status == D::Summary::Incomplete);
        QCOMPARE(s.unverifiable, 1);
        QVERIFY(s.worstRatio >= D::kUnverifiableRatio);
        QVERIFY(QString::fromStdString(s.label()).contains(QStringLiteral("NÃO VERIFICÁVEL")));
    }

    // 10) nenhuma situacao incompleta resulta em aprovacao global (varredura de entradas invalidas)
    void noIncompleteSituationApproves()
    {
        const double bad[] = {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()};
        for (double v : bad) {
            for (int field = 0; field < 4; ++field) {
                D::ColumnPanelInput p;
                p.d = 200.0;
                p.bf = 150.0;
                p.tf = 15.0;
                p.tw = 8.0;
                p.fy = 250.0;
                p.beamForce = 100e3;
                (field == 0 ? p.d : field == 1 ? p.bf : field == 2 ? p.tf : p.tw) = v;
                const auto s = D::summarize(D::checkColumnPanel(p));
                QVERIFY2(s.status != D::Summary::Approved, qPrintable(QStringLiteral("campo %1 valor %2").arg(field).arg(v)));
            }
        }
        // ligacao completa sem apoio: tambem nao aprova
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam(), 250.0, 400.0);
        QVERIFY(!ConnectionService::evaluate(c, loads()).approved());
    }

    // entradas validas continuam produzindo o mesmo resultado (EndPlateInput com a secao padrao do teste historico)
    void validCaseStillApproves()
    {
        D::EndPlateInput in;
        for (const auto &g : D::boltGrades()) {
            if (g.name.find("A325") != std::string::npos) {
                in.grade = g;
            }
        }
        in.moment = 20.0e6;
        in.shear = 30.0e3;
        in.tensionRows = 1;
        in.columnSource = "teste";
        const auto checks = D::checkEndPlate(in);
        QVERIFY(!hasUnverifiable(checks));
        QVERIFY(D::summarize(checks).executed > 5);
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestInputValidation test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestInputValidation.moc"
