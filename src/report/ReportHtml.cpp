#include "report/ReportHtml.h"

#include "report/MathRenderer.h"

#include <QBuffer>

namespace Structura::Report {

namespace {

QString esc(const std::string &s)
{
    return QString::fromStdString(s).toHtmlEscaped();
}

QString styleOpen(ParagraphStyle s)
{
    switch (s) {
    case ParagraphStyle::Normal: return QStringLiteral("<p style=\"margin:4px 0;\">");
    case ParagraphStyle::Small: return QStringLiteral("<p style=\"margin:2px 0; color:#5a6472; font-size:9pt;\">");
    case ParagraphStyle::Caption: return QStringLiteral("<p style=\"margin:8px 0 3px 0; color:#5a6472; font-size:9pt;\"><i>");
    case ParagraphStyle::Label: return QStringLiteral("<p style=\"margin:10px 0 2px 0; color:#404b5a;\"><b>");
    default: return QStringLiteral("<p>");
    }
}

QString styleClose(ParagraphStyle s)
{
    switch (s) {
    case ParagraphStyle::Caption: return QStringLiteral("</i></p>");
    case ParagraphStyle::Label: return QStringLiteral("</b></p>");
    default: return QStringLiteral("</p>");
    }
}

} // namespace

HtmlResult toHtml(const ReportDocument &doc, double eqPx, bool includeCover)
{
    HtmlResult out;
    QString h;
    h += QStringLiteral("<html><body style=\"font-family:'Segoe UI','Calibri',sans-serif; font-size:10pt; color:#1e232b;\">");
    int imageCounter = 0;
    auto addImage = [&](const QImage &img) {
        const QString key = QStringLiteral("res://img%1").arg(++imageCounter);
        out.images.insert(key, img);
        return key;
    };
    auto equationTag = [&](const std::string &latex, bool inlineMode) {
        const QImage img = renderLatex(QString::fromStdString(latex), inlineMode ? eqPx * 0.92 : eqPx, QColor(30, 35, 43), 2.0);
        const QString key = addImage(img);
        const int w = static_cast<int>(img.width() / img.devicePixelRatio());
        const int hh = static_cast<int>(img.height() / img.devicePixelRatio());
        return QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%3\" %4/>").arg(key).arg(w).arg(hh).arg(inlineMode ? QStringLiteral("style=\"vertical-align:middle;\"") : QString());
    };

    if (includeCover && doc.cover) {
        h += QStringLiteral("<div style=\"margin:40px 0 30px 0;\"><p style=\"font-size:26pt; color:#1f4e79; margin:0;\"><b>%1</b></p>").arg(esc(doc.title));
        if (!doc.subtitle.empty()) {
            h += QStringLiteral("<p style=\"font-size:14pt; color:#5a6472; margin:4px 0;\">%1</p>").arg(esc(doc.subtitle));
        }
        h += QStringLiteral("<hr/>");
        if (!doc.project.empty()) {
            h += QStringLiteral("<p><b style=\"color:#1f4e79;\">Projeto:</b> %1<br/>").arg(esc(doc.project));
        }
        if (!doc.date.empty()) {
            h += QStringLiteral("<b style=\"color:#1f4e79;\">Data:</b> %1</p>").arg(esc(doc.date));
        }
        h += QStringLiteral("</div>");
    }

    for (const Block &block : doc.blocks) {
        if (const auto *hd = std::get_if<Heading>(&block)) {
            if (hd->level == 1) {
                h += QStringLiteral("<h2 style=\"color:#1f4e79; margin:22px 0 6px 0; border-bottom:2px solid #2e75b6;\">%1</h2>").arg(esc(hd->text));
            } else if (hd->level == 2) {
                h += QStringLiteral("<h3 style=\"color:#2e75b6; margin:16px 0 4px 0;\">%1</h3>").arg(esc(hd->text));
            } else {
                h += QStringLiteral("<h4 style=\"color:#404b5a; margin:12px 0 3px 0;\">%1</h4>").arg(esc(hd->text));
            }
        } else if (const auto *p = std::get_if<Paragraph>(&block)) {
            QString runs;
            for (const Run &r : p->runs) {
                if (!r.latex.empty()) {
                    runs += equationTag(r.latex, true);
                } else {
                    QString t = esc(r.text);
                    if (r.bold) {
                        t = QStringLiteral("<b>%1</b>").arg(t);
                    }
                    if (r.italic) {
                        t = QStringLiteral("<i>%1</i>").arg(t);
                    }
                    if (!r.color.empty()) {
                        t = QStringLiteral("<span style=\"color:#%1;\">%2</span>").arg(QString::fromStdString(r.color), t);
                    }
                    runs += t;
                }
            }
            QString bg;
            QString bar;
            QString color = QStringLiteral("#1e232b");
            switch (p->style) {
            case ParagraphStyle::Note: bg = QStringLiteral("#eef3f9"); bar = QStringLiteral("#2e75b6"); color = QStringLiteral("#1f3a5a"); break;
            case ParagraphStyle::Ok: bg = QStringLiteral("#e8f5ec"); bar = QStringLiteral("#2e9e5b"); color = QStringLiteral("#1e6b3a"); break;
            case ParagraphStyle::Warn: bg = QStringLiteral("#fff6e0"); bar = QStringLiteral("#e0a100"); color = QStringLiteral("#7a4b00"); break;
            case ParagraphStyle::Fail: bg = QStringLiteral("#fbe9e7"); bar = QStringLiteral("#c0392b"); color = QStringLiteral("#8e241a"); break;
            default: break;
            }
            if (!bg.isEmpty()) {
                h += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin:6px 0;\"><tr><td width=\"5\" bgcolor=\"%1\"></td>"
                                    "<td bgcolor=\"%2\" style=\"padding:6px 10px; color:%3;\">%4</td></tr></table>")
                         .arg(bar, bg, color, runs);
            } else {
                h += styleOpen(p->style) + runs + styleClose(p->style);
            }
        } else if (const auto *eq = std::get_if<Equation>(&block)) {
            h += QStringLiteral("<p align=\"center\" style=\"margin:6px 0;\">%1</p>").arg(equationTag(eq->latex, false));
        } else if (const auto *t = std::get_if<Table>(&block)) {
            if (!t->caption.empty()) {
                h += QStringLiteral("<p style=\"margin:10px 0 3px 0; color:#5a6472; font-size:9pt;\"><i>%1</i></p>").arg(esc(t->caption));
            }
            h += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"5\" style=\"border:1px solid #c9d3e0;\">");
            double sum = 0.0;
            std::vector<double> rel = t->widths;
            const std::size_t cols = std::max<std::size_t>({t->header.size(), t->rows.empty() ? 0 : t->rows.front().size(), 1});
            rel.resize(cols, 1.0);
            for (double w : rel) {
                sum += w;
            }
            auto alignOf = [&](std::size_t c) {
                const char a = c < t->align.size() ? t->align[c] : 'l';
                return a == 'r' ? "right" : (a == 'c' ? "center" : "left");
            };
            if (!t->header.empty()) {
                h += QStringLiteral("<tr>");
                for (std::size_t c = 0; c < cols; ++c) {
                    h += QStringLiteral("<th width=\"%1%\" bgcolor=\"#1f4e79\" align=\"%2\" style=\"color:#ffffff; font-size:9pt;\">%3</th>")
                             .arg(static_cast<int>(100.0 * rel[c] / sum))
                             .arg(QLatin1String(alignOf(c)))
                             .arg(c < t->header.size() ? esc(t->header[c]) : QString());
                }
                h += QStringLiteral("</tr>");
            }
            for (std::size_t r = 0; r < t->rows.size(); ++r) {
                h += QStringLiteral("<tr>");
                for (std::size_t c = 0; c < cols; ++c) {
                    QString bg = (r % 2 == 1) ? QStringLiteral("#f2f6fa") : QStringLiteral("#ffffff");
                    QString color = QStringLiteral("#1e232b");
                    bool bold = t->firstColumnBold && c == 0;
                    if (r < t->status.size() && c < t->status[r].size()) {
                        switch (t->status[r][c]) {
                        case CellStatus::Ok: bg = QStringLiteral("#e3f4e8"); color = QStringLiteral("#1e6b3a"); bold = true; break;
                        case CellStatus::Warn: bg = QStringLiteral("#fff1d6"); color = QStringLiteral("#8a5a00"); bold = true; break;
                        case CellStatus::Fail: bg = QStringLiteral("#fbe1de"); color = QStringLiteral("#a32a1e"); bold = true; break;
                        case CellStatus::None: break;
                        }
                    }
                    QString text = c < t->rows[r].size() ? esc(t->rows[r][c]) : QString();
                    if (bold) {
                        text = QStringLiteral("<b>%1</b>").arg(text);
                    }
                    h += QStringLiteral("<td bgcolor=\"%1\" align=\"%2\" style=\"color:%3; font-size:9pt;\">%4</td>").arg(bg, QLatin1String(alignOf(c)), color, text);
                }
                h += QStringLiteral("</tr>");
            }
            h += QStringLiteral("</table>");
        } else if (const auto *card = std::get_if<CalcCard>(&block)) {
            const bool fail = card->status == CellStatus::Fail, warn = card->status == CellStatus::Warn;
            const QString accent = fail ? QStringLiteral("#c0392b") : (warn ? QStringLiteral("#e0a100") : QStringLiteral("#2e9e5b"));
            const QString fill = fail ? QStringLiteral("#f8dad6") : (warn ? QStringLiteral("#fcefcb") : QStringLiteral("#ddefe3"));
            const QString ink = fail ? QStringLiteral("#7a1e14") : (warn ? QStringLiteral("#6b4500") : QStringLiteral("#1e5c34"));
            h += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin:14px 0 4px 0;\"><tr><td width=\"6\" bgcolor=\"%1\"></td>"
                                "<td bgcolor=\"%2\" style=\"padding:6px 10px; color:%3;\"><b>%4</b>%5</td></tr></table>")
                     .arg(accent, fill, ink, esc(card->title),
                          card->clause.empty() ? QString() : QStringLiteral("&nbsp;&nbsp;<span style=\"font-size:9pt;\">(%1)</span>").arg(esc(card->clause)));
            if (!card->location.empty()) {
                h += QStringLiteral("<p style=\"margin:3px 6px 0 6px; color:#5a6472; font-size:9pt;\"><i>Seção crítica: %1</i></p>").arg(esc(card->location));
            }
            if (!card->description.empty()) {
                h += QStringLiteral("<p style=\"margin:4px 6px 6px 6px; color:#3a4352; font-size:9.5pt; text-align:justify;\">%1</p>").arg(esc(card->description));
            }
            for (const CalcStep &step : card->steps) {
                if (!step.label.empty()) {
                    h += QStringLiteral("<p style=\"margin:8px 6px 0 6px; color:#1f4e79;\"><b>&#9656; %1</b></p>").arg(esc(step.label));
                }
                for (const std::string &eq : step.equations) {
                    h += QStringLiteral("<p align=\"center\" style=\"margin:4px 0;\">%1</p>").arg(equationTag(eq, false));
                }
                for (const std::string &note : step.notes) {
                    h += QStringLiteral("<p style=\"margin:2px 6px; color:#5a6472; font-size:9pt;\"><i>%1</i></p>").arg(esc(note));
                }
                if (!step.variables.empty()) {
                    h += QStringLiteral("<p style=\"margin:6px 6px 2px 6px; color:#404b5a; font-size:9.5pt;\"><b>Onde:</b></p>");
                    h += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"3\" style=\"margin-left:6px;\">");
                    for (const CalcVariable &v : step.variables) {
                        h += QStringLiteral("<tr><td width=\"14%\" align=\"right\">%1</td><td width=\"62%\" style=\"color:#2a3340; font-size:9pt;\">&nbsp;%2</td>"
                                            "<td width=\"24%\" align=\"right\" style=\"border-bottom:1px solid #b7c3d4;\"><b style=\"color:#1f4e79;\">%3</b> <span style=\"color:#5a6472; font-size:9pt;\">%4</span></td></tr>")
                                 .arg(equationTag(v.latex, true), esc(v.meaning), esc(v.value), esc(v.unit));
                    }
                    h += QStringLiteral("</table>");
                }
            }
            h += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"4\" style=\"margin:8px 0 14px 0; border-top:2px solid %1; border-bottom:2px solid %1;\"><tr>"
                                "<td width=\"60%\">%2</td><td width=\"40%\" align=\"right\">%3 <b style=\"color:%1; font-size:13pt;\">&nbsp;%4&nbsp;</b>"
                                "<b style=\"color:%1; font-size:15pt;\">%5</b>%6</td></tr></table>")
                     .arg(accent, equationTag(card->inequalityLatex, true), equationTag(card->resultLabel + " :", true), esc(card->resultValue),
                          card->ok ? QStringLiteral("&#10004;") : QStringLiteral("&#10008;"),
                          card->resultDetail.empty() ? QString() : QStringLiteral("<br/><span style=\"color:#5a6472; font-size:8.5pt;\">%1</span>").arg(esc(card->resultDetail)));
        } else if (const auto *img = std::get_if<Image>(&block)) {
            const QImage image = QImage::fromData(img->png.data(), static_cast<int>(img->png.size()), "PNG");
            if (!image.isNull()) {
                const QString key = addImage(image);
                const int w = static_cast<int>(img->widthCm * 38.0);
                h += QStringLiteral("<p align=\"center\"><img src=\"%1\" width=\"%2\"/></p>").arg(key).arg(w);
                if (!img->caption.empty()) {
                    h += QStringLiteral("<p align=\"center\" style=\"color:#5a6472; font-size:9pt;\"><i>%1</i></p>").arg(esc(img->caption));
                }
            }
        } else if (std::holds_alternative<PageBreak>(block)) {
            h += QStringLiteral("<hr/>");
        }
    }
    h += QStringLiteral("</body></html>");
    out.html = h;
    return out;
}

} // namespace Structura::Report
