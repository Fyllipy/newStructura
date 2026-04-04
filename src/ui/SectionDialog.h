#pragma once

#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;

class SectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SectionDialog(QWidget *parent = nullptr);

    QString name() const;
    double area() const;
    double iz() const;
    double iy() const;
    double j() const;

private:
    QLineEdit *m_nameEdit;
    QDoubleSpinBox *m_areaSpin;
    QDoubleSpinBox *m_izSpin;
    QDoubleSpinBox *m_iySpin;
    QDoubleSpinBox *m_jSpin;
};
