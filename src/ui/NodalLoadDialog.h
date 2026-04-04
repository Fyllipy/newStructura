#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLabel;

class NodalLoadDialog : public QDialog
{
    Q_OBJECT

public:
    struct Values {
        double fx {0.0};
        double fy {0.0};
        double fz {0.0};
        double mx {0.0};
        double my {0.0};
        double mz {0.0};
    };

    explicit NodalLoadDialog(QWidget *parent = nullptr);

    void setInitialValues(double fx, double fy, double fz,
                          double mx, double my, double mz);
    Values values() const;
    void setSelectedCount(int count);

private:
    QDoubleSpinBox *m_fxSpin {nullptr};
    QDoubleSpinBox *m_fySpin {nullptr};
    QDoubleSpinBox *m_fzSpin {nullptr};
    QDoubleSpinBox *m_mxSpin {nullptr};
    QDoubleSpinBox *m_mySpin {nullptr};
    QDoubleSpinBox *m_mzSpin {nullptr};
    QLabel *m_selectionLabel {nullptr};
};

