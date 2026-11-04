#include "Theme.h"

#include "app/AppSettings.h"

#include <QApplication>
#include <QEvent>
#include <QObject>
#include <QPalette>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>

#include <algorithm>

namespace Structura::UI::Theme {

namespace {

constexpr const char *kThemeKey = "visual/theme";
constexpr const char *kOriginalProperty = "_structuraLightStyle";   ///< folha de estilo original (clara) do widget
constexpr const char *kAppliedProperty = "_structuraDarkStyle";     ///< ultima folha convertida que o filtro gravou

Mode g_mode = Mode::Light;
QString g_originalStyle;   // estilo nativo do Qt, restaurado no modo claro
bool g_installed = false;

QColor convert(const QColor &c)
{
    if (!c.isValid()) {
        return c;
    }
    float h = 0, s = 0, l = 0, a = 0;
    c.getHslF(&h, &s, &l, &a);
    // cores vivas (destaques e estados): so ajusta a luminosidade para ficar legivel sobre fundo escuro
    if (s > 0.45f) {
        l = std::clamp(l, 0.30f, 0.72f);   // destaques mantem a cor (barra azul do programa inclusive)
    } else {
        l = 0.09f + 0.80f * (1.0f - l);   // branco vira quase preto; texto escuro vira claro
    }
    QColor out;
    out.setHslF(h < 0 ? 0.0f : h, s, l, a);
    return out;
}

QPalette darkPalette()
{
    QPalette p;
    const QColor window(0x1b, 0x20, 0x27), base(0x13, 0x17, 0x1d), button(0x27, 0x2e, 0x38), text(0xd7, 0xde, 0xe8), accent(0x1a, 0x8f, 0xc9);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, button);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, QColor(0xff, 0x6b, 0x6b));
    p.setColor(QPalette::Link, QColor(0x58, 0xa6, 0xff));
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0x8b, 0x96, 0xa5));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6a, 0x74, 0x82));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6a, 0x74, 0x82));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6a, 0x74, 0x82));
    return p;
}

/// Converte a folha de estilo de um widget quando o modo e escuro; guarda a original para poder voltar ao claro.
void rethemeWidget(QWidget *widget)
{
    const QString current = widget->styleSheet();
    if (current.isEmpty() && !widget->property(kOriginalProperty).isValid()) {
        return;
    }
    const QString applied = widget->property(kAppliedProperty).toString();
    QString original = widget->property(kOriginalProperty).toString();
    if (current != applied || !widget->property(kOriginalProperty).isValid()) {
        original = current;   // folha nova (clara) definida pelo codigo
        widget->setProperty(kOriginalProperty, original);
    }
    const QString wanted = g_mode == Mode::Dark ? darkStyleSheet(original) : original;
    if (wanted == current) {
        widget->setProperty(kAppliedProperty, wanted);
        return;
    }
    widget->setProperty(kAppliedProperty, wanted);
    widget->setStyleSheet(wanted);
}

class StyleSheetFilter final : public QObject
{
public:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (g_mode == Mode::Dark && (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange || event->type() == QEvent::Show)) {
            if (auto *widget = qobject_cast<QWidget *>(watched)) {
                if (!widget->styleSheet().isEmpty() && widget->styleSheet() != widget->property(kAppliedProperty).toString()) {
                    rethemeWidget(widget);
                }
            }
        }
        return false;
    }
};

StyleSheetFilter *g_filter = nullptr;

} // namespace

QString darkStyleSheet(const QString &lightStyleSheet)
{
    static const QRegularExpression hex(QStringLiteral("#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\\b"));
    static const QRegularExpression rgba(QStringLiteral("rgba?\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*(?:,\\s*([0-9.]+)\\s*)?\\)"));
    QString out;
    out.reserve(lightStyleSheet.size());
    qsizetype pos = 0;
    // varre as duas formas na ordem em que aparecem
    while (pos < lightStyleSheet.size()) {
        const auto mh = hex.match(lightStyleSheet, pos);
        const auto mr = rgba.match(lightStyleSheet, pos);
        const bool useHex = mh.hasMatch() && (!mr.hasMatch() || mh.capturedStart() <= mr.capturedStart());
        const auto &m = useHex ? mh : mr;
        if (!m.hasMatch()) {
            out += lightStyleSheet.mid(pos);
            break;
        }
        out += lightStyleSheet.mid(pos, m.capturedStart() - pos);
        // texto branco (color: #fff) fica branco: ele esta sobre barras e botoes coloridos, que mantem a cor
        const QString before = lightStyleSheet.left(m.capturedStart());
        const qsizetype boundary = std::max(before.lastIndexOf(QLatin1Char(';')), std::max(before.lastIndexOf(QLatin1Char('{')), before.lastIndexOf(QLatin1Char('}'))));
        const QString property = before.mid(boundary + 1).trimmed();
        const bool textProperty = property.startsWith(QLatin1String("color"));
        if (textProperty && QColor(m.captured(0)).lightnessF() > 0.94) {
            out += m.captured(0);
            pos = m.capturedEnd();
            continue;
        }
        if (useHex) {
            out += convert(QColor(m.captured(0))).name();
        } else {
            QColor c(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
            const double alpha = m.captured(4).isEmpty() ? 1.0 : m.captured(4).toDouble();
            const QColor d = convert(c);
            out += QStringLiteral("rgba(%1, %2, %3, %4)").arg(d.red()).arg(d.green()).arg(d.blue()).arg(alpha);
        }
        pos = m.capturedEnd();
    }
    return out;
}

QColor color(const QColor &light)
{
    return g_mode == Mode::Dark ? convert(light) : light;
}

QColor color(const QString &lightHex)
{
    return color(QColor(lightHex));
}

Mode current() { return g_mode; }
bool isDark() { return g_mode == Mode::Dark; }

void apply(Mode mode, bool persist)
{
    g_mode = mode;
    if (persist) {
        auto settings = Structura::App::appSettings();
        settings.setValue(QString::fromLatin1(kThemeKey), mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
    }
    if (g_originalStyle.isEmpty() && qApp->style()) {
        g_originalStyle = qApp->style()->name();
    }
    if (mode == Mode::Dark) {
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
            QApplication::setStyle(fusion);
        }
        QApplication::setPalette(darkPalette());
    } else {
        if (!g_originalStyle.isEmpty()) {
            QApplication::setStyle(QStyleFactory::create(g_originalStyle));
        }
        QApplication::setPalette(QPalette());
    }
    // janelas ja criadas: converte (ou restaura) as folhas de estilo
    for (QWidget *widget : QApplication::allWidgets()) {
        rethemeWidget(widget);
        widget->update();
    }
}

void initialize()
{
    if (g_installed) {
        return;
    }
    g_installed = true;
    g_originalStyle = qApp->style() ? qApp->style()->name() : QString();
    g_filter = new StyleSheetFilter();
    qApp->installEventFilter(g_filter);
    const auto settings = Structura::App::appSettings();
    const bool dark = settings.value(QString::fromLatin1(kThemeKey), QStringLiteral("light")).toString() == QLatin1String("dark");
    if (dark) {
        apply(Mode::Dark, false);
    }
}

} // namespace Structura::UI::Theme
