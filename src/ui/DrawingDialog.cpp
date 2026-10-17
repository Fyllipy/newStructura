#include "DrawingDialog.h"

#include "BusyProgress.h"
#include "TitleBlockDialog.h"
#include "View3DDialog.h"
#include "drawing/SheetPainter.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTabWidget>
#include <QToolButton>
#include <QSignalBlocker>
#include <QPageSize>
#include <QPaintEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::SheetBuilder;

SheetPreviewWidget::SheetPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(520, 380);
    setCursor(Qt::OpenHandCursor);
    setMouseTracking(true);
    setToolTip(tr("Roda do mouse: zoom · arrastar o fundo: mover a folha · folha composta: arraste uma vista para posicioná-la e as alças para redimensionar · duplo clique: ajustar à janela"));
}

void SheetPreviewWidget::setSheet(const Structura::Drawing::Sheet *sheet)
{
    m_sheet = sheet;
    bool known = false;
    if (m_sheet) {
        for (const auto &c : m_sheet->cells) {
            known = known || c.index == m_selectedCell;
        }
    }
    if (!known) {
        m_selectedCell = -1;
    }
    update();
}

double SheetPreviewWidget::fitScale() const
{
    const double margin = 10.0;
    return std::max(0.01, std::min((width() - 2 * margin) / m_sheet->width, (height() - 2 * margin) / m_sheet->height));
}

QPointF SheetPreviewWidget::sheetOrigin(double k) const
{
    return QPointF((width() - m_sheet->width * k) / 2.0, (height() - m_sheet->height * k) / 2.0) + m_pan;
}

void SheetPreviewWidget::zoomFit()
{
    m_zoom = 1.0;
    m_pan = {};
    emit zoomChanged(m_zoom);
    update();
}

void SheetPreviewWidget::zoomAt(QPointF anchor, double factor)
{
    if (!m_sheet) {
        return;
    }
    const double newZoom = std::clamp(m_zoom * factor, 0.2, 40.0);
    if (std::abs(newZoom - m_zoom) < 1e-9) {
        return;
    }
    const double kOld = fitScale() * m_zoom, kNew = fitScale() * newZoom;
    const QPointF onSheet = (anchor - sheetOrigin(kOld)) / kOld;   // ponto da folha (mm) sob o cursor
    m_zoom = newZoom;
    const QPointF centered((width() - m_sheet->width * kNew) / 2.0, (height() - m_sheet->height * kNew) / 2.0);
    m_pan = anchor - onSheet * kNew - centered;
    emit zoomChanged(m_zoom);
    update();
}

void SheetPreviewWidget::wheelEvent(QWheelEvent *event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps != 0.0) {
        zoomAt(event->position(), std::pow(1.2, steps));
    }
    event->accept();
}

QPointF SheetPreviewWidget::toWidget(double x, double y, double k) const
{
    const QPointF origin = sheetOrigin(k);
    return QPointF(origin.x() + x * k, origin.y() + (m_sheet->height - y) * k);
}

QPointF SheetPreviewWidget::toSheet(QPointF p, double k) const
{
    const QPointF origin = sheetOrigin(k);
    return QPointF((p.x() - origin.x()) / k, m_sheet->height - (p.y() - origin.y()) / k);
}

int SheetPreviewWidget::cellAt(QPointF p, double k) const
{
    if (!m_sheet) {
        return -1;
    }
    const QPointF q = toSheet(p, k);
    int found = -1;
    double smallest = 1e18;
    for (const auto &c : m_sheet->cells) {
        if (q.x() >= c.x0 && q.x() <= c.x1 && q.y() >= c.y0 && q.y() <= c.y1) {
            const double area = (c.x1 - c.x0) * (c.y1 - c.y0);
            if (area < smallest) {
                smallest = area;
                found = c.index;
            }
        }
    }
    return found;
}

int SheetPreviewWidget::handleAt(QPointF p, double k, const double rect[4]) const
{
    const QPointF a = toWidget(rect[0], rect[3], k), b = toWidget(rect[2], rect[1], k);   // canto superior esquerdo e inferior direito (px)
    const double grip = 7.0;
    int mask = 0;
    if (p.y() < a.y() - grip || p.y() > b.y() + grip || p.x() < a.x() - grip || p.x() > b.x() + grip) {
        return 0;
    }
    if (std::abs(p.x() - a.x()) <= grip) {
        mask |= 1;
    } else if (std::abs(p.x() - b.x()) <= grip) {
        mask |= 2;
    }
    if (std::abs(p.y() - b.y()) <= grip) {
        mask |= 4;
    } else if (std::abs(p.y() - a.y()) <= grip) {
        mask |= 8;
    }
    return mask;
}

void SheetPreviewWidget::setSelectedCell(int index)
{
    m_selectedCell = index;
    update();
}

void SheetPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    m_mode = DragMode::Pan;
    if (m_sheet && !m_sheet->cells.empty() && event->button() == Qt::LeftButton) {
        const double k = fitScale() * m_zoom;
        // cantos / bordas da vista ja selecionada tem prioridade
        const Structura::Drawing::Sheet::EditCell *selected = nullptr;
        for (const auto &c : m_sheet->cells) {
            if (c.index == m_selectedCell) {
                selected = &c;
            }
        }
        int handle = 0;
        if (selected) {
            const double r[4] = {selected->x0, selected->y0, selected->x1, selected->y1};
            handle = handleAt(event->position(), k, r);
        }
        const int hit = handle ? m_selectedCell : cellAt(event->position(), k);
        if (hit >= 0) {
            for (const auto &c : m_sheet->cells) {
                if (c.index == hit) {
                    m_rect[0] = m_rectStart[0] = c.x0;
                    m_rect[1] = m_rectStart[1] = c.y0;
                    m_rect[2] = m_rectStart[2] = c.x1;
                    m_rect[3] = m_rectStart[3] = c.y1;
                }
            }
            if (hit != m_selectedCell) {
                m_selectedCell = hit;
                emit cellSelected(hit);
            }
            m_handle = handle;
            m_mode = handle ? DragMode::Resize : DragMode::Move;
            m_grabSheet = toSheet(event->position(), k);
            m_changed = false;
            m_dragging = true;
            m_dragFrom = event->pos();
            update();
            return;
        }
    }
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_dragFrom = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void SheetPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_mode != DragMode::Pan && m_sheet) {
        const double k = fitScale() * m_zoom;
        const QPointF now = toSheet(event->position(), k);
        const double dx = now.x() - m_grabSheet.x(), dy = now.y() - m_grabSheet.y();
        const double minSize = 15.0;   // mm
        const double ax0 = m_sheet->areaX0, ay0 = m_sheet->areaY0, ax1 = m_sheet->areaX1, ay1 = m_sheet->areaY1;
        double r[4] = {m_rectStart[0], m_rectStart[1], m_rectStart[2], m_rectStart[3]};
        if (m_mode == DragMode::Move) {
            const double w = r[2] - r[0], h = r[3] - r[1];
            r[0] = std::clamp(r[0] + dx, ax0, ax1 - w);
            r[1] = std::clamp(r[1] + dy, ay0, ay1 - h);
            r[2] = r[0] + w;
            r[3] = r[1] + h;
        } else {
            if (m_handle & 1) {
                r[0] = std::clamp(r[0] + dx, ax0, r[2] - minSize);
            }
            if (m_handle & 2) {
                r[2] = std::clamp(r[2] + dx, r[0] + minSize, ax1);
            }
            if (m_handle & 4) {
                r[1] = std::clamp(r[1] + dy, ay0, r[3] - minSize);
            }
            if (m_handle & 8) {
                r[3] = std::clamp(r[3] + dy, r[1] + minSize, ay1);
            }
        }
        for (int i = 0; i < 4; ++i) {
            m_changed = m_changed || std::abs(r[i] - m_rectStart[i]) > 1e-6;
            m_rect[i] = r[i];
        }
        update();
        return;
    }
    if (!m_dragging && m_sheet && !m_sheet->cells.empty()) {
        const double k = fitScale() * m_zoom;
        int handle = 0;
        for (const auto &c : m_sheet->cells) {
            if (c.index == m_selectedCell) {
                const double r[4] = {c.x0, c.y0, c.x1, c.y1};
                handle = handleAt(event->position(), k, r);
            }
        }
        if (handle == 1 || handle == 2) {
            setCursor(Qt::SizeHorCursor);
        } else if (handle == 4 || handle == 8) {
            setCursor(Qt::SizeVerCursor);
        } else if (handle == 5 || handle == 10) {
            setCursor(Qt::SizeBDiagCursor);
        } else if (handle != 0) {
            setCursor(Qt::SizeFDiagCursor);
        } else {
            setCursor(cellAt(event->position(), k) >= 0 ? Qt::SizeAllCursor : Qt::OpenHandCursor);
        }
    }
    if (m_dragging) {
        m_pan += QPointF(event->pos() - m_dragFrom);
        m_dragFrom = event->pos();
        update();
    }
}

void SheetPreviewWidget::mouseReleaseEvent(QMouseEvent *)
{
    const bool edited = m_dragging && m_mode != DragMode::Pan && m_changed;
    m_dragging = false;
    m_mode = DragMode::Pan;
    setCursor(Qt::OpenHandCursor);
    if (edited && m_selectedCell >= 0) {
        emit cellEdited(m_selectedCell, m_rect[0], m_rect[1], m_rect[2], m_rect[3]);
    }
    m_changed = false;
    update();
}

void SheetPreviewWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    zoomFit();
}

void SheetPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#8d96a3")));
    if (!m_sheet) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("Nenhuma prancha (selecione as vistas)."));
        return;
    }
    const double k = fitScale() * m_zoom;
    const QPointF origin = sheetOrigin(k);
    painter.fillRect(QRectF(origin + QPointF(4, 4), QSizeF(m_sheet->width * k, m_sheet->height * k)), QColor(0, 0, 0, 60));
    Structura::Drawing::paintSheet(painter, *m_sheet, k, origin, 1.0);
    if (m_sheet->cells.empty()) {
        return;
    }
    // vista selecionada: moldura tracejada com as alcas de redimensionamento (e o retangulo provisorio durante o arrasto)
    for (const auto &c : m_sheet->cells) {
        double r[4] = {c.x0, c.y0, c.x1, c.y1};
        const bool dragged = m_dragging && m_mode != DragMode::Pan && c.index == m_selectedCell;
        if (dragged) {
            for (int i = 0; i < 4; ++i) {
                r[i] = m_rect[i];
            }
        }
        if (c.index != m_selectedCell) {
            painter.setPen(QPen(QColor(13, 127, 185, 70), 1, Qt::DotLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(toWidget(r[0], r[3], k), toWidget(r[2], r[1], k)));
            continue;
        }
        const QRectF box(toWidget(r[0], r[3], k), toWidget(r[2], r[1], k));
        painter.setPen(QPen(QColor(13, 127, 185), 1.6, Qt::DashLine));
        painter.setBrush(dragged ? QColor(13, 127, 185, 40) : QColor(13, 127, 185, 14));
        painter.drawRect(box);
        painter.setPen(QPen(QColor(13, 127, 185), 1.0));
        painter.setBrush(Qt::white);
        const double hs = 4.0;
        for (const QPointF &p : {box.topLeft(), box.topRight(), box.bottomLeft(), box.bottomRight(), QPointF(box.center().x(), box.top()), QPointF(box.center().x(), box.bottom()),
                                 QPointF(box.left(), box.center().y()), QPointF(box.right(), box.center().y())}) {
            painter.drawRect(QRectF(p.x() - hs, p.y() - hs, 2 * hs, 2 * hs));
        }
    }
}

DrawingDialog::DrawingDialog(const SheetBuilder::Input &input, QWidget *parent)
    : QDialog(parent)
    , m_input(input)
{
    setWindowTitle(tr("Pranchas do projeto"));
    resize(1320, 800);
    auto *root = new QHBoxLayout(this);

    auto *side = new QVBoxLayout;
    auto *sheetGroup = new QGroupBox(tr("Folha"));
    auto *sheetForm = new QFormLayout(sheetGroup);
    m_paper = new QComboBox;
    for (const auto paper : {SheetBuilder::Paper::A4, SheetBuilder::Paper::A3, SheetBuilder::Paper::A2, SheetBuilder::Paper::A1}) {
        m_paper->addItem(SheetBuilder::paperName(paper), static_cast<int>(paper));
    }
    m_paper->setCurrentIndex(1);
    m_scale = new QComboBox;
    m_scale->addItem(tr("Automática"), 0.0);
    for (const double s : {20.0, 25.0, 50.0, 75.0, 100.0, 125.0, 150.0, 200.0, 250.0, 500.0}) {
        m_scale->addItem(QStringLiteral("1:%1").arg(s), s);
    }
    m_profiles = new QComboBox;
    m_profiles->addItem(tr("Linha de eixo"), static_cast<int>(SheetBuilder::ProfileDetail::Axis));
    m_profiles->addItem(tr("Contorno do perfil"), static_cast<int>(SheetBuilder::ProfileDetail::Outline));
    m_profiles->addItem(tr("Contorno + mesas e alma"), static_cast<int>(SheetBuilder::ProfileDetail::OutlineWithFlanges));
    m_profiles->setCurrentIndex(2);
    m_profiles->setToolTip(tr("Detalhe dos perfis em todas as pranchas: desenha a largura real da seção (e, no último modo, as faces internas das mesas e a "
                              "seção em I dos pilares na planta), como nos modelos de detalhamento."));
    m_faces = new QCheckBox(tr("Cotas entre faces (vão livre das barras)"));
    m_faces->setChecked(true);
    m_colors = new QComboBox;
    m_colors->addItem(tr("Coloridas (por camada)"), true);
    m_colors->addItem(tr("Pretas"), false);
    m_weight = new QComboBox;
    m_weight->addItem(tr("Fina"), 0.7);
    m_weight->addItem(tr("Normal"), 1.0);
    m_weight->addItem(tr("Grossa"), 1.5);
    m_weight->setCurrentIndex(1);
    m_hidden3d = new QCheckBox(tr("Mostrar linhas ocultas (vistas 3D)"));
    m_labels = new QCheckBox(tr("Marcas e perfis nas barras"));
    m_labels->setChecked(true);
    m_dims = new QCheckBox(tr("Cotas entre eixos"));
    m_dims->setChecked(true);
    sheetForm->addRow(tr("Papel (paisagem)"), m_paper);
    sheetForm->addRow(tr("Escala"), m_scale);
    sheetForm->addRow(tr("Detalhe dos perfis"), m_profiles);
    sheetForm->addRow(tr("Cor das linhas"), m_colors);
    sheetForm->addRow(tr("Espessura das linhas"), m_weight);
    sheetForm->addRow(m_labels);
    sheetForm->addRow(m_faces);
    sheetForm->addRow(m_hidden3d);
    sheetForm->addRow(m_dims);
    side->addWidget(sheetGroup);

    auto *viewsSide = new QVBoxLayout;
    auto *viewGroup = new QGroupBox(tr("Vistas"));
    auto *viewLayout = new QVBoxLayout(viewGroup);
    m_viewList = new QListWidget;
    viewLayout->addWidget(m_viewList);
    auto *view3dRow = new QHBoxLayout;
    auto *add3d = new QPushButton(tr("+ Vista 3D…"));
    add3d->setToolTip(tr("Nova vista 3D em linhas (por ângulos ou pela câmera atual do modelo)"));
    auto *edit3d = new QPushButton(tr("Editar"));
    auto *remove3d = new QPushButton(tr("Excluir"));
    view3dRow->addWidget(add3d);
    view3dRow->addWidget(edit3d);
    view3dRow->addWidget(remove3d);
    viewLayout->addLayout(view3dRow);
    viewsSide->addWidget(viewGroup, 1);

    auto *composeGroup = new QGroupBox(tr("Folhas com várias vistas"));
    auto *composeLayout = new QVBoxLayout(composeGroup);
    auto *composeHint = new QLabel(tr("Monte uma folha com várias vistas lado a lado e escolha a escala de cada uma."));
    composeHint->setWordWrap(true);
    composeHint->setStyleSheet(QStringLiteral("color:#5a6472;"));
    composeLayout->addWidget(composeHint);
    m_layoutList = new QListWidget;
    m_layoutList->setMaximumHeight(90);
    composeLayout->addWidget(m_layoutList);
    auto *layoutRow = new QHBoxLayout;
    auto *addLayoutButton = new QPushButton(tr("+ Folha"));
    auto *renameLayoutButton = new QPushButton(tr("Renomear"));
    auto *removeLayoutButton = new QPushButton(tr("Excluir"));
    layoutRow->addWidget(addLayoutButton);
    layoutRow->addWidget(renameLayoutButton);
    layoutRow->addWidget(removeLayoutButton);
    composeLayout->addLayout(layoutRow);
    composeLayout->addWidget(new QLabel(tr("Vistas da folha selecionada:")));
    m_placedList = new QListWidget;
    m_placedList->setMaximumHeight(110);
    composeLayout->addWidget(m_placedList);
    auto *placedRow = new QHBoxLayout;
    m_addPlaced = new QToolButton;
    m_addPlaced->setText(tr("+ Vista ▾"));
    m_addPlaced->setPopupMode(QToolButton::InstantPopup);
    auto *removePlacedButton = new QPushButton(tr("Remover vista"));
    placedRow->addWidget(m_addPlaced);
    placedRow->addWidget(removePlacedButton);
    placedRow->addStretch(1);
    composeLayout->addLayout(placedRow);
    auto *scaleForm = new QFormLayout;
    m_placedScale = new QComboBox;
    m_placedScale->addItem(tr("Automática"), 0.0);
    for (const double s : {20.0, 25.0, 50.0, 75.0, 100.0, 125.0, 150.0, 200.0, 250.0, 500.0}) {
        m_placedScale->addItem(QStringLiteral("1:%1").arg(s), s);
    }
    scaleForm->addRow(tr("Escala da vista"), m_placedScale);
    m_manualPos = new QCheckBox(tr("Posição manual (% da área útil)"));
    scaleForm->addRow(m_manualPos);
    auto percent = [](double value) {
        auto *box = new QDoubleSpinBox;
        box->setRange(0.0, 100.0);
        box->setDecimals(1);
        box->setSingleStep(5.0);
        box->setSuffix(QStringLiteral(" %"));
        box->setValue(value);
        return box;
    };
    m_posX = percent(0.0);
    m_posY = percent(0.0);
    m_posW = percent(50.0);
    m_posH = percent(50.0);
    auto *posRow = new QHBoxLayout;
    posRow->addWidget(new QLabel(tr("X")));
    posRow->addWidget(m_posX);
    posRow->addWidget(new QLabel(tr("Y")));
    posRow->addWidget(m_posY);
    scaleForm->addRow(posRow);
    auto *sizeRow = new QHBoxLayout;
    sizeRow->addWidget(new QLabel(tr("Larg.")));
    sizeRow->addWidget(m_posW);
    sizeRow->addWidget(new QLabel(tr("Alt.")));
    sizeRow->addWidget(m_posH);
    scaleForm->addRow(sizeRow);
    composeLayout->addLayout(scaleForm);
    viewsSide->addWidget(composeGroup);

    auto *infoGroup = new QGroupBox(tr("Carimbo"));
    auto *infoLayout = new QVBoxLayout(infoGroup);
    m_titleBlockSummary = new QLabel;
    m_titleBlockSummary->setWordWrap(true);
    m_titleBlockSummary->setStyleSheet(QStringLiteral("color:#5a6472;"));
    auto *editBlock = new QPushButton(tr("Editar carimbo…"));
    infoLayout->addWidget(m_titleBlockSummary);
    infoLayout->addWidget(editBlock);
    side->addWidget(infoGroup);

    auto *pdf = new QPushButton(tr("Exportar PDF…"));
    pdf->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 6px 14px; border-radius: 4px; }"));
    auto *dxf = new QPushButton(tr("Exportar DXF (uma folha por arquivo)…"));
    side->addWidget(pdf);
    side->addWidget(dxf);
    side->addStretch(1);
    auto *generalTab = new QWidget;
    generalTab->setLayout(side);
    auto *viewsTab = new QWidget;
    viewsTab->setLayout(viewsSide);
    auto *tabs = new QTabWidget;
    tabs->addTab(generalTab, tr("Folha e exportação"));
    tabs->addTab(viewsTab, tr("Vistas"));
    tabs->setFixedWidth(380);
    root->addWidget(tabs);

    auto *right = new QVBoxLayout;
    auto *topRow = new QHBoxLayout;
    m_page = new QComboBox;
    topRow->addWidget(m_page, 1);
    auto *zoomOutButton = new QToolButton;
    zoomOutButton->setText(QStringLiteral("−"));
    zoomOutButton->setToolTip(tr("Reduzir"));
    auto *zoomInButton = new QToolButton;
    zoomInButton->setText(QStringLiteral("+"));
    zoomInButton->setToolTip(tr("Ampliar"));
    auto *zoomFitButton = new QToolButton;
    zoomFitButton->setText(tr("Ajustar"));
    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_zoomLabel->setMinimumWidth(48);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    topRow->addWidget(zoomOutButton);
    topRow->addWidget(m_zoomLabel);
    topRow->addWidget(zoomInButton);
    topRow->addWidget(zoomFitButton);
    right->addLayout(topRow);
    m_preview = new SheetPreviewWidget;
    right->addWidget(m_preview, 1);
    m_info = new QLabel;
    m_info->setStyleSheet(QStringLiteral("color:#5a6472;"));
    m_info->setWordWrap(true);
    right->addWidget(m_info);
    root->addLayout(right, 1);

    connect(m_paper, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    connect(m_scale, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    connect(m_labels, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(m_dims, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(m_profiles, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    connect(m_faces, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(m_hidden3d, &QCheckBox::toggled, this, [this]() { rebuild(); });
    connect(m_colors, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    connect(m_weight, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
    connect(add3d, &QPushButton::clicked, this, [this]() { addView3d(); });
    connect(edit3d, &QPushButton::clicked, this, [this]() { editView3d(); });
    connect(remove3d, &QPushButton::clicked, this, [this]() { removeView3d(); });
    connect(m_viewList, &QListWidget::itemChanged, this, [this]() { rebuild(); });
    connect(editBlock, &QPushButton::clicked, this, [this]() { editTitleBlock(); });
    connect(m_page, &QComboBox::currentIndexChanged, this, [this](int index) { showPage(index); });
    connect(zoomInButton, &QToolButton::clicked, m_preview, &SheetPreviewWidget::zoomIn);
    connect(zoomOutButton, &QToolButton::clicked, m_preview, &SheetPreviewWidget::zoomOut);
    connect(zoomFitButton, &QToolButton::clicked, m_preview, &SheetPreviewWidget::zoomFit);
    connect(m_preview, &SheetPreviewWidget::zoomChanged, this, [this](double zoom) { m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100.0))); });
    connect(addLayoutButton, &QPushButton::clicked, this, [this]() { addLayout(); });
    connect(renameLayoutButton, &QPushButton::clicked, this, [this]() { renameLayout(); });
    connect(removeLayoutButton, &QPushButton::clicked, this, [this]() { removeLayout(); });
    connect(removePlacedButton, &QPushButton::clicked, this, [this]() { removePlaced(); });
    connect(m_layoutList, &QListWidget::currentRowChanged, this, [this](int row) {
        populatePlaced();
        if (row >= 0 && m_autoCount + row < m_page->count()) {
            m_page->setCurrentIndex(m_autoCount + row);
        }
    });
    connect(m_placedList, &QListWidget::currentRowChanged, this, [this](int row) {
        const int layout = selectedLayout();
        if (layout < 0 || row < 0 || row >= m_layouts[layout].views.size()) {
            return;
        }
        const double scale = m_layouts[layout].views[row].scale;
        m_preview->setSelectedCell(row);
        QSignalBlocker block(m_placedScale);
        m_placedScale->setCurrentIndex(std::max(0, m_placedScale->findData(scale)));
        syncPlacementEditors();
    });
    connect(m_preview, &SheetPreviewWidget::cellSelected, this, [this](int index) {
        if (index >= 0 && index < m_placedList->count()) {
            m_placedList->setCurrentRow(index);
        }
    });
    connect(m_preview, &SheetPreviewWidget::cellEdited, this, [this](int index, double x0, double y0, double x1, double y1) {
        const int layout = selectedLayout();
        if (layout < 0 || index < 0 || index >= m_layouts[layout].views.size() || m_shownPage < m_autoCount) {
            return;
        }
        const auto &sheet = m_sheets[static_cast<std::size_t>(m_shownPage)];
        const double aw = sheet.areaX1 - sheet.areaX0, ah = sheet.areaY1 - sheet.areaY0;
        if (aw <= 0.0 || ah <= 0.0) {
            return;
        }
        auto &p = m_layouts[layout].views[index];
        p.manual = true;
        p.x = std::clamp((x0 - sheet.areaX0) / aw, 0.0, 1.0);
        p.y = std::clamp((sheet.areaY1 - y1) / ah, 0.0, 1.0);   // origem no canto superior esquerdo da area util
        p.w = std::clamp((x1 - x0) / aw, 0.05, 1.0);
        p.h = std::clamp((y1 - y0) / ah, 0.05, 1.0);
        emit layoutsChanged(m_layouts);
        populatePlaced();
        m_placedList->setCurrentRow(index);
        syncPlacementEditors();
        rebuild();
        showCustomSheet(layout);
        m_preview->setSelectedCell(index);
    });
    connect(m_manualPos, &QCheckBox::toggled, this, [this]() { editPlacementGeometry(); });
    for (QDoubleSpinBox *box : {m_posX, m_posY, m_posW, m_posH}) {
        connect(box, &QDoubleSpinBox::valueChanged, this, [this]() { editPlacementGeometry(); });
    }
    connect(m_placedScale, &QComboBox::currentIndexChanged, this, [this]() {
        const int layout = selectedLayout();
        const int row = m_placedList->currentRow();
        if (layout < 0 || row < 0 || row >= m_layouts[layout].views.size()) {
            return;
        }
        m_layouts[layout].views[row].scale = m_placedScale->currentData().toDouble();
        emit layoutsChanged(m_layouts);
        populatePlaced();
        m_placedList->setCurrentRow(row);
        rebuild();
    });
    connect(pdf, &QPushButton::clicked, this, [this]() { exportPdf(); });
    connect(dxf, &QPushButton::clicked, this, [this]() { exportDxf(); });
    m_layouts = m_input.layouts;
    populateViews();
    populateLayouts();
    populatePlaced();
    syncPlacementEditors();
    rebuild();
}

void DrawingDialog::populateViews()
{
    QSet<QString> checked;
    const bool hadItems = m_viewList->count() > 0;
    for (int i = 0; i < m_viewList->count(); ++i) {
        if (m_viewList->item(i)->checkState() == Qt::Checked) {
            checked.insert(m_viewList->item(i)->data(Qt::UserRole).toString());
        }
    }
    m_viewList->blockSignals(true);
    m_viewList->clear();
    m_views = SheetBuilder::availableViews(m_input);
    for (const auto &view : m_views) {
        auto *item = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(view.title).arg(view.barCount), m_viewList);
        item->setData(Qt::UserRole, view.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState((hadItems ? checked.contains(view.id) : view.defaultOn) ? Qt::Checked : Qt::Unchecked);
    }
    m_viewList->blockSignals(false);
    rebuildAddMenu();
    const auto &info = m_input.info;
    m_titleBlockSummary->setText(tr("%1 · %2 · rev. %3 · %4 revisão(ões)")
                                     .arg(info.project.isEmpty() ? tr("(sem projeto)") : info.project, info.company.isEmpty() ? tr("(sem empresa)") : info.company, info.revision)
                                     .arg(info.revisions.size()));
}

int DrawingDialog::selected3dIndex() const
{
    const auto *item = m_viewList->currentItem();
    if (!item) {
        return -1;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (!id.startsWith(QLatin1String("view3d-")) || id == QLatin1String("view3d-default")) {
        return -1;
    }
    return id.mid(7).toInt();
}

void DrawingDialog::addView3d()
{
    Structura::App::ProjectData::SavedView3D view;
    view.name = tr("Vista %1").arg(m_input.views3d.size() + 1);
    View3DDialog dialog(view, m_capture, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_input.views3d.append(dialog.view());
    emit views3dChanged(m_input.views3d);
    populateViews();
    // marca a vista recem-criada
    for (int i = 0; i < m_viewList->count(); ++i) {
        if (m_viewList->item(i)->data(Qt::UserRole).toString() == QStringLiteral("view3d-%1").arg(m_input.views3d.size() - 1)) {
            m_viewList->item(i)->setCheckState(Qt::Checked);
        }
    }
    rebuild();
}

void DrawingDialog::editView3d()
{
    const int index = selected3dIndex();
    if (index < 0 || index >= m_input.views3d.size()) {
        QMessageBox::information(this, tr("Vista 3D"), tr("Selecione uma vista 3D na lista (as vistas 3D criadas por você)."));
        return;
    }
    View3DDialog dialog(m_input.views3d[index], m_capture, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_input.views3d[index] = dialog.view();
    emit views3dChanged(m_input.views3d);
    populateViews();
    rebuild();
}

void DrawingDialog::removeView3d()
{
    const int index = selected3dIndex();
    if (index < 0 || index >= m_input.views3d.size()) {
        return;
    }
    m_input.views3d.removeAt(index);
    emit views3dChanged(m_input.views3d);
    populateViews();
    rebuild();
}

void DrawingDialog::editTitleBlock()
{
    QVector<TitleBlockDialog::ViewTitle> titles;
    for (const auto &view : SheetBuilder::availableViews([&]() {
             auto copy = m_input;
             copy.info.sheetTitles.clear();
             return copy;
         }())) {
        titles.append({view.id, view.title});
    }
    TitleBlockDialog dialog(m_input.info, titles, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_input.info = dialog.settings();
    emit titleBlockChanged(m_input.info);
    populateViews();
    rebuild();
}

SheetBuilder::Options DrawingDialog::options() const
{
    SheetBuilder::Options o;
    o.paper = static_cast<SheetBuilder::Paper>(m_paper->currentData().toInt());
    o.scaleDenominator = m_scale->currentData().toDouble();
    o.barLabels = m_labels->isChecked();
    o.dimensions = m_dims->isChecked();
    o.profiles = static_cast<SheetBuilder::ProfileDetail>(m_profiles->currentData().toInt());
    o.faceDimensions = m_faces->isChecked();
    o.hiddenLines3D = m_hidden3d->isChecked();
    o.coloredLines = m_colors->currentData().toBool();
    o.lineWeightScale = m_weight->currentData().toDouble();
    o.customSheets = m_layouts;
    for (int i = 0; i < m_viewList->count(); ++i) {
        if (m_viewList->item(i)->checkState() == Qt::Checked) {
            o.views.insert(m_viewList->item(i)->data(Qt::UserRole).toString());
        }
    }
    if (o.views.isEmpty()) {
        o.views.insert(QStringLiteral("__none__"));   // nenhuma vista marcada = nenhuma prancha
    }
    return o;
}

void DrawingDialog::rebuild()
{
    if (m_input.info.date.isEmpty()) {
        m_input.info.date = QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"));
    }
    const int keep = m_page->currentIndex();
    const auto opts = options();
    m_sheets = runBusy(this, tr("Pranchas"), tr("Gerando as pranchas (vistas, cotas e linhas ocultas)…"),
                       [&]() { return SheetBuilder::build(m_input, opts); });
    m_autoCount = std::max(0, static_cast<int>(m_sheets.size()) - static_cast<int>(opts.customSheets.size()));
    m_page->blockSignals(true);
    m_page->clear();
    for (std::size_t i = 0; i < m_sheets.size(); ++i) {
        m_page->addItem(QStringLiteral("%1 — %2").arg(i + 1).arg(QString::fromStdString(m_sheets[i].name)));
    }
    m_page->blockSignals(false);
    showPage(std::clamp(keep, 0, std::max(0, static_cast<int>(m_sheets.size()) - 1)));
    if (!m_sheets.empty()) {
        m_page->setCurrentIndex(std::clamp(keep, 0, static_cast<int>(m_sheets.size()) - 1));
    }
}

int DrawingDialog::selectedLayout() const
{
    const int row = m_layoutList ? m_layoutList->currentRow() : -1;
    return row >= 0 && row < m_layouts.size() ? row : -1;
}

void DrawingDialog::populateLayouts()
{
    const int keep = m_layoutList->currentRow();
    QSignalBlocker block(m_layoutList);
    m_layoutList->clear();
    for (const auto &layout : m_layouts) {
        m_layoutList->addItem(tr("%1 (%2 vista(s))").arg(layout.name).arg(layout.views.size()));
    }
    m_layoutList->setCurrentRow(std::clamp(keep, m_layouts.isEmpty() ? -1 : 0, static_cast<int>(m_layouts.size()) - 1));
}

void DrawingDialog::populatePlaced()
{
    QSignalBlocker block(m_placedList);
    m_placedList->clear();
    const int layout = selectedLayout();
    if (layout < 0) {
        return;
    }
    for (const auto &placed : m_layouts[layout].views) {
        QString title = placed.viewId;
        for (const auto &view : m_views) {
            if (view.id == placed.viewId) {
                title = view.title;
            }
        }
        m_placedList->addItem(QStringLiteral("%1 — %2%3").arg(title, placed.scale > 0.0 ? QStringLiteral("1:%1").arg(placed.scale) : tr("escala automática"), placed.manual ? tr(" · posição manual") : QString()));
    }
}

void DrawingDialog::rebuildAddMenu()
{
    auto *menu = new QMenu(m_addPlaced);
    for (const auto &view : m_views) {
        if (view.kind == SheetBuilder::ViewKind::BarList) {
            continue;
        }
        const QString id = view.id;
        menu->addAction(view.title, this, [this, id]() { addPlaced(id); });
    }
    delete m_addPlaced->menu();
    m_addPlaced->setMenu(menu);
}

void DrawingDialog::addLayout()
{
    SheetBuilder::SheetLayout layout;
    layout.name = tr("Folha composta %1").arg(m_layouts.size() + 1);
    m_layouts.append(layout);
    emit layoutsChanged(m_layouts);
    populateLayouts();
    m_layoutList->setCurrentRow(m_layouts.size() - 1);
    populatePlaced();
    rebuild();
    showCustomSheet(m_layouts.size() - 1);
}

void DrawingDialog::renameLayout()
{
    const int index = selectedLayout();
    if (index < 0) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Renomear folha"), tr("Título da folha:"), QLineEdit::Normal, m_layouts[index].name, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_layouts[index].name = name.trimmed();
        emit layoutsChanged(m_layouts);
        populateLayouts();
        rebuild();
        showCustomSheet(index);
    }
}

void DrawingDialog::removeLayout()
{
    const int index = selectedLayout();
    if (index < 0) {
        return;
    }
    m_layouts.removeAt(index);
    emit layoutsChanged(m_layouts);
    populateLayouts();
    populatePlaced();
    rebuild();
}

void DrawingDialog::addPlaced(const QString &viewId)
{
    if (selectedLayout() < 0) {
        addLayout();
    }
    const int index = selectedLayout();
    SheetBuilder::SheetLayout::Placement placement;
    placement.viewId = viewId;
    placement.scale = m_placedScale->currentData().toDouble();
    m_layouts[index].views.append(placement);
    emit layoutsChanged(m_layouts);
    populateLayouts();
    populatePlaced();
    m_placedList->setCurrentRow(m_layouts[index].views.size() - 1);
    rebuild();
    showCustomSheet(index);
}

void DrawingDialog::removePlaced()
{
    const int index = selectedLayout();
    const int row = m_placedList->currentRow();
    if (index < 0 || row < 0 || row >= m_layouts[index].views.size()) {
        return;
    }
    m_layouts[index].views.removeAt(row);
    emit layoutsChanged(m_layouts);
    populateLayouts();
    populatePlaced();
    rebuild();
    showCustomSheet(index);
}

void DrawingDialog::syncPlacementEditors()
{
    const int layout = selectedLayout();
    const int row = m_placedList->currentRow();
    const bool valid = layout >= 0 && row >= 0 && row < m_layouts[layout].views.size();
    for (QWidget *w : {static_cast<QWidget *>(m_manualPos), static_cast<QWidget *>(m_posX), static_cast<QWidget *>(m_posY), static_cast<QWidget *>(m_posW), static_cast<QWidget *>(m_posH)}) {
        QSignalBlocker block(w);
        Q_UNUSED(block)
    }
    QSignalBlocker b1(m_manualPos), b2(m_posX), b3(m_posY), b4(m_posW), b5(m_posH);
    m_manualPos->setEnabled(valid);
    if (!valid) {
        m_manualPos->setChecked(false);
        for (QDoubleSpinBox *box : {m_posX, m_posY, m_posW, m_posH}) {
            box->setEnabled(false);
        }
        return;
    }
    const auto &p = m_layouts[layout].views[row];
    m_manualPos->setChecked(p.manual);
    m_posX->setValue(p.x * 100.0);
    m_posY->setValue(p.y * 100.0);
    m_posW->setValue(p.w * 100.0);
    m_posH->setValue(p.h * 100.0);
    for (QDoubleSpinBox *box : {m_posX, m_posY, m_posW, m_posH}) {
        box->setEnabled(p.manual);
    }
}

void DrawingDialog::editPlacementGeometry()
{
    const int layout = selectedLayout();
    const int row = m_placedList->currentRow();
    if (layout < 0 || row < 0 || row >= m_layouts[layout].views.size()) {
        return;
    }
    auto &p = m_layouts[layout].views[row];
    p.manual = m_manualPos->isChecked();
    p.x = m_posX->value() / 100.0;
    p.y = m_posY->value() / 100.0;
    p.w = std::max(0.05, m_posW->value() / 100.0);
    p.h = std::max(0.05, m_posH->value() / 100.0);
    for (QDoubleSpinBox *box : {m_posX, m_posY, m_posW, m_posH}) {
        box->setEnabled(p.manual);
    }
    emit layoutsChanged(m_layouts);
    populatePlaced();
    m_placedList->setCurrentRow(row);
    rebuild();
    showCustomSheet(layout);
}

void DrawingDialog::showCustomSheet(int layoutIndex)
{
    const int page = m_autoCount + layoutIndex;
    if (page >= 0 && page < m_page->count()) {
        m_page->setCurrentIndex(page);
    }
}

void DrawingDialog::showPage(int index)
{
    if (m_preview && index != m_shownPage) {
        m_preview->zoomFit();
    }
    m_shownPage = index;
    if (index < 0 || index >= static_cast<int>(m_sheets.size())) {
        m_preview->setSheet(nullptr);
        m_info->setText(tr("Nenhuma prancha: marque ao menos uma vista."));
        return;
    }
    m_preview->setSheet(&m_sheets[static_cast<std::size_t>(index)]);
    m_info->setText(tr("%1 prancha(s). Texto e linhas são vetoriais; o PDF e o DXF usam exatamente este desenho (cotas em mm, escala indicada no carimbo).")
                        .arg(m_sheets.size()));
}

void DrawingDialog::exportPdf()
{
    if (m_sheets.empty()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar pranchas"), QStringLiteral("pranchas.pdf"), tr("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    QPdfWriter writer(path);
    writer.setResolution(300);
    writer.setTitle(m_input.info.project);
    writer.setCreator(QStringLiteral("Structura 3D"));
    writer.setPageSize(QPageSize(QSizeF(m_sheets.front().width, m_sheets.front().height), QPageSize::Millimeter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    if (!painter.isActive()) {
        QMessageBox::warning(this, tr("Pranchas"), tr("Não foi possível gravar o PDF."));
        return;
    }
    const double k = 300.0 / 25.4;
    for (std::size_t i = 0; i < m_sheets.size(); ++i) {
        if (i > 0) {
            writer.newPage();
        }
        Structura::Drawing::paintSheet(painter, m_sheets[i], k, QPointF(0, 0), 1.0);
    }
    painter.end();
    QMessageBox::information(this, tr("Pranchas"), tr("PDF gravado com %1 prancha(s).").arg(m_sheets.size()));
}

void DrawingDialog::exportDxf()
{
    if (m_sheets.empty()) {
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Pasta para os arquivos DXF"));
    if (dir.isEmpty()) {
        return;
    }
    int written = 0;
    for (const auto &sheet : m_sheets) {
        QFile file(QDir(dir).filePath(QString::fromStdString(sheet.fileStem) + QStringLiteral(".dxf")));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(Structura::Drawing::sheetToDxf(sheet).toLatin1());
            ++written;
        }
    }
    QMessageBox::information(this, tr("Pranchas"), tr("%1 arquivo(s) DXF gravado(s) em %2.").arg(written).arg(dir));
}

} // namespace Structura::UI
