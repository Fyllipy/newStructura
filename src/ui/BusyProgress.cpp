#include "BusyProgress.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <thread>

namespace Structura::UI {

namespace {

std::atomic<int> g_busyDepth {0};
class ProgressPopup;
ProgressPopup *g_scopePopup = nullptr;   // janela de um BusyScope ativo (calculos internos a reaproveitam)

constexpr int kShowDelayMs = 250;

/// Janela flutuante com o nome do calculo, o detalhe da etapa e uma barra de progresso animada.
class ProgressPopup final : public QWidget
{
public:
    ProgressPopup(QWidget *owner, const QString &calculation, const QString &detail)
        : QWidget(owner ? owner->window() : nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
        , m_calculation(calculation)
        , m_detail(detail)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setFocusPolicy(Qt::NoFocus);
        resize(480, 190);
        m_elapsed.start();
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_phase = std::fmod(m_phase + 0.018, 1.0);
            update();
        });
        m_timer.start(16);
    }

    void setState(int value, int total, const QString &detail)
    {
        m_value = value;
        m_total = total;
        if (!detail.isEmpty()) {
            m_detail = detail;
        }
        update();
    }

    void present()
    {
        QWidget *owner = parentWidget();
        QRect area = owner ? owner->geometry() : QRect();
        if (!owner || area.isEmpty()) {
            if (auto *screen = QGuiApplication::primaryScreen()) {
                area = screen->availableGeometry();
            }
        }
        move(area.center() - QPoint(width() / 2, height() / 2));
        show();
        raise();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF card = QRectF(rect()).adjusted(14, 10, -14, -18);
        // sombra suave
        for (int i = 0; i < 8; ++i) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(20, 40, 70, 9));
            p.drawRoundedRect(card.adjusted(-i, -i + 4, i, i + 4), 14 + i, 14 + i);
        }
        QLinearGradient bg(card.topLeft(), card.bottomLeft());
        bg.setColorAt(0.0, QColor(QStringLiteral("#ffffff")));
        bg.setColorAt(1.0, QColor(QStringLiteral("#f3f7fc")));
        p.setBrush(bg);
        p.setPen(QPen(QColor(QStringLiteral("#cfd9e8")), 1));
        p.drawRoundedRect(card, 12, 12);
        // faixa de destaque no topo
        QPainterPath top;
        top.addRoundedRect(card, 12, 12);
        p.save();
        p.setClipPath(top);
        QLinearGradient accent(card.topLeft(), card.topRight());
        accent.setColorAt(0.0, QColor(QStringLiteral("#0d7fb9")));
        accent.setColorAt(1.0, QColor(QStringLiteral("#35b6e8")));
        p.fillRect(QRectF(card.left(), card.top(), card.width(), 5), accent);
        p.restore();

        // indicador giratorio
        const QPointF center(card.left() + 44, card.top() + 56);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(QStringLiteral("#dbe6f3")), 5));
        p.drawEllipse(center, 17, 17);
        QPen arc(QColor(QStringLiteral("#0d7fb9")), 5);
        arc.setCapStyle(Qt::RoundCap);
        p.setPen(arc);
        p.drawArc(QRectF(center.x() - 17, center.y() - 17, 34, 34), static_cast<int>(-m_phase * 360.0 * 16.0), 100 * 16);

        const double textLeft = card.left() + 80;
        QFont title = font();
        title.setPointSizeF(10.0);
        title.setBold(true);
        p.setFont(title);
        p.setPen(QColor(QStringLiteral("#0b6da1")));
        const int dots = 1 + static_cast<int>(m_phase * 3.0) % 3;
        p.drawText(QRectF(textLeft, card.top() + 20, card.width() - 100, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Calculando%1").arg(QString(dots, QLatin1Char('.'))));
        QFont name = font();
        name.setPointSizeF(12.5);
        name.setBold(true);
        p.setFont(name);
        p.setPen(QColor(QStringLiteral("#1e232b")));
        p.drawText(QRectF(textLeft, card.top() + 40, card.width() - 100, 24), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(name).elidedText(m_calculation, Qt::ElideRight, static_cast<int>(card.width() - 100)));
        QFont small = font();
        small.setPointSizeF(9.0);
        p.setFont(small);
        p.setPen(QColor(QStringLiteral("#5a6472")));
        p.drawText(QRectF(textLeft, card.top() + 66, card.width() - 100, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(small).elidedText(m_detail, Qt::ElideRight, static_cast<int>(card.width() - 100)));

        // barra de progresso
        const QRectF track(card.left() + 24, card.top() + 106, card.width() - 48, 12);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#e1e9f4")));
        p.drawRoundedRect(track, 6, 6);
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(track, 6, 6);
        p.setClipPath(clip);
        QLinearGradient fill(track.topLeft(), track.topRight());
        fill.setColorAt(0.0, QColor(QStringLiteral("#0d7fb9")));
        fill.setColorAt(1.0, QColor(QStringLiteral("#3ec1ee")));
        p.setBrush(fill);
        if (m_total > 0) {
            const double fraction = std::clamp(static_cast<double>(m_value) / m_total, 0.0, 1.0);
            p.drawRoundedRect(QRectF(track.left(), track.top(), std::max(12.0, track.width() * fraction), track.height()), 6, 6);
            // brilho em movimento
            const double glowX = track.left() + track.width() * fraction * m_phase;
            QLinearGradient glow(glowX - 40, 0, glowX + 40, 0);
            glow.setColorAt(0.0, QColor(255, 255, 255, 0));
            glow.setColorAt(0.5, QColor(255, 255, 255, 110));
            glow.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(glow);
            p.drawRect(QRectF(track.left(), track.top(), track.width() * fraction, track.height()));
        } else {
            const double chunk = track.width() * 0.32;
            const double x = track.left() - chunk + (track.width() + chunk) * m_phase;
            p.drawRoundedRect(QRectF(x, track.top(), chunk, track.height()), 6, 6);
        }
        p.restore();

        QFont foot = small;
        foot.setPointSizeF(8.5);
        p.setFont(foot);
        p.setPen(QColor(QStringLiteral("#7b8797")));
        const qint64 seconds = m_elapsed.elapsed() / 1000;
        QString left = m_total > 0 ? QStringLiteral("%1 de %2").arg(std::min(m_value, m_total)).arg(m_total) : QStringLiteral("Aguarde, o programa está trabalhando");
        p.drawText(QRectF(track.left(), track.bottom() + 8, track.width() * 0.7, 18), Qt::AlignLeft | Qt::AlignVCenter, left);
        p.drawText(QRectF(track.left(), track.bottom() + 8, track.width(), 18), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0')));
    }

private:
    QString m_calculation;
    QString m_detail;
    int m_value {0};
    int m_total {0};
    double m_phase {0.0};
    QTimer m_timer;
    QElapsedTimer m_elapsed;
};

} // namespace

void BusyProgress::report(int value, int total, const QString &detail)
{
    m_value = value;
    m_total = total;
    if (!detail.isEmpty()) {
        setDetail(detail);
    }
}

void BusyProgress::setDetail(const QString &detail)
{
    QMutexLocker lock(&m_mutex);
    m_detail = detail;
    m_changed = true;
}

QString BusyProgress::detail() const
{
    QMutexLocker lock(&m_mutex);
    return m_detail;
}

bool BusyProgress::takeDetailChanged()
{
    QMutexLocker lock(&m_mutex);
    const bool changed = m_changed;
    m_changed = false;
    return changed;
}

bool isBusy()
{
    return g_busyDepth.load() > 0;
}

struct BusyScope::Impl {
    ProgressPopup *popup {nullptr};
    bool counted {false};
};

BusyScope::BusyScope(QWidget *parent, const QString &calculation, const QString &detail)
    : m_impl(new Impl)
{
    if (!QCoreApplication::instance() || QThread::currentThread() != QCoreApplication::instance()->thread()) {
        return;
    }
    ++g_busyDepth;
    m_impl->counted = true;
    QApplication::setOverrideCursor(Qt::BusyCursor);
    m_impl->popup = new ProgressPopup(parent, calculation, detail);
    g_scopePopup = m_impl->popup;
    m_impl->popup->setState(0, 0, detail);
    m_impl->popup->present();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
}

BusyScope::~BusyScope()
{
    if (m_impl->popup && g_scopePopup == m_impl->popup) {
        g_scopePopup = nullptr;
    }
    if (m_impl->counted) {
        QApplication::restoreOverrideCursor();
        --g_busyDepth;
    }
    delete m_impl->popup;
    delete m_impl;
}

void BusyScope::update(int value, int total, const QString &detail)
{
    if (!m_impl->popup) {
        return;
    }
    m_impl->popup->setState(value, total, detail);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
}

void runBusyImpl(QWidget *parent, const QString &calculation, const QString &detail, const std::function<void(BusyProgress &)> &job)
{
    BusyProgress progress;
    const bool onUiThread = QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread();
    if (!onUiThread || (g_busyDepth.load() > 0 && !g_scopePopup)) {
        job(progress);   // aninhado (ou fora da interface): executa direto
        return;
    }
    if (g_scopePopup) {
        // dentro de um BusyScope: o calculo roda em outra thread e a janela do escopo continua animada e informando a etapa
        std::exception_ptr failure;
        std::atomic<bool> finished {false};
        std::thread worker([&]() {
            try {
                job(progress);
            } catch (...) {
                failure = std::current_exception();
            }
            finished = true;
        });
        while (!finished.load()) {
            (void)progress.takeDetailChanged();
            g_scopePopup->setState(progress.value(), progress.total(), progress.detail().isEmpty() ? detail : progress.detail());
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
            QThread::msleep(8);
        }
        worker.join();
        if (failure) {
            std::rethrow_exception(failure);
        }
        return;
    }
    ++g_busyDepth;
    std::exception_ptr failure;
    std::atomic<bool> finished {false};
    std::thread worker([&]() {
        try {
            job(progress);
        } catch (...) {
            failure = std::current_exception();
        }
        finished = true;
    });
    ProgressPopup *popup = nullptr;
    QElapsedTimer clock;
    clock.start();
    QApplication::setOverrideCursor(Qt::BusyCursor);
    while (!finished.load()) {
        if (!popup && clock.elapsed() > kShowDelayMs) {
            popup = new ProgressPopup(parent, calculation, detail);
            popup->setState(progress.value(), progress.total(), progress.detail());
            popup->present();
        }
        if (popup) {
            (void)progress.takeDetailChanged();
            popup->setState(progress.value(), progress.total(), progress.detail());
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
        QThread::msleep(8);
    }
    worker.join();
    QApplication::restoreOverrideCursor();
    delete popup;
    --g_busyDepth;
    if (failure) {
        std::rethrow_exception(failure);
    }
}

} // namespace Structura::UI
