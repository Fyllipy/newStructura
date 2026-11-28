// Saneamento pos-A2: origem explicita das verificacoes, advertencia de divergencia, identidade visual do estado Complementary (dialogo e relatorio),
// aviso de varias fileiras com a acao explicita "Usar 1 fileira", valor estimado de dw e mensagem de pilar nao informado na ferramenta momento-rotacao.
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "app/services/ReportService.h"
#include "design/ColumnFlangeLever.h"
#include "design/Connections.h"
#include "design/EndPlateConnection.h"
#include "design/TStub.h"
#include "report/DocxWriter.h"
#include "report/ReportHtml.h"
#include "ui/NodeConnectionsDialog.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QtTest>

using namespace Structura::App::Services;
using Structura::App::ProjectData::NodeConnection;
namespace D = Structura::Design::Connections;
namespace TS = Structura::Design::TStub;

namespace {

ConnectionService::BeamInfo ipe300()
{
    ConnectionService::BeamInfo b;
    b.name = "IPE300";
    b.family = "W Laminado";
    b.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
    b.depth = 300.0;
    b.bf = 150.0;
    b.tw = 7.1;
    b.tf = 10.7;
    return b;
}

NodeConnection connection(int rows, double columnFu = 400.0, int type = NodeConnection::kEndPlate)
{
    NodeConnection c = ConnectionService::makeDefault(type, ipe300(), 250.0, 400.0);
    c.id = 1;
    c.nodeId = 28;
    c.memberId = 13;
    c.values["supD"] = 200.0;
    c.values["supBf"] = 200.0;
    c.values["supTf"] = 15.0;
    c.values["supTw"] = 9.0;
    c.values["supR"] = 18.0;
    c.values["supFy"] = 345.0;
    c.values["supFu"] = columnFu;
    c.values["supConfirmed"] = 1.0;
    if (type == NodeConnection::kEndPlate) {
        c.values["tRows"] = rows;
        c.values["plateT"] = 25.0;
        c.values["db"] = 24.0;
    }
    return c;
}

ConnectionService::Forces forces(double momentKnm, double shearKn)
{
    ConnectionService::Forces f;
    f.moment = momentKnm * 1.0e6;
    f.shear = shearKn * 1.0e3;
    return f;
}

const D::Check *find(const std::vector<D::Check> &checks, D::CheckOrigin origin)
{
    for (const auto &c : checks) {
        if (c.origin == origin) {
            return &c;
        }
    }
    return nullptr;
}

QTableWidget *verificationTable(QWidget *dialog)
{
    for (auto *t : dialog->findChildren<QTableWidget *>()) {
        if (t->columnCount() == 5 && t->horizontalHeaderItem(0) && t->horizontalHeaderItem(0)->text() == QStringLiteral("Verificação")) {
            return t;
        }
    }
    return nullptr;
}

Structura::UI::NodeConnectionsDialog *openDialog(const NodeConnection &c, double momentKnm, double shearKn)
{
    Structura::UI::NodeConnectionsDialog::Callbacks cb;
    cb.forces = [=](const NodeConnection &, const QVector<NodeConnection> &) { return std::optional<ConnectionService::Forces>(forces(momentKnm, shearKn)); };
    auto *dlg = new Structura::UI::NodeConnectionsDialog({c}, cb);
    dlg->resize(1400, 900);
    dlg->show();
    QApplication::processEvents();
    return dlg;
}

QPushButton *buttonNamed(QWidget *w, const QString &text)
{
    for (auto *b : w->findChildren<QPushButton *>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

class TestPostA2Sanitation : public QObject
{
    Q_OBJECT
private slots:
    // ---------------------------------------------------------------- I3: origem explicita
    void originsAreAssignedStructurally()
    {
        const auto two = ConnectionService::evaluate(connection(2), forces(30.0, 40.0));
        int layerA = 0, layerB = 0, warnings = 0, section = 0, design = 0;
        for (const auto &c : two.checks) {
            switch (c.origin) {
            case D::CheckOrigin::ColumnFlangeLeverNbr: ++layerA; QVERIFY(c.status == D::CheckStatus::Unverifiable); break;
            case D::CheckOrigin::ColumnFlangeLeverEn: ++layerB; QVERIFY(c.status == D::CheckStatus::Complementary); break;
            case D::CheckOrigin::DivergenceWarning: ++warnings; break;
            case D::CheckOrigin::ColumnSectionInfo: ++section; QVERIFY(c.status == D::CheckStatus::Information); break;
            case D::CheckOrigin::Design: ++design; break;
            case D::CheckOrigin::Information: break;
            }
        }
        QCOMPARE(layerA, 1);
        QCOMPARE(layerB, 1);
        QCOMPARE(warnings, 0);
        QCOMPARE(section, 1);
        QVERIFY(design > 10);
        // 1 fileira: a camada A e AVALIADA (participa da aprovacao) e ainda e Camada A
        const auto one = ConnectionService::evaluate(connection(1), forces(20.0, 20.0));
        const D::Check *a = find(one.checks, D::CheckOrigin::ColumnFlangeLeverNbr);
        QVERIFY(a);
        QVERIFY(a->status == D::CheckStatus::Evaluated);
        QVERIFY(D::isColumnFlangeLeverCheck(*a));
        QVERIFY(find(one.checks, D::CheckOrigin::ColumnFlangeLeverEn));
        // a divergencia aparece com fu do pilar alto e tem origem propria
        const auto div = ConnectionService::evaluate(connection(1, 700.0), forces(20.0, 20.0));
        QVERIFY(find(div.checks, D::CheckOrigin::DivergenceWarning));
    }

    void nameCoincidenceNeverHidesALegitimateCheck()
    {
        // a identificacao NAO depende do texto: uma verificacao de projeto com o mesmo nome de uma do A2 nao e excluida...
        D::Check legit;
        legit.name = D::kLeverNbrName;
        legit.origin = D::CheckOrigin::Design;
        QVERIFY(!D::isColumnFlangeLeverCheck(legit));
        D::Check legit2;
        legit2.name = D::kLeverEnName;
        QVERIFY(!D::isColumnFlangeLeverCheck(legit2));
        D::Check legit3;
        legit3.name = D::kLeverWarnName;
        QVERIFY(!D::isColumnFlangeLeverCheck(legit3));
        // ... e uma verificacao do A2 com nome qualquer e identificada pela origem
        D::Check a2;
        a2.name = "qualquer";
        for (const auto o : {D::CheckOrigin::ColumnFlangeLeverNbr, D::CheckOrigin::ColumnFlangeLeverEn, D::CheckOrigin::DivergenceWarning}) {
            a2.origin = o;
            QVERIFY(D::isColumnFlangeLeverCheck(a2));
        }
        // as demais origens nunca sao A2
        for (const auto o : {D::CheckOrigin::Design, D::CheckOrigin::ColumnSectionInfo, D::CheckOrigin::Information}) {
            a2.origin = o;
            QVERIFY(!D::isColumnFlangeLeverCheck(a2));
        }
    }

    void preA2PlateLeverCheckStaysInTheSweepAndSummaryIsUnchanged()
    {
        const auto e = ConnectionService::evaluate(connection(1), forces(20.0, 20.0));
        const D::Check *plate = nullptr;
        for (const auto &c : e.checks) {
            if (c.name.find("Efeito de alavanca - espessura da placa") == 0) {
                plate = &c;
            }
        }
        QVERIFY(plate);
        QVERIFY(plate->origin == D::CheckOrigin::Design);   // anterior ao A2: continua incluida na varredura
        QVERIFY(!D::isColumnFlangeLeverCheck(*plate));
        // a origem nao altera Summary/maxRatio: recalculo independente ignorando a origem
        double worst = 0.0;
        int failed = 0;
        for (const auto &c : e.checks) {
            if (c.status == D::CheckStatus::Evaluated) {
                worst = std::max(worst, c.ratio());
                failed += c.ratio() > 1.0 + 1e-9 ? 1 : 0;
            }
        }
        QCOMPARE(e.maxRatio, worst);
        QCOMPARE(e.summary().failed, failed);
        // Complementary (qualquer origem) nunca altera o resumo
        auto withoutA2B = e.checks;
        withoutA2B.erase(std::remove_if(withoutA2B.begin(), withoutA2B.end(), [](const D::Check &c) { return c.status == D::CheckStatus::Complementary; }), withoutA2B.end());
        QVERIFY(D::summarize(withoutA2B).status == e.summary().status);
        QCOMPARE(D::summarize(withoutA2B).worstRatio, e.summary().worstRatio);
    }

    void spliceDropsColumnChecksByOrigin()
    {
        NodeConnection splice = ConnectionService::makeDefault(135, ipe300(), 250.0, 400.0);
        splice.id = 1;
        const auto e = ConnectionService::evaluate(splice, forces(20.0, 30.0));
        QVERIFY(!e.checks.empty());
        for (const auto &c : e.checks) {
            QVERIFY2(!D::isColumnFlangeLeverCheck(c) && c.origin != D::CheckOrigin::ColumnSectionInfo, c.name.c_str());
        }
    }

    // ---------------------------------------------------------------- I1: advertencia de divergencia
    void divergenceWarningIsInformativeText()
    {
        const auto e = ConnectionService::evaluate(connection(1, 700.0), forces(20.0, 20.0));
        const D::Check *w = find(e.checks, D::CheckOrigin::DivergenceWarning);
        QVERIFY(w);
        QCOMPARE(w->demand, 0.0);
        QCOMPARE(w->resistance, 0.0);
        const QString msg = QString::fromStdString(w->reason);
        QVERIFY2(msg.startsWith(QStringLiteral("Divergência entre metodologias: Camada A (NBR) = ")), qPrintable(msg));
        QVERIFY(msg.contains(QStringLiteral("Camada B (EN) = ")));
        QVERIFY(msg.contains(QStringLiteral("% superior à da Camada B. Resultado informativo, sem influência na aprovação global.")));
        QVERIFY(e.approved());   // a advertencia nao altera a aprovacao global
    }

    // ---------------------------------------------------------------- I5: dw estimado
    void estimatedHeadWidthIsIdentifiedNeverPresentedAsNormative()
    {
        for (const double db : {12.0, 16.0, 20.0, 22.0, 24.0, 27.0, 30.0, 36.0}) {
            QVERIFY2(TS::boltHeadWidth(db).tabulated, qPrintable(QString::number(db)));
        }
        for (const double db : {14.0, 18.0, 33.0}) {
            const auto w = TS::boltHeadWidth(db);
            QVERIFY(!w.tabulated);
            QVERIFY(std::abs(w.acrossPoints - 1.5 * db * 1.1547005) < 1e-9);
            QCOMPARE(TS::boltHeadAcrossPoints(db), w.acrossPoints);   // compatibilidade: mesmo numero, sem alterar o calculo
        }
        auto linesOf = [](const NodeConnection &c) {
            const auto e = ConnectionService::evaluate(c, forces(20.0, 20.0));
            const D::Check *b = find(e.checks, D::CheckOrigin::ColumnFlangeLeverEn);
            std::string all;
            if (b) {
                for (const auto &l : b->lines) {
                    all += l + "\n";
                }
            }
            return QString::fromStdString(all);
        };
        NodeConnection m14 = connection(1);
        m14.values["db"] = 14.0;
        QVERIFY(linesOf(m14).contains(QStringLiteral("VALOR ESTIMADO")));
        QVERIFY(linesOf(m14).contains(QStringLiteral("não é dado normativo")));
        QVERIFY(!linesOf(connection(1)).contains(QStringLiteral("VALOR ESTIMADO")));   // M24 tabelado
    }

    // ---------------------------------------------------------------- item 6: pilar nao informado
    void momentRotationExplainsTheMissingColumn()
    {
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, ipe300(), 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;
        const auto r = JointComponentService::evaluate(c, 200000.0);
        QVERIFY(!r.supported);
        QCOMPARE(QString::fromStdString(r.message),
                 QStringLiteral("Rigidez não calculável: informe ou confirme a seção do pilar e suas propriedades geométricas e mecânicas."));
    }

    // ---------------------------------------------------------------- I4 + item 8: dialogo
    void dialogGivesComplementaryItsOwnVisualIdentity()
    {
        std::unique_ptr<Structura::UI::NodeConnectionsDialog> dlg(openDialog(connection(1, 700.0), 20.0, 20.0));
        auto *table = verificationTable(dlg.get());
        QVERIFY(table);
        const QColor indigo(QStringLiteral("#5b4bb7")), gray(QStringLiteral("#6b7785"));
        int complementary = 0, information = 0;
        for (int r = 0; r < table->rowCount(); ++r) {
            const auto *state = table->item(r, 4);
            const QString text = state->text();
            if (text.contains(QStringLiteral("MÉTODO COMPLEMENTAR"))) {
                ++complementary;
                QVERIFY(text.startsWith(QStringLiteral("◇ ")));
                QVERIFY(text.contains(QStringLiteral("NÃO PARTICIPA DA APROVAÇÃO GLOBAL")));
                QCOMPARE(state->foreground().color(), indigo);
                QVERIFY(state->font().italic());
            } else if (text.contains(QStringLiteral("INFORMAÇÃO"))) {
                ++information;
                QCOMPARE(state->foreground().color(), gray);   // INFORMACAO continua cinza: a cor do complementar e propria
            }
        }
        QVERIFY(complementary >= 2);   // camada B + advertencia
        QVERIFY(information >= 1);
        // a advertencia mostra a MENSAGEM, sem demanda/resistencia/indice
        bool foundWarning = false;
        for (int r = 0; r < table->rowCount(); ++r) {
            if (table->item(r, 0)->text().startsWith(QStringLiteral("Divergência entre metodologias: Camada A (NBR) = "))) {
                foundWarning = true;
                QCOMPARE(table->item(r, 2)->text(), QStringLiteral("-"));
                QCOMPARE(table->item(r, 3)->text(), QStringLiteral("-"));
                QVERIFY(!table->item(r, 4)->text().contains(QStringLiteral("η")));
            }
        }
        QVERIFY(foundWarning);
    }

    void oneRowGuidanceAndExplicitAction()
    {
        std::unique_ptr<Structura::UI::NodeConnectionsDialog> dlg(openDialog(connection(2), 30.0, 40.0));
        auto *button = buttonNamed(dlg.get(), QStringLiteral("Usar 1 fileira"));
        QVERIFY(button);
        QVERIFY(button->isVisible());
        QLabel *notice = nullptr;
        for (auto *l : dlg->findChildren<QLabel *>()) {
            if (l->text().startsWith(QStringLiteral("Limite da verificação NBR da mesa do pilar"))) {
                notice = l;
            }
        }
        QVERIFY(notice);
        QVERIFY(notice->isVisible());
        QVERIFY(notice->text().contains(QStringLiteral("6.3.5.4")));
        QVERIFY(notice->text().contains(QStringLiteral("NÃO VERIFICÁVEL")));
        // o padrao de 2 fileiras NAO foi alterado sozinho: nada mudou ate o clique
        QCOMPARE(dlg->connections().first().values.value(QStringLiteral("tRows")), 2.0);
        auto *table = verificationTable(dlg.get());
        QVERIFY(table);
        auto stateOfLayerA = [&](bool &unverifiable) {
            for (int r = 0; r < table->rowCount(); ++r) {
                if (table->item(r, 0)->text().startsWith(QStringLiteral("Efeito de alavanca - mesa do pilar"))) {
                    unverifiable = table->item(r, 4)->text().contains(QStringLiteral("NÃO VERIFICÁVEL"));
                    return true;
                }
            }
            return false;
        };
        bool nv = false;
        QVERIFY(stateOfLayerA(nv));
        QVERIFY(nv);
        const int rowsBefore = table->rowCount();
        QTest::mouseClick(button, Qt::LeftButton);
        QApplication::processEvents();
        // acao explicita: dado, verificacoes e aviso atualizados de forma consistente e visivel
        QCOMPARE(dlg->connections().first().values.value(QStringLiteral("tRows")), 1.0);
        table = verificationTable(dlg.get());
        QVERIFY(stateOfLayerA(nv));
        QVERIFY(!nv);   // a camada A agora e avaliada
        QVERIFY(table->rowCount() != 0 && rowsBefore != 0);
        QVERIFY(!button->isVisible());
        QVERIFY(notice->isVisible());
        QVERIFY(notice->text().contains(QStringLiteral("alteradas de 2 para 1 por ação do usuário")));
    }

    void oneRowConnectionHasNoGuidanceButton()
    {
        std::unique_ptr<Structura::UI::NodeConnectionsDialog> dlg(openDialog(connection(1), 20.0, 20.0));
        auto *button = buttonNamed(dlg.get(), QStringLiteral("Usar 1 fileira"));
        QVERIFY(button);
        QVERIFY(!button->isVisible());
        std::unique_ptr<Structura::UI::NodeConnectionsDialog> splice([&] {
            NodeConnection s = ConnectionService::makeDefault(135, ipe300(), 250.0, 400.0);
            s.id = 1;
            return openDialog(s, 20.0, 30.0);
        }());
        QVERIFY(!buttonNamed(splice.get(), QStringLiteral("Usar 1 fileira"))->isVisible());   // a emenda viga-viga nao tem pilar
    }

    // ---------------------------------------------------------------- I4: relatorio
    void reportCarriesTheComplementaryIdentity()
    {
        ReportService::Input in;
        in.projectName = QStringLiteral("Saneamento");
        ReportService::Input::ConnectionEntry entry;
        entry.connection = connection(1, 700.0);
        entry.forces = forces(20.0, 20.0);
        entry.hasForces = true;
        entry.evaluation = ConnectionService::evaluate(entry.connection, entry.forces);
        in.connections.push_back(entry);
        ReportService::Options opt;
        Structura::Report::ReportDocument doc;
        ReportService::appendDesignChapters(doc, in, opt, [](const QString &) {});
        const QString html = Structura::Report::toHtml(doc).html;
        QVERIFY(html.contains(QStringLiteral("◇ MÉTODO COMPLEMENTAR — NÃO PARTICIPA DA APROVAÇÃO GLOBAL")));
        QVERIFY(html.contains(QStringLiteral("#4b3fa0")));   // cor propria
        QVERIFY(html.contains(QStringLiteral("Divergência entre metodologias: Camada A (NBR) = ")));
        QVERIFY(!html.contains(QStringLiteral("η informativo")));
        const auto bytes = Structura::Report::writeDocx(doc);
        QVERIFY(bytes.size() > 1000);
        const QByteArray raw(reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size()));
        QVERIFY(raw.contains("ECE9FA"));   // preenchimento do estado complementar no DOCX (zip sem compressao)
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestPostA2Sanitation test;
    return QTest::qExec(&test, argc, argv);
}
#include "TestPostA2Sanitation.moc"
