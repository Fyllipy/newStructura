#include "AreaLoadDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::AreaLoadService;

namespace {

QDoubleSpinBox *spin(double min, double max, double value, int decimals, double step, const QString &suffix = {})
{
    auto *box = new QDoubleSpinBox;
    box->setRange(min, max);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setValue(value);
    if (!suffix.isEmpty()) {
        box->setSuffix(suffix);
    }
    return box;
}

} // namespace

AreaLoadDialog::AreaLoadDialog(const QStringList &caseNames, const QString &defaultCase, int selectedBars, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Carga de área"));
    setMinimumWidth(560);
    resize(600, 520);

    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    auto *root = new QVBoxLayout(content);

    auto *intro = new QLabel(tr("Distribui uma pressão (kN/m²) nas barras selecionadas, usando a largura de influência de cada uma "
                                "(metade da distância às barras paralelas vizinhas). Selecione apenas as barras que recebem a carga."));
    intro->setWordWrap(true);
    root->addWidget(intro);
    auto *selection = new QLabel(tr("Barras selecionadas: %1").arg(selectedBars));
    selection->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(selection);

    {
        auto *load = new QGroupBox(tr("Pressão"));
        auto *form = new QFormLayout(load);
        m_pressure = spin(-1e4, 1e4, 1.0, 3, 0.1, tr(" kN/m²"));
        m_direction = new QComboBox;
        m_direction->addItem(tr("−Z (gravidade)"), 0);
        m_direction->addItem(tr("+Z"), 1);
        m_direction->addItem(tr("+X"), 2);
        m_direction->addItem(tr("−X"), 3);
        m_direction->addItem(tr("+Y"), 4);
        m_direction->addItem(tr("−Y"), 5);
        m_direction->addItem(tr("Normal à superfície, para dentro da estrutura"), 6);
        m_direction->addItem(tr("Normal à superfície, para fora da estrutura"), 7);
        m_projected = new QCheckBox(tr("Pressão sobre a projeção ortogonal à direção (ex.: sobrecarga em telhado inclinado)"));
        form->addRow(tr("Pressão"), m_pressure);
        form->addRow(tr("Direção da força"), m_direction);
        form->addRow(m_projected);
        root->addWidget(load);
    }

    auto *distribution = new QGroupBox(tr("Distribuição nas barras"));
    auto *dform = new QFormLayout(distribution);
    m_manualWidth = spin(0.0, 1000.0, 0.0, 3, 0.25, tr(" m"));
    m_manualWidth->setSpecialValueText(tr("automática"));
    m_manualWidth->setToolTip(tr("Largura de influência fixa para todas as barras; automática usa a distância entre barras paralelas"));
    m_overhang = spin(0.0, 100.0, 0.0, 3, 0.25, tr(" m"));
    m_overhang->setToolTip(tr("Prolongamento da superfície além das barras das extremidades (beiral)"));
    m_case = new QComboBox;
    m_case->setEditable(true);
    m_case->addItems(caseNames);
    m_case->setCurrentIndex(std::max(0, m_case->findText(defaultCase)));
    m_case->setToolTip(tr("Caso de carga; digite um nome novo para criar o caso"));
    dform->addRow(tr("Largura de influência"), m_manualWidth);
    dform->addRow(tr("Prolongamento nas extremidades"), m_overhang);
    dform->addRow(tr("Caso de carga"), m_case);
    root->addWidget(distribution);
    root->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

}

AreaLoadRequest AreaLoadDialog::request() const
{
    AreaLoadRequest r;
    r.caseName = m_case->currentText().trimmed();
    r.manualWidth = m_manualWidth->value();
    r.overhang = m_overhang->value();
    r.pressure = m_pressure->value();
    r.projected = m_projected->isChecked();
    switch (m_direction->currentData().toInt()) {
    case 0: r.direction = {0, 0, -1}; break;
    case 1: r.direction = {0, 0, 1}; break;
    case 2: r.direction = {1, 0, 0}; break;
    case 3: r.direction = {-1, 0, 0}; break;
    case 4: r.direction = {0, 1, 0}; break;
    case 5: r.direction = {0, -1, 0}; break;
    case 6: r.directionKind = 2; break;
    default: r.directionKind = 1; break;
    }
    r.description = tr("pressão %1 kN/m²").arg(r.pressure, 0, 'f', 3);
    return r;
}

} // namespace Structura::UI
