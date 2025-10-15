#pragma once

#include <QDialog>
#include <QPair>
#include <QVector>
#include <QUuid>
#include <vector>

#include "SceneController.h"

class QListWidget;
class QComboBox;

class AssignBarPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    AssignBarPropertiesDialog(const QVector<QPair<QUuid, QString>> &materials,
                              const QVector<QPair<QUuid, QString>> &sections,
                              const std::vector<SceneController::BarInfo> &bars,
                              const std::vector<SceneController::NodeInfo> &nodes,
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

    void populateCombo(QComboBox *combo, const QVector<QPair<QUuid, QString>> &options, const QString &emptyLabel);
    int indexForId(const QVector<QPair<QUuid, QString>> &options, const QUuid &id) const;
};
