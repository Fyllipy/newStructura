#include "PropertiesPanel.h"

#include "ProfilePickerDialog.h"

#include "SectionPlacementWidget.h"

#include "app/services/SectionPlacementService.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QPushButton>
#include <QLocale>

namespace {
constexpr int s_decimalPlaces = 3;

QString loadsLabelText(int count)
{
    if (count <= 0) {
        return QObject::tr("Cargas: nenhuma");
    }
    return QObject::tr("Cargas: %1").arg(count);
}
} // namespace

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(16);

    buildNodeGroup();
    buildBarGroup();

    layout->addWidget(m_nodeGroup);
    layout->addWidget(m_barGroup);
    layout->addStretch(1);

    setNodeEntries({});
    setBarEntries({});
}

void PropertiesPanel::buildNodeGroup()
{
    m_nodeGroup = new QGroupBox(tr("Propriedades do nó"), this);
    auto *form = new QFormLayout(m_nodeGroup);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_nodeIdLabel = new QLabel(tr("-"), m_nodeGroup);
    form->addRow(tr("ID"), m_nodeIdLabel);

    auto validator = new QDoubleValidator(-1e9, 1e9, s_decimalPlaces, this);
    validator->setNotation(QDoubleValidator::StandardNotation);

    m_nodeXEdit = new QLineEdit(m_nodeGroup);
    m_nodeXEdit->setValidator(validator);
    connect(m_nodeXEdit, &QLineEdit::editingFinished, this, &PropertiesPanel::handleNodeXEditing);
    form->addRow(tr("X (m)"), m_nodeXEdit);

    m_nodeYEdit = new QLineEdit(m_nodeGroup);
    m_nodeYEdit->setValidator(validator);
    connect(m_nodeYEdit, &QLineEdit::editingFinished, this, &PropertiesPanel::handleNodeYEditing);
    form->addRow(tr("Y (m)"), m_nodeYEdit);

    m_nodeZEdit = new QLineEdit(m_nodeGroup);
    m_nodeZEdit->setValidator(validator);
    connect(m_nodeZEdit, &QLineEdit::editingFinished, this, &PropertiesPanel::handleNodeZEditing);
    form->addRow(tr("Z (m)"), m_nodeZEdit);

    m_nodeRestraintsLabel = new QLabel(tr("Restrições: -"), m_nodeGroup);
    form->addRow(tr("Restrições"), m_nodeRestraintsLabel);

    m_nodeLoadsLabel = new QLabel(loadsLabelText(0), m_nodeGroup);
    form->addRow(m_nodeLoadsLabel);
}

void PropertiesPanel::buildBarGroup()
{
    m_barGroup = new QGroupBox(tr("Propriedades da barra"), this);
    auto *form = new QFormLayout(m_barGroup);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_barIdLabel = new QLabel(tr("-"), m_barGroup);
    form->addRow(tr("ID"), m_barIdLabel);

    m_barNodesLabel = new QLabel(tr("-"), m_barGroup);
    form->addRow(tr("Nós"), m_barNodesLabel);

    m_barLengthLabel = new QLabel(tr("-"), m_barGroup);
    form->addRow(tr("Comprimento"), m_barLengthLabel);

    m_barDistributedLoadsLabel = new QLabel(QObject::tr("Cargas distrib.: nenhuma"), m_barGroup);
    form->addRow(m_barDistributedLoadsLabel);
    m_barLoadDetailsLabel = new QLabel(m_barGroup);
    m_barLoadDetailsLabel->setTextFormat(Qt::RichText);
    m_barLoadDetailsLabel->setWordWrap(true);
    m_barLoadDetailsLabel->setStyleSheet(QStringLiteral("color:#3b4654; font-size:8.5pt;"));
    form->addRow(m_barLoadDetailsLabel);

    m_materialCombo = new QComboBox(m_barGroup);
    m_materialCombo->setPlaceholderText(tr("Selecionar material"));
    connect(m_materialCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::handleMaterialComboChanged);
    form->addRow(tr("Material"), m_materialCombo);

    m_sectionPicker = new Structura::UI::SectionPickerButton(m_barGroup);
    connect(m_sectionPicker, &Structura::UI::SectionPickerButton::picked,
            this, &PropertiesPanel::handleSectionPicked);
    connect(m_sectionPicker, &Structura::UI::SectionPickerButton::manageRequested,
            this, &PropertiesPanel::manageProfilesRequested);
    form->addRow(tr("Seção"), m_sectionPicker);

    // --- Disposicao da secao: miniatura + resumo + atalho para a janela de posicionamento ---
    m_placementPreview = new Structura::UI::SectionPlacementWidget(m_barGroup);
    m_placementPreview->setCompact(true);
    m_placementPreview->setInteractive(false);
    m_placementPreview->setFixedHeight(150);
    form->addRow(m_placementPreview);

    m_placementSummary = new QLabel(tr("-"), m_barGroup);
    m_placementSummary->setWordWrap(true);
    m_placementSummary->setStyleSheet(QStringLiteral("color: #5a6574; font-size: 11px;"));
    form->addRow(m_placementSummary);

    m_placementButton = new QPushButton(tr("Posicionar seção e articulações…"), m_barGroup);
    m_placementButton->setCursor(Qt::PointingHandCursor);
    m_placementButton->setToolTip(tr("Abre a janela para girar a seção, escolher o ponto de referência e definir articulações."));
    connect(m_placementButton, &QPushButton::clicked, this, &PropertiesPanel::placementEditorRequested);
    form->addRow(m_placementButton);

    // --- Barra composta (5.3.6): espacamento entre as ligacoes dos perfis ---
    m_compositeRow = new QWidget(m_barGroup);
    auto *compositeLayout = new QFormLayout(m_compositeRow);
    compositeLayout->setContentsMargins(0, 0, 0, 0);
    compositeLayout->setSpacing(6);
    m_compositeGap = new QDoubleSpinBox(m_compositeRow);
    m_compositeGap->setRange(0.0, 2000.0);
    m_compositeGap->setDecimals(1);
    m_compositeGap->setSingleStep(1.0);
    m_compositeGap->setSuffix(tr(" mm"));
    m_compositeGap->setToolTip(tr("Afastamento e entre os dois perfis (espessura da chapa espaçadora ou de nó). É um parâmetro da geometria da seção: ao alterá-lo, "
                                  "área, inércias e constante de torção são recalculadas e enviadas ao cálculo (a barra passa a usar uma seção com o novo afastamento)."));
    m_compositeSpacing = new QDoubleSpinBox(m_compositeRow);
    m_compositeSpacing->setRange(0.0, 1e5);
    m_compositeSpacing->setDecimals(0);
    m_compositeSpacing->setSingleStep(50.0);
    m_compositeSpacing->setSuffix(tr(" mm"));
    m_compositeSpacing->setSpecialValueText(tr("L/3 (2 ligações)"));
    m_compositeSpacing->setToolTip(tr("Barra composta (NBR 8800:2024, 5.3.6): distância ℓ entre as ligações (chapas espaçadoras com parafusos ou soldas) dos dois perfis. "
                                      "Usada no índice de esbeltez do perfil isolado (ℓ/rmín) e na esbeltez modificada da barra. 0 = duas ligações intermediárias (ℓ = L/3)."));
    m_compositeKind = new QComboBox(m_compositeRow);
    m_compositeKind->addItem(tr("Parafusos com aperto normal"), false);
    m_compositeKind->addItem(tr("Soldas ou parafusos protendidos"), true);
    compositeLayout->addRow(tr("Afastamento e entre perfis"), m_compositeGap);
    compositeLayout->addRow(tr("Espaçamento ℓ entre ligações"), m_compositeSpacing);
    compositeLayout->addRow(tr("Ligações"), m_compositeKind);
    auto emitComposite = [this]() {
        if (!m_blockSignals && !m_currentBarIds.isEmpty()) {
            emit barCompositeEdited(m_currentBarIds, m_compositeSpacing->value(), m_compositeKind->currentData().toBool());
        }
    };
    connect(m_compositeGap, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_blockSignals && !m_currentBarIds.isEmpty()) {
            emit barCompositeGapEdited(m_currentBarIds, m_compositeGap->value());
        }
    });
    connect(m_compositeSpacing, &QDoubleSpinBox::editingFinished, this, emitComposite);
    connect(m_compositeKind, qOverload<int>(&QComboBox::currentIndexChanged), this, emitComposite);
    form->addRow(m_compositeRow);
    m_compositeRow->hide();

    auto *reportButton = new QPushButton(tr("Memorial de cálculo da barra…"), m_barGroup);
    reportButton->setCursor(Qt::PointingHandCursor);
    reportButton->setToolTip(tr("Mostra o memorial de cálculo desta barra (completo ou por item de cálculo) e permite exportar para Word."));
    connect(reportButton, &QPushButton::clicked, this, &PropertiesPanel::barReportRequested);
    form->addRow(reportButton);
}

QString PropertiesPanel::formatDouble(double value) const
{
    return QLocale::c().toString(value, 'f', s_decimalPlaces);
}

QString PropertiesPanel::restraintsSummary(const NodeEntry &entry) const
{
    static const char *labels[6] = { "UX", "UY", "UZ", "RX", "RY", "RZ" };
    QStringList fixed;
    for (int i = 0; i < 6; ++i) {
        if (entry.restraints[static_cast<std::size_t>(i)]) {
            fixed << QLatin1String(labels[i]);
        }
    }
    if (fixed.isEmpty()) {
        return tr("Restrições: livres");
    }
    return tr("Restrições: %1").arg(fixed.join(QLatin1String(", ")));
}

void PropertiesPanel::setNodeEntries(const QVector<NodeEntry> &entries)
{
    m_blockSignals = true;
    m_currentNodeIds.clear();
    for (const auto &entry : entries) {
        m_currentNodeIds.push_back(entry.id);
    }

    if (entries.isEmpty()) {
        m_nodeGroup->hide();
    } else {
        m_nodeGroup->show();
        if (entries.size() == 1) {
            const auto &entry = entries.first();
            m_nodeIdLabel->setText(tr("N%1").arg(entry.externalId));
            m_nodeXEdit->setText(formatDouble(entry.x));
            m_nodeYEdit->setText(formatDouble(entry.y));
            m_nodeZEdit->setText(formatDouble(entry.z));
            m_nodeXEdit->setPlaceholderText(QString());
            m_nodeYEdit->setPlaceholderText(QString());
            m_nodeZEdit->setPlaceholderText(QString());
            m_nodeRestraintsLabel->setText(restraintsSummary(entry));
            m_nodeLoadsLabel->setText(loadsLabelText(entry.loadCount));
        } else {
            m_nodeIdLabel->setText(tr("%1 nós").arg(entries.size()));

            auto setMixedField = [](QLineEdit *edit, const QString &placeholder) {
                edit->clear();
                edit->setPlaceholderText(placeholder);
            };

            auto allEqual = [](const QVector<NodeEntry> &list, auto selector) {
                if (list.isEmpty()) return true;
                const auto first = selector(list.first());
                for (const auto &item : list) {
                    if (!qFuzzyCompare(selector(item) + 1.0, first + 1.0)) {
                        return false;
                    }
                }
                return true;
            };

            const QString mixedText = tr("Vários");

            if (allEqual(entries, [](const NodeEntry &e) { return e.x; })) {
                m_nodeXEdit->setText(formatDouble(entries.first().x));
                m_nodeXEdit->setPlaceholderText(QString());
            } else {
                setMixedField(m_nodeXEdit, mixedText);
            }

            if (allEqual(entries, [](const NodeEntry &e) { return e.y; })) {
                m_nodeYEdit->setText(formatDouble(entries.first().y));
                m_nodeYEdit->setPlaceholderText(QString());
            } else {
                setMixedField(m_nodeYEdit, mixedText);
            }

            if (allEqual(entries, [](const NodeEntry &e) { return e.z; })) {
                m_nodeZEdit->setText(formatDouble(entries.first().z));
                m_nodeZEdit->setPlaceholderText(QString());
            } else {
                setMixedField(m_nodeZEdit, mixedText);
            }

            m_nodeRestraintsLabel->setText(tr("Restrições: vários"));

            int totalLoads = 0;
            for (const auto &entry : entries) {
                totalLoads += entry.loadCount;
            }
            m_nodeLoadsLabel->setText(loadsLabelText(totalLoads));
        }
    }

    m_blockSignals = false;
}

void PropertiesPanel::setBarEntries(const QVector<BarEntry> &entries)
{
    m_blockSignals = true;
    m_currentBarIds.clear();
    for (const auto &entry : entries) {
        m_currentBarIds.push_back(entry.id);
    }

    if (entries.isEmpty()) {
        m_barGroup->hide();
    } else {
        m_barGroup->show();
        QStringList idTexts;
        QStringList nodesTexts;
        QStringList lengths;
        int totalDistributed = 0;
        for (const auto &entry : entries) {
            idTexts << tr("B%1").arg(entry.externalId);
            nodesTexts << tr("N%1 - N%2").arg(entry.nodeI).arg(entry.nodeJ);
            lengths << formatDouble(entry.length);
            totalDistributed += entry.distributedLoadCount;
        }

        if (entries.size() == 1) {
            const auto &entry = entries.first();
            m_barIdLabel->setText(idTexts.first());
            m_barNodesLabel->setText(nodesTexts.first());
            m_barLengthLabel->setText(tr("%1 m").arg(lengths.first()));
            m_barDistributedLoadsLabel->setText(
                entry.distributedLoadCount > 0
                    ? tr("Cargas distrib.: %1").arg(entry.distributedLoadCount)
                    : tr("Cargas distrib.: nenhuma"));
            m_barLoadDetailsLabel->setText(entry.loadDetails);
            m_barLoadDetailsLabel->setVisible(!entry.loadDetails.isEmpty());
        } else {
            m_barLoadDetailsLabel->clear();
            m_barLoadDetailsLabel->hide();
            m_barIdLabel->setText(tr("%1 barras").arg(entries.size()));
            m_barNodesLabel->setText(tr("Nós: vários"));
            m_barLengthLabel->setText(tr("Comprimentos variados"));
            m_barDistributedLoadsLabel->setText(
                totalDistributed > 0
                    ? tr("Cargas distrib.: %1").arg(totalDistributed)
                    : tr("Cargas distrib.: nenhuma"));
        }

        updateMaterialComboSelection(entries);
        updateSectionComboSelection(entries);
        updatePlacementSummary(entries);
        const bool allComposite = std::all_of(entries.begin(), entries.end(), [](const BarEntry &e) { return e.compositeSection; });
        m_compositeRow->setVisible(allComposite);
        if (allComposite) {
            m_compositeGap->setValue(entries.first().compositeGapMm);
            m_compositeSpacing->setValue(entries.first().compositeSpacingMm);
            m_compositeKind->setCurrentIndex(entries.first().compositeWelded ? 1 : 0);
        }
    }

    m_blockSignals = false;
}

void PropertiesPanel::updatePlacementSummary(const QVector<BarEntry> &entries)
{
    if (entries.isEmpty()) {
        return;
    }
    using Structura::App::Services::SectionPlacementService;
    namespace Release = Structura::App::ProjectData::MemberRelease;
    const BarEntry &first = entries.first();

    if (first.hasSection) {
        m_placementPreview->setSection(first.section);
    } else {
        m_placementPreview->clearSection();
    }
    m_placementPreview->setPlacement(first.rotationDeg, first.reference, first.offsetA, first.offsetB);

    auto releaseText = [](int mask, int rx, int ry, int rz) {
        QStringList parts;
        if (mask & rx) parts << QStringLiteral("Mx");
        if (mask & ry) parts << QStringLiteral("My");
        if (mask & rz) parts << QStringLiteral("Mz");
        return parts.isEmpty() ? QStringLiteral("—") : parts.join(QLatin1Char('+'));
    };
    const auto offset = SectionPlacementService::axisOffset(first.hasSection ? &first.section : nullptr,
                                                            [&first]() {
                                                                Structura::App::ProjectData::MemberPlacement placement;
                                                                placement.reference = first.reference;
                                                                placement.offsetA = first.offsetA;
                                                                placement.offsetB = first.offsetB;
                                                                return placement;
                                                            }());
    QString text = tr("Giro %1°  ·  ponto: %2\nCentroide: y = %3 mm, z = %4 mm\nLiberações  I: %5   J: %6")
                       .arg(first.rotationDeg, 0, 'f', 1)
                       .arg(SectionPlacementService::referenceLabels().value(first.reference))
                       .arg(offset.dy * 1000.0, 0, 'f', 1)
                       .arg(offset.dz * 1000.0, 0, 'f', 1)
                       .arg(releaseText(first.releaseMask, Release::kStartRx, Release::kStartRy, Release::kStartRz))
                       .arg(releaseText(first.releaseMask, Release::kEndRx, Release::kEndRy, Release::kEndRz));
    if (first.memberType == Structura::App::ProjectData::MemberType::kTruss) {
        text.prepend(tr("Tipo: treliça (biarticulada, só esforço axial)\n"));
    } else if (first.memberType == Structura::App::ProjectData::MemberType::kTie) {
        text.prepend(tr("Tipo: tirante (biarticulado, só tração)\n"));
    }
    if (entries.size() > 1) {
        text.prepend(tr("(valores da primeira barra)\n"));
    }
    m_placementSummary->setText(text);
}

void PropertiesPanel::setMaterialOptions(const QVector<QPair<QUuid, QString>> &options)
{
    m_materialOptions = options;
    const bool blocked = m_blockSignals;
    m_blockSignals = true;
    m_materialCombo->clear();
    m_materialCombo->addItem(tr("Sem material"), QVariant());
    for (const auto &opt : m_materialOptions) {
        m_materialCombo->addItem(opt.second, opt.first);
    }
    m_blockSignals = blocked;
}

void PropertiesPanel::setSections(const QVector<Structura::App::ProjectData::SectionInfo> &sections)
{
    m_sectionPicker->setSections(sections);
}

void PropertiesPanel::handleNodeXEditing()
{
    if (m_blockSignals || m_currentNodeIds.isEmpty()) {
        return;
    }
    bool ok = false;
    const double value = m_nodeXEdit->text().toDouble(&ok);
    if (!ok) {
        return;
    }
    emit nodeCoordinateEdited(m_currentNodeIds, 'x', value);
}

void PropertiesPanel::handleNodeYEditing()
{
    if (m_blockSignals || m_currentNodeIds.isEmpty()) {
        return;
    }
    bool ok = false;
    const double value = m_nodeYEdit->text().toDouble(&ok);
    if (!ok) {
        return;
    }
    emit nodeCoordinateEdited(m_currentNodeIds, 'y', value);
}

void PropertiesPanel::handleNodeZEditing()
{
    if (m_blockSignals || m_currentNodeIds.isEmpty()) {
        return;
    }
    bool ok = false;
    const double value = m_nodeZEdit->text().toDouble(&ok);
    if (!ok) {
        return;
    }
    emit nodeCoordinateEdited(m_currentNodeIds, 'z', value);
}

void PropertiesPanel::handleMaterialComboChanged(int index)
{
    if (m_blockSignals || m_currentBarIds.isEmpty()) {
        return;
    }
    if (index < 0) {
        return;
    }
    QVariant data = m_materialCombo->itemData(index);
    std::optional<QUuid> material;
    if (data.isValid()) {
        material = data.toUuid();
    } else {
        material = QUuid(); // explicit null
    }
    emit barMaterialEdited(m_currentBarIds, material);
}

void PropertiesPanel::handleSectionPicked(const QUuid &id)
{
    if (m_blockSignals || m_currentBarIds.isEmpty()) {
        return;
    }
    emit barSectionEdited(m_currentBarIds, std::optional<QUuid>(id));
}

void PropertiesPanel::updateMaterialComboSelection(const QVector<BarEntry> &entries)
{
    if (entries.isEmpty()) {
        return;
    }
    m_materialCombo->blockSignals(true);
    bool mixed = false;
    std::optional<QUuid> firstId;
    for (const auto &entry : entries) {
        if (!firstId.has_value()) {
            firstId = entry.materialId;
        } else if (entry.materialId != firstId.value()) {
            mixed = true;
            break;
        }
    }

    if (mixed) {
        m_materialCombo->setCurrentIndex(-1);
        m_materialCombo->setPlaceholderText(tr("Materiais variados"));
    } else {
        const QUuid matId = firstId.value_or(QUuid());
        int index = -1;
        if (matId.isNull()) {
            index = 0;
        } else {
            for (int i = 0; i < m_materialCombo->count(); ++i) {
                QVariant data = m_materialCombo->itemData(i);
                if (data.isValid() && data.toUuid() == matId) {
                    index = i;
                    break;
                }
            }
        }
        m_materialCombo->setCurrentIndex(index);
        if (!matId.isNull()) {
            m_materialCombo->setPlaceholderText(QString());
        }
    }
    m_materialCombo->blockSignals(false);
}

void PropertiesPanel::updateSectionComboSelection(const QVector<BarEntry> &entries)
{
    if (entries.isEmpty()) {
        return;
    }
    bool mixed = false;
    for (const auto &entry : entries) {
        if (entry.sectionId != entries.first().sectionId) {
            mixed = true;
            break;
        }
    }
    if (mixed) {
        m_sectionPicker->setMixed();
    } else {
        m_sectionPicker->setCurrent(entries.first().sectionId);
    }
}
