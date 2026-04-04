#pragma once

#include <QDialog>
#include <QString>

class QDoubleSpinBox;
class QComboBox;
class QLabel;

class DistributedLoadDialog : public QDialog
{
    Q_OBJECT

public:
    struct Values {
        double qx {0.0};
        double qy {0.0};
        double qz {0.0};
        QString system;
    };

    explicit DistributedLoadDialog(QWidget *parent = nullptr);

    void setInitialValues(double qx, double qy, double qz, const QString &system);
    Values values() const;
    void setSelectedCount(int count);

private:
    QDoubleSpinBox *m_qxSpin {nullptr};
    QDoubleSpinBox *m_qySpin {nullptr};
    QDoubleSpinBox *m_qzSpin {nullptr};
    QComboBox *m_systemCombo {nullptr};
    QLabel *m_selectionLabel {nullptr};
};

