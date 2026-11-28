// SONDA TEMPORARIA (nao entra na entrega): exercita o NodeConnectionsDialog real, fora da tela, para os cenarios da verificacao funcional de A1/M2.
#include "app/ISceneModelGateway.h"
#include "app/services/ConnectionService.h"
#include "app/services/DatModelIoService.h"
#include "app/services/ReportService.h"
#include "report/DocxWriter.h"
#include "report/ReportHtml.h"
#include "ui/NodeConnectionsDialog.h"
#include <QFile>
#include <QBrush>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>

#include <deque>
#include <iostream>

using namespace Structura::App;
using namespace Structura::App::Services;
using Structura::App::ProjectData::NodeConnection;
using Structura::UI::NodeConnectionsDialog;

namespace {

class MemoryGateway : public ISceneModelGateway
{
public:
    std::vector<SceneNodeInfo> nodeInfos() const override { return {}; }
    std::vector<SceneBarInfo> bars() const override { return {}; }
    QUuid addPointWithId(double, double, double, int) override { return QUuid::createUuid(); }
    Structura::Model::Node *findNode(const QUuid &) override { return nullptr; }
    QUuid addBar(const QUuid &, const QUuid &, const QUuid &, const QUuid &) override { return QUuid::createUuid(); }
    void setBarExternalId(const QUuid &, int) override {}
};

QString g_out;
QString g_shots;

void say(const QString &s)
{
    std::cout << s.toLocal8Bit().constData() << std::endl;
}

ConnectionService::BeamInfo beam()
{
    ConnectionService::BeamInfo b;
    b.name = "IPE300";
    b.family = "W Laminado";
    b.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
    b.depth = 300.0;
    b.bf = 150.0;
    b.tw = 7.1;
    b.tf = 10.7;
    b.fy = 250.0;
    b.fu = 400.0;
    return b;
}

void column(NodeConnection &c, double d, double bf, double tf, double tw, double r, bool confirmed)
{
    c.values["supD"] = d;
    c.values["supBf"] = bf;
    c.values["supTf"] = tf;
    c.values["supTw"] = tw;
    c.values["supR"] = r;
    c.values["supFy"] = 345.0;
    c.values["supFu"] = 450.0;
    c.values["supType"] = 1.0;
    c.values["supConfirmed"] = confirmed ? 1.0 : 0.0;
}

QList<QWidget *> fieldWidgets(QWidget *w, const QString &labelStart)
{
    QList<QWidget *> out;
    for (auto *form : w->findChildren<QFormLayout *>()) {
        for (int row = 0; row < form->rowCount(); ++row) {
            auto *label = qobject_cast<QLabel *>(form->itemAt(row, QFormLayout::LabelRole) ? form->itemAt(row, QFormLayout::LabelRole)->widget() : nullptr);
            auto *field = form->itemAt(row, QFormLayout::FieldRole) ? form->itemAt(row, QFormLayout::FieldRole)->widget() : nullptr;
            if (label && field && label->text().startsWith(labelStart)) {
                out << field;
            }
        }
    }
    return out;
}

QString report(NodeConnectionsDialog &dlg, const QString &tag, bool memorial)
{
    QApplication::processEvents();
    dlg.resize(1400, 900);
    dlg.show();
    QApplication::processEvents();
    QString text;
    QTextStream o(&text);
    o << "=== " << tag << "\n";
    for (auto *l : dlg.findChildren<QLabel *>()) {
        const QString t = l->text();
        if (t.startsWith(QStringLiteral("Atende")) || t.startsWith(QStringLiteral("NÃO")) || t.contains(QStringLiteral("VERIFICÁVEL")) || t.startsWith(QStringLiteral("A ligação"))) {
            o << "VEREDITO: " << t << "\n";
        }
    }
    const auto tables = dlg.findChildren<QTableWidget *>();
    for (auto *t : tables) {
        for (int r = 0; r < t->rowCount() && r < 40; ++r) {
            QStringList cells;
            for (int c = 0; c < t->columnCount(); ++c) {
                cells << (t->item(r, c) ? t->item(r, c)->text() : QString());
            }
            o << "  [" << (t->columnCount() > 4 ? "verif" : "lista") << "] " << cells.join(QStringLiteral(" | ")).left(170) << "\n";
        }
    }
    if (memorial) {
        for (auto *b : dlg.findChildren<QPushButton *>()) {
            if (b->text().contains(QStringLiteral("Copiar memorial"))) {
                b->click();
            }
        }
        o << "--- MEMORIAL ---\n" << QApplication::clipboard()->text().left(9000) << "\n";
    }
    dlg.grab().save(g_shots + QStringLiteral("/") + tag.left(2) + QStringLiteral(".png"));
    return text;
}

NodeConnectionsDialog *open(const QVector<NodeConnection> &list, double moment, double shear)
{
    NodeConnectionsDialog::Callbacks cb;
    cb.forces = [=](const NodeConnection &, const QVector<NodeConnection> &) {
        ConnectionService::Forces f;
        f.moment = moment;
        f.shear = shear;
        return std::optional<ConnectionService::Forces>(f);
    };
    return new NodeConnectionsDialog(list, cb);
}

} // namespace


int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    g_shots = QString::fromLocal8Bit(qgetenv("PROBE_OUT"));
    QDir().mkpath(g_shots);
    QString all;
    auto base = [&]() { NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam(), 250.0, 400.0); c.id = 1; c.nodeId = 28; c.memberId = 13; return c; };
    struct Sc { QString tag; NodeConnection c; double m, v; };
    QVector<Sc> sc;
    { NodeConnection c = base(); column(c, 200, 200, 15, 9, 18, true); sc.push_back({"S1 padrao 2 fileiras (M=30 V=40)", c, 30e6, 40e3}); }
    { NodeConnection c = base(); column(c, 200, 200, 15, 9, 18, true); c.values["tRows"] = 1; c.values["plateT"] = 25; c.values["db"] = 24; sc.push_back({"S2 1 fileira robusta (M=20 V=20)", c, 20e6, 20e3}); }
    { NodeConnection c = base(); column(c, 200, 200, 8, 6, 10, true); c.values["tRows"] = 1; c.values["plateT"] = 25; c.values["db"] = 24; sc.push_back({"S3 1 fileira mesa esbelta tf=8 (M=20 V=20)", c, 20e6, 20e3}); }
    { NodeConnection c = base(); column(c, 200, 200, 15, 9, 18, true); c.values["plateT"] = 25; c.values["db"] = 24; sc.push_back({"S4 2 fileiras, esforco baixo (M=10 V=10): camada B favoravel", c, 10e6, 10e3}); }
    { NodeConnection c = base(); column(c, 200, 200, 15, 9, 18, true); c.values["tRows"] = 1; c.values["plateT"] = 25; c.values["db"] = 24; c.values["supFu"] = 700.0; sc.push_back({"S5 1 fileira, fu do pilar 700 (divergencia A x B)", c, 20e6, 20e3}); }
    QVector<ReportService::Input::ConnectionEntry> entries;
    int idx = 0;
    for (const auto &s : sc) {
        auto *d = open({s.c}, s.m, s.v);
        all += report(*d, s.tag, true);
        delete d;
        ReportService::Input::ConnectionEntry e;
        e.connection = s.c;
        e.connection.id = ++idx;
        e.forces.moment = s.m;
        e.forces.shear = s.v;
        e.hasForces = true;
        e.evaluation = ConnectionService::evaluate(s.c, e.forces);
        all += QStringLiteral("   [servico] %1 | maxRatio=%2 | aprovada=%3 | incompleta=%4\n").arg(s.tag.left(2)).arg(e.evaluation.maxRatio).arg(e.evaluation.approved()).arg(e.evaluation.incomplete());
        entries.push_back(e);
    }
    // relatorio (memorial global): HTML e DOCX
    ReportService::Input in;
    in.projectName = QStringLiteral("Auditoria A2");
    in.connections = entries;
    ReportService::Options opt;
    Structura::Report::ReportDocument doc;
    ReportService::appendDesignChapters(doc, in, opt, [&](const QString &) {});
    const auto html = Structura::Report::toHtml(doc);
    { QFile f(g_shots + "/relatorio.html"); f.open(QIODevice::WriteOnly); f.write(html.html.toUtf8()); }
    const auto docx = Structura::Report::writeDocx(doc);
    { QFile f(g_shots + "/relatorio.docx"); f.open(QIODevice::WriteOnly); f.write(reinterpret_cast<const char *>(docx.data()), static_cast<qint64>(docx.size())); }

    // controle: DOCX so com ligacoes SEM a verificacao A2 (chapa de cortante e mesas soldadas)
    {
        ReportService::Input in2;
        in2.projectName = QStringLiteral("Controle sem A2");
        for (int type : {int(NodeConnection::kShearTab), 144}) {
            NodeConnection c = ConnectionService::makeDefault(type, beam(), 250.0, 400.0);
            c.id = type; c.nodeId = 28; c.memberId = 13;
            column(c, 200, 200, 15, 9, 18, true);
            ReportService::Input::ConnectionEntry e;
            e.connection = c;
            e.forces.moment = 20e6; e.forces.shear = 30e3; e.hasForces = true;
            e.evaluation = ConnectionService::evaluate(c, e.forces);
            in2.connections.push_back(e);
        }
        Structura::Report::ReportDocument d2;
        ReportService::appendDesignChapters(d2, in2, opt, [&](const QString &) {});
        const auto dx2 = Structura::Report::writeDocx(d2);
        QFile f2(g_shots + "/relatorio_controle.docx"); f2.open(QIODevice::WriteOnly); f2.write(reinterpret_cast<const char *>(dx2.data()), static_cast<qint64>(dx2.size()));
    }
    all += QStringLiteral("=== relatorio: html %1 caracteres, docx %2 bytes\n").arg(html.html.size()).arg(docx.size());
    QFile f(g_shots + QStringLiteral("/probe_ui_a2.txt"));
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    f.write(all.toUtf8());
    say(QStringLiteral("ok"));
    return 0;
}
