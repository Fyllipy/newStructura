#pragma once

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
class QGroupBox;

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
    };

    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setNodeEntries(const QVector<NodeEntry> &entries);
    void setBarEntries(const QVector<BarEntry> &entries);
    void setGridInfo(bool hasGrid,
                     double dx,
                     double dy,
                     double dz,
                     int nx,
                     int ny,
                     int nz);
    void setMaterialOptions(const QVector<QPair<QUuid, QString>> &options);
    void setSectionOptions(const QVector<QPair<QUuid, QString>> &options);

signals:
    void nodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value);
    void barMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId);
    void barSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId);

public slots:
    void clearSelections();

private slots:
    void handleNodeXEditing();
    void handleNodeYEditing();
    void handleNodeZEditing();
    void handleMaterialComboChanged(int index);
    void handleSectionComboChanged(int index);

private:
    void buildNodeGroup();
    void buildBarGroup();
    void buildGridGroup();
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
    QComboBox *m_sectionCombo {nullptr};

    QGroupBox *m_gridGroup {nullptr};
    QLabel *m_gridStatusLabel {nullptr};
    QLabel *m_gridSpacingLabel {nullptr};
    QLabel *m_gridCountLabel {nullptr};

    QVector<QUuid> m_currentNodeIds;
    QVector<QUuid> m_currentBarIds;
    QVector<QPair<QUuid, QString>> m_materialOptions;
    QVector<QPair<QUuid, QString>> m_sectionOptions;

    bool m_blockSignals {false};
};
