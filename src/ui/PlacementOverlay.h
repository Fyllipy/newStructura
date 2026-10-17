#pragma once

#include <QFrame>
#include <QHash>
#include <QLineEdit>
#include <QMatrix4x4>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QWidget>

#include <array>
#include <vector>

class QLabel;

namespace Structura::UI {

/// Marcador azul que identifica o alvo de snap ativo (malha, extremidade, centro).
class SnapMarkerWidget : public QWidget
{
public:
    enum class Kind { Grid, Endpoint, Center, OnBar };

    explicit SnapMarkerWidget(QWidget *parent);

    void setKind(Kind kind);
    /// Centraliza o marcador em `screen` (coordenadas do widget pai).
    void placeAt(const QPointF &screen);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Kind m_kind {Kind::Grid};
};

/// Caixa X/Y/Z (editavel) e angulo mostrada junto ao cursor durante o lancamento.
class CoordinateReadoutWidget : public QObject
{
    Q_OBJECT

public:
    explicit CoordinateReadoutWidget(QWidget *parent);

    [[nodiscard]] QLineEdit *coordinateEditor(int index) const { return m_values[static_cast<std::size_t>(index)]; }
    [[nodiscard]] QLineEdit *angleEditor() const { return m_angleEditor; }
    [[nodiscard]] QFrame *coordinateFrame() const { return m_frame; }
    [[nodiscard]] QFrame *angleFrame() const { return m_angleFrame; }

    void setCoordinateText(int index, const QString &text);
    void setAngleText(const QString &text);
    void setEditing(int index, bool editing);
    void setAllReadOnly();
    void hideAll();
    /// Posiciona (e mostra) as caixas perto do cursor, sem sair do widget pai.
    void showNear(const QPointF &cursor, bool angleVisible);

private:
    void equalizeWidths();

    QWidget *m_parent;
    QFrame *m_frame;
    QFrame *m_angleFrame;
    std::array<QLineEdit *, 3> m_values {};
    QLineEdit *m_angleEditor;
};

/**
 * @brief Camada transparente que desenha rotulos (ids, eixos de referencia, cargas)
 * projetados na tela, fora do pipeline VTK.
 */
class ViewportLabelOverlay : public QWidget
{
public:
    explicit ViewportLabelOverlay(QWidget *parent);

    void setGroup(const QString &kind,
                  const std::vector<std::array<double, 3>> &positions,
                  const QVector<QString> &labels,
                  bool visible,
                  const std::vector<QColor> &colors = {},
                  const std::vector<double> &weights = {},
                  bool declutter = false);
    void setGroupVisible(const QString &kind, bool visible);
    void clearAll();

    /// Reprojeta todos os grupos com a matriz composta (projecao * visao) da camera.
    void sync(const QMatrix4x4 &compositeMatrix);
    void resizeToParent();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Group {
        std::vector<std::array<double, 3>> positions;
        QVector<QString> labels;
        std::vector<QPointF> screen;
        std::vector<bool> onScreen;
        bool visible {true};
        std::vector<QColor> colors;     ///< vazio = cor padrao
        std::vector<double> weights;    ///< prioridade (maior primeiro) quando `declutter`
        bool declutter {false};         ///< evita sobreposicao e repeticao de rotulos iguais proximos
    };

    QHash<QString, Group> m_groups;
};

} // namespace Structura::UI
