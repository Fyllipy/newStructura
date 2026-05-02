#pragma once

#include <QDialog>
#include <QPair>
#include <QVector>
#include <QUuid>
#include <vector>

class QListWidget;
class QComboBox;

class AssignBarPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    struct BarInfo {
        QUuid id;
        QUuid startNodeId;
        QUuid endNodeId;
        QUuid materialId;
        QUuid sectionId;
        int externalId {0};
    };

    struct NodeInfo {
        QUuid id;
        int externalId {0};
        double x {0.0};
        double y {0.0};
        double z {0.0};
    };

    AssignBarPropertiesDialog(const QVector<QPair<QUuid, QString>> &materials,
                              const QVector<QPair<QUuid, QString>> &sections,
                              const std::vector<BarInfo> &bars,
                              const std::vector<NodeInfo> &nodes,
                              QWidget *parent = nullptr);

    void setCurrentMaterial(const QUuid &id);
    void setCurrentSection(const QUuid &id);
    QUuid selectedMaterial() const;
    QUuid selectedSection() const;
    QList<int> selectedBarIndices() const;

private:
    QVector<QPair<QUuid, QString>> m_materialOptions;
    QVector<QPair<QUuid, QString>> m_sectionOptions;
    QListWidget *m_barList;
    QComboBox *m_materialCombo;
    QComboBox *m_sectionCombo;
};
