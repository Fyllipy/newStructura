#include "JointCharts.h"

#include <QFontMetricsF>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSvgGenerator>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

using namespace Structura::Design::JointClass;

namespace {

const QColor kPositive(0x0d, 0x7f, 0xb9);
const QColor kNegative(0xc8, 0x5a, 0x12);
const QColor kGrid(0xdd, 0xe3, 0xea);
const QColor kText(0x1f, 0x2a, 0x37);
const QColor kMuted(0x6b, 0x77, 0x85);

QString num(double v, int digits = 4)
{
    const double a = std::abs(v);
    if (a >= 1.0e4 && a < 1.0e9) {
        return QString::number(v, 'f', 0);
    }
    return QString::number(v, 'g', std::max(digits, 4));
}

double niceStep(double range, int target)
{
    const double raw = range / std::max(target, 1);
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / mag;
    return (f < 1.5 ? 1.0 : f < 3.5 ? 2.0 : f < 7.5 ? 5.0 : 10.0) * mag;
}

QString classText(const Classification &c)
{
    return QString::fromStdString(className(c.final));
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------
JointChartBase::JointChartBase(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(360, 260);
    setMouseTracking(true);
    setAutoFillBackground(false);
}

void JointChartBase::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawChart(painter, QRectF(rect()));
}

bool JointChartBase::exportPng(const QString &path)
{
    QImage image(size() * 2, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(2.0, 2.0);
    drawChart(painter, QRectF(QPointF(0, 0), QSizeF(size())));
    painter.end();
    return image.save(path, "PNG");
}

bool JointChartBase::exportSvg(const QString &path, const QString &title)
{
    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setSize(size());
    generator.setViewBox(QRect(QPoint(0, 0), size()));
    generator.setTitle(title);
    QPainter painter;
    if (!painter.begin(&generator)) {
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawChart(painter, QRectF(QPointF(0, 0), QSizeF(size())));
    painter.end();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
MomentRotationChart::MomentRotationChart(QWidget *parent)
    : JointChartBase(parent)
{
}

void MomentRotationChart::setAnalysis(const JointAnalysis &analysis)
{
    m_analysis = analysis;
    if (!m_userView) {
        fitView();
    }
    update();
}

void MomentRotationChart::setUnits(const JointChartUnits &units)
{
    m_units = units;
    m_userView = false;
    fitView();
    update();
}

void MomentRotationChart::setHint(const QString &hint)
{
    m_hint = hint;
    update();
}

void MomentRotationChart::resetView()
{
    m_userView = false;
    fitView();
    update();
}

void MomentRotationChart::fitView()
{
    double thMax = 0.0, mMax = 0.0, thMin = 0.0, mMin = 0.0;
    auto scan = [&](const DirectionAnalysis &d, double sign) {
        if (!d.hasData) {
            return;
        }
        for (const CurvePoint &p : d.behavior.curve) {
            (sign > 0 ? thMax : thMin) = sign > 0 ? std::max(thMax, p.theta) : std::min(thMin, -p.theta);
            (sign > 0 ? mMax : mMin) = sign > 0 ? std::max(mMax, p.moment) : std::min(mMin, -p.moment);
        }
        if (d.stiffness.valid) {
            (sign > 0 ? thMax : thMin) = sign > 0 ? std::max(thMax, 1.3 * d.stiffness.theta23) : std::min(thMin, -1.3 * d.stiffness.theta23);
            (sign > 0 ? mMax : mMin) = sign > 0 ? std::max(mMax, 1.15 * d.stiffness.momentRd) : std::min(mMin, -1.15 * d.stiffness.momentRd);
        }
    };
    scan(m_analysis.positive, 1.0);
    scan(m_analysis.negative, -1.0);
    if (thMax == 0.0 && thMin == 0.0) {
        thMax = 0.02;
    }
    if (mMax == 0.0 && mMin == 0.0) {
        mMax = 100.0;
    }
    const double tf = m_units.thetaFactor, mf = m_units.momentFactor;
    const double x0 = thMin == 0.0 ? 0.0 : thMin * 1.08, x1 = thMax == 0.0 ? 0.0 : thMax * 1.08;
    const double y0 = mMin == 0.0 ? 0.0 : mMin * 1.08, y1 = mMax == 0.0 ? 0.0 : mMax * 1.08;
    m_view = QRectF(QPointF(x0 * tf, y0 * mf), QPointF(x1 * tf, y1 * mf)).normalized();
    if (m_view.width() <= 0.0) {
        m_view.setWidth(1.0);
    }
    if (m_view.height() <= 0.0) {
        m_view.setHeight(1.0);
    }
}

QRectF MomentRotationChart::plotArea(const QRectF &area) const
{
    return area.adjusted(78, 40, -24, -52);
}

QPointF MomentRotationChart::toScreen(const QPointF &d, const QRectF &plot) const
{
    return {plot.left() + (d.x() - m_view.left()) / m_view.width() * plot.width(), plot.bottom() - (d.y() - m_view.top()) / m_view.height() * plot.height()};
}

QPointF MomentRotationChart::toData(const QPointF &s, const QRectF &plot) const
{
    return {m_view.left() + (s.x() - plot.left()) / plot.width() * m_view.width(), m_view.top() + (plot.bottom() - s.y()) / plot.height() * m_view.height()};
}

void MomentRotationChart::drawChart(QPainter &p, const QRectF &area)
{
    p.fillRect(area, Qt::white);
    const QRectF plot = plotArea(area);
    p.setPen(kText);
    QFont base = p.font();
    base.setPointSizeF(std::max(8.0, base.pointSizeF()));
    p.setFont(base);
    p.drawText(QRectF(area.left() + 8, area.top() + 6, area.width() - 16, 22), Qt::AlignLeft | Qt::AlignVCenter,
               QObject::tr("Diagrama momento-rotação da ligação"));
    // grade e rotulos
    const double xs = niceStep(m_view.width(), 8), ys = niceStep(m_view.height(), 6);
    p.setClipRect(plot.adjusted(-1, -1, 1, 1));
    for (double x = std::ceil(m_view.left() / xs) * xs; x <= m_view.right() + 1e-12; x += xs) {
        const QPointF a = toScreen({x, m_view.top()}, plot);
        p.setPen(std::abs(x) < xs * 1e-6 ? QPen(kMuted, 1.4) : QPen(kGrid, 1));
        p.drawLine(QPointF(a.x(), plot.top()), QPointF(a.x(), plot.bottom()));
    }
    for (double y = std::ceil(m_view.top() / ys) * ys; y <= m_view.bottom() + 1e-12; y += ys) {
        const QPointF a = toScreen({m_view.left(), y}, plot);
        p.setPen(std::abs(y) < ys * 1e-6 ? QPen(kMuted, 1.4) : QPen(kGrid, 1));
        p.drawLine(QPointF(plot.left(), a.y()), QPointF(plot.right(), a.y()));
    }
    p.setClipping(false);
    p.setPen(kMuted);
    for (double x = std::ceil(m_view.left() / xs) * xs; x <= m_view.right() + 1e-12; x += xs) {
        const QPointF a = toScreen({x, m_view.top()}, plot);
        p.drawText(QRectF(a.x() - 30, plot.bottom() + 4, 60, 16), Qt::AlignCenter, num(x, 4));
    }
    for (double y = std::ceil(m_view.top() / ys) * ys; y <= m_view.bottom() + 1e-12; y += ys) {
        const QPointF a = toScreen({m_view.left(), y}, plot);
        p.drawText(QRectF(plot.left() - 72, a.y() - 8, 66, 16), Qt::AlignRight | Qt::AlignVCenter, num(y, 4));
    }
    p.setPen(kText);
    p.drawText(QRectF(plot.left(), plot.bottom() + 24, plot.width(), 18), Qt::AlignCenter, QObject::tr("Rotação relativa θ (%1)").arg(m_units.thetaLabel));
    p.save();
    p.translate(area.left() + 14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2, -8, plot.height(), 16), Qt::AlignCenter, QObject::tr("Momento M (%1)").arg(m_units.momentLabel));
    p.restore();
    p.setPen(QPen(kMuted, 1));
    p.drawRect(plot);

    m_drawnCurves = 0;
    m_insufficient = true;
    QStringList legend;
    QVector<QColor> legendColors;
    p.setClipRect(plot);
    const double tf = m_units.thetaFactor, mf = m_units.momentFactor;
    auto drawDirection = [&](const DirectionAnalysis &d, double sign, const QColor &color, const QString &name) {
        if (!d.hasData) {
            return;
        }
        auto P = [&](double theta, double moment) { return toScreen({sign * theta * tf, sign * moment * mf}, plot); };
        if (!d.behavior.curve.empty()) {
            QPainterPath path;
            path.moveTo(P(0.0, 0.0));
            for (const CurvePoint &pt : d.behavior.curve) {
                path.lineTo(P(pt.theta, pt.moment));
            }
            p.setPen(QPen(color, 2.4));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setBrush(Qt::white);
            for (const CurvePoint &pt : d.behavior.curve) {
                p.drawEllipse(P(pt.theta, pt.moment), 3.0, 3.0);
            }
            ++m_drawnCurves;
            m_insufficient = false;
            legend << name + (m_analysis.behavior.origin == DataOrigin::ComponentMethod ? QObject::tr(" (curva CALCULADA: método dos componentes)") : QObject::tr(" (curva INFORMADA/importada, não calculada)"));
            legendColors << color;
        }
        const StiffnessResult &s = d.stiffness;
        if (!s.valid) {
            return;
        }
        if (s.momentRd > 0.0) {
            // Mj,Rd e 2/3 Mj,Rd
            p.setPen(QPen(kMuted, 1.2, Qt::DashLine));
            p.drawLine(QPointF(plot.left(), P(0.0, s.momentRd).y()), QPointF(plot.right(), P(0.0, s.momentRd).y()));
            p.drawLine(QPointF(plot.left(), P(0.0, s.moment23).y()), QPointF(plot.right(), P(0.0, s.moment23).y()));
            p.setPen(color);
            const double ly = sign > 0 ? 1.0 : -1.0;
            p.drawText(QPointF(plot.left() + 6, P(0.0, s.momentRd).y() - 4 * ly), QObject::tr("Mj,Rd = %1 %2").arg(num(sign * s.momentRd * mf)).arg(m_units.momentLabel));
            p.drawText(QPointF(plot.left() + 6, P(0.0, s.moment23).y() - 4 * ly), QObject::tr("2/3 Mj,Rd = %1 %2").arg(num(sign * s.moment23 * mf)).arg(m_units.momentLabel));
            if (s.theta23 > 0.0) {
                // reta secante da origem ao ponto de 2/3, prolongada ate Mj,Rd
                const double thetaAtMrd = s.stiffness > 0.0 ? s.momentRd / s.stiffness : 0.0;
                p.setPen(QPen(color, 1.8, Qt::DashDotLine));
                p.drawLine(P(0.0, 0.0), P(thetaAtMrd, s.momentRd));
                // ponto de 2/3 e projecao no eixo
                p.setPen(QPen(color, 1.0, Qt::DotLine));
                p.drawLine(P(s.theta23, s.moment23), P(s.theta23, 0.0));
                p.setPen(QPen(Qt::white, 1.5));
                p.setBrush(color);
                p.drawEllipse(P(s.theta23, s.moment23), 5.5, 5.5);
                p.setPen(color);
                p.drawText(P(s.theta23, 0.0) + QPointF(4, sign > 0 ? -6 : 14), QObject::tr("θ(2/3) = %1 %2").arg(num(sign * s.theta23 * tf)).arg(m_units.thetaLabel));
                p.drawText(P(thetaAtMrd * 0.55, s.momentRd * 0.55) + QPointF(8, sign > 0 ? 14 : -6),
                           QObject::tr("Si = %1 kN·m/rad").arg(num(s.stiffness)));
                legend << name + QObject::tr(" (secante Si)");
                legendColors << color;
                if (d.behavior.curve.empty()) {
                    m_insufficient = false;   // so o ponto e a secante (sem curva): nada e inventado alem deles
                }
            }
        }
        if (!d.behavior.curve.empty()) {
            double mMax = 0.0;
            double thAtMax = 0.0;
            for (const CurvePoint &pt : d.behavior.curve) {
                if (pt.moment >= mMax) {
                    mMax = pt.moment;
                    thAtMax = pt.theta;
                }
            }
            p.setPen(QPen(color, 1.0));
            p.setBrush(Qt::NoBrush);
            const QPointF mp = P(thAtMax, mMax);
            p.drawRect(QRectF(mp.x() - 5, mp.y() - 5, 10, 10));
            p.setPen(color);
            p.drawText(mp + QPointF(8, sign > 0 ? -8 : 16), QObject::tr("Mmáx = %1 %2").arg(num(sign * mMax * mf)).arg(m_units.momentLabel));
        }
    };
    drawDirection(m_analysis.positive, 1.0, kPositive, QObject::tr("M positivo"));
    drawDirection(m_analysis.negative, -1.0, kNegative, QObject::tr("M negativo"));
    p.setClipping(false);
    // dados insuficientes
    bool anyValid = m_analysis.positive.stiffness.valid || m_analysis.negative.stiffness.valid;
    if (m_insufficient) {
        p.setPen(QColor(0xb0, 0x30, 0x20));
        QFont big = base;
        big.setBold(true);
        big.setPointSizeF(base.pointSizeF() + 3);
        p.setFont(big);
        p.drawText(plot, Qt::AlignCenter | Qt::TextWordWrap, QObject::tr("DADOS INSUFICIENTES\nA curva momento-rotação não pode ser desenhada"));
        p.setFont(base);
        QStringList msgs;
        for (const std::string &m : m_analysis.positive.stiffness.messages) {
            msgs << QString::fromStdString(m);
        }
        if (!m_hint.isEmpty()) {
            msgs << m_hint;
        }
        if (!anyValid) {
            msgs << QObject::tr("Informe a curva, ou Mj,Rd e θ(2/3), ou Si, na aba \"Dados da ligação\".");
        }
        if (!msgs.isEmpty()) {
            p.setPen(kMuted);
            p.drawText(plot.adjusted(10, plot.height() * 0.62, -10, -6), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, msgs.join(QLatin1Char('\n')));
        }
    }
    // legenda
    if (!legend.isEmpty()) {
        const qreal h = 16.0 * legend.size() + 8;
        QRectF box(plot.right() - 350, plot.top() + 6, 344, h);
        p.setPen(QPen(kGrid, 1));
        p.setBrush(QColor(255, 255, 255, 230));
        p.drawRect(box);
        for (int i = 0; i < legend.size(); ++i) {
            p.setPen(QPen(legendColors[i], 2));
            p.drawLine(QPointF(box.left() + 6, box.top() + 12 + 16 * i), QPointF(box.left() + 24, box.top() + 12 + 16 * i));
            p.setPen(kText);
            p.drawText(QPointF(box.left() + 30, box.top() + 16 + 16 * i), legend[i]);
        }
    }
    // cursor
    if (m_hasHover && plot.contains(m_hover)) {
        const QPointF d = toData(m_hover, plot);
        p.setPen(QPen(kMuted, 1, Qt::DotLine));
        p.drawLine(QPointF(m_hover.x(), plot.top()), QPointF(m_hover.x(), plot.bottom()));
        p.drawLine(QPointF(plot.left(), m_hover.y()), QPointF(plot.right(), m_hover.y()));
        m_hoverText = QObject::tr("θ = %1 %2;  M = %3 %4").arg(num(d.x(), 5)).arg(m_units.thetaLabel).arg(num(d.y(), 5)).arg(m_units.momentLabel);
        // ponto mais proximo da curva (a 12 px)
        double best = 12.0;
        QPointF found;
        bool has = false;
        auto probe = [&](const DirectionAnalysis &dir, double sign) {
            for (const CurvePoint &pt : dir.behavior.curve) {
                const QPointF s = toScreen({sign * pt.theta * tf, sign * pt.moment * mf}, plot);
                const double dist = std::hypot(s.x() - m_hover.x(), s.y() - m_hover.y());
                if (dist < best) {
                    best = dist;
                    found = s;
                    has = true;
                    m_hoverText = QObject::tr("Ponto da curva: θ = %1 %2;  M = %3 %4").arg(num(sign * pt.theta * tf, 5)).arg(m_units.thetaLabel).arg(num(sign * pt.moment * mf, 5)).arg(m_units.momentLabel);
                }
            }
        };
        probe(m_analysis.positive, 1.0);
        probe(m_analysis.negative, -1.0);
        if (has) {
            p.setPen(QPen(kText, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(found, 7, 7);
        }
        p.setPen(kText);
        p.drawText(QRectF(plot.left(), plot.bottom() - 20, plot.width(), 18), Qt::AlignRight | Qt::AlignVCenter, m_hoverText);
    } else {
        m_hoverText.clear();
    }
}

void MomentRotationChart::wheelEvent(QWheelEvent *event)
{
    const QRectF plot = plotArea(QRectF(rect()));
    if (!plot.contains(event->position())) {
        return;
    }
    const double factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    const QPointF c = toData(event->position(), plot);
    QRectF v = m_view;
    v.setLeft(c.x() - (c.x() - v.left()) * factor);
    v.setRight(c.x() + (v.right() - c.x()) * factor);
    v.setTop(c.y() - (c.y() - v.top()) * factor);
    v.setBottom(c.y() + (v.bottom() - c.y()) * factor);
    m_view = v;
    m_userView = true;
    update();
}

void MomentRotationChart::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && plotArea(QRectF(rect())).contains(event->position())) {
        m_dragging = true;
        m_dragStart = event->position();
        m_dragView = m_view;
        setCursor(Qt::ClosedHandCursor);
    }
}

void MomentRotationChart::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = event->position();
    m_hasHover = true;
    if (m_dragging) {
        const QRectF plot = plotArea(QRectF(rect()));
        const double dx = (event->position().x() - m_dragStart.x()) / plot.width() * m_dragView.width();
        const double dy = (event->position().y() - m_dragStart.y()) / plot.height() * m_dragView.height();
        m_view = m_dragView.translated(-dx, dy);
        m_userView = true;
    }
    update();
}

void MomentRotationChart::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
    unsetCursor();
}

void MomentRotationChart::mouseDoubleClickEvent(QMouseEvent *)
{
    resetView();
}

void MomentRotationChart::leaveEvent(QEvent *)
{
    m_hasHover = false;
    update();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
ClassificationChart::ClassificationChart(QWidget *parent)
    : JointChartBase(parent)
{
}

void ClassificationChart::setAnalysis(const JointAnalysis &analysis)
{
    m_analysis = analysis;
    // faixa em decadas de log10: eta de 0,01 a 1000 por padrao, ampliada para conter os marcadores
    double lo = -2.0, hi = 3.0;
    for (const DirectionAnalysis *d : {&m_analysis.positive, &m_analysis.negative}) {
        if (d->hasData && d->stiffness.valid && d->classification.eta > 0.0) {
            lo = std::min(lo, std::floor(std::log10(d->classification.eta)) - 0.0);
            hi = std::max(hi, std::ceil(std::log10(d->classification.eta)) + 0.0);
        }
    }
    const double ref = analysis.limits.referenceStiffness > 0.0 ? analysis.limits.referenceStiffness : 1.0;
    m_range = QRectF(lo, std::log10(ref) + lo, hi - lo, hi - lo);
    update();
}

bool ClassificationChart::markerPosition(Direction direction, QPointF &out) const
{
    const DirectionAnalysis &d = direction == Direction::Positive ? m_analysis.positive : m_analysis.negative;
    if (!d.hasData || !d.stiffness.valid || !d.classification.computed || d.classification.eta <= 0.0) {
        return false;
    }
    out = QPointF(d.classification.eta, d.stiffness.stiffness);
    return true;
}

void ClassificationChart::drawChart(QPainter &p, const QRectF &area)
{
    p.fillRect(area, Qt::white);
    QFont base = p.font();
    base.setPointSizeF(std::max(8.0, base.pointSizeF()));
    p.setFont(base);
    p.setPen(kText);
    p.drawText(QRectF(area.left() + 8, area.top() + 6, area.width() - 16, 22), Qt::AlignLeft | Qt::AlignVCenter,
               QObject::tr("Classificação da rigidez — NBR 8800:2024, 6.1.2"));
    const QRectF plot = area.adjusted(78, 40, -24, -78);
    const double xlo = m_range.left(), xhi = m_range.right();
    const double ylo = m_range.top(), yhi = m_range.bottom();
    auto sx = [&](double logEta) { return plot.left() + (logEta - xlo) / (xhi - xlo) * plot.width(); };
    auto sy = [&](double logS) { return plot.bottom() - (logS - ylo) / (yhi - ylo) * plot.height(); };
    const bool applicable = m_analysis.kind == JointKind::BeamColumn;
    const DirectionAnalysis *main = m_analysis.primary();
    const bool storeyFails = main && main->classification.reachedRigidLimit && !main->classification.storeyConditionMet;
    // faixas
    const double l05 = std::log10(Limits::kPinnedFactor), l25 = std::log10(Limits::kRigidFactor);
    const QColor pinned(0xf3, 0xd9, 0xc6), semi(0xf6, 0xee, 0xc2), rigid(0xcf, 0xe6, 0xd2), rigidFail(0xe9, 0xe9, 0xe9);
    p.fillRect(QRectF(sx(xlo), plot.top(), sx(l05) - sx(xlo), plot.height()), pinned);
    p.fillRect(QRectF(sx(l05), plot.top(), sx(l25) - sx(l05), plot.height()), semi);
    QRectF rigidArea(sx(l25), plot.top(), sx(xhi) - sx(l25), plot.height());
    p.fillRect(rigidArea, storeyFails || (main && main->classification.storeyPending) ? rigidFail : rigid);
    if (storeyFails || (main && main->classification.storeyPending)) {
        p.fillRect(rigidArea, QBrush(QColor(0xb0, 0x30, 0x20, 70), Qt::BDiagPattern));
    }
    if (!applicable) {
        p.fillRect(plot, QBrush(QColor(0, 0, 0, 40), Qt::Dense6Pattern));
    }
    // grade (decadas)
    p.setPen(QPen(QColor(255, 255, 255, 160), 1));
    for (int k = static_cast<int>(std::ceil(xlo)); k <= static_cast<int>(std::floor(xhi)); ++k) {
        p.drawLine(QPointF(sx(k), plot.top()), QPointF(sx(k), plot.bottom()));
    }
    for (int k = static_cast<int>(std::ceil(ylo)); k <= static_cast<int>(std::floor(yhi)); ++k) {
        p.drawLine(QPointF(plot.left(), sy(k)), QPointF(plot.right(), sy(k)));
    }
    // limites verticais
    p.setPen(QPen(kText, 1.5, Qt::DashLine));
    p.drawLine(QPointF(sx(l05), plot.top()), QPointF(sx(l05), plot.bottom()));
    p.drawLine(QPointF(sx(l25), plot.top()), QPointF(sx(l25), plot.bottom()));
    // reta de referencia Si = eta E Iv/Lv
    p.setClipRect(plot);
    p.setPen(QPen(kMuted, 1.2));
    p.drawLine(QPointF(sx(xlo), sy(m_range.top())), QPointF(sx(xhi), sy(m_range.top() + (xhi - xlo))));
    p.setClipping(false);
    // rotulos das regioes
    QFont bold = base;
    bold.setBold(true);
    p.setFont(bold);
    p.setPen(kText);
    auto label = [&](double a, double b, const QString &t) {
        const double w = sx(b) - sx(a);
        p.save();
        p.translate(sx(a) + w / 2, plot.top() + 16);
        p.drawText(QRectF(-w / 2, -8, w, 18), Qt::AlignCenter, t);
        p.restore();
    };
    label(xlo, l05, QObject::tr("ROTULADA"));
    label(l05, l25, QObject::tr("SEMIRRÍGIDA"));
    label(l25, xhi, QObject::tr("RÍGIDA"));
    p.setFont(base);
    p.setPen(kMuted);
    p.drawText(QRectF(sx(xlo), plot.top() + 34, sx(l05) - sx(xlo), 16), Qt::AlignCenter, QObject::tr("η ≤ 0,5"));
    p.drawText(QRectF(sx(l05), plot.top() + 34, sx(l25) - sx(l05), 16), Qt::AlignCenter, QObject::tr("0,5 < η < 25"));
    p.drawText(QRectF(sx(l25), plot.top() + 34, sx(xhi) - sx(l25), 16), Qt::AlignCenter,
               QObject::tr("η ≥ 25"));
    // eixos
    p.setPen(kMuted);
    p.drawRect(plot);
    for (int k = static_cast<int>(std::ceil(xlo)); k <= static_cast<int>(std::floor(xhi)); ++k) {
        p.drawText(QRectF(sx(k) - 30, plot.bottom() + 4, 60, 16), Qt::AlignCenter, num(std::pow(10.0, k), 3));
    }
    for (int k = static_cast<int>(std::ceil(ylo)); k <= static_cast<int>(std::floor(yhi)); ++k) {
        p.drawText(QRectF(plot.left() - 74, sy(k) - 8, 68, 16), Qt::AlignRight | Qt::AlignVCenter, num(std::pow(10.0, k), 3));
    }
    p.setPen(kText);
    p.drawText(QRectF(plot.left(), plot.bottom() + 22, plot.width(), 18), Qt::AlignCenter, QObject::tr("Rigidez relativa η = Si / (E·Iv/Lv)  (escala logarítmica)"));
    p.save();
    p.translate(area.left() + 14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2, -8, plot.height(), 16), Qt::AlignCenter, QObject::tr("Si (kN·m/rad, logarítmica)"));
    p.restore();
    // marcadores
    QStringList info;
    auto marker = [&](const DirectionAnalysis &d, bool positive) {
        if (!d.hasData || !d.stiffness.valid) {
            return;
        }
        const Classification &c = d.classification;
        const QColor color = positive ? kPositive : kNegative;
        double leTa = c.eta;
        const bool zero = leTa <= 0.0;
        const double lx = zero ? xlo : std::log10(leTa);
        const double ly = zero ? ylo : std::log10(d.stiffness.stiffness);
        const QPointF pos(std::clamp(sx(lx), plot.left() + 4, plot.right() - 4), std::clamp(sy(ly), plot.top() + 4, plot.bottom() - 4));
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(color);
        if (positive) {
            p.drawEllipse(pos, 8, 8);
        } else {
            QPolygonF diamond;
            diamond << pos + QPointF(0, -9) << pos + QPointF(9, 0) << pos + QPointF(0, 9) << pos + QPointF(-9, 0);
            p.drawPolygon(diamond);
        }
        info << QObject::tr("%1: Si = %2 kN·m/rad, η = %3 → %4%5")
                    .arg(positive ? QObject::tr("M positivo") : QObject::tr("M negativo"))
                    .arg(num(d.stiffness.stiffness))
                    .arg(num(c.eta))
                    .arg(c.applicable ? classText(c) : QObject::tr("não classificada (configuração %1)").arg(QString::fromStdString(kindName(m_analysis.kind))))
                    .arg(c.reachedRigidLimit && c.final != StiffnessClass::Rigid
                             ? (c.storeyPending ? QObject::tr(" — limite de rigidez alcançado; Kv/Kp pendente") : QObject::tr(" — limite de rigidez alcançado, mas Kv/Kp < 0,1"))
                             : QString());
    };
    marker(m_analysis.positive, true);
    marker(m_analysis.negative, false);
    if (info.isEmpty()) {
        p.setPen(QColor(0xb0, 0x30, 0x20));
        p.setFont(bold);
        p.drawText(plot.adjusted(0, plot.height() * 0.3, 0, 0), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                   QObject::tr("SEM Si CALCULADA — o marcador não é posicionado"));
        p.setFont(base);
    }
    // texto inferior
    p.setPen(kText);
    QStringList lines = info;
    lines << QObject::tr("Limites: Smin = %1 kN·m/rad (0,5 E Iv/Lv);  Smax = %2 kN·m/rad (25 E Iv/Lv)").arg(num(m_analysis.limits.sMin)).arg(num(m_analysis.limits.sMax));
    if (m_analysis.storey.status == StoreyStatus::Ok) {
        lines << QObject::tr("Kv/Kp = %1 (%2 ≥ 0,1)").arg(num(m_analysis.storey.ratio)).arg(m_analysis.storey.ratio >= Limits::kMinStoreyRatio ? QObject::tr("atendida") : QObject::tr("NÃO atendida"));
    } else {
        lines << QObject::tr("Kv/Kp: verificação pendente");
    }
    p.drawText(QRectF(area.left() + 8, plot.bottom() + 42, area.width() - 16, 36), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, lines.join(QLatin1String("   |   ")));
    if (m_hasHover && plot.contains(m_hover)) {
        const double lx = xlo + (m_hover.x() - plot.left()) / plot.width() * (xhi - xlo);
        const double ly = ylo + (plot.bottom() - m_hover.y()) / plot.height() * (yhi - ylo);
        p.setPen(kText);
        p.drawText(QRectF(plot.left(), plot.bottom() - 20, plot.width(), 18), Qt::AlignRight | Qt::AlignVCenter,
                   QObject::tr("η = %1;  Si = %2 kN·m/rad").arg(num(std::pow(10.0, lx), 4)).arg(num(std::pow(10.0, ly), 4)));
    }
}

void ClassificationChart::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = event->position();
    m_hasHover = true;
    update();
}

void ClassificationChart::leaveEvent(QEvent *)
{
    m_hasHover = false;
    update();
}

} // namespace Structura::UI
