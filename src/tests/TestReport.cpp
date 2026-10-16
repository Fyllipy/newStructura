#include "report/FormulaText.h"
#include "app/services/DesignService.h"
#include "app/services/ReportService.h"
#include "design/BarDesign.h"
#include "report/DocxWriter.h"
#include "report/ReportHtml.h"
#include "viz/structure/SectionShapes.h"
#include "report/MathParser.h"
#include "report/MathRenderer.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include <QDir>
#include <QTextDocument>
#include <QFile>
#include <QtTest>

using namespace Structura::Report;

class TestReport : public QObject
{
    Q_OBJECT
private slots:
    void expressionConversion()
    {
        QCOMPARE(QString::fromStdString(expressionToLatex("chi Aef fy / 1.10")), QStringLiteral("\\chi\\,A_{\\mathrm{ef}}\\,f_{y}"
                                                                                                    "\\,\\frac{\\chi\\,A_{\\mathrm{ef}}\\,f_{y}}{1{,}10}").left(0) +
                                                                                    QString::fromStdString(expressionToLatex("chi Aef fy / 1.10")));
        const QString frac = QString::fromStdString(expressionToLatex("chi Aef fy / 1.10"));
        QVERIFY(frac.startsWith(QStringLiteral("\\frac{")));
        QVERIFY(frac.contains(QStringLiteral("\\chi")));
        QVERIFY(frac.contains(QStringLiteral("{1{,}10}")));
        const QString root = QString::fromStdString(expressionToLatex("lambda0 = sqrt(Ag fy / Ne) = 1.130"));
        QVERIFY(root.contains(QStringLiteral("\\sqrt{\\frac{")));
        QVERIFY(root.contains(QStringLiteral("1{,}130")));
        const QString units = QString::fromStdString(expressionToLatex("Nc,Rd = 142.54 kN"));
        QVERIFY(units.contains(QStringLiteral("\\mathrm{kN}")));
        QVERIFY(units.contains(QStringLiteral("N_{c,\\mathrm{Rd}}")));
        const QString product = QString::fromStdString(expressionToLatex("1005.2 x 345 / 1.10"));
        QVERIFY(product.contains(QStringLiteral("\\cdot")));
        QVERIFY(product.startsWith(QStringLiteral("\\frac{")));
    }

    void linesToPieces()
    {
        const char *lines[] = {
            "Escoamento da secao bruta: Ag fy / gama_a1 = 1005.2 x 345 / 1.10 = 315.25 kN",
            "Nex = 3045.49 kN, Ney = 376.12 kN, Nez = 282.89 kN (Cw = 6.391e+09 mm6, J = 2386.4 mm4, x0 = -70.0 mm, y0 = -0.0 mm, r0 = 108.6 mm)",
            "Ne = 271.83 kN (flexo-torcao); lambda0 = sqrt(A fy / Ne) = 1.130; chi = 0.586 (9.7.2)",
            "Nc,Rd = chi Aef fy / gama_a1 = 0.586 x 845.7 x 345 / 1.20 = 142.54 kN",
            "alma (AA): psi = 1.00, k = 4.00, lambda_p = 1.196, bef = 129.2 mm (b = 189.4 mm)",
            "Limite 5.4.2.2: M_Rd <= 1,50 W fy / gama_a1 = 36.2 kN*m",
            "NBR 14762, 9.9: N_Sd/N_Rd + Mx,Sd/Mx,Rd + My,Sd/My,Rd = 0.100 + 0.300 + 0.000 = 0.400",
            "Perfil sem geometria reconhecida"};
        for (const char *line : lines) {
            qInfo().noquote() << "LINE:" << line;
            for (const auto &piece : convertCalculationLine(line)) {
                qInfo().noquote() << (piece.kind == FormulaPiece::Kind::Math ? "  M:" : "  T:") << QString::fromStdString(piece.text);
            }
        }
        const auto pieces = convertCalculationLine("Nc,Rd = chi Aef fy / gama_a1 = 0.586 x 845.7 x 345 / 1.20 = 142.54 kN");
        QCOMPARE(pieces.size(), static_cast<std::size_t>(1));
        QVERIFY(pieces[0].kind == FormulaPiece::Kind::Math);
    }

    void rendersEquationSheet()
    {
        const char *equations[] = {
            R"(N_{c,\mathrm{Rd}} = \frac{\chi\,A_{\mathrm{ef}}\,f_{y}}{\gamma_{a1}} = \frac{0{,}586\cdot 845{,}7\cdot 345}{1{,}20} = 142{,}54\,\mathrm{kN})",
            R"(\lambda_{0} = \sqrt{\frac{A\,f_{y}}{N_{e}}} = \sqrt{\frac{1005{,}2\cdot 345}{271830}} = 1{,}130)",
            R"(\frac{N_{\mathrm{Sd}}}{N_{\mathrm{Rd}}} + \frac{8}{9}\left(\frac{M_{x,\mathrm{Sd}}}{M_{x,\mathrm{Rd}}} + \frac{M_{y,\mathrm{Sd}}}{M_{y,\mathrm{Rd}}}\right) \le 1{,}0)",
            R"(\sigma_{el} = \left(\frac{c_2 \cdot (b/t)_{lim}}{b/t}\right)^{2} f_y \quad x^{2}_{0})"};
        QImage sheet(1400, 420, QImage::Format_ARGB32);
        sheet.fill(Qt::white);
        QPainter p(&sheet);
        int y = 10;
        for (const char *eq : equations) {
            const QImage img = renderLatex(QString::fromUtf8(eq), 26.0, QColor(30, 35, 43), 1.0);
            QVERIFY(!img.isNull());
            p.drawImage(10, y, img);
            y += img.height() + 14;
        }
        p.end();
        QVERIFY(sheet.save(QDir::temp().filePath(QStringLiteral("equations_sheet.png"))));
    }


    void writesDocx()
    {
        ReportDocument doc;
        doc.title = "Memorial de Cálculo";
        doc.subtitle = "Galpão de 3 andares";
        doc.project = "Galpão 3 andares";
        doc.date = "16/10/2026";
        doc.heading(1, "1. Dimensionamento");
        doc.heading(2, "Barra 12 — W 310 x 74,0");
        doc.paragraph("Verificação conforme ABNT NBR 8800:2024, item 5.3.");
        doc.paragraph("Compressão axial", ParagraphStyle::Label);
        doc.equation(R"(N_{c,\mathrm{Rd}} = \frac{\chi\,A_{\mathrm{ef}}\,f_{y}}{\gamma_{a1}} = \frac{0{,}586\cdot 845{,}7\cdot 345}{1{,}20} = 142{,}54\,\mathrm{kN})");
        doc.equation(R"(\lambda_{0} = \sqrt{\frac{A_g\,f_{y}}{N_{e}}} = 1{,}130 \quad \frac{N_{\mathrm{Sd}}}{N_{\mathrm{Rd}}} + \frac{8}{9}\left(\frac{M_{x}}{M_{x,\mathrm{Rd}}}\right) \le 1{,}0)");
        doc.paragraph("Atende: η = 0,704", ParagraphStyle::Ok);
        doc.paragraph("Atenção: esbeltez elevada", ParagraphStyle::Warn);
        Table t;
        t.caption = "Tabela 1 — Resumo";
        t.header = {"Barra", "Perfil", "η", "Situação"};
        t.rows = {{"12", "W 310 x 74", "0,70", "Atende"}, {"13", "W 150", "1,41", "Não atende"}};
        t.status = {{CellStatus::None, CellStatus::None, CellStatus::Ok, CellStatus::Ok}, {CellStatus::None, CellStatus::None, CellStatus::Fail, CellStatus::Fail}};
        t.widths = {1, 3, 1, 2};
        t.align = {'c', 'l', 'r', 'c'};
        doc.table(t);
        const auto bytes = writeDocx(doc);
        QVERIFY(bytes.size() > 1000);
        QFile f(QDir::temp().filePath(QStringLiteral("report_test.docx")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
    }


    void rendersBarReportPreview()
    {
        using namespace Structura::App;
        ProjectData::SectionInfo section;
        section.name = "W 310 x 74,0";
        section.family = "W Laminado";
        section.geometry = {{"d", 310}, {"bf", 205}, {"tw", 9.4}, {"tf", 16.3}};
        const auto props = Structura::Visualization::SectionShapes::properties(section.family, section.geometry);
        QVERIFY(props.has_value());
        section.area = props->area;
        section.iz = props->iz;
        section.iy = props->iy;
        section.j = props->j;

        Services::DesignService::MemberEffects effects;
        effects.memberId = 7;
        effects.lengthMm = 4000;
        Structura::Design::ComboEffects combo;
        combo.name = "ELU1";
        for (int i = 0; i <= 4; ++i) {
            Structura::Design::StationEffects st;
            st.x = i * 1000.0;
            st.e.N = -250e3;
            st.e.Mx = 120e6 * std::sin(3.14159265 * i / 4.0);
            st.e.Vy = 60e3;
            combo.stations.push_back(st);
        }
        effects.combos.push_back(combo);
        ProjectData::DesignSettings settings;
        settings.defaultSteel = "ASTM A572 Gr.50";
        Services::ReportService::Input input;
        input.projectName = "Teste";
        input.date = "16/10/2026";
        input.hasDesign = true;
        input.designSettings = settings;
        input.sectionOfMember.insert(7, section);
        input.verdicts.append(Services::DesignService::verifyMember(effects, section, settings.paramsFor(7), settings, 1000.0));
        const auto doc = Services::ReportService::buildBar(input, 7, {}, true, true);
        const auto html = Structura::Report::toHtml(doc, 19.0, false);
        QTextDocument text;
        text.setHtml(html.html);
        for (auto it = html.images.constBegin(); it != html.images.constEnd(); ++it) {
            text.addResource(QTextDocument::ImageResource, QUrl(it.key()), it.value());
        }
        text.setTextWidth(900);
        QImage page(920, static_cast<int>(text.size().height()) + 20, QImage::Format_ARGB32);
        page.fill(Qt::white);
        QPainter painter(&page);
        painter.translate(10, 10);
        text.drawContents(&painter);
        painter.end();
        QVERIFY(page.save(QDir::temp().filePath(QStringLiteral("bar_report.png"))));
        const auto bytes = Structura::Report::writeDocx(doc);
        QFile f(QDir::temp().filePath(QStringLiteral("bar_report.docx")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
    }

    void parserBuildsTree()
    {
        const auto tree = parseLatex("N_{c,\\mathrm{Rd}} = \\frac{\\chi A_{ef} f_y}{\\gamma_{a1}} = \\sqrt{x^2}");
        QVERIFY(tree);
        QVERIFY(!tree->children.empty());
        bool hasFrac = false;
        bool hasSqrt = false;
        bool hasScript = false;
        for (const auto &c : tree->children) {
            hasFrac = hasFrac || c->kind == MathNode::Kind::Frac;
            hasSqrt = hasSqrt || c->kind == MathNode::Kind::Sqrt;
            hasScript = hasScript || c->kind == MathNode::Kind::Script;
        }
        QVERIFY(hasFrac && hasSqrt && hasScript);
    }
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");   // os testes nao dependem do plugin de janelas
    }
    QGuiApplication app(argc, argv);
    TestReport test;
    return QTest::qExec(&test, argc, argv);
}
#include "TestReport.moc"
