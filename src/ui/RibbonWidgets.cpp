#include "RibbonWidgets.h"

#include <QAction>
#include <QApplication>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidgetAction>

#include <algorithm>

namespace Structura::UI {

namespace {

QColor color(const char *hex) { return QColor(QString::fromLatin1(hex)); }

QFont labelFont()
{
    QFont font = QApplication::font();
    font.setPixelSize(12);
    return font;
}

/// Divide o rotulo em ate duas linhas: respeita quebras manuais; senao divide no espaco que equilibra as larguras.
QStringList splitLabel(const QString &label, const QFontMetrics &fm, int maxWidth)
{
    if (label.contains(QLatin1Char('\n'))) {
        return label.split(QLatin1Char('\n'));
    }
    if (fm.horizontalAdvance(label) <= maxWidth || !label.contains(QLatin1Char(' '))) {
        return {label};
    }
    int bestIndex = -1, bestWidth = 1 << 30;
    for (int i = 0; i < label.size(); ++i) {
        if (label.at(i) != QLatin1Char(' ')) {
            continue;
        }
        const int w = std::max(fm.horizontalAdvance(label.left(i)), fm.horizontalAdvance(label.mid(i + 1)));
        if (w < bestWidth) {
            bestWidth = w;
            bestIndex = i;
        }
    }
    return {label.left(bestIndex), label.mid(bestIndex + 1)};
}

void drawChevron(QPainter &p, const QPointF &center, const QColor &c)
{
    QPen pen(c, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(center.x() - 3.5, center.y() - 1.5);
    path.lineTo(center.x(), center.y() + 2.0);
    path.lineTo(center.x() + 3.5, center.y() - 1.5);
    p.drawPath(path);
}

} // namespace

// ---- dica estendida ------------------------------------------------------------------------------------------------------------------

RibbonToolTip &RibbonToolTip::instance()
{
    static RibbonToolTip *tip = new RibbonToolTip();
    return *tip;
}

RibbonToolTip::RibbonToolTip()
    : QFrame(nullptr, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("RibbonToolTip"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("#RibbonToolTip { background: #ffffff; border: 1px solid #b9c6d8; border-radius: 6px; }"
                                 "#RibbonToolTip QLabel { background: transparent; color: #1e232b; }"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 8);
    layout->setSpacing(6);
    auto *header = new QHBoxLayout();
    header->setSpacing(10);
    m_title = new QLabel(this);
    QFont bold = labelFont();
    bold.setPixelSize(13);
    bold.setBold(true);
    m_title->setFont(bold);
    header->addWidget(m_title, 1);
    m_badge = new QLabel(this);
    m_badge->setStyleSheet(QStringLiteral("QLabel { background: #f2f5fa; color: #6b7686; border: 1px solid #cbd4e2; border-radius: 4px; padding: 0 6px; font-size: 11px; }"));
    header->addWidget(m_badge, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addLayout(header);
    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setMaximumWidth(280);
    m_description->setMinimumWidth(240);
    QFont small = labelFont();
    small.setPixelSize(12);
    m_description->setFont(small);
    layout->addWidget(m_description);
    m_line = new QFrame(this);
    m_line->setFixedHeight(1);
    m_line->setStyleSheet(QStringLiteral("background: #e1e7ef;"));
    layout->addWidget(m_line);
    m_help = new QWidget(this);
    auto *helpLayout = new QHBoxLayout(m_help);
    helpLayout->setContentsMargins(0, 0, 0, 0);
    helpLayout->setSpacing(6);
    auto *mark = new QLabel(QStringLiteral("?"), m_help);
    mark->setFixedSize(14, 14);
    mark->setAlignment(Qt::AlignCenter);
    mark->setStyleSheet(QStringLiteral("QLabel { border: 1px solid #6b7686; border-radius: 7px; color: #6b7686; font-size: 10px; font-weight: 700; }"));
    helpLayout->addWidget(mark);
    auto *text = new QLabel(tr("Pressione F1 para abrir a ajuda"), m_help);
    text->setStyleSheet(QStringLiteral("QLabel { color: #6b7686; font-size: 11px; }"));
    helpLayout->addWidget(text, 1);
    layout->addWidget(m_help);
    // seguranca: some depois de um tempo longo sem o mouse sair do botao (um unico temporizador, reiniciado a cada exibicao)
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(30000);
    connect(m_timeout, &QTimer::timeout, this, &QWidget::hide);
}

void RibbonToolTip::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 6, 6);
    setMask(QRegion(path.toFillPolygon().toPolygon()));   // cantos arredondados sem depender de janela translucida
}

void RibbonToolTip::fill(const Content &content)
{
    m_content = content;
    m_title->setText(content.title);
    m_badge->setText(content.shortcut);
    m_badge->setVisible(!content.shortcut.isEmpty());
    m_description->setText(content.description);
    m_description->setVisible(!content.description.isEmpty());
    m_line->setVisible(true);
}

void RibbonToolTip::showFor(QWidget *anchor, const Content &content)
{
    if (!anchor) {
        return;
    }
    auto &tip = instance();
    tip.fill(content);
    tip.adjustSize();
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 6));
    if (const QScreen *screen = QGuiApplication::screenAt(anchor->mapToGlobal(QPoint(anchor->width() / 2, anchor->height() / 2)))) {
        const QRect area = screen->availableGeometry();
        pos.setX(std::clamp(pos.x(), area.left() + 4, std::max(area.left() + 4, area.right() - tip.width() - 4)));
        if (pos.y() + tip.height() > area.bottom()) {
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - tip.height() - 6);
        }
    }
    tip.move(pos);
    tip.show();
    tip.raise();
    tip.m_timeout->start();
}

void RibbonToolTip::hideTip()
{
    instance().m_timeout->stop();
    instance().hide();
}

const RibbonToolTip::Content &RibbonToolTip::last()
{
    return instance().m_content;
}

// ---- botao ---------------------------------------------------------------------------------------------------------------------------

RibbonButton::RibbonButton(const RibbonItem &item, bool large, QWidget *parent)
    : QToolButton(parent)
    , m_item(item)
    , m_large(large)
{
    setDefaultAction(item.action);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setAutoRaise(false);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setToolTip(QString());
    m_label = item.label.isEmpty() ? item.action->text() : item.label;
    m_icon = item.icon.isNull() ? item.action->icon() : item.icon;
    if (!item.variants.isEmpty()) {
        m_split = true;
        buildMenu(item.variants);
    }
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(sizeHint());
}

RibbonButton::RibbonButton(const QString &label, const QIcon &icon, QMenu *menu, QWidget *parent)
    : QToolButton(parent)
    , m_label(label)
    , m_icon(icon)
    , m_large(true)
    , m_dropdownOnly(true)
    , m_menu(menu)
{
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setAutoRaise(false);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(sizeHint());
}

void RibbonButton::buildMenu(const QList<RibbonVariant> &variants)
{
    m_menu = new QMenu(this);
    QList<QPair<QAction *, QAction *>> proxies;
    for (const auto &variant : variants) {
        if (variant.separator || !variant.action) {
            m_menu->addSeparator();
            continue;
        }
        const QString shortcut = variant.action->shortcut().toString(QKeySequence::NativeText);
        QAction *proxy = m_menu->addAction(shortcut.isEmpty() ? variant.text : variant.text + QLatin1Char('\t') + shortcut);
        proxy->setIcon(variant.action->icon());
        QAction *target = variant.action;
        connect(proxy, &QAction::triggered, target, &QAction::trigger);
        proxies.append(qMakePair(proxy, target));
    }
    connect(m_menu, &QMenu::aboutToShow, this, [proxies]() {
        for (const auto &pair : proxies) {
            pair.first->setEnabled(pair.second->isEnabled());
        }
    });
}

QStringList RibbonButton::labelLines() const
{
    QString text = m_label;
    if (!m_large) {
        text.replace(QLatin1Char('\n'), QLatin1Char(' '));
        return {text};
    }
    return splitLabel(text, QFontMetrics(labelFont()), 84);
}

QSize RibbonButton::sizeHint() const
{
    const QFontMetrics fm(labelFont());
    const QStringList lines = labelLines();
    int textWidth = 0;
    for (const QString &line : lines) {
        textWidth = std::max(textWidth, fm.horizontalAdvance(line));
    }
    if (m_large) {
        int w = std::max(56, textWidth + 18);
        if ((m_split || m_dropdownOnly) && lines.size() > 1) {
            w = std::max(w, fm.horizontalAdvance(lines.last()) + 28);
        }
        return {w, RibbonStyle::kLargeHeight};
    }
    int w = 6 + RibbonStyle::kSmallIcon + 6 + textWidth + 8;
    if (m_split) {
        w += 16;
    }
    return {w, RibbonStyle::kSmallHeight};
}

QRect RibbonButton::mainZone() const
{
    if (m_dropdownOnly) {
        return {};
    }
    if (!m_split) {
        return rect();
    }
    return m_large ? QRect(0, 0, width(), 42) : QRect(0, 0, width() - 16, height());
}

QRect RibbonButton::arrowZone() const
{
    if (m_dropdownOnly) {
        return rect();
    }
    if (!m_split) {
        return {};
    }
    return m_large ? QRect(0, 42, width(), height() - 42) : QRect(width() - 16, 0, 16, height());
}

int RibbonButton::zoneAt(const QPoint &p) const
{
    if (arrowZone().contains(p)) {
        return Arrow;
    }
    return mainZone().contains(p) ? Main : None;
}

QPixmap RibbonButton::iconPixmap(int size, QIcon::Mode mode) const
{
    return m_icon.pixmap(QSize(size, size), devicePixelRatioF(), mode);
}

RibbonToolTip::Content RibbonButton::tipContent() const
{
    RibbonToolTip::Content content;
    QString title = m_label;
    title.replace(QLatin1Char('\n'), QLatin1Char(' '));
    content.title = title;
    if (QAction *action = defaultAction()) {
        content.shortcut = action->shortcut().toString(QKeySequence::NativeText);
        const QString tip = action->toolTip();
        const QString plain = action->text();
        content.description = !m_item.description.isEmpty() ? m_item.description : (tip != plain ? tip : QString());
    } else {
        content.description = tr("Mostra os comandos do grupo.");
    }
    return content;
}

bool RibbonButton::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        if (!m_menuOpen) {
            RibbonToolTip::showFor(this, tipContent());
            // memoriza para a ajuda contextual (F1) mesmo depois de a dica sumir
        }
        return true;
    }
    if (event->type() == QEvent::Hide || event->type() == QEvent::EnabledChange) {
        RibbonToolTip::hideTip();
    }
    return QToolButton::event(event);
}

void RibbonButton::showMenu()
{
    if (!m_menu) {
        return;
    }
    RibbonToolTip::hideTip();
    m_menuOpen = true;
    update();
    m_menu->exec(mapToGlobal(QPoint(0, height())));
    m_menuOpen = false;
    m_pressed = None;
    update();
}

void RibbonButton::mousePressEvent(QMouseEvent *event)
{
    RibbonToolTip::hideTip();
    if (event->button() == Qt::LeftButton && (m_dropdownOnly || (m_split && zoneAt(event->pos()) == Arrow))) {
        m_pressed = Arrow;
        showMenu();
        event->accept();
        return;
    }
    m_pressed = Main;
    QToolButton::mousePressEvent(event);
}

void RibbonButton::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = None;
    QToolButton::mouseReleaseEvent(event);
}

void RibbonButton::mouseMoveEvent(QMouseEvent *event)
{
    const int zone = zoneAt(event->pos());
    if (zone != m_hover) {
        m_hover = zone;
        update();
    }
    QToolButton::mouseMoveEvent(event);
}

void RibbonButton::leaveEvent(QEvent *event)
{
    m_hover = None;
    RibbonToolTip::hideTip();
    update();
    QToolButton::leaveEvent(event);
}

void RibbonButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const bool enabled = isEnabled() && (m_dropdownOnly || (defaultAction() && defaultAction()->isEnabled()));
    const bool active = enabled && isChecked();
    const QColor hoverFill(QStringLiteral("#dfe8f3")), hoverBorder(QStringLiteral("#c6d3e4"));
    const QColor pressedFill(QStringLiteral("#cdd8e7")), pressedBorder(QStringLiteral("#b3c3d9"));
    const QColor activeFill(QStringLiteral("#d6e7f3"));
    const QRectF whole = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    auto paintZone = [&](const QRectF &zone, bool hover, bool pressed) {
        if (!enabled) {
            return;
        }
        if (pressed) {
            p.setPen(QPen(pressedBorder, 1));
            p.setBrush(pressedFill);
            p.drawRoundedRect(zone, 4, 4);
        } else if (hover) {
            p.setPen(QPen(hoverBorder, 1));
            p.setBrush(hoverFill);
            p.drawRoundedRect(zone, 4, 4);
        }
    };
    if (active) {
        p.setPen(QPen(color(RibbonStyle::kAccent), 1.5));
        p.setBrush(activeFill);
        p.drawRoundedRect(whole.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
    const bool mouseIn = underMouse();
    if (m_split || m_dropdownOnly) {
        const QRectF mz = QRectF(mainZone()).adjusted(0.5, 0.5, -0.5, -0.5), az = QRectF(arrowZone()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (m_split) {
            paintZone(mz, mouseIn && m_hover == Main, isDown() && m_pressed == Main);
        }
        paintZone(az, mouseIn && m_hover == Arrow, m_menuOpen);
        if (!active && enabled && mouseIn) {
            p.setPen(QPen(hoverBorder, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(whole, 4, 4);
        }
    } else {
        paintZone(whole, mouseIn, isDown());
    }

    const QColor textColor = !enabled ? QColor(QStringLiteral("#9aa5b4")) : (m_item.danger ? color(RibbonStyle::kDanger) : color(RibbonStyle::kText));
    const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
    p.setFont(labelFont());
    const QFontMetrics fm(p.font());
    const QStringList lines = labelLines();
    if (m_large) {
        const int iconSize = RibbonStyle::kLargeIcon;
        const QPixmap pixmap = iconPixmap(iconSize, mode);
        p.drawPixmap((width() - iconSize) / 2, 5, iconSize, iconSize, pixmap);
        const int lineHeight = fm.lineSpacing() - 1;
        int y = 5 + iconSize + 3;
        p.setPen(textColor);
        for (int i = 0; i < lines.size(); ++i) {
            const QRect lineRect(2, y + i * lineHeight, width() - 4, lineHeight);
            int shift = 0;
            if ((m_split || m_dropdownOnly) && i == lines.size() - 1 && lines.size() > 1) {
                shift = -6;   // abre espaco para a seta ao lado da ultima linha
            }
            p.drawText(lineRect.translated(shift, 0), Qt::AlignHCenter | Qt::AlignVCenter, lines.at(i));
            if ((m_split || m_dropdownOnly) && i == lines.size() - 1 && lines.size() > 1) {
                const int textWidth = fm.horizontalAdvance(lines.at(i));
                drawChevron(p, QPointF(width() / 2.0 + textWidth / 2.0 + 4 + shift, lineRect.center().y() + 0.5), enabled ? textColor : QColor(QStringLiteral("#9aa5b4")));
            }
        }
        if ((m_split || m_dropdownOnly) && lines.size() == 1) {
            drawChevron(p, QPointF(width() / 2.0, y + lineHeight + lineHeight / 2.0 - 1), enabled ? textColor : QColor(QStringLiteral("#9aa5b4")));
        }
    } else {
        const int iconSize = RibbonStyle::kSmallIcon;
        const QPixmap pixmap = iconPixmap(iconSize, mode);
        p.drawPixmap(6, (height() - iconSize) / 2, iconSize, iconSize, pixmap);
        p.setPen(textColor);
        const int right = width() - 8 - (m_split ? 16 : 0);
        const QString text = fm.elidedText(lines.first(), Qt::ElideRight, right - 28);
        p.drawText(QRect(28, 0, right - 28, height()), Qt::AlignLeft | Qt::AlignVCenter, text);
        if (m_split) {
            drawChevron(p, QPointF(width() - 8, height() / 2.0), enabled ? textColor : QColor(QStringLiteral("#9aa5b4")));
        }
    }
}

// ---- grupo ---------------------------------------------------------------------------------------------------------------------------

RibbonGroup::RibbonGroup(const QString &title, QList<RibbonItem> items, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_items(std::move(items))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 0);
    m_layout->setSpacing(2);
    m_caption = new QLabel(title, this);
    m_caption->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_caption->setStyleSheet(QStringLiteral("QLabel { color: #6b7686; font-size: 11px; }"));
    m_layout->addWidget(m_caption);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    rebuild();
}

void RibbonGroup::setLevel(int level)
{
    level = std::clamp(level, 0, 2);
    if (level == m_level) {
        return;
    }
    m_level = level;
    rebuild();
}

QWidget *RibbonGroup::buildColumns(int level, QWidget *parent, QMenu *closeOn) const
{
    auto *content = new QWidget(parent);
    auto *row = new QHBoxLayout(content);
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(2);
    QVBoxLayout *column = nullptr;
    int count = 0;
    auto add = [&](RibbonButton *button) {
        if (closeOn) {
            connect(button, &QToolButton::clicked, closeOn, &QMenu::close);
        }
        return button;
    };
    for (const RibbonItem &item : m_items) {
        if (!item.action) {
            continue;
        }
        const bool large = level == 0 && item.large;
        if (large) {
            column = nullptr;
            row->addWidget(add(new RibbonButton(item, true, content)), 0, Qt::AlignTop);
            continue;
        }
        if (!column || count >= 3) {
            column = new QVBoxLayout();
            column->setContentsMargins(0, 0, 0, 0);
            column->setSpacing(0);
            row->addLayout(column);
            count = 0;
        }
        column->addWidget(add(new RibbonButton(item, false, content)), 0, Qt::AlignTop);
        ++count;
    }
    for (int i = 0; i < row->count(); ++i) {
        if (auto *box = qobject_cast<QVBoxLayout *>(row->itemAt(i)->layout())) {
            box->addStretch(1);
        }
    }
    row->setAlignment(Qt::AlignTop);
    return content;
}

void RibbonGroup::rebuild()
{
    if (m_content) {
        m_layout->removeWidget(m_content);
        m_content->deleteLater();
        m_content = nullptr;
    }
    if (m_level >= 2) {
        QIcon icon;
        for (const RibbonItem &item : m_items) {
            if (item.action && (!item.icon.isNull() || !item.action->icon().isNull())) {
                icon = item.icon.isNull() ? item.action->icon() : item.icon;
                break;
            }
        }
        auto *host = new QWidget(this);
        auto *hostLayout = new QHBoxLayout(host);
        hostLayout->setContentsMargins(0, 2, 0, 0);
        auto *menu = new QMenu(host);
        auto *widgetAction = new QWidgetAction(menu);
        auto *panel = new QWidget(menu);
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(8, 6, 8, 6);
        panelLayout->setSpacing(4);
        panelLayout->addWidget(buildColumns(1, panel, menu));
        auto *title = new QLabel(m_title, panel);
        title->setAlignment(Qt::AlignHCenter);
        title->setStyleSheet(QStringLiteral("QLabel { color: #6b7686; font-size: 11px; }"));
        panelLayout->addWidget(title);
        widgetAction->setDefaultWidget(panel);
        menu->addAction(widgetAction);
        hostLayout->addWidget(new RibbonButton(m_title, icon, menu, host), 0, Qt::AlignTop);
        m_content = host;
        m_caption->hide();
    } else {
        m_content = buildColumns(m_level, this, nullptr);
        m_caption->show();
    }
    m_layout->insertWidget(0, m_content, 0, Qt::AlignTop);
    updateGeometry();
}

// ---- pagina --------------------------------------------------------------------------------------------------------------------------

RibbonPage::RibbonPage(QWidget *parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("RibbonPageScroll"));
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFixedHeight(kHeight);
    setStyleSheet(QStringLiteral(
        "QScrollArea#RibbonPageScroll { background: #f2f5fa; border: none; }"
        "QScrollArea#RibbonPageScroll > QWidget > QWidget { background: #f2f5fa; }"
        "QScrollBar:horizontal { background: #e7edf5; height: 11px; margin: 0 12px; border-radius: 5px; }"
        "QScrollBar::handle:horizontal { background: #b9c6d8; border-radius: 5px; min-width: 40px; }"
        "QScrollBar::handle:horizontal:hover { background: #9fb0c8; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"));
    m_inner = new QWidget(this);
    m_inner->setObjectName(QStringLiteral("RibbonPage"));
    m_layout = new QHBoxLayout(m_inner);
    m_layout->setContentsMargins(6, 6, 6, 0);
    m_layout->setSpacing(0);
    m_layout->addStretch(1);
    setWidget(m_inner);
}

RibbonGroup *RibbonPage::addGroup(const QString &title, const QList<RibbonItem> &items)
{
    if (!m_groups.isEmpty()) {
        auto *separator = new QFrame(m_inner);
        separator->setFixedSize(1, 86);
        separator->setStyleSheet(QStringLiteral("background: #d6dde8;"));
        m_layout->insertWidget(m_layout->count() - 1, separator, 0, Qt::AlignTop);
        m_layout->insertSpacing(m_layout->count() - 1, 0);
    }
    auto *group = new RibbonGroup(title, items, m_inner);
    m_layout->insertWidget(m_layout->count() - 1, group, 0, Qt::AlignTop);
    m_groups.append(group);
    return group;
}

void RibbonPage::setLevel(int level)
{
    for (RibbonGroup *group : m_groups) {
        group->setLevel(level);
    }
}

void RibbonPage::wheelEvent(QWheelEvent *event)
{
    auto *bar = horizontalScrollBar();
    const int delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
    bar->setValue(bar->value() - delta);
    event->accept();
}

} // namespace Structura::UI
