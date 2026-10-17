#include "ReportService.h"

#include "report/FormulaText.h"
#include "viz/structure/SectionShapes.h"

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

using namespace Structura::Report;
using Structura::App::ProjectData::SectionInfo;

namespace {

std::string S(const QString &s) { return s.toStdString(); }

QString num(double v, int decimals = 2)
{
    QString text = QString::number(v, 'f', decimals);
    text.replace(QLatin1Char('.'), QLatin1Char(','));
    return text;
}

QString sci(double v, int decimals = 3)
{
    if (v != 0.0 && (std::abs(v) >= 1e6 || std::abs(v) < 1e-2)) {
        QString t = QString::number(v, 'e', decimals);
        t.replace(QLatin1Char('.'), QLatin1Char(','));
        return t;
    }
    return num(v, v == std::floor(v) && std::abs(v) > 100 ? 0 : decimals);
}

std::string latexNumber(double v, int decimals = 3)
{
    QString t = QString::number(v, 'f', decimals);
    t.replace(QLatin1Char('.'), QLatin1String("{,}"));
    return S(t);
}

QString etaText(double eta) { return num(eta, 3); }

Paragraph paragraphOf(const std::string &text, ParagraphStyle style, bool bold = false)
{
    Paragraph p;
    p.style = style;
    Run r;
    r.text = text;
    r.bold = bold;
    p.runs.push_back(r);
    return p;
}

/// Desenho da secao transversal (PNG) com eixos pelo centroide.
std::vector<unsigned char> sectionPng(const SectionInfo &section)
{
    using Structura::Visualization::SectionShapes;
    const auto shape = SectionShapes::build(section.family, section.geometry, section.mirrored);
    if (!shape || shape->loops.empty()) {
        return {};
    }
    const int W = 640;
    const int H = 440;
    QImage image(W, H, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto &b = shape->bounds;   // min a, max a, min b, max b (relativo ao centroide)
    const double width = std::max(1e-6, b[1] - b[0]);
    const double height = std::max(1e-6, b[3] - b[2]);
    const double scale = std::min((W - 150.0) / width, (H - 130.0) / height);
    const double cx = W / 2.0 - 0.5 * (b[0] + b[1]) * scale;
    const double cy = H / 2.0 + 0.5 * (b[2] + b[3]) * scale;
    auto map = [&](double a, double bb) { return QPointF(cx + a * scale, cy - bb * scale); };

    QPainterPath path;
    for (const auto &loop : shape->loops) {
        if (loop.empty()) {
            continue;
        }
        path.moveTo(map(loop[0][0], loop[0][1]));
        for (std::size_t i = 1; i < loop.size(); ++i) {
            path.lineTo(map(loop[i][0], loop[i][1]));
        }
        path.closeSubpath();
    }
    path.setFillRule(Qt::OddEvenFill);
    p.setPen(QPen(QColor("#1f4e79"), 3.0));
    p.setBrush(QColor("#cfe0f2"));
    p.drawPath(path);

    // eixos pelo centroide
    p.setPen(QPen(QColor("#c0392b"), 1.2, Qt::DashDotLine));
    p.drawLine(map(b[0] - 0.12 * width, 0), map(b[1] + 0.12 * width, 0));
    p.drawLine(map(0, b[2] - 0.12 * height), map(0, b[3] + 0.12 * height));
    QFont f(QStringLiteral("Segoe UI"), 20);
    p.setFont(f);
    p.setPen(QColor("#c0392b"));
    p.drawText(map(b[1] + 0.12 * width, 0) + QPointF(6, 5), QStringLiteral("x"));
    p.drawText(map(0, b[3] + 0.12 * height) + QPointF(6, -4), QStringLiteral("y"));

    // cotas gerais
    p.setPen(QPen(QColor("#5a6472"), 1.0));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 17));
    const QPointF bottomLeft = map(b[0], b[2]);
    const QPointF bottomRight = map(b[1], b[2]);
    const double y0 = bottomLeft.y() + 22;
    p.drawLine(QPointF(bottomLeft.x(), y0), QPointF(bottomRight.x(), y0));
    p.drawLine(QPointF(bottomLeft.x(), y0 - 4), QPointF(bottomLeft.x(), y0 + 4));
    p.drawLine(QPointF(bottomRight.x(), y0 - 4), QPointF(bottomRight.x(), y0 + 4));
    p.drawText(QRectF(bottomLeft.x(), y0 + 2, bottomRight.x() - bottomLeft.x(), 18), Qt::AlignCenter, QStringLiteral("%1 mm").arg(width, 0, 'f', 1).replace('.', ','));
    const QPointF topRight = map(b[1], b[3]);
    const double x0 = bottomRight.x() + 24;
    p.drawLine(QPointF(x0, topRight.y()), QPointF(x0, bottomRight.y()));
    p.drawLine(QPointF(x0 - 4, topRight.y()), QPointF(x0 + 4, topRight.y()));
    p.drawLine(QPointF(x0 - 4, bottomRight.y()), QPointF(x0 + 4, bottomRight.y()));
    p.save();
    p.translate(x0 + 14, (topRight.y() + bottomRight.y()) / 2.0);
    p.rotate(-90);
    p.drawText(QRectF(-60, -9, 120, 18), Qt::AlignCenter, QStringLiteral("%1 mm").arg(height, 0, 'f', 1).replace('.', ','));
    p.restore();
    p.end();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return std::vector<unsigned char>(bytes.begin(), bytes.end());
}

/// Linhas de calculo (texto simples) -> blocos do documento.
void appendCalculationLines(ReportDocument &doc, const std::vector<std::string> &lines)
{
    for (const std::string &line : lines) {
        const auto pieces = convertCalculationLine(line);
        std::vector<std::string> mathGroup;
        auto flushMath = [&]() {
            if (mathGroup.empty()) {
                return;
            }
            // junta formulas curtas na mesma linha
            std::string joined;
            std::size_t length = 0;
            for (const std::string &m : mathGroup) {
                if (!joined.empty() && length + m.size() > 150) {
                    doc.equation(joined);
                    joined.clear();
                    length = 0;
                }
                if (!joined.empty()) {
                    joined += "\\qquad ";
                }
                joined += m;
                length += m.size();
            }
            if (!joined.empty()) {
                doc.equation(joined);
            }
            mathGroup.clear();
        };
        for (const FormulaPiece &piece : pieces) {
            if (piece.kind == FormulaPiece::Kind::Math) {
                mathGroup.push_back(piece.text);
            } else {
                flushMath();
                const bool label = !piece.text.empty() && piece.text.back() == ':';
                doc.blocks.push_back(paragraphOf(piece.text, label ? ParagraphStyle::Label : ParagraphStyle::Small));
            }
        }
        flushMath();
    }
}

CellStatus statusOf(double eta)
{
    return eta > 1.0 ? CellStatus::Fail : (eta > 0.9 ? CellStatus::Warn : CellStatus::Ok);
}

QString situationOf(double eta)
{
    return eta > 1.0 ? QStringLiteral("Não atende") : (eta > 0.9 ? QStringLiteral("Atende (limite)") : QStringLiteral("Atende"));
}

void appendCheck(ReportDocument &doc, const Structura::Design::CheckResult &check, bool showFormulas)
{
    doc.heading(3, restoreAccents(check.title) + "  —  " + restoreAccents(check.clause));
    if (showFormulas) {
        appendCalculationLines(doc, check.lines);
    }
    // resultado
    const QString unit = QString::fromStdString(check.unit);
    if (check.resistance == 1.0 && check.unit.empty()) {
        doc.equation("\\eta = " + latexNumber(check.ratio) + (check.ratio <= 1.0 ? " \\le 1{,}000" : " > 1{,}000"));
    } else {
        std::string unitLatex;
        if (!unit.isEmpty()) {
            QStringList parts = unit.split(QLatin1Char('*'));
            QString text;
            for (int i = 0; i < parts.size(); ++i) {
                text += (i ? QStringLiteral("\\cdot ") : QString()) + QStringLiteral("\\mathrm{%1}").arg(parts[i]);
            }
            unitLatex = "\\," + S(text);
        }
        doc.equation("\\eta = \\frac{S_{d}}{R_{d}} = \\frac{" + latexNumber(check.demand, 2) + unitLatex + "}{" + latexNumber(check.resistance, 2) + unitLatex + "} = " +
                     latexNumber(check.ratio, 3));
    }
    const QString verdict = QStringLiteral("η = %1  —  %2").arg(etaText(check.ratio), situationOf(check.ratio));
    doc.blocks.push_back(paragraphOf(S(verdict), check.ratio > 1.0 ? ParagraphStyle::Fail : (check.ratio > 0.9 ? ParagraphStyle::Warn : ParagraphStyle::Ok), true));
}

} // namespace

QVector<ReportService::ItemInfo> ReportService::calculationItems()
{
    return {{QStringLiteral("geometry"), QStringLiteral("Relação largura-espessura")},
            {QStringLiteral("slenderness"), QStringLiteral("Limitação de esbeltez")},
            {QStringLiteral("tension"), QStringLiteral("Tração axial")},
            {QStringLiteral("compression"), QStringLiteral("Compressão axial")},
            {QStringLiteral("flexure_x"), QStringLiteral("Flexão em torno de X")},
            {QStringLiteral("flexure_y"), QStringLiteral("Flexão em torno de Y")},
            {QStringLiteral("shear_y"), QStringLiteral("Cortante (alma)")},
            {QStringLiteral("shear_z"), QStringLiteral("Cortante (mesas)")},
            {QStringLiteral("interaction"), QStringLiteral("Interação esforço normal e momentos")},
            {QStringLiteral("mv_x"), QStringLiteral("Momento e cortante combinados (X)")},
            {QStringLiteral("mv_y"), QStringLiteral("Momento e cortante combinados (Y)")},
            {QStringLiteral("torsion"), QStringLiteral("Torção")}};
}

namespace {

void appendBarDetail(ReportDocument &doc, const ReportService::Input &input, const DesignService::MemberVerdict &v, const QSet<QString> &itemIds,
                     bool showFormulas, bool drawing, int level)
{
    using Structura::App::Services::DesignService;
    const SectionInfo section = input.sectionOfMember.value(v.memberId);
    doc.heading(level, S(QStringLiteral("Barra %1 — %2").arg(v.memberId).arg(v.sectionName)));

    // dados da barra
    const auto params = input.designSettings.paramsFor(v.memberId);
    Table data;
    data.header = {S(QStringLiteral("Dado")), S(QStringLiteral("Valor"))};
    data.widths = {2, 3};
    data.firstColumnBold = true;
    const auto steel = DesignService::findSteel(v.steel);
    data.rows = {{"Perfil", S(v.sectionName)},
                 {"Aço", S(steel ? QStringLiteral("%1 (fy = %2 MPa, fu = %3 MPa)").arg(v.steel).arg(steel->fy, 0, 'f', 0).arg(steel->fu, 0, 'f', 0) : v.steel)},
                 {"Norma", v.output.standard},
                 {"Comprimento", S(num(v.lengthM, 3) + QStringLiteral(" m"))},
                 {"Comprimentos de flambagem", S(QStringLiteral("x: %1; y: %2; z: %3")
                     .arg(params.lx > 0.0 ? QStringLiteral("%1 m (informado)").arg(num(params.lx, 3)) : QStringLiteral("Kx = %1").arg(num(params.kx, 2)),
                          params.ly > 0.0 ? QStringLiteral("%1 m (informado)").arg(num(params.ly, 3)) : QStringLiteral("Ky = %1").arg(num(params.ky, 2)),
                          params.lz > 0.0 ? QStringLiteral("%1 m (informado)").arg(num(params.lz, 3)) : QStringLiteral("Kz = %1").arg(num(params.kz, 2))))},
                 {"Contenção lateral", S(params.braced ? QStringLiteral("mesa comprimida contida") : QStringLiteral("%1 trecho(s) destravado(s)").arg(params.segments))}};
    if (v.output.maxRatio > 0.0) {
        data.rows.push_back({"Combinação governante", v.output.governingCombo + S(QStringLiteral(" (x = %1 m)").arg(num(v.output.governingX / 1000.0, 3)))});
    }
    doc.table(data);

    // secao
    if (drawing && !section.family.isEmpty()) {
        const auto ds = DesignService::makeSection(section, input.designSettings.rectTubeStandard);
        if (ds.supported()) {
            Table props;
            props.header = {S(QStringLiteral("Propriedade")), S(QStringLiteral("Valor")), S(QStringLiteral("Propriedade")), S(QStringLiteral("Valor"))};
            props.widths = {2, 2, 2, 2};
            props.align = {'l', 'r', 'l', 'r'};
            props.rows = {{"A (mm²)", S(sci(ds.g.area, 1)), "J (mm⁴)", S(sci(ds.J, 3))},
                          {"Ix (mm⁴)", S(sci(ds.g.Ix, 3)), "Iy (mm⁴)", S(sci(ds.g.Iy, 3))},
                          {"Wx mín. (mm³)", S(sci(ds.g.Wx(), 3)), "Wy mín. (mm³)", S(sci(ds.g.Wy(), 3))},
                          {"rx (mm)", S(num(ds.g.rx, 1)), "ry (mm)", S(num(ds.g.ry, 1))},
                          {"Zx (mm³)", S(sci(ds.g.Zx, 3)), "Zy (mm³)", S(sci(ds.g.Zy, 3))},
                          {"Cw (mm⁶)", S(sci(ds.Cw, 3)), "Peso (kg/m)", S(num(ds.weightPerMeter(), 2))}};
            props.firstColumnBold = true;
            Image img;
            img.png = sectionPng(section);
            img.widthCm = 7.5;
            img.caption = S(QStringLiteral("Seção transversal (eixos principais pelo centroide)"));
            if (!img.png.empty()) {
                doc.blocks.push_back(img);
            }
            doc.table(props);
        }
    }

    if (!v.supported || v.output.checks.empty()) {
        doc.blocks.push_back(paragraphOf("Barra não verificada: " + (v.output.warnings.empty() ? std::string("sem esforços ou perfil sem geometria de aço.") : v.output.warnings.front()),
                                         ParagraphStyle::Warn));
        return;
    }

    doc.blocks.push_back(paragraphOf(S(QStringLiteral("Aproveitamento máximo η = %1 — %2").arg(etaText(v.output.maxRatio), situationOf(v.output.maxRatio))),
                                     v.output.maxRatio > 1.0 ? ParagraphStyle::Fail : (v.output.maxRatio > 0.9 ? ParagraphStyle::Warn : ParagraphStyle::Ok), true));

    // resumo das verificacoes
    Table summary;
    summary.caption = S(QStringLiteral("Verificações da barra %1").arg(v.memberId));
    summary.header = {S(QStringLiteral("Verificação")), S(QStringLiteral("Item da norma")), S(QStringLiteral("η")), S(QStringLiteral("Situação"))};
    summary.widths = {4, 3, 1, 2};
    summary.align = {'l', 'l', 'r', 'c'};
    for (const auto &c : v.output.checks) {
        if (!itemIds.isEmpty() && !itemIds.contains(QString::fromStdString(c.id))) {
            continue;
        }
        summary.rows.push_back({restoreAccents(c.title), restoreAccents(c.clause), S(etaText(c.ratio)), S(situationOf(c.ratio))});
        summary.status.push_back({CellStatus::None, CellStatus::None, statusOf(c.ratio), statusOf(c.ratio)});
    }
    if (!summary.rows.empty()) {
        doc.table(summary);
    }

    if (showFormulas && !v.output.summaryLines.empty()) {
        doc.blocks.push_back(paragraphOf("Dados utilizados", ParagraphStyle::Label));
        for (const std::string &line : v.output.summaryLines) {
            doc.blocks.push_back(paragraphOf(restoreAccents(decimalComma(line)), ParagraphStyle::Small));
        }
    }

    for (const auto &c : v.output.checks) {
        if (!itemIds.isEmpty() && !itemIds.contains(QString::fromStdString(c.id))) {
            continue;
        }
        appendCheck(doc, c, showFormulas);
    }
    for (const std::string &w : v.output.warnings) {
        doc.blocks.push_back(paragraphOf(restoreAccents(w), ParagraphStyle::Warn));
    }
}

} // namespace

ReportDocument ReportService::buildBar(const Input &input, int memberId, const QSet<QString> &itemIds, bool showFormulas, bool withSectionDrawing)
{
    ReportDocument doc;
    doc.title = S(QStringLiteral("Memorial de cálculo — barra %1").arg(memberId));
    doc.project = S(input.projectName);
    doc.author = S(input.author);
    doc.date = S(input.date);
    doc.cover = false;
    doc.tableOfContents = false;
    doc.headerText = doc.title;
    for (const auto &v : input.verdicts) {
        if (v.memberId == memberId) {
            appendBarDetail(doc, input, v, itemIds, showFormulas, withSectionDrawing, 1);
            break;
        }
    }
    return doc;
}

ReportDocument ReportService::buildGlobal(const Input &input, const Options &o)
{
    ReportDocument doc;
    doc.title = S(QStringLiteral("Memorial de Cálculo"));
    doc.subtitle = S(QStringLiteral("Análise e dimensionamento de estrutura de aço"));
    doc.project = S(input.projectName);
    doc.author = S(input.author);
    doc.date = S(input.date);
    doc.cover = o.cover;
    doc.tableOfContents = o.toc;
    doc.headerText = S(QStringLiteral("Memorial de Cálculo — %1").arg(input.projectName));
    int chapter = 0;
    auto chapterTitle = [&](const QString &title) { doc.heading(1, S(QStringLiteral("%1. %2").arg(++chapter).arg(title))); };

    if (o.criteria) {
        chapterTitle(QStringLiteral("Critérios e normas"));
        doc.paragraph("Este memorial apresenta os dados de entrada, as ações, os resultados da análise estrutural e a verificação dos perfis de aço. "
                      "As verificações seguem as normas indicadas abaixo; os cálculos são reproduzidos com as fórmulas e os valores utilizados.");
        Table t;
        t.header = {"Assunto", "Critério"};
        t.widths = {2, 5};
        t.firstColumnBold = true;
        t.rows = {{"Aço laminado e tubos circulares", "ABNT NBR 8800:2024 — projeto de estruturas de aço"},
                  {"Aço formado a frio e tubos retangulares", "ABNT NBR 14762:2010 — perfis formados a frio (método da largura efetiva)"},
                  {"Ações do vento", "ABNT NBR 6123 — forças devidas ao vento em edificações"},
                  {"Unidades", S(QStringLiteral("Forças em %1, comprimentos em m; dimensionamento em mm, N e MPa").arg(input.forceUnit))},
                  {"Convenção de esforços", "N > 0 tração; Mx > 0 comprime a fibra superior (+y local); My > 0 comprime +z local"},
                  {"Efeitos de segunda ordem", S(input.designSettings.secondOrder
                                                      ? QStringLiteral("Análise de 2ª ordem (P-Δ e P-δ) com redução de rigidez %1 nas combinações últimas").arg(num(input.designSettings.stiffnessReduction, 2))
                                                      : (input.designSettings.autoB2 ? QStringLiteral("Análise de 1ª ordem com amplificação B1 e B2 (NBR 8800, Anexo C)") : QStringLiteral("Análise de 1ª ordem sem amplificação")))}};
        doc.table(t);
    }

    if (o.materialsAndSections) {
        chapterTitle(QStringLiteral("Materiais e seções"));
        Table m;
        m.caption = S(QStringLiteral("Materiais"));
        m.header = {"Material", "E", "G", "fy (MPa)", "fu (MPa)", "γ (kN/m³)"};
        m.widths = {3, 2, 2, 1, 1, 1};
        m.align = {'l', 'r', 'r', 'r', 'r', 'r'};
        for (const auto &mat : input.materials) {
            m.rows.push_back({S(mat.name), S(sci(mat.youngModulus, 3)), S(sci(mat.shearModulus, 3)), mat.fy > 0 ? S(num(mat.fy, 0)) : "-", mat.fu > 0 ? S(num(mat.fu, 0)) : "-",
                              S(num(mat.unitWeight, 1))});
        }
        doc.table(m);
        Table s;
        s.caption = S(QStringLiteral("Seções"));
        s.header = {"Seção", "Família", "A (cm²)", "Iz (cm⁴)", "Iy (cm⁴)", "J (cm⁴)", "kg/m"};
        s.widths = {3, 3, 1, 1.4, 1.4, 1, 1};
        s.align = {'l', 'l', 'r', 'r', 'r', 'r', 'r'};
        for (const auto &sec : input.sections) {
            s.rows.push_back({S(sec.name), S(sec.family.isEmpty() ? QStringLiteral("propriedades") : sec.family), S(num(sec.area * 1e4, 2)), S(num(sec.iz * 1e8, 1)), S(num(sec.iy * 1e8, 1)),
                              S(num(sec.j * 1e8, 2)), S(num(sec.area * 7850.0, 2))});
        }
        doc.table(s);
    }

    if (o.geometry) {
        chapterTitle(QStringLiteral("Geometria do modelo"));
        Table n;
        n.caption = S(QStringLiteral("Nós (coordenadas em m)"));
        n.header = {"Nó", "X", "Y", "Z", "Vínculo"};
        n.widths = {1, 1.2, 1.2, 1.2, 3};
        n.align = {'c', 'r', 'r', 'r', 'l'};
        for (const auto &node : input.nodes) {
            n.rows.push_back({std::to_string(node.id), S(num(node.x, 3)), S(num(node.y, 3)), S(num(node.z, 3)), S(node.support.isEmpty() ? QStringLiteral("livre") : node.support)});
        }
        doc.table(n);
        Table b;
        b.caption = S(QStringLiteral("Barras"));
        b.header = {"Barra", "Nó i", "Nó j", "L (m)", "Seção", "Grupo", "Posição / vínculos"};
        b.widths = {1, 1, 1, 1.2, 3, 1.5, 3};
        b.align = {'c', 'c', 'c', 'r', 'l', 'l', 'l'};
        for (const auto &bar : input.bars) {
            b.rows.push_back({std::to_string(bar.id), std::to_string(bar.nodeI), std::to_string(bar.nodeJ), S(num(bar.length, 3)), S(bar.section), S(bar.group), S(bar.placement)});
        }
        doc.table(b);
    }

    if (o.loads) {
        chapterTitle(QStringLiteral("Ações e combinações"));
        Table c;
        c.caption = S(QStringLiteral("Casos de carga"));
        c.header = {"Caso", "Abreviação", "Peso próprio"};
        c.widths = {3, 1, 1};
        for (const auto &lc : input.extras.loadCases) {
            c.rows.push_back({S(lc.name), S(lc.abbreviation), lc.selfWeight ? "sim" : "não"});
        }
        doc.table(c);
        Table comb;
        comb.caption = S(QStringLiteral("Combinações"));
        comb.header = {"Combinação", "Composição"};
        comb.widths = {2, 6};
        for (const auto &co : input.extras.combinations) {
            QStringList parts;
            for (const auto &term : co.factors) {
                parts << QStringLiteral("%1 × %2").arg(num(term.second, 2), term.first);
            }
            comb.rows.push_back({S(co.name), S(parts.join(QStringLiteral("  +  ")))});
        }
        if (!comb.rows.empty()) {
            doc.table(comb);
        }
        if (!input.nodalLoads.isEmpty()) {
            Table nl;
            nl.caption = S(QStringLiteral("Cargas nodais (%1, %1·m)").arg(input.forceUnit));
            nl.header = {"Nó", "Caso", "Fx", "Fy", "Fz", "Mx", "My", "Mz"};
            nl.widths = {1, 2, 1, 1, 1, 1, 1, 1};
            nl.align = {'c', 'l', 'r', 'r', 'r', 'r', 'r', 'r'};
            for (const auto &l : input.nodalLoads) {
                nl.rows.push_back({std::to_string(l.nodeId), S(l.loadCase), S(num(l.fx)), S(num(l.fy)), S(num(l.fz)), S(num(l.mx)), S(num(l.my)), S(num(l.mz))});
            }
            doc.table(nl);
        }
        if (!input.memberLoads.isEmpty()) {
            Table ml;
            ml.caption = S(QStringLiteral("Cargas distribuídas (%1/m)").arg(input.forceUnit));
            ml.header = {"Barra", "Caso", "Sistema", "qx", "qy", "qz"};
            ml.widths = {1, 2, 1.5, 1, 1, 1};
            ml.align = {'c', 'l', 'c', 'r', 'r', 'r'};
            for (const auto &l : input.memberLoads) {
                ml.rows.push_back({std::to_string(l.memberId), S(l.loadCase), S(l.system), S(num(l.qx)), S(num(l.qy)), S(num(l.qz))});
            }
            doc.table(ml);
        }
    }

    if (o.analysisResults && !input.results.isEmpty()) {
        chapterTitle(QStringLiteral("Resultados da análise"));
        Table t;
        t.caption = S(QStringLiteral("Resumo por combinação / caso"));
        t.header = {"Combinação / caso", "Deslocamento máx. (mm)", "Nó", S(QStringLiteral("ΣRx (%1)").arg(input.forceUnit)), S(QStringLiteral("ΣRy (%1)").arg(input.forceUnit)), S(QStringLiteral("ΣRz (%1)").arg(input.forceUnit))};
        t.widths = {3, 2, 1, 2, 2, 2};
        t.align = {'l', 'r', 'c', 'r', 'r', 'r'};
        for (const auto &r : input.results) {
            t.rows.push_back({S(r.name), S(num(r.maxDisplacementMm, 2)), std::to_string(r.maxDisplacementNode), S(num(r.sumRx)), S(num(r.sumRy)), S(num(r.sumRz))});
        }
        doc.table(t);
        for (const auto &r : input.results) {
            if (!r.isCombination || r.reactions.isEmpty()) {
                continue;
            }
            Table rt;
            rt.caption = S(QStringLiteral("Reações de apoio — %1").arg(r.name));
            rt.header = {"Nó", "Rx", "Ry", "Rz", "Mx", "My", "Mz"};
            rt.widths = {1, 1, 1, 1, 1, 1, 1};
            rt.align = {'c', 'r', 'r', 'r', 'r', 'r', 'r'};
            for (const auto &row : r.reactions) {
                std::vector<std::string> cells {std::to_string(row.node)};
                for (double value : row.values) {
                    cells.push_back(S(num(value)));
                }
                rt.rows.push_back(cells);
            }
            doc.table(rt);
        }
    }

    if (input.hasDesign && (o.designSummary || o.designDetails || o.serviceChecks)) {
        chapterTitle(QStringLiteral("Dimensionamento dos perfis"));
        QVector<DesignService::MemberVerdict> verdicts = input.verdicts;
        std::sort(verdicts.begin(), verdicts.end(), [](const auto &a, const auto &b) { return a.output.maxRatio > b.output.maxRatio; });
        QVector<DesignService::MemberVerdict> selected;
        for (const auto &v : verdicts) {
            const bool checked = v.supported && !v.output.checks.empty();
            if (o.scope == BarScope::Failing && !(checked && v.output.maxRatio > 1.0)) {
                continue;
            }
            if (o.scope == BarScope::Selected && !o.selectedMembers.contains(v.memberId)) {
                continue;
            }
            selected.append(v);
        }
        int ok = 0, fail = 0;
        double worst = 0.0;
        for (const auto &v : input.verdicts) {
            if (!v.supported || v.output.checks.empty()) {
                continue;
            }
            (v.output.maxRatio > 1.0 ? fail : ok)++;
            worst = std::max(worst, v.output.maxRatio);
        }
        doc.blocks.push_back(paragraphOf(S(QStringLiteral("%1 barra(s) atendem e %2 não atendem; maior aproveitamento η = %3.").arg(ok).arg(fail).arg(etaText(worst))),
                                         fail > 0 ? ParagraphStyle::Fail : ParagraphStyle::Ok, true));
        if (o.designSummary) {
            Table t;
            t.caption = S(QStringLiteral("Resumo do dimensionamento (ordenado por aproveitamento)"));
            t.header = {"Barra", "Perfil", "Aço", "L (m)", "η", "Verificação governante", "Situação"};
            t.widths = {1, 3, 2, 1, 1, 4, 2};
            t.align = {'c', 'l', 'l', 'r', 'r', 'l', 'c'};
            for (const auto &v : selected) {
                const bool checked = v.supported && !v.output.checks.empty();
                if (!checked) {
                    t.rows.push_back({std::to_string(v.memberId), S(v.sectionName), S(v.steel), S(num(v.lengthM, 2)), "-", "não verificada", "-"});
                    t.status.push_back({});
                    continue;
                }
                t.rows.push_back({std::to_string(v.memberId), S(v.sectionName), S(v.steel), S(num(v.lengthM, 2)), S(etaText(v.output.maxRatio)), restoreAccents(v.output.checks.front().title),
                                  S(situationOf(v.output.maxRatio))});
                t.status.push_back({CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, statusOf(v.output.maxRatio), CellStatus::None, statusOf(v.output.maxRatio)});
            }
            doc.table(t);
        }
        if (o.designDetails) {
            doc.heading(2, S(QStringLiteral("Memória de cálculo por barra")));
            int detailed = 0;
            for (const auto &v : selected) {
                if (o.maxDetailBars > 0 && detailed++ >= o.maxDetailBars) {
                    doc.blocks.push_back(paragraphOf(S(QStringLiteral("(prévia) demais barras omitidas; o arquivo exportado contém todas.")), ParagraphStyle::Note));
                    break;
                }
                appendBarDetail(doc, input, v, o.itemIds, o.showFormulas, o.sectionDrawings, 2);
            }
        }
        if (o.serviceChecks) {
            doc.heading(2, S(QStringLiteral("Estados-limites de serviço e estabilidade")));
            if (!input.deflections.isEmpty()) {
                Table t;
                t.caption = S(QStringLiteral("Flechas (combinações ELS; NBR 8800, Tabela B.1)"));
                t.header = {"Barra", "Comb.", "δ (mm)", "Limite (mm)", "Limite", "δ / limite"};
                t.widths = {1, 2, 1, 1, 1, 1};
                t.align = {'c', 'l', 'r', 'r', 'c', 'r'};
                QVector<StructuralChecks::DeflectionResult> defl = input.deflections;
                std::sort(defl.begin(), defl.end(), [](const auto &a, const auto &b) { return a.ratio > b.ratio; });
                for (const auto &d : defl) {
                    t.rows.push_back({std::to_string(d.memberId), S(d.combination), S(num(d.deflectionMm, 1)), S(num(d.limitMm, 1)),
                                      S(d.cantilever ? QStringLiteral("2L/%1").arg(d.denominator) : QStringLiteral("L/%1").arg(d.denominator)), S(num(d.ratio, 2))});
                    t.status.push_back({CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, statusOf(d.ratio)});
                }
                doc.table(t);
                doc.equation("\\delta_{\\max} \\le \\frac{L}{n}");
            }
            if (!input.storyResults.isEmpty()) {
                Table t;
                t.caption = S(QStringLiteral("Deslocamentos por andar e amplificação B2 (Anexo C da NBR 8800)"));
                t.header = {"Andar", "Comb.", "Δ rel. (mm)", "h (mm)", "B2 x", "B2 y"};
                t.widths = {2, 2, 1, 1, 1, 1};
                t.align = {'l', 'l', 'r', 'r', 'r', 'r'};
                for (const auto &r : input.storyResults) {
                    const auto &story = r.story < input.stories.size() ? input.stories[r.story] : StructuralChecks::Story {};
                    t.rows.push_back({S(QStringLiteral("Andar %1 (%2–%3 m)").arg(r.story + 1).arg(num(story.zBottom, 1)).arg(num(story.zTop, 1))), S(r.combination),
                                      S(num(std::max(r.driftXMm, r.driftYMm), 1)), S(num(r.heightMm, 0)), S(num(r.b2x, 3)), S(num(r.b2y, 3))});
                }
                doc.table(t);
                doc.equation("B_{2} = \\frac{1}{1 - \\frac{1}{R_{s}}\\,\\frac{\\Delta_{h}}{h}\\,\\frac{\\Sigma N_{Sd}}{\\Sigma H_{Sd}}}");
            }
            for (const QString &w : input.designWarnings) {
                doc.blocks.push_back(paragraphOf(S(w), ParagraphStyle::Warn));
            }
        }
    }
    if (o.connections && !input.connections.isEmpty()) {
        chapterTitle(QStringLiteral("Ligações"));
        doc.paragraph("Ligações verificadas conforme a ABNT NBR 8800:2024 (6.2 solda, 6.3 parafusos, 6.5 elementos de ligação, 6.7 bases de pilares), a ABNT NBR 6118:2023 (pressão de contato no bloco) "
                      "e o CIDECT Design Guide 1 (ligações soldadas de perfis tubulares circulares). "
                      "Esforços de cálculo: combinações últimas na extremidade da barra (viga–apoio: cortante resultante e axial tratado como cortante nos parafusos; chapa de nó: tração e compressão máximas da diagonal; "
                      "emenda, base e ligação tubular: combinação a combinação, prevalece a mais desfavorável). Distribuição elástica de parafusos e soldas; efeito alavanca e o apoio (alma/mesa do pilar ou do banzo) não verificados, exceto na ligação tubular.");
        Table summary;
        summary.caption = S(QStringLiteral("Resumo das ligações"));
        summary.header = {"Nó", "Viga", "Tipo", "Descrição", "V (kN)", "N (kN)", "η"};
        summary.widths = {1, 1.4, 2.4, 5, 1, 1, 1};
        summary.align = {'c', 'c', 'l', 'l', 'r', 'r', 'r'};
        for (const auto &entry : input.connections) {
            const auto &c = entry.connection;
            summary.rows.push_back({std::to_string(c.nodeId), S(QStringLiteral("%1 (%2)").arg(c.memberId).arg(c.atEnd ? QStringLiteral("fim") : QStringLiteral("início"))),
                                    S(ConnectionService::typeName(c.type)), S(entry.evaluation.description),
                                    entry.hasForces ? S(num(entry.forces.shear / 1000.0, 1)) : "-", entry.hasForces ? S(num(entry.forces.axial / 1000.0, 1)) : "-",
                                    S(num(entry.evaluation.maxRatio, 2))});
            summary.status.push_back({CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, statusOf(entry.evaluation.maxRatio)});
        }
        doc.table(summary);
        for (const auto &entry : input.connections) {
            const auto &c = entry.connection;
            doc.heading(2, S(QStringLiteral("Ligação no nó %1 — viga %2 (%3)").arg(c.nodeId).arg(c.memberId).arg(ConnectionService::typeName(c.type))));
            doc.paragraph(S(entry.evaluation.description));
            if (!entry.evaluation.governingCase.isEmpty() && entry.hasForces) {
                doc.paragraph(S(QStringLiteral("Combinação governante: %1.").arg(entry.evaluation.governingCase)));
            }
            Table t;
            t.header = {"Verificação", "Item NBR", "Solicitação", "Resistência", "Razão"};
            t.widths = {5, 2, 1.6, 1.6, 1};
            t.align = {'l', 'l', 'r', 'r', 'r'};
            for (const auto &check : entry.evaluation.checks) {
                t.rows.push_back({check.name, check.clause, S(QStringLiteral("%1 %2").arg(num(check.demand, 2), QString::fromStdString(check.unit))),
                                  S(QStringLiteral("%1 %2").arg(num(check.resistance, 2), QString::fromStdString(check.unit))), S(num(check.ratio(), 3))});
                t.status.push_back({CellStatus::None, CellStatus::None, CellStatus::None, CellStatus::None, statusOf(check.ratio())});
            }
            doc.table(t);
            if (o.showFormulas) {
                for (const auto &check : entry.evaluation.checks) {
                    if (check.lines.empty()) {
                        continue;
                    }
                    doc.blocks.push_back(paragraphOf(check.name + ":", ParagraphStyle::Label));
                    appendCalculationLines(doc, check.lines);
                }
            }
        }
    }
    return doc;
}

} // namespace Structura::App::Services
