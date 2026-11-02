#include "ReportService.h"

#include "app/Edition.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

using namespace Structura::Report;

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

} // namespace

ReportDocument ReportService::buildGlobal(const Input &input, const Options &o)
{
    ReportDocument doc;
    doc.title = S(Structura::App::Edition::kHasDesign ? QStringLiteral("Memorial de Cálculo") : QStringLiteral("Relatório de Análise Estrutural"));
    doc.subtitle = S(Structura::App::Edition::kHasDesign ? QStringLiteral("Análise e dimensionamento de estrutura de aço") : QStringLiteral("Análise estrutural de estrutura de aço"));
    doc.project = S(input.projectName);
    doc.author = S(input.author);
    doc.date = S(input.date);
    doc.cover = o.cover;
    doc.tableOfContents = o.toc;
    doc.headerText = S(QStringLiteral("%1 — %2").arg(Structura::App::Edition::kHasDesign ? QStringLiteral("Memorial de Cálculo") : QStringLiteral("Relatório de Análise"), input.projectName));
    int chapter = 0;
    auto chapterTitle = [&](const QString &title) { doc.heading(1, S(QStringLiteral("%1. %2").arg(++chapter).arg(title))); };

    {
        Table revisions;
        revisions.header = {"Rev.", "Data", "Descrição", "Responsável"};
        revisions.widths = {1, 2, 6, 3};
        revisions.firstColumnBold = true;
        for (const QString &line : o.revisions) {
            const QStringList parts = line.split(QLatin1Char(';'));
            if (parts.join(QString()).trimmed().isEmpty()) {
                continue;
            }
            std::vector<std::string> row;
            for (int k = 0; k < 4; ++k) {
                row.push_back(S(k < parts.size() ? parts[k].trimmed() : QString()));
            }
            revisions.rows.push_back(std::move(row));
        }
        if (!revisions.rows.empty()) {
            chapterTitle(QStringLiteral("Controle de revisões"));
            doc.table(std::move(revisions));
        }
    }
    if (o.criteria) {
        chapterTitle(QStringLiteral("Critérios e normas"));
        if (Structura::App::Edition::kHasDesign) {
            doc.paragraph("Este memorial apresenta os dados de entrada, as ações, os resultados da análise estrutural e a verificação dos perfis de aço. "
                          "As verificações seguem as normas indicadas abaixo; os cálculos são reproduzidos com as fórmulas e os valores utilizados.");
        } else {
            doc.paragraph("Este relatório apresenta os dados de entrada, as ações e os resultados da análise estrutural.");
        }
        Table t;
        t.header = {"Assunto", "Critério"};
        t.widths = {2, 5};
        t.firstColumnBold = true;
        if (Structura::App::Edition::kHasDesign) {
            t.rows.push_back({"Aço laminado e tubos circulares", "ABNT NBR 8800:2024 — projeto de estruturas de aço"});
            t.rows.push_back({"Aço formado a frio e tubos retangulares", "ABNT NBR 14762:2010 — perfis formados a frio (método da largura efetiva)"});
        }
        const std::vector<std::vector<std::string>> analysisRows = {
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
        t.rows.insert(t.rows.end(), analysisRows.begin(), analysisRows.end());
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
        if (!input.extras.memberActions.isEmpty()) {
            namespace Kind = Structura::App::ProjectData::MemberActionKind;
            Table al;
            al.caption = S(QStringLiteral("Cargas de barra com posição e deformações impostas (%1, m)").arg(input.forceUnit));
            al.header = {"Barra", "Caso", "Tipo", "Sistema", "a", "b", "Valor em a", "Valor em b"};
            al.widths = {1, 2, 2.5, 1.2, 1, 1, 3, 3};
            al.align = {'c', 'l', 'l', 'c', 'r', 'r', 'l', 'l'};
            auto triple = [&](const std::array<double, 3> &v) { return S(num(v[0]) + QStringLiteral("; ") + num(v[1]) + QStringLiteral("; ") + num(v[2])); };
            for (const auto &a : input.extras.memberActions) {
                QString type;
                switch (a.kind) {
                case Kind::kTrapezoid: type = QStringLiteral("Trapezoidal/parcial (q)"); break;
                case Kind::kPointForce: type = QStringLiteral("Força concentrada"); break;
                case Kind::kPointMoment: type = QStringLiteral("Momento concentrado"); break;
                case Kind::kTemperature: type = QStringLiteral("Temperatura (ΔT; grad. y; grad. z)"); break;
                default: type = QStringLiteral("Retração (ε)"); break;
                }
                const bool hasPosition = a.kind <= Kind::kPointMoment;
                al.rows.push_back({std::to_string(a.memberId), S(a.loadCase), S(type), S(a.system.isEmpty() ? QStringLiteral("LOCAL") : a.system),
                                   hasPosition ? S(num(a.a)) : std::string("-"),
                                   a.kind == Kind::kTrapezoid ? S(num(a.b)) : std::string("-"), triple(a.v1),
                                   a.kind == Kind::kTrapezoid ? triple(a.v2) : std::string("-")});
            }
            doc.table(al);
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

    appendDesignChapters(doc, input, o, chapterTitle);
    return doc;
}

} // namespace Structura::App::Services
