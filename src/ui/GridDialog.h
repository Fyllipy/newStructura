#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;

class GridDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GridDialog(QWidget *parent = nullptr);

    double dx() const;
    double dy() const;
    double dz() const;
    int nx() const;
    int ny() const;
    int nz() const;

private:
    QDoubleSpinBox *m_dx;
    QDoubleSpinBox *m_dy;
    QDoubleSpinBox *m_dz;
    QSpinBox *m_nx;
    QSpinBox *m_ny;
    QSpinBox *m_nz;
};

