#include "DesignDialogs.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::ProjectData::DesignMemberParams;
using Structura::App::ProjectData::DesignSettings;
using Structura::App::Services::DesignService;

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

// ---------------------------------------------------------------------------------------------------------------------
// DesignSettingsDialog
// ---------------------------------------------------------------------------------------------------------------------

QWidget *DesignSettingsDialog::buildParamForm(ParamEditors &e, bool allowInheritSteel)
{
    auto *widget = new QWidget;
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(0, 0, 0, 0);
    e.steel = new QComboBox;
    if (allowInheritSteel) {
        e.steel->addItem(tr("(padrão do projeto)"), QString());
    }
    for (const auto &steel : DesignService::steelGrades()) {
        e.steel->addItem(QStringLiteral("%1  (fy %2 / fu %3 MPa)").arg(steel.name).arg(steel.fy).arg(steel.fu), steel.name);
    }
    e.kx = spin(0.05, 10.0, 1.0, 2, 0.1);
    e.ky = spin(0.05, 10.0, 1.0, 2, 0.1);
    e.kz = spin(0.05, 10.0, 1.0, 2, 0.1);
    for (QDoubleSpinBox **box : {&e.lx, &e.ly, &e.lz}) {
        *box = spin(0.0, 1000.0, 0.0, 3, 0.5, tr(" m"));
        (*box)->setSpecialValueText(tr("usar K · L"));
        (*box)->setToolTip(tr("Comprimento de flambagem informado (substitui K · L). Na NBR 8800:2024 é o comprimento destravado; "
                              "na NBR 14762 é o comprimento efetivo KL."));
    }
    e.segments = new QSpinBox;
    e.segments->setRange(1, 50);
    e.lb = spin(0.0, 1000.0, 0.0, 3, 0.5, tr(" m"));
    e.lb->setSpecialValueText(tr("L / trechos"));
    e.braced = new QCheckBox(tr("Mesa comprimida contida continuamente (sem flambagem lateral)"));
    e.cbOne = new QCheckBox(tr("Adotar Cb = 1,0 (ex.: balanço)"));
    e.b2 = spin(1.0, 5.0, 1.0, 3, 0.05);
    e.netArea = spin(0.1, 1.0, 1.0, 3, 0.05);
    e.defl = new QSpinBox;
    e.defl->setRange(0, 2000);
    e.defl->setSpecialValueText(tr("automático"));
    e.defl->setPrefix(QStringLiteral("L / "));
    form->addRow(tr("Aço"), e.steel);
    form->addRow(tr("Kx (flexão em torno de X, plano da altura)"), e.kx);
    form->addRow(tr("Ky (flexão em torno de Y)"), e.ky);
    form->addRow(tr("Kz (torção)"), e.kz);
    form->addRow(tr("Lx informado (substitui Kx · L)"), e.lx);
    form->addRow(tr("Ly informado (substitui Ky · L)"), e.ly);
    form->addRow(tr("Lz informado (substitui Kz · L)"), e.lz);
    auto *note = new QLabel(tr("A NBR 8800:2024 não usa K: o esforço vem de análise de 2ª ordem com imperfeições e Ne usa o comprimento destravado "
                               "(use Lx/Ly/Lz para travamentos intermediários, K = 1). A NBR 14762 usa o comprimento efetivo K·L (K ou comprimento informado)."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    form->addRow(note);
    form->addRow(tr("Trechos destravados (mesa comprimida)"), e.segments);
    form->addRow(tr("Lb informado"), e.lb);
    form->addRow(QString(), e.braced);
    form->addRow(QString(), e.cbOne);
    form->addRow(tr("Amplificação global B2"), e.b2);
    form->addRow(tr("An / Ag na ligação (tração)"), e.netArea);
    form->addRow(tr("Limite de flecha (ELS)"), e.defl);
    return widget;
}

void DesignSettingsDialog::load(ParamEditors &e, const DesignMemberParams &p)
{
    const int idx = e.steel->findData(p.steel);
    e.steel->setCurrentIndex(idx >= 0 ? idx : 0);
    e.kx->setValue(p.kx);
    e.ky->setValue(p.ky);
    e.kz->setValue(p.kz);
    e.lx->setValue(p.lx);
    e.ly->setValue(p.ly);
    e.lz->setValue(p.lz);
    e.segments->setValue(p.segments);
    e.lb->setValue(p.lb);
    e.braced->setChecked(p.braced);
    e.cbOne->setChecked(p.cbOne);
    e.b2->setValue(p.b2);
    e.netArea->setValue(p.netAreaRatio);
    e.defl->setValue(p.deflDenominator);
}

DesignMemberParams DesignSettingsDialog::read(const ParamEditors &e)
{
    DesignMemberParams p;
    p.steel = e.steel->currentData().toString();
    p.kx = e.kx->value();
    p.ky = e.ky->value();
    p.kz = e.kz->value();
    p.lx = e.lx->value();
    p.ly = e.ly->value();
    p.lz = e.lz->value();
    p.segments = e.segments->value();
    p.lb = e.lb->value();
    p.braced = e.braced->isChecked();
    p.cbOne = e.cbOne->isChecked();
    p.b2 = e.b2->value();
    p.netAreaRatio = e.netArea->value();
    p.deflDenominator = e.defl->value();
    return p;
}

DesignSettingsDialog::DesignSettingsDialog(const DesignSettings &settings, const QStringList &combinationNames,
                                           const QVector<int> &selectedMemberIds, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Parâmetros de dimensionamento"));
    setMinimumWidth(600);
    resize(660, 720);
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    auto *root = new QVBoxLayout(content);

    auto *global = new QGroupBox(tr("Projeto"));
    auto *globalForm = new QFormLayout(global);
    m_defaultSteel = new QComboBox;
    for (const auto &steel : DesignService::steelGrades()) {
        m_defaultSteel->addItem(QStringLiteral("%1  (fy %2 / fu %3 MPa)").arg(steel.name).arg(steel.fy).arg(steel.fu), steel.name);
    }
    m_defaultSteel->setCurrentIndex(std::max(0, m_defaultSteel->findData(settings.defaultSteel)));
    m_tubeStandard = new QComboBox;
    m_tubeStandard->addItem(tr("NBR 14762 (formados a frio)"), QStringLiteral("NBR14762"));
    m_tubeStandard->addItem(tr("NBR 8800 (laminados / tubos pesados)"), QStringLiteral("NBR8800"));
    m_tubeStandard->setCurrentIndex(std::max(0, m_tubeStandard->findData(settings.rectTubeStandard)));
    m_amplify = new QCheckBox(tr("Amplificar momentos pelo efeito P-δ local (B1, Anexo C da NBR 8800)"));
    m_amplify->setChecked(settings.amplifyB1);
    m_slenderness = new QCheckBox(tr("Considerar o limite de esbeltez no aproveitamento"));
    m_slenderness->setChecked(settings.slendernessGoverns);
    globalForm->addRow(tr("Aço padrão"), m_defaultSteel);
    globalForm->addRow(tr("Norma dos tubulares retangulares"), m_tubeStandard);
    globalForm->addRow(QString(), m_amplify);
    globalForm->addRow(QString(), m_slenderness);
    root->addWidget(global);

    auto *analysis = new QGroupBox(tr("Efeitos de segunda ordem e serviço"));
    auto *analysisForm = new QFormLayout(analysis);
    m_secondOrder = new QCheckBox(tr("Análise de 2ª ordem (P-Δ e P-δ) nas combinações — dispensa B1/B2"));
    m_secondOrder->setChecked(settings.secondOrder);
    m_secondOrder->setToolTip(tr("Cada combinação é resolvida com rigidez geométrica (não há superposição). Requer refazer a análise."));
    m_reduction = spin(0.5, 1.0, settings.stiffnessReduction, 2, 0.05);
    m_reduction->setToolTip(tr("Redução de EA e EI nas combinações últimas (NBR 8800, 4.10.5: 0,8)"));
    m_autoB2 = new QCheckBox(tr("Calcular B2 por andar (Anexo C) na análise de 1ª ordem"));
    m_autoB2->setChecked(settings.autoB2);
    m_rs = spin(0.5, 1.0, settings.rs, 2, 0.05);
    m_rs->setToolTip(tr("Rs = 0,85 em pórticos; 1,0 nos demais"));
    m_singleStorey = new QCheckBox(tr("Galpão / edificação de um pavimento (limite de topo H/300; senão H/400)"));
    m_singleStorey->setChecked(settings.singleStorey);
    analysisForm->addRow(m_secondOrder);
    analysisForm->addRow(tr("Fator de redução de rigidez (2ª ordem)"), m_reduction);
    analysisForm->addRow(m_autoB2);
    analysisForm->addRow(tr("Rs (Anexo C)"), m_rs);
    analysisForm->addRow(m_singleStorey);
    m_notional = new QCheckBox(tr("Forças nocionais (NBR 8800, 4.9.7.2): 0,3% das cargas gravitacionais, em X e Y"));
    m_notional->setChecked(settings.notionalLoads);
    m_notional->setToolTip(tr("Cada combinação última gera quatro variantes (±X, ±Y) com forças horizontais nodais proporcionais às cargas verticais "
                              "da combinação, para considerar imperfeições geométricas. Requer refazer a análise."));
    m_notionalFactor = spin(0.0005, 0.02, settings.notionalFactor, 4, 0.0005);
    analysisForm->addRow(m_notional);
    analysisForm->addRow(tr("Fator das forças nocionais"), m_notionalFactor);
    root->addWidget(analysis);

    auto *automatic = new QGroupBox(tr("Dimensionamento automático"));
    auto *autoForm = new QFormLayout(automatic);
    m_target = spin(0.3, 1.0, settings.targetEta, 2, 0.05);
    m_target->setToolTip(tr("Aproveitamento máximo aceito ao escolher o perfil (η alvo)"));
    m_groupByFamily = new QCheckBox(tr("Agrupar por família: barras da mesma família recebem o mesmo perfil (o mais solicitado define)"));
    m_groupByFamily->setChecked(settings.groupByFamily);
    autoForm->addRow(tr("η alvo"), m_target);
    autoForm->addRow(m_groupByFamily);
    root->addWidget(automatic);

    auto *comboGroup = new QGroupBox(tr("Combinações últimas verificadas"));
    auto *comboLayout = new QVBoxLayout(comboGroup);
    m_combinations = new QListWidget;
    m_combinations->setMaximumHeight(110);
    for (const QString &name : combinationNames) {
        auto *item = new QListWidgetItem(name, m_combinations);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool checked = settings.combinations.isEmpty() ? !name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive)
                                                             : settings.combinations.contains(name);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
    comboLayout->addWidget(m_combinations);
    comboLayout->addWidget(new QLabel(tr("Por padrão são verificadas as combinações que não começam por \"ELS\".")));
    root->addWidget(comboGroup);

    auto *defaults = new QGroupBox(tr("Parâmetros padrão das barras"));
    auto *defaultsLayout = new QVBoxLayout(defaults);
    defaultsLayout->addWidget(buildParamForm(m_defaults, false));
    load(m_defaults, settings.defaults);
    m_defaults.steel->setEnabled(false);
    m_defaults.steel->setVisible(false);
    root->addWidget(defaults);

    m_selectionGroup = new QGroupBox(tr("Barras selecionadas (%1): parâmetros próprios").arg(selectedMemberIds.size()));
    m_selectionGroup->setCheckable(true);
    m_selectionGroup->setChecked(false);
    auto *selLayout = new QVBoxLayout(m_selectionGroup);
    selLayout->addWidget(buildParamForm(m_selected, true));
    DesignMemberParams start = selectedMemberIds.isEmpty() ? settings.defaults : settings.paramsFor(selectedMemberIds.first());
    load(m_selected, start);
    root->addWidget(m_selectionGroup);
    m_selectionGroup->setVisible(!selectedMemberIds.isEmpty());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

DesignSettings DesignSettingsDialog::settings() const
{
    DesignSettings s = m_settings;
    s.defaultSteel = m_defaultSteel->currentData().toString();
    s.rectTubeStandard = m_tubeStandard->currentData().toString();
    s.amplifyB1 = m_amplify->isChecked();
    s.slendernessGoverns = m_slenderness->isChecked();
    s.secondOrder = m_secondOrder->isChecked();
    s.stiffnessReduction = m_reduction->value();
    s.autoB2 = m_autoB2->isChecked();
    s.rs = m_rs->value();
    s.singleStorey = m_singleStorey->isChecked();
    s.notionalLoads = m_notional->isChecked();
    s.notionalFactor = m_notionalFactor->value();
    s.groupByFamily = m_groupByFamily->isChecked();
    s.targetEta = m_target->value();
    s.defaults = read(m_defaults);
    s.defaults.steel.clear();
    QStringList checked;
    QStringList all;
    for (int i = 0; i < m_combinations->count(); ++i) {
        const auto *item = m_combinations->item(i);
        all.append(item->text());
        if (item->checkState() == Qt::Checked) {
            checked.append(item->text());
        }
    }
    s.combinations = checked == DesignService::defaultUltimateCombinations(all) ? QStringList() : checked;
    return s;
}

bool DesignSettingsDialog::applyToSelection() const
{
    return m_selectionGroup && m_selectionGroup->isVisible() && m_selectionGroup->isChecked();
}

DesignMemberParams DesignSettingsDialog::selectionParams() const
{
    return read(m_selected);
}

} // namespace Structura::UI
