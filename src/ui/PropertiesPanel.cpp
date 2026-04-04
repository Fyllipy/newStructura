#include "PropertiesPanel.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleValidator>
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
    buildGridGroup();

    layout->addWidget(m_nodeGroup);
    layout->addWidget(m_barGroup);
    layout->addWidget(m_gridGroup);
    layout->addStretch(1);

    setNodeEntries({});
    setBarEntries({});
    setGridInfo(false, 0.0, 0.0, 0.0, 0, 0, 0);
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

    m_materialCombo = new QComboBox(m_barGroup);
    m_materialCombo->setPlaceholderText(tr("Selecionar material"));
    connect(m_materialCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::handleMaterialComboChanged);
    form->addRow(tr("Material"), m_materialCombo);

    m_sectionCombo = new QComboBox(m_barGroup);
    m_sectionCombo->setPlaceholderText(tr("Selecionar seção"));
    connect(m_sectionCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PropertiesPanel::handleSectionComboChanged);
    form->addRow(tr("Seção"), m_sectionCombo);
}

void PropertiesPanel::buildGridGroup()
{
    m_gridGroup = new QGroupBox(tr("Grid"), this);
    auto *form = new QFormLayout(m_gridGroup);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_gridStatusLabel = new QLabel(tr("Nenhum grid gerado"), m_gridGroup);
    form->addRow(m_gridStatusLabel);

    m_gridSpacingLabel = new QLabel(tr("-"), m_gridGroup);
    form->addRow(tr("Passo (dx/dy/dz)"), m_gridSpacingLabel);

    m_gridCountLabel = new QLabel(tr("-"), m_gridGroup);
    form->addRow(tr("Contagem (nx/ny/nz)"), m_gridCountLabel);
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
        } else {
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
    }

    m_blockSignals = false;
}

void PropertiesPanel::setGridInfo(bool hasGrid,
                                  double dx,
                                  double dy,
                                  double dz,
                                  int nx,
                                  int ny,
                                  int nz)
{
    if (!hasGrid) {
        m_gridStatusLabel->setText(tr("Nenhum grid gerado"));
        m_gridSpacingLabel->setText(tr("-"));
        m_gridCountLabel->setText(tr("-"));
        return;
    }
    m_gridStatusLabel->setText(tr("Grid ativo"));
    m_gridSpacingLabel->setText(
        tr("%1 / %2 / %3 m").arg(formatDouble(dx), formatDouble(dy), formatDouble(dz)));
    m_gridCountLabel->setText(tr("%1 / %2 / %3").arg(nx).arg(ny).arg(nz));
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

void PropertiesPanel::setSectionOptions(const QVector<QPair<QUuid, QString>> &options)
{
    m_sectionOptions = options;
    const bool blocked = m_blockSignals;
    m_blockSignals = true;
    m_sectionCombo->clear();
    m_sectionCombo->addItem(tr("Sem seção"), QVariant());
    for (const auto &opt : m_sectionOptions) {
        m_sectionCombo->addItem(opt.second, opt.first);
    }
    m_blockSignals = blocked;
}

void PropertiesPanel::clearSelections()
{
    setNodeEntries({});
    setBarEntries({});
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

void PropertiesPanel::handleSectionComboChanged(int index)
{
    if (m_blockSignals || m_currentBarIds.isEmpty()) {
        return;
    }
    if (index < 0) {
        return;
    }
    QVariant data = m_sectionCombo->itemData(index);
    std::optional<QUuid> section;
    if (data.isValid()) {
        section = data.toUuid();
    } else {
        section = QUuid();
    }
    emit barSectionEdited(m_currentBarIds, section);
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
    m_sectionCombo->blockSignals(true);
    bool mixed = false;
    std::optional<QUuid> firstId;
    for (const auto &entry : entries) {
        if (!firstId.has_value()) {
            firstId = entry.sectionId;
        } else if (entry.sectionId != firstId.value()) {
            mixed = true;
            break;
        }
    }

    if (mixed) {
        m_sectionCombo->setCurrentIndex(-1);
        m_sectionCombo->setPlaceholderText(tr("Seções variadas"));
    } else {
        const QUuid secId = firstId.value_or(QUuid());
        int index = -1;
        if (secId.isNull()) {
            index = 0;
        } else {
            for (int i = 0; i < m_sectionCombo->count(); ++i) {
                QVariant data = m_sectionCombo->itemData(i);
                if (data.isValid() && data.toUuid() == secId) {
                    index = i;
                    break;
                }
            }
        }
        m_sectionCombo->setCurrentIndex(index);
        if (!secId.isNull()) {
            m_sectionCombo->setPlaceholderText(QString());
        }
    }
    m_sectionCombo->blockSignals(false);
}
