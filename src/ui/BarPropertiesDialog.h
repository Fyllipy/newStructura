#pragma once

#include <QDialog>
#include <QPair>
#include <QVector>
#include <QUuid>

class QComboBox;

class BarPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    BarPropertiesDialog(const QVector<QPair<QUuid, QString>> &materials,
                        const QVector<QPair<QUuid, QString>> &sections,
                        QWidget *parent = nullptr);

    void setCurrentMaterial(const QUuid &id);
    void setCurrentSection(const QUuid &id);
    QUuid selectedMaterial() const;
    QUuid selectedSection() const;

private:
    QVector<QPair<QUuid, QString>> m_materialOptions;
    QVector<QPair<QUuid, QString>> m_sectionOptions;
    QComboBox *m_materialCombo;
    QComboBox *m_sectionCombo;

    void populateCombo(QComboBox *combo, const QVector<QPair<QUuid, QString>> &options, const QString &emptyLabel);
    int indexForId(const QVector<QPair<QUuid, QString>> &options, const QUuid &id) const;
};

