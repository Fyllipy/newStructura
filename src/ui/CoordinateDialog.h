#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QPushButton;

class CoordinateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoordinateDialog(QWidget *parent = nullptr);

    double x() const;
    double y() const;
    double z() const;

signals:
    void requestScreenInsertion();

private:
    QDoubleSpinBox *m_xInput;
    QDoubleSpinBox *m_yInput;
    QDoubleSpinBox *m_zInput;
    QPushButton *m_screenButton;
};
