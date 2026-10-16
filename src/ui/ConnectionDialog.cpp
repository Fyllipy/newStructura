#include "ConnectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace Structura::UI {

namespace C = Structura::Design::Connections;

namespace {

QDoubleSpinBox *spin(double min, double max, double value, const QString &suffix, int decimals = 1, double step = 1.0)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSuffix(suffix);
    s->setSingleStep(step);
    return s;
}

} // namespace

ConnectionDialog::ConnectionDialog(Provider provider, QWidget *parent)
    : QDialog(parent)
    , m_provider(std::move(provider))
{
    setWindowTitle(tr("Verificação de ligações"));
    resize(820, 720);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Verifica um grupo de parafusos (NBR 8800:2024, 6.3: cisalhamento, pressão de contato, tração e interação, espaçamentos) "
                                "ou um grupo de filetes de solda (6.2). Distribuição elástica no plano da ligação; efeito alavanca, cisalhamento de bloco, "
                                "escoamento/ruptura das chapas e dos elementos ligados (cantoneiras, chapas de topo) e ligações por atrito não são verificados — complete à parte."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    // ---- esforcos --------------------------------------------------------------------------------------------
    auto *loads = new QGroupBox(tr("Esforços solicitantes de cálculo"));
    auto *lf = new QFormLayout(loads);
    m_vx = spin(-1e6, 1e6, 0, tr(" kN"), 2);
    m_vy = spin(-1e6, 1e6, 0, tr(" kN"), 2);
    m_mz = spin(-1e6, 1e6, 0, tr(" kN·m"), 2);
    m_tension = spin(0, 1e6, 0, tr(" kN"), 2);
    m_force = spin(0, 1e7, 0, tr(" kN"), 2);
    lf->addRow(tr("Cortante na direção x do grupo"), m_vx);
    lf->addRow(tr("Cortante na direção y do grupo"), m_vy);
    lf->addRow(tr("Momento no plano do grupo (excentricidade)"), m_mz);
    lf->addRow(tr("Tração axial nos parafusos (total)"), m_tension);
    lf->addRow(tr("Força resultante na solda"), m_force);
    auto *fetchRow = new QHBoxLayout;
    auto *fetchStart = new QPushButton(tr("Esforços da barra selecionada — início"));
    auto *fetchEnd = new QPushButton(tr("— fim"));
    fetchRow->addWidget(fetchStart);
    fetchRow->addWidget(fetchEnd);
    lf->addRow(fetchRow);
    m_origin = new QLabel(tr("Preencha manualmente ou obtenha a envoltória das combinações últimas da barra selecionada (rode o dimensionamento antes)."));
    m_origin->setWordWrap(true);
    m_origin->setStyleSheet(QStringLiteral("color:#5a6472;"));
    lf->addRow(m_origin);
    root->addWidget(loads);

    // ---- abas ------------------------------------------------------------------------------------------------
    m_tabs = new QTabWidget;
    auto *bolts = new QWidget;
    auto *bf = new QFormLayout(bolts);
    m_grade = new QComboBox;
    for (const auto &g : C::boltGrades()) {
        m_grade->addItem(QString::fromStdString(g.name));
    }
    m_grade->setCurrentIndex(2);
    m_db = spin(8, 60, 20, tr(" mm"), 1);
    m_thread = new QCheckBox(tr("Plano de corte passa pela rosca"));
    m_thread->setChecked(true);
    m_planes = new QSpinBox;
    m_planes->setRange(1, 2);
    m_nx = new QSpinBox;
    m_nx->setRange(1, 20);
    m_nx->setValue(2);
    m_ny = new QSpinBox;
    m_ny->setRange(1, 20);
    m_sx = spin(20, 500, 70, tr(" mm"));
    m_sy = spin(20, 500, 70, tr(" mm"));
    m_t = spin(1, 100, 10, tr(" mm"), 2);
    m_fu = spin(100, 1000, 400, tr(" MPa"), 0);
    m_le = spin(10, 300, 35, tr(" mm"));
    m_lt = spin(10, 300, 35, tr(" mm"));
    m_deformation = new QCheckBox(tr("Deformação no furo em serviço é limitação de projeto (1,2 / 2,4)"));
    m_deformation->setChecked(true);
    bf->addRow(tr("Parafuso"), m_grade);
    bf->addRow(tr("Diâmetro"), m_db);
    bf->addRow(m_thread);
    bf->addRow(tr("Planos de corte"), m_planes);
    bf->addRow(tr("Parafusos em x"), m_nx);
    bf->addRow(tr("Parafusos em y"), m_ny);
    bf->addRow(tr("Espaçamento em x"), m_sx);
    bf->addRow(tr("Espaçamento em y"), m_sy);
    bf->addRow(tr("Espessura da chapa (contato)"), m_t);
    bf->addRow(tr("fu da chapa"), m_fu);
    bf->addRow(tr("Distância furo extremo – borda (na força)"), m_le);
    bf->addRow(tr("Distância à borda transversal"), m_lt);
    bf->addRow(m_deformation);
    m_tabs->addTab(bolts, tr("Parafusos"));

    auto *weld = new QWidget;
    auto *wf = new QFormLayout(weld);
    m_leg = spin(3, 30, 6, tr(" mm"));
    m_length = spin(10, 5000, 200, tr(" mm"));
    m_electrode = new QComboBox;
    m_electrode->addItem(tr("E60 (fw = 415 MPa)"), 415.0);
    m_electrode->addItem(tr("E70 (fw = 485 MPa)"), 485.0);
    m_electrode->addItem(tr("E80 (fw = 550 MPa)"), 550.0);
    m_electrode->setCurrentIndex(1);
    m_angle = spin(0, 90, 0, tr(" °"), 0);
    m_wt = spin(1, 100, 10, tr(" mm"), 2);
    m_wfy = spin(100, 800, 250, tr(" MPa"), 0);
    m_wfu = spin(100, 1000, 400, tr(" MPa"), 0);
    wf->addRow(tr("Perna do filete"), m_leg);
    wf->addRow(tr("Comprimento efetivo total"), m_length);
    wf->addRow(tr("Eletrodo"), m_electrode);
    wf->addRow(tr("Ângulo da resultante com o eixo da solda"), m_angle);
    wf->addRow(tr("Espessura da parte mais fina"), m_wt);
    wf->addRow(tr("fy do metal-base"), m_wfy);
    wf->addRow(tr("fu do metal-base"), m_wfu);
    m_tabs->addTab(weld, tr("Solda de filete"));
    root->addWidget(m_tabs);

    auto *verify = new QPushButton(tr("Verificar"));
    verify->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 6px 18px; border-radius: 4px; }"));
    root->addWidget(verify, 0, Qt::AlignLeft);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({tr("Verificação"), tr("Item NBR"), tr("Solicitação / valor"), tr("Resistência / limite"), tr("Razão")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(170);
    root->addWidget(m_table, 1);
    m_verdict = new QLabel;
    m_verdict->setWordWrap(true);
    root->addWidget(m_verdict);

    connect(verify, &QPushButton::clicked, this, [this]() { this->verify(); });
    connect(fetchStart, &QPushButton::clicked, this, [this]() { fetch(false); });
    connect(fetchEnd, &QPushButton::clicked, this, [this]() { fetch(true); });
}

void ConnectionDialog::fetch(bool atEnd)
{
    if (!m_provider) {
        return;
    }
    const EndForceEnvelope env = m_provider(atEnd);
    if (!env.ok) {
        m_origin->setText(tr("Não há esforços disponíveis: selecione uma barra e rode a análise/verificação antes."));
        return;
    }
    // cortantes viram cortante nas duas direcoes do grupo; tracao/compressao axial vira cortante ou tracao conforme o caso
    m_vx->setValue(env.vz);
    m_vy->setValue(env.vy);
    m_mz->setValue(0.0);
    m_tension->setValue(env.n);
    m_force->setValue(std::hypot(env.vy, env.vz));
    m_origin->setText(tr("%1: |N| tração %2 kN, compressão %3 kN, |Vy| %4 kN, |Vz| %5 kN, |Mx| %6 kN·m, |My| %7 kN·m, |T| %8 kN·m (máximos em módulo entre as combinações; "
                         "cortantes e tração carregados nos campos — ajuste a distribuição conforme a ligação).")
                          .arg(env.label).arg(env.n, 0, 'f', 1).arg(env.nc, 0, 'f', 1).arg(env.vy, 0, 'f', 1).arg(env.vz, 0, 'f', 1).arg(env.mx, 0, 'f', 1).arg(env.my, 0, 'f', 1).arg(env.t, 0, 'f', 1));
}

void ConnectionDialog::verify()
{
    std::vector<C::Check> checks;
    if (m_tabs->currentIndex() == 0) {
        C::BoltedInput in;
        in.grade = C::boltGrades()[static_cast<std::size_t>(m_grade->currentIndex())];
        in.db = m_db->value();
        in.threadInShearPlane = m_thread->isChecked() || !in.grade.highStrength;
        in.shearPlanes = m_planes->value();
        in.nx = m_nx->value();
        in.ny = m_ny->value();
        in.sx = m_sx->value();
        in.sy = m_sy->value();
        in.plateThickness = m_t->value();
        in.plateFu = m_fu->value();
        in.endDistance = m_le->value();
        in.edgeDistance = m_lt->value();
        in.deformationLimit = m_deformation->isChecked();
        in.shearX = m_vx->value() * 1000.0;
        in.shearY = m_vy->value() * 1000.0;
        in.momentZ = m_mz->value() * 1e6;
        in.tension = m_tension->value() * 1000.0;
        checks = C::checkBolted(in);
    } else {
        C::WeldInput in;
        in.leg = m_leg->value();
        in.length = m_length->value();
        in.fw = m_electrode->currentData().toDouble();
        in.angleDeg = m_angle->value();
        in.plateThickness = m_wt->value();
        in.plateFy = m_wfy->value();
        in.plateFu = m_wfu->value();
        in.force = m_force->value() * 1000.0;
        checks = C::checkFilletWeld(in);
    }
    showChecks(checks);
}

void ConnectionDialog::showChecks(const std::vector<C::Check> &checks)
{
    m_table->setRowCount(static_cast<int>(checks.size()));
    double worst = 0.0;
    QStringList memorial;
    for (std::size_t i = 0; i < checks.size(); ++i) {
        const auto &c = checks[i];
        const int row = static_cast<int>(i);
        const QString unit = QString::fromStdString(c.unit);
        const QStringList cells {QString::fromStdString(c.name), QString::fromStdString(c.clause),
                                 QStringLiteral("%1 %2").arg(c.minimumRule ? c.demand : c.demand, 0, 'f', 2).arg(unit),
                                 QStringLiteral("%1 %2").arg(c.resistance, 0, 'f', 2).arg(unit),
                                 QString::number(c.ratio(), 'f', 3)};
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = new QTableWidgetItem(cells[col]);
            if (col == 4) {
                item->setForeground(c.ok() ? QColor(QStringLiteral("#1e8449")) : QColor(QStringLiteral("#c0392b")));
                item->setFont([&]() { QFont f; f.setBold(true); return f; }());
            }
            if (col == 0) {
                QStringList tip;
                for (const auto &l : c.lines) {
                    tip << QString::fromStdString(l);
                }
                item->setToolTip(tip.join(QLatin1Char('\n')));
            }
            m_table->setItem(row, col, item);
        }
        worst = std::max(worst, c.ratio());
    }
    const bool ok = worst <= 1.0 + 1e-9;
    m_verdict->setStyleSheet(ok ? QStringLiteral("color:#1e8449; font-weight:600;") : QStringLiteral("color:#c0392b; font-weight:600;"));
    m_verdict->setText(ok ? tr("A ligação atende às verificações listadas (razão máxima %1).").arg(worst, 0, 'f', 3)
                          : tr("A ligação NÃO atende (razão máxima %1).").arg(worst, 0, 'f', 3));
}

} // namespace Structura::UI
