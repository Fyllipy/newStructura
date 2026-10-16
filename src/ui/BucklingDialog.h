#pragma once

#include <QDialog>
#include <QVector>

class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;
class QTableWidget;

namespace Structura::UI {

/// Flambagem elastica: escolha da combinacao, lista de fatores criticos e controle da forma modal no 3D.
class BucklingDialog final : public QDialog
{
    Q_OBJECT

public:
    BucklingDialog(const QStringList &combinations, QWidget *parent = nullptr);

    void setResults(const QVector<double> &factors);
    void setError(const QString &message);

signals:
    void computeRequested(const QString &combination, int modes);
    /// `fraction`: tamanho da deformada como fracao da maior dimensao do modelo.
    void modeChanged(int mode, double fraction);
    void closedByUser();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void emitMode();

    QComboBox *m_combination {nullptr};
    QSpinBox *m_modes {nullptr};
    QTableWidget *m_table {nullptr};
    QSlider *m_scale {nullptr};
    QLabel *m_status {nullptr};
};

} // namespace Structura::UI
