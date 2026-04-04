#pragma once

#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;

class MaterialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MaterialDialog(QWidget *parent = nullptr);

    QString name() const;
    double youngModulus() const;
    double shearModulus() const;

private:
    QLineEdit *m_nameEdit;
    QDoubleSpinBox *m_modulusSpin;
    QDoubleSpinBox *m_shearSpin;
};
