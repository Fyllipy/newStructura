#pragma once

#include <QDialog>
#include <QVector>

#include <array>

class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;
class QTableWidget;

namespace Structura::UI {

/// Analise modal: fonte de massa, tabela de frequencias/periodos/participacao e forma modal no 3D.
class ModalDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ModalDialog(const QStringList &massSources, QWidget *parent = nullptr);

    /// omega em rad/s; ratios = fracao de massa efetiva (X, Y, Z).
    void setResults(const QVector<double> &omega, const QVector<std::array<double, 3>> &ratios);
    void setError(const QString &message);

signals:
    /// `source` vazio = apenas a massa dos perfis.
    void computeRequested(const QString &source, int modes);
    void modeChanged(int mode, double fraction);
    void closedByUser();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void emitMode();

    QComboBox *m_source {nullptr};
    QSpinBox *m_modes {nullptr};
    QTableWidget *m_table {nullptr};
    QSlider *m_scale {nullptr};
    QLabel *m_status {nullptr};
};

} // namespace Structura::UI
