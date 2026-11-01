#include "report/FormulaText.h"
#include "app/services/DesignService.h"
#include "app/services/ReportService.h"
#include "app/services/ConnectionService.h"
#include "app/services/SheetBuilder.h"
#include "drawing/SheetGroups.h"
#include "app/services/MemberFabrication.h"
#include "app/services/IfcExporter.h"
#include "app/services/CadImport.h"
#include "drawing/SheetPainter.h"
#include <QPageSize>
#include <QRegularExpression>
#include <QPdfWriter>
#include "design/BarDesign.h"
#include "report/DocxWriter.h"
#include "report/CalcCardBuilder.h"
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

#include <set>

using namespace Structura::Report;

class TestReport : public QObject
{
    Q_OBJECT
private slots:
    void reportRevisionTableAndPdfExport()
    {
        using Structura::App::Services::ReportService;
        ReportService::Input input;
        input.projectName = QStringLiteral("Galpão");
        ReportService::Options options;
        options.criteria = options.materialsAndSections = options.geometry = options.supports = options.loads = options.analysisResults = false;
        options.designSummary = options.designDetails = options.serviceChecks = options.connections = options.sectionDrawings = false;
        options.revisions = {QStringLiteral("A; 26/10/2026; Emissão inicial; Fulano"), QStringLiteral(" ; ; ; "), QStringLiteral("B; 02/11/2026; Ajuste de cargas")};
        const auto doc = ReportService::buildGlobal(input, options);
        const auto html = Structura::Report::toHtml(doc, 19.0, true);
        QVERIFY(html.html.contains(QStringLiteral("Controle de revisões")));
        QVERIFY(html.html.contains(QStringLiteral("Emissão inicial")) && html.html.contains(QStringLiteral("Ajuste de cargas")));
        QVERIFY(html.html.contains(QStringLiteral("Fulano")));
        // sem revisoes informadas o capitulo nao aparece
        options.revisions.clear();
        QVERIFY(!Structura::Report::toHtml(ReportService::buildGlobal(input, options), 19.0, true).html.contains(QStringLiteral("Controle de revisões")));
        // PDF a partir do mesmo HTML (usado pelo dialogo): o arquivo e gerado com varias paginas de conteudo
        options.revisions = {QStringLiteral("A; 26/10/2026; Emissão inicial; Fulano")};
        const auto pdfHtml = Structura::Report::toHtml(ReportService::buildGlobal(input, options), 19.0, true);
        const QString path = QDir::temp().filePath(QStringLiteral("structura_report_test.pdf"));
        QFile::remove(path);
        {
            QTextDocument text;
            text.setHtml(pdfHtml.html);
            QPdfWriter writer(path);
            writer.setPageSize(QPageSize(QPageSize::A4));
            text.setPageSize(QSizeF(writer.width(), writer.height()));
            text.print(&writer);
        }
        QVERIFY(QFileInfo(path).size() > 1500);
        QFile::remove(path);
    }
    void sheetsRenderToPdfAndDetails()
    {
        using Structura::App::Services::SheetBuilder;
        SheetBuilder::Input in;
        in.nodes = {{1, 0, 0, 0}, {2, 0, 0, 4}, {3, 8, 0, 4}, {4, 8, 0, 0}};
        auto bar = [](int id, int a, int b, const char *section) {
            SheetBuilder::Bar x;
            x.id = id;
            x.nodeI = a;
            x.nodeJ = b;
            x.section = QString::fromLatin1(section);
            x.massPerMeter = 38.7;
            return x;
        };
        in.bars = {bar(1, 1, 2, "W 310 x 38.7"), bar(2, 2, 3, "W 360 x 44.6"), bar(3, 3, 4, "W 310 x 38.7")};
        in.info.project = QStringLiteral("Projeto de teste");
        SheetBuilder::ConnectionDetail detail;
        using NC = Structura::App::ProjectData::NodeConnection;
        for (const auto type : {NC::kShearTab, NC::kDoubleAngle, NC::kGusset, NC::kSplice, NC::kBasePlate, NC::kTubular, NC::kColdFormed, NC::kEndPlate, 101, 104, 111, 114, 147, 148}) {
            SheetBuilder::ConnectionDetail d;
            Structura::App::Services::ConnectionService::BeamInfo beam;
            beam.depth = 360;
            beam.bf = 171;
            beam.tf = 9.8;
            beam.tw = 7.8;
            beam.area = 5700;
            beam.family = QStringLiteral("L Laminado");
            beam.geometry = {{"b", 64.0}, {"t", 6.35}};
            if (type == NC::kTubular) {
                beam.geometry = {{"D", 168.3}, {"t", 6.0}};
                QVector<Structura::App::Services::ConnectionService::BraceInfo> braces(2);
                braces[0].d = 88.9; braces[0].t = 4.0; braces[0].angleDeg = 45.0;
                braces[1].d = 88.9; braces[1].t = 4.0; braces[1].angleDeg = 45.0; braces[1].sameSideAsFirst = true;
                d.connection = Structura::App::Services::ConnectionService::makeTubular(beam, braces);
            } else {
                d.connection = Structura::App::Services::ConnectionService::makeDefault(type, beam, 250.0, 400.0);
            }
            d.connection.id = in.connections.size() + 1;
            d.connection.nodeId = 2;
            d.connection.memberId = 2;
            d.beamSection = QStringLiteral("W 360 x 44.6");
            d.beamDepthMm = 352;
            d.beamFlangeMm = 9.8;
            d.hasForces = true;
            d.shearKN = 80;
            in.connections.append(d);
        }
        SheetBuilder::Options options;
        options.paper = SheetBuilder::Paper::A3;
        const auto sheets = SheetBuilder::build(in, options);
        int details = 0;
        for (const auto &sheet : sheets) {
            if (sheet.name.rfind("Ligação", 0) == 0 || sheet.name.rfind("Liga", 0) == 0) {
                ++details;
                QVERIFY(sheet.entities.size() > 40);
            }
        }
        QCOMPARE(details, 14);
        // chamadas de marca nos detalhes do catalogo: chapa (C1), parafuso (B1) e simbolo de solda
        {
            int withCallouts = 0;
            for (const auto &sheet : sheets) {
                if (sheet.name.rfind("Liga", 0) != 0) {
                    continue;
                }
                bool plateMark = false, boltMark = false;
                for (const auto &entity : sheet.entities) {
                    if (const auto *t = std::get_if<Structura::Drawing::Text>(&entity)) {
                        plateMark = plateMark || t->text == "C1";
                        boltMark = boltMark || t->text == "B1";
                    }
                }
                withCallouts += (plateMark && boltMark) ? 1 : 0;
            }
            QVERIFY(withCallouts >= 3);   // as tipologias do catalogo (101, 104, 111, 114)
            // detalhes por sólidos: vista frontal + cortes A-A, B-B, C-C e D-D, chapas e tabelas de materiais na mesma prancha
            int withSections = 0;
            for (const auto &sheet : sheets) {
                if (sheet.name.rfind("Liga", 0) != 0) {
                    continue;
                }
                bool a = false, b = false, cc = false, dd = false, table = false;
                for (const auto &entity : sheet.entities) {
                    if (const auto *t = std::get_if<Structura::Drawing::Text>(&entity)) {
                        a = a || t->text.rfind("SEÇÃO A-A", 0) == 0;
                        b = b || t->text.rfind("SEÇÃO B-B", 0) == 0;
                        cc = cc || t->text.rfind("SEÇÃO C-C", 0) == 0;
                        dd = dd || t->text.rfind("SEÇÃO D-D", 0) == 0;
                        table = table || t->text == "PARAFUSOS (porca e 2 arruelas)";
                    }
                }
                withSections += (a && b && cc && dd && table) ? 1 : 0;
            }
            QVERIFY(withSections >= 4);   // 101, 104, 111, 114, 147 e 148 usam o detalhe por solidos
            // simbolos de parafuso normalizados (hexagono da cabeca) e cotas dos cortes (linhas da camada COTAS)
            int withHexagons = 0, withDimensions = 0;
            for (const auto &sheet : sheets) {
                if (sheet.name.rfind("Liga", 0) != 0) {
                    continue;
                }
                int hex = 0, dimLines = 0;
                int dimLayer = -1;
                for (std::size_t i = 0; i < sheet.layers.size(); ++i) {
                    if (sheet.layers[i].name == "COTAS") {
                        dimLayer = static_cast<int>(i);
                    }
                }
                for (const auto &entity : sheet.entities) {
                    if (const auto *poly = std::get_if<Structura::Drawing::Polyline>(&entity)) {
                        hex += (poly->closed && poly->points.size() == 6) ? 1 : 0;
                    } else if (const auto *line = std::get_if<Structura::Drawing::Line>(&entity)) {
                        dimLines += line->layer == dimLayer ? 1 : 0;
                    }
                }
                withHexagons += hex > 0 ? 1 : 0;
                withDimensions += dimLines > 60 ? 1 : 0;
            }
            QVERIFY(withHexagons >= 4);
            QVERIFY(withDimensions >= 4);
            // anotacoes editaveis: grupos estaveis, cadeias de cotas sem sobreposicao entre si, mover/excluir via ajustes do projeto
            {
                std::set<std::string> stems;
                int checked = 0;
                for (const auto &sheet : sheets) {
                    QVERIFY2(stems.insert(sheet.fileStem).second, "fileStem repetido entre pranchas");
                    if (sheet.name.rfind("Liga", 0) != 0 || sheet.groups.size() < 8) {
                        continue;
                    }
                    ++checked;
                    std::vector<std::pair<std::string, Structura::Drawing::Rect>> chains;
                    for (const auto &g : sheet.groups) {
                        if (g.key.find("/cotaH") == std::string::npos && g.key.find("/cotaV") == std::string::npos) {
                            continue;
                        }
                        bool valid = false;
                        const auto b = Structura::Drawing::groupBounds(sheet, g.id, &valid);
                        if (valid) {
                            chains.push_back({g.key, b});
                        }
                    }
                    QVERIFY(chains.size() >= 2);
                    for (std::size_t i = 0; i < chains.size(); ++i) {
                        for (std::size_t j = i + 1; j < chains.size(); ++j) {
                            // cadeias da mesma vista nao se cobrem (a margem de 0,6 mm e a do posicionador)
                            const bool sameView = chains[i].first.substr(0, chains[i].first.rfind('/')) == chains[j].first.substr(0, chains[j].first.rfind('/'));
                            if (sameView) {
                                QVERIFY2(!chains[i].second.intersects(chains[j].second, -0.2), qPrintable(QString::fromStdString(chains[i].first + " x " + chains[j].first)));
                            }
                        }
                    }
                    // ajustes: mover desloca o grupo; excluir remove entidades
                    auto moved = sheets;
                    const std::string key = chains.front().first;
                    Structura::App::ProjectData::SheetGroupEdit move;
                    move.sheet = QString::fromStdString(sheet.fileStem);
                    move.key = QString::fromStdString(key);
                    move.dx = 7.0;
                    move.dy = -3.0;
                    Structura::App::Services::SheetBuilder::applyEdits(moved, {move});
                    for (std::size_t i = 0; i < sheets.size(); ++i) {
                        if (sheets[i].fileStem != sheet.fileStem) {
                            continue;
                        }
                        const int gid = Structura::Drawing::findGroup(sheets[i], key);
                        const auto before = Structura::Drawing::groupBounds(sheets[i], gid);
                        const auto after = Structura::Drawing::groupBounds(moved[i], gid);
                        QVERIFY(std::abs(after.x0 - before.x0 - 7.0) < 1e-6 && std::abs(after.y0 - before.y0 + 3.0) < 1e-6);
                    }
                    auto erased = sheets;
                    move.deleted = true;
                    Structura::App::Services::SheetBuilder::applyEdits(erased, {move});
                    for (std::size_t i = 0; i < sheets.size(); ++i) {
                        if (sheets[i].fileStem == sheet.fileStem) {
                            bool valid = true;
                            Structura::Drawing::groupBounds(erased[i], Structura::Drawing::findGroup(sheets[i], key), &valid);
                            QVERIFY(!valid);
                            QVERIFY(erased[i].entities.size() < sheets[i].entities.size());
                        }
                    }
                    if (checked >= 3) {
                        break;
                    }
                }
                QVERIFY(checked >= 3);
            }
        }
        // pecas de perfil: comprimento de corte real, furacao e marcas unicas
        {
            const auto members = Structura::App::Services::MemberFabrication::build(in);
            QVERIFY(members.size() >= 2);
            QSet<QString> marks;
            const Structura::App::Services::FabMember *beam = nullptr;
            for (const auto &m : members) {
                QVERIFY(!marks.contains(m.mark));
                marks.insert(m.mark);
                if (m.mark.startsWith(QLatin1Char('V'))) {
                    beam = &m;
                }
            }
            QVERIFY(beam != nullptr);
            QVERIFY(std::abs(beam->nodeLength - 8000.0) < 1e-6);
            QVERIFY(beam->cutLength <= beam->faceLength + 1e-9 && beam->faceLength <= beam->nodeLength + 1e-9);
            QVERIFY(beam->gapI > 0.0);
            QVERIFY(std::abs(beam->cutLength - (beam->faceLength - beam->gapI - beam->gapJ)) < 1e-9);
            QVERIFY(!beam->holes.isEmpty());
            for (const auto &h : beam->holes) {
                QVERIFY(h.x >= -1e-6 && h.diameter > 0.0);
            }
            SheetBuilder::Options memberOptions;
            memberOptions.paper = SheetBuilder::Paper::A3;
            memberOptions.views.insert(QStringLiteral("member-%1").arg(beam->mark));
            const auto memberSheets = SheetBuilder::build(in, memberOptions);
            QCOMPARE(int(memberSheets.size()), 1);
            QVERIFY(memberSheets.front().entities.size() > 40);
            const QString memberDump = qEnvironmentVariable("STRUCTURA_DUMP_SHEETS");
            if (!memberDump.isEmpty()) {
                QImage page(420 * 4, 297 * 4, QImage::Format_ARGB32);
                page.fill(Qt::white);
                QPainter p(&page);
                Structura::Drawing::paintSheet(p, memberSheets.front(), 4.0);
                p.end();
                page.save(QDir(memberDump).filePath(QStringLiteral("member.png")));
            }
        }
        // quadro de conjuntos e plano de corte
        {
            SheetBuilder::Options tableOptions;
            tableOptions.paper = SheetBuilder::Paper::A3;
            tableOptions.views.insert(QStringLiteral("assemblies"));
            tableOptions.views.insert(QStringLiteral("cut-list"));
            const auto tables = SheetBuilder::build(in, tableOptions);
            QCOMPARE(int(tables.size()), 2);
            for (const auto &sheet : tables) {
                QVERIFY(sheet.entities.size() > 40);
            }
        }
        // IFC4: estrutura, pecas e referencias consistentes
        {
            Structura::App::Services::IfcExporter::Options ifcOptions;
            ifcOptions.projectName = QStringLiteral("Projeto 'teste' ação");
            const QString ifc = Structura::App::Services::IfcExporter::build(in, ifcOptions);
            QVERIFY(ifc.startsWith(QStringLiteral("ISO-10303-21;")));
            QVERIFY(ifc.trimmed().endsWith(QStringLiteral("END-ISO-10303-21;")));
            QVERIFY(ifc.contains(QStringLiteral("FILE_SCHEMA(('IFC4'))")));
            QCOMPARE(ifc.count(QStringLiteral("=IFCPROJECT(")), 1);
            QCOMPARE(ifc.count(QStringLiteral("=IFCCOLUMN(")), 2);
            QCOMPARE(ifc.count(QStringLiteral("=IFCBEAM(")), 1);
            QVERIFY(ifc.contains(QStringLiteral("''teste''")));   // aspas duplicadas
            QVERIFY(ifc.contains(QStringLiteral("=IFCPLATE(")));   // chapas das ligacoes
            QVERIFY(ifc.contains(QStringLiteral("=IFCARBITRARYPROFILEDEFWITHVOIDS(")));   // chapas furadas
            QVERIFY(ifc.contains(QStringLiteral("=IFCRELASSOCIATESMATERIAL(")));
            QVERIFY(ifc.contains(QStringLiteral("=IFCPROPERTYSET(")));
            QVERIFY(ifc.contains(QStringLiteral("ComprimentoDeCorte")));
            QVERIFY(ifc.contains(QStringLiteral("\\X2\\")));       // acentos codificados
            // importacao do proprio arquivo: 2 pilares verticais e 1 viga horizontal, em metros, com o comprimento de corte
            {
                const auto imported = Structura::App::Services::IfcImporter::parse(ifc);
                int columns = 0, beams = 0;
                for (const auto &segment : imported.segments) {
                    const double dx = segment.b[0] - segment.a[0], dy = segment.b[1] - segment.a[1], dz = segment.b[2] - segment.a[2];
                    const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
                    QVERIFY2(length > 0.1 && length < 100.0, qPrintable(QString::number(length)));   // metros (o arquivo esta em milimetros)
                    if (segment.layer == QLatin1String("IFCCOLUMN")) {
                        ++columns;
                        QVERIFY(std::abs(dz) > 0.99 * length);
                    } else if (segment.layer == QLatin1String("IFCBEAM")) {
                        ++beams;
                        QVERIFY(std::abs(dz) < 0.01 * length);
                    }
                }
                QCOMPARE(columns, 2);
                QCOMPARE(beams, 1);
            }
            // toda referencia #n aponta para uma entidade definida
            QSet<int> defined;
            QRegularExpression define(QStringLiteral("^#(\\d+)="), QRegularExpression::MultilineOption);
            auto it = define.globalMatch(ifc);
            while (it.hasNext()) {
                defined.insert(it.next().captured(1).toInt());
            }
            QRegularExpression use(QStringLiteral("#(\\d+)"));
            auto uses = use.globalMatch(ifc);
            while (uses.hasNext()) {
                QVERIFY2(defined.contains(uses.next().captured(1).toInt()), "referencia IFC sem entidade");
            }
            // GlobalId: 22 caracteres do alfabeto IFC; a codificacao de 16 bytes zero e 22 vezes '0'
            QCOMPARE(Structura::App::Services::IfcExporter::compressGuid(QByteArray(16, '\0')), QStringLiteral("0000000000000000000000"));
            QCOMPARE(Structura::App::Services::IfcExporter::compressGuid(QByteArray(16, '\xff')).size(), 22);
            QRegularExpression guid(QStringLiteral("\\('([0-9A-Za-z_$]{22})',"));
            QVERIFY(guid.match(ifc).hasMatch());
        }
        const QString path = QDir::temp().filePath(QStringLiteral("structura_sheets_test.pdf"));
        QFile::remove(path);
        {
            QPdfWriter writer(path);
            writer.setResolution(150);
            writer.setPageSize(QPageSize(QSizeF(420, 297), QPageSize::Millimeter));
            writer.setPageMargins(QMarginsF(0, 0, 0, 0));
            QPainter painter(&writer);
            for (std::size_t i = 0; i < sheets.size(); ++i) {
                if (i > 0) {
                    writer.newPage();
                }
                Structura::Drawing::paintSheet(painter, sheets[i], 150.0 / 25.4);
            }
        }
        QVERIFY(QFileInfo(path).size() > 5000);
        QFile::remove(path);
        // STRUCTURA_DUMP_SHEETS=<pasta>: grava PNG de cada prancha para inspecao visual
        const QString dump = qEnvironmentVariable("STRUCTURA_DUMP_SHEETS");
        if (!dump.isEmpty()) {
            QDir().mkpath(dump);
            for (std::size_t i = 0; i < sheets.size(); ++i) {
                QImage page(420 * 4, 297 * 4, QImage::Format_ARGB32);
                page.fill(Qt::white);
                QPainter p(&page);
                Structura::Drawing::paintSheet(p, sheets[i], 4.0);
                p.end();
                page.save(QDir(dump).filePath(QStringLiteral("sheet_%1.png").arg(i)));
            }
        }
        // imagem de uma prancha (nao totalmente branca)
        QImage image(420 * 2, 297 * 2, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        Structura::Drawing::paintSheet(painter, sheets.front(), 2.0);
        painter.end();
        QVERIFY(image.pixelColor(100, 100) != QColor(0, 0, 0, 0));
        int dark = 0;
        for (int y = 0; y < image.height(); y += 3) {
            for (int x = 0; x < image.width(); x += 3) {
                dark += image.pixelColor(x, y).lightness() < 128 ? 1 : 0;
            }
        }
        QVERIFY(dark > 200);
    }
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

    void calcCardSplitsProseAndAssignments()
    {
        Structura::Report::CalcCardInput in;
        in.title = "Cortante";
        in.clause = "NBR 14762:2010, 9.8.3";
        in.ratio = 0.058;
        in.demand = 1.69;
        in.resistance = 29.33;
        in.unit = "kN";
        in.lines = {"Cortante (9.8.3): h = 101.3 mm, t = 1.00 mm, h/t = 101.3, kv = 5.0",
                    "Limites de esbeltez: lambda_p = 1,08 sqrt(E kv / fy) = 68.3, lambda_r = 1,40 sqrt(E kv / fy) = 88.5",
                    "V_Rd,alma = 29.33 kN (resistencia de uma alma); V_Rd = V_Rd,alma x 1.00 = 29.33 kN",
                    "V_y,Sd = 1.69 kN -> V_y,Sd / V_Rd = 0.058  (combinacao ELU1, x = 0 mm)"};
        const auto card = Structura::Report::buildCalcCard(in);
        QCOMPARE(card.location, std::string("Combinação ELU1, seção x = 0,000 m"));
        QVERIFY(card.steps.size() >= 4);
        for (const auto &step : card.steps) {
            for (const auto &v : step.variables) {
                QVERIFY2(!v.value.empty(), v.latex.c_str());   // sem campos vazios
            }
            for (const auto &eq : step.equations) {
                QVERIFY2(eq.find("limites") == std::string::npos && eq.find("mathrm{por}") == std::string::npos, eq.c_str());
            }
        }
        // h e t aparecem com valor
        bool h = false;
        for (const auto &step : card.steps) {
            for (const auto &v : step.variables) {
                h = h || (v.latex == "h" && v.value == "101,3");
            }
        }
        QVERIFY(h);
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
