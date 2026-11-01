#include "CapacityCurveDialog.h"

#include "LineChartWidget.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

namespace {

QString number(double value, int decimals = 4)
{
    return QString::number(value, 'g', decimals + 2);
}

QTableWidget *makeTable(const QStringList &headers, QWidget *parent)
{
    auto *table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    return table;
}

void addRow(QTableWidget *table, const QStringList &cells)
{
    const int row = table->rowCount();
    table->insertRow(row);
    for (int c = 0; c < cells.size(); ++c) {
        table->setItem(row, c, new QTableWidgetItem(cells[c]));
    }
}

} // namespace

CapacityCurveDialog::CapacityCurveDialog(const QString &title, const Structura::App::SolverBridge::NonlinearResult &result, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Análise não linear — %1").arg(title));
    resize(780, 520);
    auto *root = new QVBoxLayout(this);
    auto *summary = new QLabel(result.converged
                                   ? tr("Convergiu: fator de carga final %1 em %2 passo(s) e %3 iteração(ões) no total.").arg(number(result.loadFactor)).arg(result.steps).arg(result.iterations)
                                   : tr("<b>Parou antes do pedido</b>: fator de carga alcançado %1 em %2 passo(s). A estrutura atingiu sua capacidade (mecanismo) ou a iteração perdeu a "
                                        "convergência; os resultados são os do último passo convergido.").arg(number(result.loadFactor)).arg(result.steps),
                               this);
    summary->setWordWrap(true);
    summary->setTextFormat(Qt::RichText);
    root->addWidget(summary);

    auto *tabs = new QTabWidget(this);
    auto *chart = new LineChartWidget(tabs);
    chart->setTitles(tr("Curva de capacidade"), tr("Deslocamento de controle"), tr("Fator de carga"));
    LineChartWidget::Series series;
    series.name = tr("Fator de carga");
    series.markers = false;
    for (const auto &point : result.curve) {
        series.points.append({point[1], point[0]});
    }
    LineChartWidget::Series dots = series;
    dots.name = tr("Passos");
    dots.markers = true;
    chart->setSeries({series, dots});
    tabs->addTab(chart, tr("Curva de capacidade"));

    auto *hinges = makeTable({tr("Barra"), tr("Grau de liberdade"), tr("Momento (kN·m)"), tr("Rotação plástica (rad)"), tr("Estado")}, tabs);
    static const char *dofNames[12] = {"ux I", "uy I", "uz I", "rx I", "ry I", "rz I", "ux J", "uy J", "uz J", "rx J", "ry J", "rz J"};
    for (const auto &h : result.hinges) {
        addRow(hinges, {QString::number(h.memberId), QString::fromLatin1(dofNames[h.dof % 12]), number(h.moment), number(h.plasticRotation, 6), h.yielded ? tr("plastificada") : tr("elástica")});
    }
    tabs->addTab(hinges, tr("Rótulas plásticas (%1)").arg(result.hinges.size()));

    auto *links = makeTable({tr("Ligação"), tr("Deformações (ux uy uz rx ry rz)"), tr("Forças (ux uy uz rx ry rz)")}, tabs);
    for (const auto &l : result.links) {
        QStringList deformation, force;
        for (std::size_t i = 0; i < 6; ++i) {
            deformation << number(l.deformation[i]);
            force << number(l.force[i]);
        }
        addRow(links, {QString::number(l.id), deformation.join(QStringLiteral("; ")), force.join(QStringLiteral("; "))});
    }
    tabs->addTab(links, tr("Ligações (%1)").arg(result.links.size()));
    root->addWidget(tabs, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(box);
}

} // namespace Structura::UI
