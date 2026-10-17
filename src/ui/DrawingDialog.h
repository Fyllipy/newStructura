#pragma once

#include "app/services/SheetBuilder.h"

#include <QDialog>

#include <functional>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QLabel;
class QToolButton;

namespace Structura::UI {

/// Mostra uma prancha na tela (ajustada a largura disponivel).
class SheetPreviewWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SheetPreviewWidget(QWidget *parent = nullptr);
    void setSheet(const Structura::Drawing::Sheet *sheet);
    [[nodiscard]] double zoom() const { return m_zoom; }

public slots:
    void zoomIn() { zoomAt(rect().center(), 1.25); }
    void zoomOut() { zoomAt(rect().center(), 0.8); }
    void zoomFit();

    /// Vista selecionada na folha composta (indice em SheetLayoutDef::views) ou -1.
    void setSelectedCell(int index);

signals:
    void zoomChanged(double zoom);
    /// O usuario clicou em uma vista da folha composta.
    void cellSelected(int index);
    /// O usuario moveu ou redimensionou uma vista: novo retangulo em mm do papel (y para cima).
    void cellEdited(int index, double x0, double y0, double x1, double y1);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    QSize sizeHint() const override { return {760, 540}; }

private:
    void zoomAt(QPointF anchor, double factor);
    [[nodiscard]] double fitScale() const;   ///< px por mm que encaixa a folha na area
    [[nodiscard]] QPointF sheetOrigin(double k) const;

    enum class DragMode { Pan, Move, Resize };
    [[nodiscard]] QPointF toWidget(double x, double y, double k) const;
    [[nodiscard]] QPointF toSheet(QPointF p, double k) const;
    [[nodiscard]] int cellAt(QPointF p, double k) const;
    [[nodiscard]] int handleAt(QPointF p, double k, const double rect[4]) const;   ///< bit 1 = esquerda, 2 = direita, 4 = base, 8 = topo

    const Structura::Drawing::Sheet *m_sheet {nullptr};
    int m_selectedCell {-1};
    DragMode m_mode {DragMode::Pan};
    int m_handle {0};
    QPointF m_grabSheet;
    double m_rect[4] {0, 0, 0, 0};       ///< x0, y0, x1, y1 (mm, y para cima) da vista em edicao
    double m_rectStart[4] {0, 0, 0, 0};
    bool m_changed {false};
    double m_zoom {1.0};          ///< 1 = folha ajustada a area
    QPointF m_pan;                ///< deslocamento (px) em relacao a folha centralizada
    QPoint m_dragFrom;
    bool m_dragging {false};
};

/// Pranchas do projeto: plantas, elevacoes, lista de barras e detalhes de ligacoes, com exportacao em PDF e DXF.
class DrawingDialog final : public QDialog
{
    Q_OBJECT

public:
    DrawingDialog(const Structura::App::Services::SheetBuilder::Input &input, QWidget *parent = nullptr);

    /// Leitura da camera do modelo para criar vistas 3D (posicao do olho, alvo, cima em m).
    void setCameraCapture(std::function<bool(Structura::App::ProjectData::SavedView3D &)> capture) { m_capture = std::move(capture); }

signals:
    /// Carimbo editado (para gravar no projeto).
    void titleBlockChanged(const Structura::App::ProjectData::TitleBlockSettings &settings);
    /// Vistas 3D criadas / editadas / excluidas (para gravar no projeto).
    void views3dChanged(const QVector<Structura::App::ProjectData::SavedView3D> &views);
    /// Folhas compostas criadas / editadas / excluidas (para gravar no projeto).
    void layoutsChanged(const QVector<Structura::App::ProjectData::SheetLayoutDef> &layouts);

private:
    void populateViews();
    void addView3d();
    void editView3d();
    void removeView3d();
    [[nodiscard]] int selected3dIndex() const;
    void editTitleBlock();
    void rebuild();
    void populateLayouts();
    void populatePlaced();
    void addLayout();
    void renameLayout();
    void removeLayout();
    void removePlaced();
    void addPlaced(const QString &viewId);
    void rebuildAddMenu();
    void syncPlacementEditors();
    void editPlacementGeometry();
    [[nodiscard]] int selectedLayout() const;
    void showCustomSheet(int layoutIndex);
    void showPage(int index);
    void exportPdf();
    void exportDxf();
    [[nodiscard]] Structura::App::Services::SheetBuilder::Options options() const;

    Structura::App::Services::SheetBuilder::Input m_input;
    QVector<Structura::App::Services::SheetBuilder::ViewDef> m_views;
    std::vector<Structura::Drawing::Sheet> m_sheets;
    QComboBox *m_paper {nullptr};
    QComboBox *m_scale {nullptr};
    QCheckBox *m_labels {nullptr};
    QCheckBox *m_dims {nullptr};
    QListWidget *m_viewList {nullptr};
    QComboBox *m_profiles {nullptr};
    QCheckBox *m_faces {nullptr};
    QCheckBox *m_hidden3d {nullptr};
    QComboBox *m_colors {nullptr};
    QComboBox *m_weight {nullptr};
    std::function<bool(Structura::App::ProjectData::SavedView3D &)> m_capture;
    QLabel *m_titleBlockSummary {nullptr};
    QVector<Structura::App::Services::SheetBuilder::SheetLayout> m_layouts;
    QListWidget *m_layoutList {nullptr};
    QListWidget *m_placedList {nullptr};
    QComboBox *m_placedScale {nullptr};
    QToolButton *m_addPlaced {nullptr};
    QCheckBox *m_manualPos {nullptr};
    QDoubleSpinBox *m_posX {nullptr}, *m_posY {nullptr}, *m_posW {nullptr}, *m_posH {nullptr};
    QLabel *m_zoomLabel {nullptr};
    int m_autoCount {0};
    int m_shownPage {-1};
    QComboBox *m_page {nullptr};
    QLabel *m_info {nullptr};
    SheetPreviewWidget *m_preview {nullptr};
};

} // namespace Structura::UI
