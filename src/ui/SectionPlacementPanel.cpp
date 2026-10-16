#include "SectionPlacementPanel.h"

#include "SectionPlacementWidget.h"
#include "SvgIcons.h"

#include "app/services/SectionPlacementService.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

using Structura::App::ProjectData::MemberPlacementPatch;
using Structura::App::Services::SectionPlacementService;
namespace Release = Structura::App::ProjectData::MemberRelease;

namespace {

constexpr double kToMm = 1000.0;
constexpr double kToCm4 = 1e8;

// Linhas da grade 3x3 de pontos de referencia (de cima para baixo), ver SectionReference.
constexpr int kReferenceLayout[3][3] = {{3, 7, 4}, {8, 0, 6}, {1, 5, 2}};

QString referenceTip(int reference)
{
    const QStringList labels = SectionPlacementService::referenceLabels();
    return labels.value(reference);
}

QDoubleSpinBox *makeSpin(QWidget *parent, double min, double max, double step, int decimals, const QString &suffix)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    spin->setAccelerated(true);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    return spin;
}

QFrame *makeCard(QWidget *parent, const QString &title, QVBoxLayout **bodyOut)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("placementCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);
    auto *label = new QLabel(title, card);
    label->setObjectName(QStringLiteral("placementCardTitle"));
    layout->addWidget(label);
    *bodyOut = layout;
    return card;
}

} // namespace

SectionPlacementPanel::SectionPlacementPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("placementPanel"));
    setFixedWidth(372);
    setStyleSheet(QStringLiteral(
        "QFrame#placementPanel { background: #f2f5fa; border: 1px solid #cbd4e2; border-radius: 8px; }"
        "QFrame#placementHeader { background: #0d7fb9; border: 0; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
        "QLabel#placementTitle { color: #ffffff; font-size: 13px; font-weight: 700; background: transparent; }"
        "QLabel#placementTarget { color: #1e232b; font-weight: 600; background: transparent; }"
        "QLabel#placementHint, QLabel#placementMixed { color: #5a6574; font-size: 11px; background: transparent; }"
        "QLabel#placementMixed { color: #b3560c; }"
        "QFrame#placementCard { background: #ffffff; border: 1px solid #d6dde8; border-radius: 6px; }"
        "QLabel#placementCardTitle { color: #1e232b; font-weight: 700; background: transparent; }"
        "QLabel#fieldLabel { color: #5a6574; font-size: 11px; background: transparent; }"
        "QLabel#placementResult { color: #1e232b; font-size: 11px; background: transparent; }"
        "QCheckBox { background: transparent; color: #1e232b; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #9fb0c6; border-radius: 3px; background: #ffffff; }"
        "QCheckBox::indicator:hover { border-color: #0d7fb9; }"
        "QCheckBox::indicator:checked { background: #0d7fb9; border-color: #0b6a9a; }"
        "QDoubleSpinBox { background: #ffffff; border: 1px solid #cbd4e2; border-radius: 4px; padding: 3px 6px; color: #1e232b; }"
        "QDoubleSpinBox:focus { border: 1px solid #0d7fb9; background: #f4f9fd; }"
        "QToolButton#refButton { border: 1px solid #cbd4e2; border-radius: 4px; background: #ffffff; color: #5a6574; font-size: 13px; }"
        "QToolButton#refButton:hover { border-color: #0d7fb9; background: rgba(19, 147, 214, 0.12); }"
        "QToolButton#refButton:checked { background: #f2992e; border-color: #c97a14; color: #ffffff; }"
        "QToolButton#endTab { border: 1px solid #cbd4e2; background: #ffffff; padding: 3px 12px; color: #1e232b; }"
        "QToolButton#endTab:checked { background: #0d7fb9; border-color: #0d7fb9; color: #ffffff; }"
        "QPushButton#smallAction { background: transparent; color: #0d7fb9; border: 1px solid #0d7fb9; border-radius: 4px; padding: 2px 8px; font-weight: 600; }"
        "QPushButton#smallAction:hover { background: rgba(19, 147, 214, 0.14); }"
        "QSlider::groove:horizontal { height: 4px; background: #d6dde8; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: #0d7fb9; }"));
    buildUi();
    if (parent) {
        parent->installEventFilter(this);
    }
    setTargets({});
    hide();
}

void SectionPlacementPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 10);
    root->setSpacing(8);

    // --- cabecalho ---
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("placementHeader"));
    header->setFixedHeight(34);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 6, 0);
    auto *title = new QLabel(tr("Posicionamento da seção"), header);
    title->setObjectName(QStringLiteral("placementTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    auto *close = new QToolButton(header);
    close->setIcon(svgIcon(QStringLiteral("close.svg"), QColor(Qt::white), 14));
    close->setIconSize(QSize(14, 14));
    close->setFixedSize(24, 24);
    close->setToolTip(tr("Fechar"));
    close->setStyleSheet(QStringLiteral(
        "QToolButton { border: 0; border-radius: 4px; background: transparent; }"
        "QToolButton:hover { background: rgba(255, 255, 255, 0.25); }"));
    connect(close, &QToolButton::clicked, this, &QWidget::hide);
    headerLayout->addWidget(close);
    root->addWidget(header);

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; } QScrollArea > QWidget > QWidget { background: transparent; }"));
    m_content = new QWidget();
    m_content->setObjectName(QStringLiteral("placementContent"));
    m_content->setStyleSheet(QStringLiteral("QWidget#placementContent { background: transparent; }"));
    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    m_scroll->setWidget(m_content);
    root->addWidget(m_scroll, 1);

    m_emptyLabel = new QLabel(tr("Selecione uma ou mais barras para posicionar a seção em relação ao nó e definir giro e articulações."), m_content);
    m_emptyLabel->setObjectName(QStringLiteral("placementHint"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setContentsMargins(14, 8, 14, 8);
    contentLayout->addWidget(m_emptyLabel);

    m_body = new QWidget(m_content);
    auto *bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(10, 0, 10, 0);
    bodyLayout->setSpacing(8);
    contentLayout->addWidget(m_body);
    contentLayout->addStretch(1);

    m_targetLabel = new QLabel(m_body);
    m_targetLabel->setObjectName(QStringLiteral("placementTarget"));
    bodyLayout->addWidget(m_targetLabel);
    m_mixedLabel = new QLabel(tr("As barras têm valores diferentes: o painel mostra a primeira e cada alteração vale para todas."), m_body);
    m_mixedLabel->setObjectName(QStringLiteral("placementMixed"));
    m_mixedLabel->setWordWrap(true);
    bodyLayout->addWidget(m_mixedLabel);

    // --- desenho ---
    m_preview = new SectionPlacementWidget(m_body);
    m_preview->setFixedHeight(230);
    bodyLayout->addWidget(m_preview);
    connect(m_preview, &SectionPlacementWidget::referenceChanged, this, [this](int reference) {
        MemberPlacementPatch patch;
        patch.reference = reference;
        emitPatch(patch);
    });
    connect(m_preview, &SectionPlacementWidget::offsetChanged, this, [this](double a, double b) {
        MemberPlacementPatch patch;
        if (m_end == 1 && current().endDiffers) {
            patch.offsetA2 = a;
            patch.offsetB2 = b;
        } else {
            patch.offsetA = a;
            patch.offsetB = b;
        }
        emitPatch(patch);
    });

    // --- giro ---
    QVBoxLayout *rotationBody = nullptr;
    auto *rotationCard = makeCard(m_body, tr("Giro da seção"), &rotationBody);
    auto *rotationRow = new QHBoxLayout();
    rotationRow->setSpacing(6);
    m_rotationSlider = new QSlider(Qt::Horizontal, rotationCard);
    m_rotationSlider->setRange(-180, 180);
    m_rotationSlider->setPageStep(15);
    m_rotationSpin = makeSpin(rotationCard, -360.0, 360.0, 5.0, 2, QStringLiteral(" °"));
    m_rotationSpin->setFixedWidth(90);
    m_rotationSpin->setToolTip(tr("Positivo: sentido horário visto do nó I para o nó J (regra da mão direita em torno de x local).\n"
                                  "As inércias principais passam a atuar nos eixos girados."));
    rotationRow->addWidget(m_rotationSlider, 1);
    rotationRow->addWidget(m_rotationSpin);
    rotationBody->addLayout(rotationRow);
    auto *rotationButtons = new QHBoxLayout();
    rotationButtons->setSpacing(6);
    for (const auto &entry : {std::make_pair(QStringLiteral("-90°"), -90.0), std::make_pair(QStringLiteral("+90°"), 90.0),
                              std::make_pair(QStringLiteral("180°"), 180.0)}) {
        auto *button = new QPushButton(entry.first, rotationCard);
        button->setObjectName(QStringLiteral("smallAction"));
        button->setCursor(Qt::PointingHandCursor);
        const double delta = entry.second;
        connect(button, &QPushButton::clicked, this, [this, delta]() {
            double value = std::fmod(current().rotationDeg + delta, 360.0);
            if (value > 180.0) value -= 360.0;
            if (value <= -180.0) value += 360.0;
            MemberPlacementPatch patch;
            patch.rotationDeg = value;
            emitPatch(patch);
        });
        rotationButtons->addWidget(button);
    }
    auto *resetRotation = new QPushButton(tr("Zerar"), rotationCard);
    resetRotation->setObjectName(QStringLiteral("smallAction"));
    resetRotation->setCursor(Qt::PointingHandCursor);
    connect(resetRotation, &QPushButton::clicked, this, [this]() {
        MemberPlacementPatch patch;
        patch.rotationDeg = 0.0;
        emitPatch(patch);
    });
    rotationButtons->addStretch(1);
    rotationButtons->addWidget(resetRotation);
    rotationBody->addLayout(rotationButtons);
    bodyLayout->addWidget(rotationCard);
    connect(m_rotationSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_block) return;
        MemberPlacementPatch patch;
        patch.rotationDeg = static_cast<double>(value);
        emitPatch(patch);
    });
    connect(m_rotationSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        if (m_block) return;
        MemberPlacementPatch patch;
        patch.rotationDeg = value;
        emitPatch(patch);
    });

    // --- posicao: ponto de referencia + deslocamento ---
    QVBoxLayout *positionBody = nullptr;
    auto *positionCard = makeCard(m_body, tr("Ponto da seção sobre o nó"), &positionBody);
    auto *positionRow = new QHBoxLayout();
    positionRow->setSpacing(12);
    auto *grid = new QGridLayout();
    grid->setSpacing(3);
    m_referenceGroup = new QButtonGroup(this);
    m_referenceGroup->setExclusive(true);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int reference = kReferenceLayout[row][column];
            auto *button = new QToolButton(positionCard);
            button->setObjectName(QStringLiteral("refButton"));
            button->setCheckable(true);
            button->setText(reference == 0 ? QStringLiteral("+") : QStringLiteral("●"));
            button->setFixedSize(30, 26);
            button->setToolTip(referenceTip(reference));
            button->setCursor(Qt::PointingHandCursor);
            m_referenceGroup->addButton(button, reference);
            m_referenceButtons[static_cast<std::size_t>(reference)] = button;
            grid->addWidget(button, row, column);
        }
    }
    connect(m_referenceGroup, &QButtonGroup::idClicked, this, [this](int reference) {
        if (m_block) return;
        MemberPlacementPatch patch;
        patch.reference = reference;
        emitPatch(patch);
    });
    positionRow->addLayout(grid);

    auto *offsetForm = new QGridLayout();
    offsetForm->setHorizontalSpacing(6);
    offsetForm->setVerticalSpacing(4);
    auto *offsetTitle = new QLabel(tr("Deslocamento adicional"), positionCard);
    offsetTitle->setObjectName(QStringLiteral("fieldLabel"));
    offsetForm->addWidget(offsetTitle, 0, 0, 1, 2);
    auto *labelA = new QLabel(tr("Largura (z)"), positionCard);
    labelA->setObjectName(QStringLiteral("fieldLabel"));
    auto *labelB = new QLabel(tr("Altura (y)"), positionCard);
    labelB->setObjectName(QStringLiteral("fieldLabel"));
    m_offsetASpin = makeSpin(positionCard, -5000.0, 5000.0, 5.0, 1, QStringLiteral(" mm"));
    m_offsetBSpin = makeSpin(positionCard, -5000.0, 5000.0, 5.0, 1, QStringLiteral(" mm"));
    m_offsetASpin->setToolTip(tr("Desloca a seção ao longo da largura (eixo z da seção girada)."));
    m_offsetBSpin->setToolTip(tr("Desloca a seção ao longo da altura (eixo y da seção girada)."));
    offsetForm->addWidget(labelA, 1, 0);
    offsetForm->addWidget(m_offsetASpin, 1, 1);
    offsetForm->addWidget(labelB, 2, 0);
    offsetForm->addWidget(m_offsetBSpin, 2, 1);
    auto *resetOffsets = new QPushButton(tr("Zerar deslocamento"), positionCard);
    resetOffsets->setObjectName(QStringLiteral("smallAction"));
    resetOffsets->setCursor(Qt::PointingHandCursor);
    connect(resetOffsets, &QPushButton::clicked, this, [this]() {
        MemberPlacementPatch patch;
        if (m_end == 1 && current().endDiffers) {
            patch.offsetA2 = 0.0;
            patch.offsetB2 = 0.0;
        } else {
            patch.offsetA = 0.0;
            patch.offsetB = 0.0;
        }
        emitPatch(patch);
    });
    offsetForm->addWidget(resetOffsets, 3, 0, 1, 2);
    positionRow->addLayout(offsetForm, 1);
    positionBody->addLayout(positionRow);

    m_endDiffers = new QCheckBox(tr("Deslocamento diferente no nó J"), positionCard);
    m_endDiffers->setToolTip(tr("Permite alinhar a seção de forma distinta em cada extremidade (por exemplo, mísulas e vigas de topo inclinado)."));
    positionBody->addWidget(m_endDiffers);
    connect(m_endDiffers, &QCheckBox::toggled, this, [this](bool on) {
        if (m_block) return;
        MemberPlacementPatch patch;
        patch.endDiffers = on;
        if (on) {
            patch.offsetA2 = current().offsetA;
            patch.offsetB2 = current().offsetB;
        } else {
            m_end = 0;
        }
        emitPatch(patch);
    });
    m_endTabs = new QWidget(positionCard);
    auto *tabsLayout = new QHBoxLayout(m_endTabs);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(0);
    m_endButtonI = new QToolButton(m_endTabs);
    m_endButtonI->setObjectName(QStringLiteral("endTab"));
    m_endButtonI->setText(tr("Nó I"));
    m_endButtonI->setCheckable(true);
    m_endButtonJ = new QToolButton(m_endTabs);
    m_endButtonJ->setObjectName(QStringLiteral("endTab"));
    m_endButtonJ->setText(tr("Nó J"));
    m_endButtonJ->setCheckable(true);
    tabsLayout->addWidget(m_endButtonI);
    tabsLayout->addWidget(m_endButtonJ);
    tabsLayout->addStretch(1);
    positionBody->addWidget(m_endTabs);
    connect(m_endButtonI, &QToolButton::clicked, this, [this]() { setEnd(0); });
    connect(m_endButtonJ, &QToolButton::clicked, this, [this]() { setEnd(1); });
    bodyLayout->addWidget(positionCard);

    auto offsetEdited = [this]() {
        if (m_block) return;
        MemberPlacementPatch patch;
        const double a = m_offsetASpin->value() / kToMm;
        const double b = m_offsetBSpin->value() / kToMm;
        if (m_end == 1 && current().endDiffers) {
            patch.offsetA2 = a;
            patch.offsetB2 = b;
        } else {
            patch.offsetA = a;
            patch.offsetB = b;
        }
        emitPatch(patch);
    };
    connect(m_offsetASpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, offsetEdited);
    connect(m_offsetBSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, offsetEdited);

    // --- vinculos de extremidade ---
    QVBoxLayout *releaseBody = nullptr;
    auto *releaseCard = makeCard(m_body, tr("Vínculos de extremidade (articulações)"), &releaseBody);
    auto *releaseGrid = new QGridLayout();
    releaseGrid->setHorizontalSpacing(10);
    releaseGrid->setVerticalSpacing(3);
    const QStringList rowNames = {tr("Torção (Mx)"), tr("Flexão em y (My)"), tr("Flexão em z (Mz)")};
    auto *headI = new QLabel(tr("Nó I"), releaseCard);
    headI->setObjectName(QStringLiteral("fieldLabel"));
    auto *headJ = new QLabel(tr("Nó J"), releaseCard);
    headJ->setObjectName(QStringLiteral("fieldLabel"));
    releaseGrid->addWidget(headI, 0, 1, Qt::AlignHCenter);
    releaseGrid->addWidget(headJ, 0, 2, Qt::AlignHCenter);
    for (int row = 0; row < 3; ++row) {
        auto *name = new QLabel(rowNames[row], releaseCard);
        releaseGrid->addWidget(name, row + 1, 0);
        for (int end = 0; end < 2; ++end) {
            auto *box = new QCheckBox(releaseCard);
            box->setToolTip(tr("Liberar (articular) este grau de liberdade na extremidade."));
            const std::size_t index = static_cast<std::size_t>(end * 3 + row);
            m_releaseBoxes[index] = box;
            releaseGrid->addWidget(box, row + 1, end + 1, Qt::AlignHCenter);
            connect(box, &QCheckBox::toggled, this, [this](bool) {
                if (m_block) return;
                MemberPlacementPatch patch;
                patch.releaseMask = currentReleaseMask();
                emitPatch(patch);
            });
        }
    }
    releaseBody->addLayout(releaseGrid);
    auto *presets = new QHBoxLayout();
    presets->setSpacing(4);
    struct Preset { QString label; int mask; };
    const int hingeI = Release::kStartRy | Release::kStartRz;
    const int hingeJ = Release::kEndRy | Release::kEndRz;
    for (const Preset &preset : {Preset {tr("Contínua"), 0}, Preset {tr("Articulada I"), hingeI},
                                 Preset {tr("Articulada J"), hingeJ}, Preset {tr("Bi-articulada"), hingeI | hingeJ}}) {
        auto *button = new QPushButton(preset.label, releaseCard);
        button->setObjectName(QStringLiteral("smallAction"));
        button->setCursor(Qt::PointingHandCursor);
        const int mask = preset.mask;
        connect(button, &QPushButton::clicked, this, [this, mask]() { applyReleasePreset(mask); });
        presets->addWidget(button);
    }
    releaseBody->addLayout(presets);
    bodyLayout->addWidget(releaseCard);

    // --- resultado ---
    QVBoxLayout *resultBody = nullptr;
    auto *resultCard = makeCard(m_body, tr("Resultado"), &resultBody);
    m_resultCentroid = new QLabel(resultCard);
    m_resultCentroid->setObjectName(QStringLiteral("placementResult"));
    m_resultCentroid->setWordWrap(true);
    m_resultInertia = new QLabel(resultCard);
    m_resultInertia->setObjectName(QStringLiteral("placementResult"));
    m_resultInertia->setWordWrap(true);
    m_resultNote = new QLabel(resultCard);
    m_resultNote->setObjectName(QStringLiteral("placementHint"));
    m_resultNote->setWordWrap(true);
    resultBody->addWidget(m_resultCentroid);
    resultBody->addWidget(m_resultInertia);
    resultBody->addWidget(m_resultNote);
    bodyLayout->addWidget(resultCard);
}

void SectionPlacementPanel::setEnd(int end)
{
    m_end = end;
    refreshAll();
}

SectionPlacementPanel::MemberPlacement SectionPlacementPanel::current() const
{
    if (m_targets.isEmpty()) {
        return MemberPlacement();
    }
    return m_targets.first().placement;
}

double SectionPlacementPanel::currentOffsetA() const
{
    const auto placement = current();
    return (m_end == 1 && placement.endDiffers) ? placement.offsetA2 : placement.offsetA;
}

double SectionPlacementPanel::currentOffsetB() const
{
    const auto placement = current();
    return (m_end == 1 && placement.endDiffers) ? placement.offsetB2 : placement.offsetB;
}

int SectionPlacementPanel::currentReleaseMask() const
{
    int mask = current().releaseMask & ~(Release::kStartRx | Release::kStartRy | Release::kStartRz
                                         | Release::kEndRx | Release::kEndRy | Release::kEndRz);
    const int bits[6] = {Release::kStartRx, Release::kStartRy, Release::kStartRz,
                         Release::kEndRx, Release::kEndRy, Release::kEndRz};
    for (std::size_t i = 0; i < 6; ++i) {
        if (m_releaseBoxes[i] && m_releaseBoxes[i]->isChecked()) {
            mask |= bits[i];
        }
    }
    return mask;
}

void SectionPlacementPanel::applyReleasePreset(int mask)
{
    MemberPlacementPatch patch;
    // Preserva liberacoes que a interface nao edita (translacoes) e substitui as de rotacao.
    const int rotations = Release::kStartRx | Release::kStartRy | Release::kStartRz
        | Release::kEndRx | Release::kEndRy | Release::kEndRz;
    patch.releaseMask = (current().releaseMask & ~rotations) | mask;
    emitPatch(patch);
}

void SectionPlacementPanel::emitPatch(const MemberPlacementPatch &patch)
{
    if (m_targets.isEmpty()) {
        return;
    }
    // Atualiza ja o primeiro alvo para a interface reagir sem esperar a volta do MainWindow.
    patch.applyTo(m_targets[0].placement);
    refreshAll();
    QVector<QUuid> ids;
    for (const auto &target : m_targets) {
        ids.append(target.barId);
    }
    emit placementEdited(ids, patch);
}

void SectionPlacementPanel::setTargets(const QVector<Target> &targets)
{
    m_targets = targets;
    if (m_targets.isEmpty() || !current().endDiffers) {
        m_end = 0;
    }
    refreshAll();
}

void SectionPlacementPanel::refreshAll()
{
    const bool wasBlocked = m_block;
    m_block = true;

    const bool hasTargets = !m_targets.isEmpty();
    m_emptyLabel->setVisible(!hasTargets);
    m_body->setVisible(hasTargets);

    if (hasTargets) {
        const Target &first = m_targets.first();
        const MemberPlacement placement = first.placement;

        if (m_targets.size() == 1) {
            m_targetLabel->setText(tr("Barra B%1  ·  %2")
                                       .arg(first.externalId)
                                       .arg(first.hasSection ? first.section.name : tr("sem seção")));
        } else {
            m_targetLabel->setText(tr("%1 barras selecionadas  ·  %2")
                                       .arg(m_targets.size())
                                       .arg(first.hasSection ? first.section.name : tr("sem seção")));
        }
        bool mixed = false;
        for (const auto &target : m_targets) {
            MemberPlacement a = target.placement;
            MemberPlacement b = placement;
            a.memberId = b.memberId = 0;
            if (!(a == b)) {
                mixed = true;
                break;
            }
        }
        m_mixedLabel->setVisible(mixed);

        if (first.hasSection) {
            m_preview->setSection(first.section);
        } else {
            m_preview->clearSection();
        }
        m_preview->setPlacement(placement.rotationDeg, placement.reference, currentOffsetA(), currentOffsetB());

        m_rotationSlider->setValue(static_cast<int>(std::lround(placement.rotationDeg)));
        m_rotationSpin->setValue(placement.rotationDeg);
        if (auto *button = m_referenceButtons[static_cast<std::size_t>(std::clamp(placement.reference, 0, 8))]) {
            button->setChecked(true);
        }
        m_offsetASpin->setValue(currentOffsetA() * kToMm);
        m_offsetBSpin->setValue(currentOffsetB() * kToMm);
        m_endDiffers->setChecked(placement.endDiffers);
        m_endTabs->setVisible(placement.endDiffers);
        m_endButtonI->setChecked(m_end == 0);
        m_endButtonJ->setChecked(m_end == 1);

        const int bits[6] = {Release::kStartRx, Release::kStartRy, Release::kStartRz,
                             Release::kEndRx, Release::kEndRy, Release::kEndRz};
        for (std::size_t i = 0; i < 6; ++i) {
            m_releaseBoxes[i]->setChecked((placement.releaseMask & bits[i]) != 0);
        }
        refreshResult();
    }

    m_block = wasBlocked;
    applyHeight();
}

void SectionPlacementPanel::refreshResult()
{
    const Target &first = m_targets.first();
    const MemberPlacement placement = first.placement;
    const bool jEnd = (m_end == 1);

    if (!first.hasSection) {
        m_resultCentroid->setText(tr("Atribua uma seção à barra para ver o resultado."));
        m_resultInertia->clear();
        m_resultNote->clear();
        return;
    }

    MemberPlacement forEnd = placement;
    forEnd.offsetA = currentOffsetA();
    forEnd.offsetB = currentOffsetB();
    const auto offset = SectionPlacementService::axisOffset(&first.section, forEnd);
    m_resultCentroid->setText(tr("Centroide em relação ao nó %1:  y = %2 mm   z = %3 mm")
                                  .arg(jEnd ? QStringLiteral("J") : QStringLiteral("I"))
                                  .arg(offset.dy * kToMm, 0, 'f', 1)
                                  .arg(offset.dz * kToMm, 0, 'f', 1));

    const auto principal = SectionPlacementService::principalProperties(first.section);
    double iy = first.section.iy;
    double iz = first.section.iz;
    double angle = placement.rotationDeg;
    if (principal.valid && principal.inclined) {
        iy = principal.iyPrincipal;
        iz = principal.izPrincipal;
        angle += principal.thetaDeg;
    }
    const auto rotated = SectionPlacementService::inertiaInReferenceAxes(iy, iz, angle);
    m_resultInertia->setText(tr("Inércias nos eixos de referência (cm⁴):\nIy = %1    Iz = %2    Iyz = %3")
                                 .arg(rotated.iy * kToCm4, 0, 'f', 1)
                                 .arg(rotated.iz * kToCm4, 0, 'f', 1)
                                 .arg(rotated.iyz * kToCm4, 0, 'f', 1));
    if (principal.valid && principal.inclined) {
        m_resultNote->setText(tr("Seção assimétrica: os eixos principais ficam a %1° dos eixos do desenho e já entram no cálculo.")
                                  .arg(principal.thetaDeg, 0, 'f', 2));
    } else {
        m_resultNote->setText(tr("Esforços e diagramas são calculados no eixo da barra (centroide)."));
    }
}

void SectionPlacementPanel::applyHeight()
{
    if (!parentWidget() || !m_content) {
        return;
    }
    // O conteudo rola quando a janela principal e menor que o painel.
    const int available = std::max(240, parentWidget()->height() - 24);
    const int wanted = 34 + 8 + m_content->sizeHint().height() + 10;
    setFixedHeight(std::min(wanted, available));
}

void SectionPlacementPanel::reposition()
{
    if (!parentWidget()) {
        return;
    }
    applyHeight();
    move(std::max(0, parentWidget()->width() - width() - 20), 12);
    raise();
}

bool SectionPlacementPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        reposition();
    }
    return QFrame::eventFilter(watched, event);
}

} // namespace Structura::UI
