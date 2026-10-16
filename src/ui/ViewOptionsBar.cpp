#include "ViewOptionsBar.h"

#include "SvgIcons.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QStringList>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

const QString ViewOptionsBar::kGrid = QStringLiteral("grid");
const QString ViewOptionsBar::kReferenceAxes = QStringLiteral("reference_axes");
const QString ViewOptionsBar::kNodeLabels = QStringLiteral("node_labels");
const QString ViewOptionsBar::kBarLabels = QStringLiteral("bar_labels");
const QString ViewOptionsBar::kLocalAxes = QStringLiteral("local_axes");
const QString ViewOptionsBar::kNodes = QStringLiteral("nodes");
const QString ViewOptionsBar::kSupports = QStringLiteral("supports");
const QString ViewOptionsBar::kSolids = QStringLiteral("solids");
const QString ViewOptionsBar::kAllLevels = QStringLiteral("all_levels");
const QString ViewOptionsBar::kIsolate = QStringLiteral("isolate");
const QString ViewOptionsBar::kPlaneOnly = QStringLiteral("planeonly");
const QString ViewOptionsBar::kNodeForces = QStringLiteral("node_forces");
const QString ViewOptionsBar::kNodeMoments = QStringLiteral("node_moments");
const QString ViewOptionsBar::kBarForces = QStringLiteral("bar_forces");
const QString ViewOptionsBar::kBarMoments = QStringLiteral("bar_moments");
const QString ViewOptionsBar::kAreaPanels = QStringLiteral("area_panels");
const QString ViewOptionsBar::kSnap = QStringLiteral("snap");
const QString ViewOptionsBar::kSnapGrid = QStringLiteral("snap_grid");
const QString ViewOptionsBar::kSnapEndpoint = QStringLiteral("snap_endpoint");
const QString ViewOptionsBar::kSnapCenter = QStringLiteral("snap_center");
const QString ViewOptionsBar::kSnapPerpendicular = QStringLiteral("snap_perpendicular");
const QString ViewOptionsBar::kSnapOrthogonal = QStringLiteral("snap_orthogonal");

namespace {

const QString kButtonStyle = QStringLiteral(
    "QToolButton { border: 0; border-radius: 6px; background: transparent; padding: 2px; }"
    "QToolButton:hover { background: rgba(19, 147, 214, 0.20); }"
    "QToolButton:checked { background: rgba(19, 147, 214, 0.30); }"
    "QToolButton:pressed { background: rgba(13, 127, 185, 0.42); }"
    "QToolButton::menu-button { border: 0; width: 10px; }");

constexpr int kButtonSize = 26;
constexpr int kIconSize = 19;

} // namespace

ViewOptionsBar::ViewOptionsBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *label = new QLabel(tr("Exibicao:"), this);
    label->setStyleSheet(QStringLiteral("font-weight: 600; color: #1e232b;"));
    layout->addWidget(label);
    layout->addSpacing(4);

    addToggle(kGrid, QStringLiteral("grid.svg"), tr("Plano (grid)"));
    addToggle(kReferenceAxes, QStringLiteral("axes-ref.svg"), tr("Eixos de referencia"));
    addToggle(kNodeLabels, QStringLiteral("node-id.svg"), tr("Identificadores dos nos"), false);
    addToggle(kBarLabels, QStringLiteral("bar-id.svg"), tr("Identificadores das barras"), false);
    addToggle(kLocalAxes, QStringLiteral("local-axes.svg"), tr("Eixos locais das barras"), false);
    addToggle(kNodes, QStringLiteral("nodes.svg"), tr("Nos"));
    addToggle(kSolids, QStringLiteral("solids.svg"), tr("Secoes extrudadas (solido)"), false);
    addToggle(kSupports, QStringLiteral("supports.svg"), tr("Apoios dos nos"));
    addSeparator();
    addToggle(kNodeForces, QStringLiteral("node-force.svg"), tr("Forcas nodais"));
    addToggle(kNodeMoments, QStringLiteral("node-moment.svg"), tr("Momentos nodais"));
    addToggle(kBarForces, QStringLiteral("bar-force.svg"), tr("Cargas distribuidas nas barras"));
    addToggle(kBarMoments, QStringLiteral("bar-moment.svg"), tr("Momentos distribuidos nas barras"));
    addToggle(kAreaPanels, QStringLiteral("area-panel.svg"), tr("Panos das cargas de area e vento (e sua acao nas barras)"));
    addSeparator();

    // Snap: botao principal + menu com os tipos individuais.
    QToolButton *snap = addToggle(kSnap, QStringLiteral("snap.svg"), tr("Snap (magnetismo)"));
    auto *snapMenu = new QMenu(snap);
    const struct { const QString *key; QString text; } types[] = {
        {&kSnapGrid, tr("Malha / eixos")},
        {&kSnapEndpoint, tr("Extremidade de no")},
        {&kSnapCenter, tr("Centro de barra")},
        {&kSnapPerpendicular, tr("Perpendicular ao plano")},
        {&kSnapOrthogonal, tr("Ortogonal no plano")},
    };
    for (const auto &type : types) {
        QAction *action = snapMenu->addAction(type.text);
        action->setCheckable(true);
        action->setChecked(true);
        m_snapActions.insert(*type.key, action);
        const QString key = *type.key;
        connect(action, &QAction::toggled, this, [this, key](bool checked) { emit snapTypeToggled(key, checked); });
    }
    snap->setMenu(snapMenu);
    snap->setPopupMode(QToolButton::MenuButtonPopup);
    snap->setFixedWidth(kButtonSize + 12);
    addSeparator();

    // Plano de construcao: anterior / alternar plano / proximo.
    QToolButton *previous = makeButton(QStringLiteral("chevron-left.svg"), tr("Plano anterior (nivel)"));
    connect(previous, &QToolButton::clicked, this, &ViewOptionsBar::previousPlaneRequested);
    layout->addWidget(previous);
    m_planeButton = makeButton(QStringLiteral("plane-xy.svg"), tr("Alterar plano de construcao"));
    connect(m_planeButton, &QToolButton::clicked, this, &ViewOptionsBar::cyclePlaneRequested);
    layout->addWidget(m_planeButton);
    m_levelCombo = new QComboBox(this);
    m_levelCombo->setMinimumWidth(104);
    m_levelCombo->setMaxVisibleItems(16);
    m_levelCombo->setToolTip(tr("Nivel (pavimento) ativo: escolha para saltar direto a ele"));
    layout->addWidget(m_levelCombo);
    connect(m_levelCombo, &QComboBox::activated, this, [this](int index) {
        if (index >= 0) {
            emit levelSelected(m_levelCombo->itemData(index).toDouble());
        }
    });
    QToolButton *next = makeButton(QStringLiteral("chevron-right.svg"), tr("Proximo plano (nivel)"));
    connect(next, &QToolButton::clicked, this, &ViewOptionsBar::nextPlaneRequested);
    layout->addWidget(next);
    addToggle(kAllLevels, QStringLiteral("levels-all.svg"), tr("Mostrar todos os niveis ao mesmo tempo (plano XY)"), false);
    QToolButton *planView = makeButton(QStringLiteral("plan-view.svg"),
                                       tr("Vista em planta: camera perpendicular ao plano ativo"));
    connect(planView, &QToolButton::clicked, this, &ViewOptionsBar::planViewRequested);
    layout->addWidget(planView);
    addToggle(kPlaneOnly, QStringLiteral("plane-only.svg"),
              tr("Mostrar somente os elementos do plano ativo (oculta o resto)"), false);
    addSeparator();

    auto *spacingLabel = new QLabel(tr("Malha:"), this);
    spacingLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #1e232b;"));
    layout->addWidget(spacingLabel);
    m_spacingBox = new QDoubleSpinBox(this);
    m_spacingBox->setRange(0.05, 100.0);
    m_spacingBox->setDecimals(2);
    m_spacingBox->setSingleStep(0.25);
    m_spacingBox->setValue(1.0);
    m_spacingBox->setSuffix(QStringLiteral(" m"));
    m_spacingBox->setKeyboardTracking(false);
    m_spacingBox->setFixedWidth(78);
    m_spacingBox->setToolTip(tr("Tamanho da malha do grid (e do snap de malha)"));
    layout->addWidget(m_spacingBox);
    connect(m_spacingBox, &QDoubleSpinBox::valueChanged, this, &ViewOptionsBar::gridSpacingChanged);
    addSeparator();

    // Selecao: filtros e acoes rapidas.
    m_selectionButton = makeButton(QStringLiteral("select.svg"), tr("Selecao: filtros e acoes"));
    m_selectionButton->setCheckable(false);
    auto *selectionMenu = new QMenu(m_selectionButton);
    selectionMenu->addAction(tr("Filtros de selecao..."), this, &ViewOptionsBar::selectionFilterRequested);
    selectionMenu->addAction(tr("Selecionar tudo que atende ao filtro"), this, &ViewOptionsBar::selectMatchingRequested);
    selectionMenu->addAction(tr("Inverter selecao"), this, &ViewOptionsBar::invertSelectionRequested);
    selectionMenu->addAction(tr("Limpar selecao"), this, &ViewOptionsBar::clearSelectionRequested);
    selectionMenu->addSeparator();
    selectionMenu->addAction(tr("Cargas da selecao..."), this, &ViewOptionsBar::loadsPanelRequested);
    m_selectionButton->setMenu(selectionMenu);
    m_selectionButton->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(m_selectionButton);

    // Isolar nivel (esmaecer ou ocultar os demais).
    addToggle(kIsolate, QStringLiteral("isolate.svg"),
              tr("Esmaecer os outros niveis (destaca o nivel ativo)"), false);

    // Cores por secao / material.
    QToolButton *colors = makeButton(QStringLiteral("palette.svg"), tr("Colorir barras por secao ou material"));
    auto *colorMenu = new QMenu(colors);
    auto *colorGroup = new QActionGroup(colorMenu);
    const QString colorTexts[4] = {tr("Sem cores"), tr("Por secao"), tr("Por material"), tr("Por aproveitamento (dimensionamento)")};
    for (int i = 0; i < 4; ++i) {
        QAction *action = colorMenu->addAction(colorTexts[i]);
        m_colorActions.append(action);
        action->setCheckable(true);
        action->setChecked(i == 0);
        colorGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, i]() {
            m_colorMode = i;
            emit colorModeChanged(i);
        });
    }
    colors->setMenu(colorMenu);
    colors->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(colors);

    layout->addStretch(1);

    auto *caseLabel = new QLabel(tr("Caso:"), this);
    caseLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #1e232b;"));
    layout->addWidget(caseLabel);
    m_caseCombo = new QComboBox(this);
    m_caseCombo->setMinimumWidth(110);
    m_caseCombo->setToolTip(tr("Caso de carga exibido no viewport e usado ao lancar cargas"));
    layout->addWidget(m_caseCombo);
    connect(m_caseCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        emit activeCaseChanged(activeCase());
    });

    auto *resultLabel = new QLabel(tr("Resultado:"), this);
    resultLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: #1e232b;"));
    layout->addWidget(resultLabel);
    m_resultCombo = new QComboBox(this);
    m_resultCombo->setMinimumWidth(120);
    m_resultCombo->setEnabled(false);
    m_resultCombo->setToolTip(tr("Combinacao ou caso usado nos diagramas de esforcos"));
    layout->addWidget(m_resultCombo);
    connect(m_resultCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) {
            emit resultViewChanged(index);
        }
    });
}

void ViewOptionsBar::setLevels(const QStringList &texts, const QVector<double> &values, double current)
{
    const QSignalBlocker blocker(m_levelCombo);
    m_levelCombo->clear();
    int selected = 0;
    double best = 1e300;
    for (int i = 0; i < texts.size() && i < values.size(); ++i) {
        m_levelCombo->addItem(texts.at(i), values.at(i));
        if (std::abs(values.at(i) - current) < best) {
            best = std::abs(values.at(i) - current);
            selected = i;
        }
    }
    m_levelCombo->setCurrentIndex(m_levelCombo->count() > 0 ? selected : -1);
}

void ViewOptionsBar::setSelectionFilterActive(bool active)
{
    m_selectionButton->setStyleSheet(active ? kButtonStyle + QStringLiteral("QToolButton { background: rgba(242, 153, 46, 0.35); }")
                                            : kButtonStyle);
    m_selectionButton->setToolTip(active ? tr("Selecao: filtro ATIVO") : tr("Selecao: filtros e acoes"));
}

void ViewOptionsBar::setCases(const QStringList &names, const QString &active)
{
    const QSignalBlocker blocker(m_caseCombo);
    m_caseCombo->clear();
    m_caseCombo->addItem(tr("(todos)"), QString());
    for (const QString &name : names) {
        m_caseCombo->addItem(name, name);
    }
    const int index = m_caseCombo->findData(active);
    m_caseCombo->setCurrentIndex(index >= 0 ? index : 0);
}

QString ViewOptionsBar::activeCase() const
{
    return m_caseCombo->currentData().toString();
}

void ViewOptionsBar::setResultViews(const QStringList &names, int current)
{
    const QSignalBlocker blocker(m_resultCombo);
    m_resultCombo->clear();
    m_resultCombo->addItems(names);
    m_resultCombo->setEnabled(!names.isEmpty());
    m_resultCombo->setCurrentIndex(names.isEmpty() ? -1 : std::clamp(current, 0, static_cast<int>(names.size()) - 1));
}

int ViewOptionsBar::currentResultView() const
{
    return m_resultCombo->currentIndex();
}

QToolButton *ViewOptionsBar::makeButton(const QString &icon, const QString &tooltip)
{
    auto *button = new QToolButton(this);
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setIcon(svgIcon(icon, QColor(QStringLiteral("#1e232b")), kIconSize));
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setStyleSheet(kButtonStyle);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QToolButton *ViewOptionsBar::addToggle(const QString &key, const QString &icon, const QString &tooltip, bool checked)
{
    QToolButton *button = makeButton(icon, tooltip);
    button->setCheckable(true);
    button->setChecked(checked);
    connect(button, &QToolButton::toggled, this, [this, key](bool state) { emit toggled(key, state); });
    layout()->addWidget(button);
    m_toggles.insert(key, button);
    return button;
}

void ViewOptionsBar::addSeparator()
{
    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setFixedHeight(18);
    separator->setStyleSheet(QStringLiteral("color: #cbd4e2;"));
    layout()->addWidget(separator);
}

bool ViewOptionsBar::isChecked(const QString &key) const
{
    const auto *button = m_toggles.value(key, nullptr);
    return button && button->isChecked();
}

void ViewOptionsBar::setChecked(const QString &key, bool checked)
{
    if (auto *button = m_toggles.value(key, nullptr)) {
        const QSignalBlocker blocker(button);
        button->setChecked(checked);
    }
}

bool ViewOptionsBar::isSnapTypeChecked(const QString &key) const
{
    const auto *action = m_snapActions.value(key, nullptr);
    return action && action->isChecked();
}

void ViewOptionsBar::setSnapTypeChecked(const QString &key, bool checked)
{
    if (auto *action = m_snapActions.value(key, nullptr)) {
        const QSignalBlocker blocker(action);
        action->setChecked(checked);
    }
}

double ViewOptionsBar::gridSpacing() const
{
    return m_spacingBox->value();
}

void ViewOptionsBar::setGridSpacing(double spacing)
{
    const QSignalBlocker blocker(m_spacingBox);
    m_spacingBox->setValue(spacing);
}

void ViewOptionsBar::setPlaneName(const QString &name)
{
    const QString icon = name == QLatin1String("XZ") ? QStringLiteral("plane-xz.svg")
                       : name == QLatin1String("YZ") ? QStringLiteral("plane-yz.svg")
                                                     : QStringLiteral("plane-xy.svg");
    m_planeButton->setIcon(svgIcon(icon, QColor(QStringLiteral("#1e232b")), kIconSize));
    m_planeButton->setToolTip(tr("Plano de construcao: %1 (clique para alternar)").arg(name));
}


void ViewOptionsBar::setColorMode(int mode)
{
    if (mode < 0 || mode >= m_colorActions.size()) {
        return;
    }
    m_colorMode = mode;
    m_colorActions[mode]->setChecked(true);
}

} // namespace Structura::UI
