#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QDoubleSpinBox;
class QComboBox;
class QLabel;

class MemberMomentDialog : public QDialog
{
    Q_OBJECT

public:
    struct Values {
        double mx {0.0};
        double my {0.0};
        double mz {0.0};
        QString system;
    };

    explicit MemberMomentDialog(QWidget *parent = nullptr);

    void setInitialValues(double mx, double my, double mz, const QString &system);
    Values values() const;
    void setSelectedCount(int count);
    /// Casos de carga disponiveis e o caso inicialmente selecionado.
    void setLoadCases(const QStringList &names, const QString &current);
    [[nodiscard]] QString loadCase() const;

private:
    QDoubleSpinBox *m_mxSpin {nullptr};
    QDoubleSpinBox *m_mySpin {nullptr};
    QDoubleSpinBox *m_mzSpin {nullptr};
    QComboBox *m_systemCombo {nullptr};
    QComboBox *m_caseCombo {nullptr};
    QLabel *m_selectionLabel {nullptr};
};

