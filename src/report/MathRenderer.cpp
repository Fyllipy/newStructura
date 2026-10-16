#include "report/MathRenderer.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <functional>

namespace Structura::Report {

namespace {

struct Box {
    double width {0.0};
    double ascent {0.0};    ///< acima da linha de base
    double descent {0.0};   ///< abaixo da linha de base
    std::function<void(QPainter &, double, double)> draw;   ///< (painter, x esquerdo, y da linha de base)
    bool isOpenParen {false};
    bool isCloseParen {false};
    [[nodiscard]] double height() const { return ascent + descent; }
};

QFont mathFont(double pixelSize, bool italic)
{
    QFont font;
    font.setFamilies({QStringLiteral("Cambria Math"), QStringLiteral("Cambria"), QStringLiteral("Times New Roman"), QStringLiteral("Serif")});
    font.setPixelSize(std::max(4, static_cast<int>(std::lround(pixelSize))));
    font.setItalic(italic);
    return font;
}

bool isBinaryOrRelation(const QString &s)
{
    static const QStringList ops = {QStringLiteral("="), QStringLiteral("+"), QStringLiteral("-"), QStringLiteral("−"), QStringLiteral("<"),
                                    QStringLiteral(">"), QStringLiteral("≤"), QStringLiteral("≥"), QStringLiteral("≠"), QStringLiteral("≈"),
                                    QStringLiteral("→"), QStringLiteral("⇒"), QStringLiteral("×"), QStringLiteral("±")};
    return ops.contains(s);
}

class Layout
{
public:
    Layout(double px, const QColor &color) : m_px(px), m_color(color) {}

    Box layout(const MathPtr &node, double scale) const
    {
        if (!node) {
            return {};
        }
        switch (node->kind) {
        case MathNode::Kind::Run: return run(*node, scale);
        case MathNode::Kind::Space: {
            Box b;
            b.width = node->width * m_px * scale;
            b.draw = [](QPainter &, double, double) {};
            return b;
        }
        case MathNode::Kind::Row: return row(*node, scale);
        case MathNode::Kind::Frac: return frac(*node, scale);
        case MathNode::Kind::Script: return script(*node, scale);
        case MathNode::Kind::Sqrt: return sqrtBox(*node, scale);
        }
        return {};
    }

private:
    double m_px;
    QColor m_color;

    Box run(const MathNode &node, double scale) const
    {
        const QString text = QString::fromStdString(node.text);
        const QFont font = mathFont(m_px * scale, !node.upright);
        const QFontMetricsF fm(font);
        Box b;
        b.width = fm.horizontalAdvance(text);
        // altura real do texto para alinhar sub/sup e fracoes
        const QRectF bounds = fm.tightBoundingRect(text);
        b.ascent = std::max(-bounds.top(), 0.0);
        b.descent = std::max(bounds.bottom(), 0.0);
        if (text.isEmpty()) {
            b.ascent = b.descent = 0.0;
        }
        const QColor color = m_color;
        b.draw = [text, font, color](QPainter &p, double x, double y) {
            p.setFont(font);
            p.setPen(color);
            p.drawText(QPointF(x, y), text);
        };
        b.isOpenParen = text == QLatin1String("(") || text == QLatin1String("[");
        b.isCloseParen = text == QLatin1String(")") || text == QLatin1String("]");
        return b;
    }

    Box row(const MathNode &node, double scale) const
    {
        std::vector<Box> boxes;
        std::vector<bool> spaced;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            const auto &child = node.children[i];
            Box b = layout(child, scale);
            const bool op = child && child->kind == MathNode::Kind::Run && isBinaryOrRelation(QString::fromStdString(child->text)) && i > 0 &&
                            i + 1 < node.children.size();
            boxes.push_back(std::move(b));
            spaced.push_back(op);
        }
        // parenteses que acompanham a altura do conteudo
        std::vector<std::size_t> stack;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if (boxes[i].isOpenParen) {
                stack.push_back(i);
            } else if (boxes[i].isCloseParen && !stack.empty()) {
                const std::size_t open = stack.back();
                stack.pop_back();
                double asc = 0.0;
                double desc = 0.0;
                for (std::size_t k = open + 1; k < i; ++k) {
                    asc = std::max(asc, boxes[k].ascent);
                    desc = std::max(desc, boxes[k].descent);
                }
                if (asc + desc > m_px * scale * 1.35) {
                    boxes[open] = paren(true, asc, desc, scale);
                    boxes[i] = paren(false, asc, desc, scale);
                }
            }
        }
        Box out;
        double x = 0.0;
        const double gap = m_px * scale * 0.22;
        std::vector<double> offsets;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if (spaced[i]) {
                x += gap;
            }
            offsets.push_back(x);
            x += boxes[i].width;
            if (spaced[i]) {
                x += gap;
            }
            out.ascent = std::max(out.ascent, boxes[i].ascent);
            out.descent = std::max(out.descent, boxes[i].descent);
        }
        out.width = x;
        out.draw = [boxes, offsets](QPainter &p, double ox, double y) {
            for (std::size_t i = 0; i < boxes.size(); ++i) {
                if (boxes[i].draw) {
                    boxes[i].draw(p, ox + offsets[i], y);
                }
            }
        };
        return out;
    }

    Box paren(bool open, double asc, double desc, double scale) const
    {
        Box b;
        const double em = m_px * scale;
        const double pad = em * 0.08;
        b.width = em * 0.34;
        b.ascent = asc + pad;
        b.descent = desc + pad;
        const QColor color = m_color;
        b.draw = [=](QPainter &p, double x, double y) {
            QPainterPath path;
            const double top = y - asc - pad;
            const double bottom = y + desc + pad;
            const double mid = (top + bottom) / 2.0;
            const double h = bottom - top;
            const double depth = std::min(em * 0.22, h * 0.18);
            if (open) {
                path.moveTo(x + b.width * 0.85, top);
                path.cubicTo(x + b.width * 0.85 - depth, top + h * 0.25, x + b.width * 0.85 - depth, bottom - h * 0.25, x + b.width * 0.85, bottom);
            } else {
                path.moveTo(x + b.width * 0.15, top);
                path.cubicTo(x + b.width * 0.15 + depth, top + h * 0.25, x + b.width * 0.15 + depth, bottom - h * 0.25, x + b.width * 0.15, bottom);
            }
            (void)mid;
            p.setPen(QPen(color, std::max(1.0, em * 0.055), Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        return b;
    }

    Box frac(const MathNode &node, double scale) const
    {
        const double inner = std::max(scale * 0.93, 0.5);
        Box num = layout(node.children[0], inner);
        Box den = layout(node.children[1], inner);
        const double em = m_px * scale;
        const double axis = em * 0.27;
        const double gap = em * 0.14;
        const double pad = em * 0.12;
        Box b;
        b.width = std::max(num.width, den.width) + 2.0 * pad;
        b.ascent = axis + gap + num.height();
        b.descent = std::max(0.0, den.height() + gap - axis);
        const double w = b.width;
        const QColor color = m_color;
        const double lineWidth = std::max(1.0, em * 0.05);
        b.draw = [=](QPainter &p, double x, double y) {
            if (num.draw) {
                num.draw(p, x + (w - num.width) / 2.0, y - axis - gap - num.descent);
            }
            if (den.draw) {
                den.draw(p, x + (w - den.width) / 2.0, y - axis + gap + den.ascent);
            }
            p.setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(x + pad * 0.5, y - axis), QPointF(x + w - pad * 0.5, y - axis));
        };
        return b;
    }

    Box script(const MathNode &node, double scale) const
    {
        Box base = layout(node.children[0], scale);
        const double small = std::max(scale * 0.7, 0.45);
        Box sub = node.children.size() > 1 && node.children[1] ? layout(node.children[1], small) : Box {};
        Box sup = node.children.size() > 2 && node.children[2] ? layout(node.children[2], small) : Box {};
        const double em = m_px * scale;
        Box b;
        const double extra = em * 0.04;
        const double scriptWidth = std::max(sub.width, sup.width);
        b.width = base.width + scriptWidth + extra;
        const double supRaise = std::max(base.ascent * 0.62, em * 0.36);
        const double subDrop = std::max(em * 0.22, base.descent + em * 0.08);
        b.ascent = base.ascent;
        b.descent = base.descent;
        if (sup.width > 0.0 || sup.height() > 0.0) {
            b.ascent = std::max(b.ascent, supRaise + sup.ascent);
        }
        if (sub.width > 0.0 || sub.height() > 0.0) {
            b.descent = std::max(b.descent, subDrop + sub.descent);
        }
        b.draw = [=](QPainter &p, double x, double y) {
            if (base.draw) {
                base.draw(p, x, y);
            }
            if (sup.draw) {
                sup.draw(p, x + base.width + extra, y - supRaise);
            }
            if (sub.draw) {
                sub.draw(p, x + base.width + extra, y + subDrop);
            }
        };
        return b;
    }

    Box sqrtBox(const MathNode &node, double scale) const
    {
        Box body = layout(node.children[0], scale);
        const double em = m_px * scale;
        const double left = em * 0.62;
        const double pad = em * 0.12;
        Box b;
        b.width = left + body.width + pad;
        b.ascent = body.ascent + em * 0.2;
        b.descent = body.descent + em * 0.05;
        const QColor color = m_color;
        const double lineWidth = std::max(1.0, em * 0.055);
        b.draw = [=](QPainter &p, double x, double y) {
            if (body.draw) {
                body.draw(p, x + left, y);
            }
            const double top = y - body.ascent - em * 0.14;
            const double bottom = y + body.descent + em * 0.04;
            QPainterPath path;
            path.moveTo(x + em * 0.04, y - em * 0.18 - (body.ascent > em * 0.9 ? 0 : 0));
            path.lineTo(x + em * 0.16, y - em * 0.22);
            path.lineTo(x + em * 0.30, bottom);
            path.lineTo(x + left - em * 0.04, top);
            path.lineTo(x + b.width, top);
            p.setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        return b;
    }
};

} // namespace

QImage renderMath(const MathPtr &tree, double pixelSize, const QColor &color, qreal dpr)
{
    Layout layout(pixelSize, color);
    const Box root = layout.layout(tree, 1.0);
    const double margin = pixelSize * 0.15;
    const double w = std::max(1.0, root.width + 2.0 * margin);
    const double h = std::max(1.0, root.height() + 2.0 * margin);
    QImage image(QSize(static_cast<int>(std::ceil(w * dpr)), static_cast<int>(std::ceil(h * dpr))), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (root.draw) {
        root.draw(painter, margin, margin + root.ascent);
    }
    return image;
}

QImage renderLatex(const QString &latex, double pixelSize, const QColor &color, qreal dpr)
{
    return renderMath(parseLatex(latex.toStdString()), pixelSize, color, dpr);
}

} // namespace Structura::Report
