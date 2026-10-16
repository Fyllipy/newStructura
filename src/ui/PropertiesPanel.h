#pragma once

#include "app/ProjectDataTypes.h"

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QUuid>
#include <QPair>
#include <optional>
#include <array>

class QLabel;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;

namespace Structura::UI {
class SectionPickerButton;
class SectionPlacementWidget;
}

class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    struct NodeEntry {
        QUuid id;
        int externalId {0};
        double x {0.0};
        double y {0.0};
        double z {0.0};
        std::array<bool, 6> restraints;
        int loadCount {0};
    };

    struct BarEntry {
        QUuid id;
        int externalId {0};
        int nodeI {0};
        int nodeJ {0};
        double length {0.0};
        QUuid materialId;
        QString materialName;
        QUuid sectionId;
        QString sectionName;
        int distributedLoadCount {0};
        // Posicionamento da secao (giro, ponto de referencia, deslocamento adicional) e inercias principais.
        double rotationDeg {0.0};
        int reference {0};
        double offsetA {0.0};
        double offsetB {0.0};
        double sectionIy {0.0};
        double sectionIz {0.0};
        Structura::App::ProjectData::SectionInfo section;
        bool hasSection {false};
        int releaseMask {0};
        int memberType {0};
    };

    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setNodeEntries(const QVector<NodeEntry> &entries);
    void setBarEntries(const QVector<BarEntry> &entries);
    void setMaterialOptions(const QVector<QPair<QUuid, QString>> &options);
    void setSections(const QVector<Structura::App::ProjectData::SectionInfo> &sections);

signals:
    void nodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value);
    void barMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId);
    void barSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId);
    void manageProfilesRequested();
    /// Abre a janela de posicionamento da secao para as barras selecionadas.
    void placementEditorRequested();
    /// Abre o memorial de calculo da barra selecionada.
    void barReportRequested();

private slots:
    void handleNodeXEditing();
    void handleNodeYEditing();
    void handleNodeZEditing();
    void handleMaterialComboChanged(int index);
    void handleSectionPicked(const QUuid &id);

private:
    void updatePlacementSummary(const QVector<BarEntry> &entries);
    void buildNodeGroup();
    void buildBarGroup();
    QString formatDouble(double value) const;
    QString restraintsSummary(const NodeEntry &entry) const;
    void updateMaterialComboSelection(const QVector<BarEntry> &entries);
    void updateSectionComboSelection(const QVector<BarEntry> &entries);

    QGroupBox *m_nodeGroup {nullptr};
    QLabel *m_nodeIdLabel {nullptr};
    QLineEdit *m_nodeXEdit {nullptr};
    QLineEdit *m_nodeYEdit {nullptr};
    QLineEdit *m_nodeZEdit {nullptr};
    QLabel *m_nodeRestraintsLabel {nullptr};
    QLabel *m_nodeLoadsLabel {nullptr};

    QGroupBox *m_barGroup {nullptr};
    QLabel *m_barIdLabel {nullptr};
    QLabel *m_barNodesLabel {nullptr};
    QLabel *m_barLengthLabel {nullptr};
    QLabel *m_barDistributedLoadsLabel {nullptr};
    QComboBox *m_materialCombo {nullptr};
    Structura::UI::SectionPickerButton *m_sectionPicker {nullptr};
    Structura::UI::SectionPlacementWidget *m_placementPreview {nullptr};
    QLabel *m_placementSummary {nullptr};
    QPushButton *m_placementButton {nullptr};


    QVector<QUuid> m_currentNodeIds;
    QVector<QUuid> m_currentBarIds;
    QVector<QPair<QUuid, QString>> m_materialOptions;

    bool m_blockSignals {false};
};
