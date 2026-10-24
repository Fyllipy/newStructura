#include "ReportService.h"

#include "report/CalcCardBuilder.h"
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

/// 9.031e+07 -> "9,031 × 10⁷"
QString sciPretty(double v)
{
    const int exponent = static_cast<int>(std::floor(std::log10(std::abs(v))));
    const double mantissa = v / std::pow(10.0, exponent);
    static const QString digits[] = {QStringLiteral("⁰"), QStringLiteral("¹"), QStringLiteral("²"), QStringLiteral("³"), QStringLiteral("⁴"),
                                     QStringLiteral("⁵"), QStringLiteral("⁶"), QStringLiteral("⁷"), QStringLiteral("⁸"), QStringLiteral("⁹")};
    QString power;
    for (const QChar c : QString::number(exponent)) {
        power += c == QLatin1Char('-') ? QStringLiteral("⁻") : digits[c.digitValue()];
    }
    return num(mantissa, 3) + QStringLiteral(" × 10") + power;
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

std::string descriptionFor(const std::string &id, bool cold)
{
    if (id == "slenderness") {
        return cold ? "O índice de esbeltez das barras comprimidas, tomado como a maior relação entre o comprimento de flambagem e o raio de giração, não deve ser superior a 200 (NBR 14762, 9.7.4)."
                    : "O índice de esbeltez das barras comprimidas, tomado como a maior relação entre o comprimento de flambagem e o raio de giração, não deve ser superior a 200 (NBR 8800, 5.3.7.1).";
    }
    if (id == "tension") {
        return cold ? "A força axial de tração resistente de cálculo é o menor valor entre o escoamento da seção bruta e a ruptura da seção líquida efetiva (NBR 14762, 9.6)."
                    : "A força axial de tração resistente de cálculo é o menor valor entre o escoamento da seção bruta (γa1 = 1,10) e a ruptura da seção líquida efetiva (γa2 = 1,35), conforme a NBR 8800, 5.2.2.";
    }
    if (id == "compression") {
        return cold ? "A força axial de compressão resistente de cálculo é Nc,Rd = χ Aef fy / γ, com χ da flambagem global (9.7.2: flexão, torção e flexo-torção), área efetiva Aef pela flambagem local "
                      "(método da largura efetiva, 9.2) e verificação da flambagem distorcional (9.7.3)."
                    : "A força axial de compressão resistente de cálculo é Nc,Rd = χ Q Ag fy / γa1. O fator χ depende do índice de esbeltez reduzido λ0, obtido da força axial de flambagem elástica Ne "
                      "(a menor entre flexão em X, flexão em Y e torção ou flexo-torção); Q reduz a resistência pela flambagem local dos elementos esbeltos (NBR 8800, 5.3.3 a 5.3.5).";
    }
    if (id == "flexure_x" || id == "flexure_y") {
        return cold ? "O momento fletor resistente de cálculo é o menor valor entre o início de escoamento da seção efetiva, a flambagem lateral com torção e a flambagem distorcional (NBR 14762, 9.8.2)."
                    : "O momento fletor resistente de cálculo é o menor valor entre os estados-limites aplicáveis ao tipo de seção: flambagem lateral com torção (FLT), flambagem local da mesa (FLM) e "
                      "flambagem local da alma (FLA), calculados conforme o Anexo D e limitados pela capacidade elástica de 1,5 W fy / γa1 (NBR 8800, 5.4.2).";
    }
    if (id == "shear_y" || id == "shear_z") {
        return cold ? "A força cortante resistente de cálculo depende da esbeltez h/t do painel: escoamento por cisalhamento quando h/t ≤ λp e redução por flambagem por cisalhamento acima disso (NBR 14762, 9.8.3)."
                    : "A força cortante resistente de cálculo depende da esbeltez λ do painel (alma ou mesas) comparada aos parâmetros λp e λr: plastificação por cisalhamento Vpl = 0,60 Aw fy quando λ ≤ λp e "
                      "redução por flambagem acima disso (NBR 8800, 5.4.3).";
    }
    if (id == "interaction") {
        return cold ? "Barras sob força axial e momentos fletores: NSd/NRd + Mx,Sd/Mx,Rd + My,Sd/My,Rd ≤ 1 (NBR 14762, 9.9)."
                    : "Barras sob força axial e momentos fletores: para NSd/NRd ≥ 0,2, NSd/NRd + 8/9 (Mx,Sd/Mx,Rd + My,Sd/My,Rd) ≤ 1; para NSd/NRd < 0,2, NSd/(2 NRd) + (Mx,Sd/Mx,Rd + My,Sd/My,Rd) ≤ 1 "
                      "(NBR 8800, 5.5.1.2).";
    }
    if (id == "mv_x" || id == "mv_y") {
        return "Atuação simultânea de momento fletor e força cortante: (MSd/MRd)² + (VSd/VRd)² ≤ 1,0 (NBR 14762, 9.8.4).";
    }
    if (id == "torsion") {
        return "Seções tubulares com momento de torção: TSd ≤ TRd (NBR 8800, 5.5.2.1). Quando TSd/TRd > 0,20 a torção entra nas interações com força axial, momento fletor e cortante (5.5.2.2).";
    }
    if (id == "warping_normal") {
        return "Seção I/H com torção com empenamento: as tensões normais pela Teoria da Elasticidade (N/A, M/W e B ω/Cw) devem ser menores que a resistência ao escoamento e, no lado comprimido, à instabilidade "
               "(NBR 8800, 5.5.2.3-a e 5.5.2.3-c).";
    }
    if (id == "warping_shear") {
        return "Seção I/H com torção com empenamento: as tensões de cisalhamento (Saint-Venant, empenamento e força cortante) devem ser menores que 0,60 fy/γa1, com redução na alma por flambagem "
               "(NBR 8800, 5.5.2.3-b e 5.5.2.3-d).";
    }
    if (id == "geometry") {
        return "Relações largura-espessura máximas dos elementos da seção, para que o método da largura efetiva seja aplicável (NBR 14762, 9.1.2, Tabela 4).";
    }
    if (id == "web_flange_weld" || id == "web_flange_weld_min") {
        return id == "web_flange_weld" ? "Solda de filete mesa–alma de perfil soldado: a força por unidade de comprimento Vs Qs/Ix (mais a força aplicada diretamente na mesa) é comparada à resistência do filete, "
                                         "0,60 fw Aw/γw2 (NBR 8800, 5.4.5)."
                                       : "A perna do filete mesa–alma não deve ser inferior à mínima da Tabela 10 em função da espessura do metal-base mais espesso (NBR 8800, 6.2.5 e 5.4.5).";
    }
    if (id.rfind("cover_plate_", 0) == 0) {
        return "Perfil com lamela soldada à mesa: a lamela deve se prolongar além do ponto teórico de corte por a_ℓ (b, 1,5 b ou 2 b conforme a solda de extremidade) e a solda de ligação deve resistir ao "
               "fluxo de cisalhamento Vs Qs/Ix (NBR 8800, 5.4.4).";
    }
    if (id == "plastic_hinge") {
        return "Viga contínua com análise rígido-plástica: o comprimento destravado da mesa comprimida junto às rótulas plásticas não deve exceder Lpd = [0,12 + 0,076 (M1/M2)] (E/fy) ry em seções I, "
               "e a seção deve ser compacta (NBR 8800, 4.11.5).";
    }
    if (id == "cc_limits") {
        return "Limites de aplicabilidade do método simplificado para pilares mistos: dupla simetria, seção constante, fator de contribuição do aço 0,1 ≤ δ ≤ 0,9, esbeltez reduzida λ0,m ≤ 2,0, "
               "área do aço ≥ 1 % da seção, armadura mínima de 0,4 %, cobrimentos e classificação da seção (NBR 8800:2024, M.1.3 e M.1.4).";
    }
    if (id == "cc_creep") {
        return "Quando a esbeltez reduzida ultrapassa λ0,lim = (90/π) √(Np,R/(EA)e), somam-se ao momento atuante os efeitos de retração e fluência do concreto, Mcc = NSd ecc, "
               "ecc = (MSd/NSd + L/300)(e^m − 1) (NBR 8800:2024, M.3.3).";
    }
    if (id == "cc_interface") {
        return "Transferência de esforços entre o perfil de aço e o concreto: a tensão de cisalhamento nas superfícies de contato não excede τRd da Tabela M.2; caso contrário são necessários "
               "conectores de cisalhamento na região de introdução de cargas (NBR 8800:2024, M.2).";
    }
    if (id == "composite") {
        return "Barra composta: o espaçamento entre as ligações dos perfis e o número de ligações intermediárias devem atender à NBR 8800, 5.3.6.1.";
    }
    return {};
}

std::string inequalityFor(const std::string &id)
{
    if (id == "tension") return "N_{t,Sd} \\le N_{t,Rd}";
    if (id == "compression") return "N_{c,Sd} \\le N_{c,Rd}";
    if (id == "flexure_x") return "M_{x,Sd} \\le M_{x,Rd}";
    if (id == "flexure_y") return "M_{y,Sd} \\le M_{y,Rd}";
    if (id == "shear_y") return "V_{y,Sd} \\le V_{y,Rd}";
    if (id == "shear_z") return "V_{z,Sd} \\le V_{z,Rd}";
    if (id == "slenderness") return "\\lambda \\le \\lambda_{lim}";
    if (id == "mv_x" || id == "mv_y") return "\\left(\\frac{M_{Sd}}{M_{Rd}}\\right)^{2} + \\left(\\frac{V_{Sd}}{V_{Rd}}\\right)^{2} \\le 1";
    if (id == "torsion") return "T_{Sd} \\le T_{Rd}";
    if (id == "warping_normal") return "\\sigma_{Sd} \\le \\sigma_{Rd}";
    if (id == "warping_shear") return "\\tau_{Sd} \\le \\tau_{Rd}";
    if (id == "composite") return "\\frac{\\ell}{r_{min}} \\le 0{,}75\\,\\left(\\frac{L}{r}\\right)_{0}";
    if (id == "geometry") return "\\left(\\frac{b}{t}\\right) \\le \\left(\\frac{b}{t}\\right)_{max}";
    return "\\eta \\le 1";
}

/// "NBR 8800:2024, 5.3.7" -> "ABNT NBR 8800:2024, Artigo 5.3.7"
std::string clauseTitle(const std::string &clause)
{
    const auto comma = clause.find(", ");
    if (clause.rfind("NBR", 0) != 0) {
        return clause;
    }
    return "ABNT " + (comma == std::string::npos ? clause : clause.substr(0, comma) + ", Artigo " + clause.substr(comma + 2));
}

Structura::Report::CalcCard cardOf(const std::string &id, const std::string &title, const std::string &clause, double demand, double resistance, const std::string &unit, double ratio,
                                    const std::vector<std::string> &lines, bool showFormulas, const CalcEnv &env)
{
    CalcCardInput in;
    in.title = title;
    in.clause = clauseTitle(clause);
    const bool cold = clause.find("14762") != std::string::npos;
    if (showFormulas) {
        in.description = descriptionFor(id, cold);
        if (clause.find("NBR 8800:2024, M.") != std::string::npos) {
            // pilar misto (Anexo M): os ids de compressao, flexao e interacao reaproveitam os das barras de aco, mas com outro texto
            if (id == "compression") {
                in.description = "Pilar misto de aço e concreto: NRd = χ Np,Rd, com Np,Rd = Npℓ,Rd na seção compacta (Aa fyd + Ac fcd1 + As fsd) e χ da curva de 5.3.3 com o índice de esbeltez reduzido "
                                 "λ0,m = √(Np,R/Ne), Ne = π² (EI)e/L² e (EI)e = Ea Ia + αc Ec Ic + Es Is (NBR 8800:2024, M.3 e M.4).";
            } else if (id == "flexure_x" || id == "flexure_y") {
                in.description = "Momento fletor resistente do pilar misto: Mpℓ,Rd = fyd (Za − Zan) + 0,5 fcd1 (Zc − Zcn) + fsd (Zs − Zsn), com a linha neutra plástica hn de M.5.5.3 (revestidos) ou M.5.5.4 (tubulares); "
                                 "seções tubulares semicompactas e esbeltas têm Me,Rd e Mcr,Rd (NBR 8800:2024, M.5.5).";
            } else if (id == "interaction") {
                in.description = "Flexo-compressão do pilar misto pelo modelo de cálculo I (interação de 5.5.1, M.5.2), II (momentos totais com imperfeição L/300 e fatores μ, M.5.3) ou III (cp, cm e csr, "
                                 "tubulares preenchidos, M.5.4) (NBR 8800:2024, Anexo M).";
            }
        }
        in.lines = lines;
    }
    in.demand = demand;
    in.resistance = resistance;
    in.unit = unit;
    in.ratio = ratio;
    in.inequalityLatex = inequalityFor(id);
    in.env = env;
    {
        // solicitante e resistente da propria verificacao (valores das linhas, quando houver, prevalecem)
        static const std::map<std::string, std::pair<const char *, const char *>> names = {
            {"tension", {"Nt,Sd", "Nt,Rd"}}, {"compression", {"Nc,Sd", "Nc,Rd"}}, {"flexure_x", {"Mx,Sd", "Mx,Rd"}}, {"flexure_y", {"My,Sd", "My,Rd"}},
            {"shear_y", {"Vy,Sd", "VRd"}}, {"shear_z", {"Vz,Sd", "VRd"}}};
        const auto it = names.find(id);
        if (it != names.end()) {
            in.env[normalizeSymbol(it->second.first)] = {formatNumber(demand, 2), unit};
            in.env[normalizeSymbol(it->second.second)] = {formatNumber(resistance, 2), unit};
        }
    }
    CalcCard card = buildCalcCard(in);
    if (id == "slenderness") {
        card.resultLabel = "\\lambda";
        card.resultValue = formatNumber(demand, 1);
        card.resultDetail = "limite " + formatNumber(resistance, 0);
    }
    return card;
}

void appendCheck(ReportDocument &doc, const Structura::Design::CheckResult &check, bool showFormulas, const CalcEnv &env)
{
    doc.blocks.push_back(cardOf(check.id, check.title, check.clause, check.demand, check.resistance, check.unit, check.ratio, check.lines, showFormulas, env));
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

    CalcEnv env;
    {
        auto put = [&](const char *key, double value, const char *unit, int decimals = 1) {
            if (value != 0.0) {
                env[normalizeSymbol(key)] = {value >= 1e6 ? S(sciPretty(value)) : formatNumber(value, decimals), unit};
            }
        };
        if (steel) {
            put("fy", steel->fy, "MPa", 0);
            put("fu", steel->fu, "MPa", 0);
        }
        put("E", 200000.0, "MPa", 0);
        put("G", 77000.0, "MPa", 0);
        put("L", v.lengthM * 1000.0, "mm", 0);
        put("Lx", params.lx > 0.0 ? params.lx * 1000.0 : params.kx * v.lengthM * 1000.0, "mm", 0);
        put("Ly", params.ly > 0.0 ? params.ly * 1000.0 : params.ky * v.lengthM * 1000.0, "mm", 0);
        put("Lz", params.lz > 0.0 ? params.lz * 1000.0 : params.kz * v.lengthM * 1000.0, "mm", 0);
        if (!section.family.isEmpty()) {
            const auto ds = DesignService::makeSection(section, input.designSettings.rectTubeStandard);
            if (ds.supported()) {
                put("A", ds.g.area, "mm2");
                put("Ag", ds.g.area, "mm2");
                put("Ix", ds.g.Ix, "mm4");
                put("Iy", ds.g.Iy, "mm4");
                put("J", ds.J, "mm4");
                put("Cw", ds.Cw, "mm6");
                put("Wx", ds.g.Wx(), "mm3");
                put("Wy", ds.g.Wy(), "mm3");
                put("Zx", ds.g.Zx, "mm3");
                put("Zy", ds.g.Zy, "mm3");
                put("rx", ds.g.rx, "mm");
                put("ry", ds.g.ry, "mm");
                put("x0", ds.x0, "mm");
                put("y0", ds.y0, "mm");
                put("d", ds.d, "mm");
                put("bf", ds.bf, "mm");
                put("tw", ds.tw, "mm", 2);
                put("tf", ds.tf, "mm", 2);
                put("h", ds.h, "mm");
                put("t", ds.t, "mm", 2);
            }
        }
    }
    for (const auto &c : v.output.checks) {
        if (!itemIds.isEmpty() && !itemIds.contains(QString::fromStdString(c.id))) {
            continue;
        }
        appendCheck(doc, c, showFormulas, env);
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
                  {"Deformação por cortante", S(input.designSettings.shearDeformation ? QStringLiteral("Considerada (barras de Timoshenko com áreas de cisalhamento das seções)")
                                                                                       : QStringLiteral("Desprezada (barras de Euler-Bernoulli)"))},
                  {"Imperfeições geométricas", S(input.designSettings.geometricActive()
                                                     ? QStringLiteral("Desaprumo inicial direto H/%1 nas combinações últimas (variantes ±X, ±Y), análise de 2ª ordem").arg(std::lround(1.0 / input.designSettings.imperfectionRatio))
                                                     : (input.designSettings.notionalActive() ? QStringLiteral("Forças nocionais (NBR 8800, 4.9.7.2)") : QStringLiteral("Não consideradas na análise")))},
                  {"Zonas rígidas nas extremidades", S(input.designSettings.rigidZoneFactor > 0.0
                                                           ? QStringLiteral("Fator %1 sobre a penetração da barra no perfil que a recebe; esforços fornecidos nos nós").arg(num(input.designSettings.rigidZoneFactor, 2))
                                                           : QStringLiteral("Não consideradas"))},
                  {"Efeitos de segunda ordem", S(input.designSettings.secondOrder
                                                      ? QStringLiteral("Análise de 2ª ordem (P-Δ e P-δ%1) com redução de rigidez %2 nas combinações últimas")
                                                            .arg(input.designSettings.stabilityFunctions && !input.designSettings.shearDeformation
                                                                     ? QStringLiteral(", funções de estabilidade") : QString(),
                                                                 num(input.designSettings.stiffnessReduction, 2))
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
                    doc.blocks.push_back(cardOf("", check.name, check.clause, check.demand, check.resistance, check.unit, check.ratio(), check.lines, true, {}));
                }
            }
        }
    }
    return doc;
}

} // namespace Structura::App::Services
