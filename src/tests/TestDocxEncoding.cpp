// Codificacao e estrutura do DOCX: letras gregas, subscritos/sobrescritos e texto em portugues nunca podem gerar UTF-8 invalido nem XML malformado.
// NAO executa o Word: valida o pacote OOXML por analise estrutural propria (zip, tipos de conteudo, relacoes, XML bem formado, UTF-8 valido).
#include "report/DocxWriter.h"
#include "report/FormulaText.h"
#include "report/ReportModel.h"

#include <QSet>
#include <QStringDecoder>
#include <QXmlStreamReader>
#include <QtTest>

#include <map>

using namespace Structura::Report;

namespace {

// Le as partes de um zip SEM compressao (metodo 0), que e o que o gerador produz; falha se encontrar outro metodo.
std::map<QString, QByteArray> readStoredZip(const std::vector<unsigned char> &bytes, QString &error)
{
    std::map<QString, QByteArray> parts;
    const QByteArray data(reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size()));
    auto u16 = [&](qsizetype o) { return static_cast<quint16>(static_cast<quint8>(data[o]) | (static_cast<quint8>(data[o + 1]) << 8)); };
    auto u32 = [&](qsizetype o) { return static_cast<quint32>(u16(o)) | (static_cast<quint32>(u16(o + 2)) << 16); };
    qsizetype eocd = data.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0) {
        error = QStringLiteral("sem fim de diretorio central");
        return parts;
    }
    const int count = u16(eocd + 10);
    qsizetype p = static_cast<qsizetype>(u32(eocd + 16));
    for (int i = 0; i < count; ++i) {
        if (u32(p) != 0x02014b50u) {
            error = QStringLiteral("cabecalho central invalido");
            return parts;
        }
        const quint16 method = u16(p + 10);
        const quint32 size = u32(p + 24);
        const quint16 nameLen = u16(p + 28), extraLen = u16(p + 30), commentLen = u16(p + 32);
        const quint32 local = u32(p + 42);
        const QString name = QString::fromUtf8(data.mid(p + 46, nameLen));
        if (method != 0) {
            error = QStringLiteral("metodo de compressao inesperado em ") + name;
            return parts;
        }
        const qsizetype dataStart = static_cast<qsizetype>(local) + 30 + u16(local + 26) + u16(local + 28);
        parts[name] = data.mid(dataStart, size);
        p += 46 + nameLen + extraLen + commentLen;
    }
    return parts;
}

bool validUtf8(const QByteArray &b)
{
    QStringDecoder dec(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString s = dec.decode(b);
    return !dec.hasError() && !s.contains(QChar::ReplacementCharacter);
}

QString wellFormed(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
    }
    return r.hasError() ? r.errorString() + QStringLiteral(" (linha %1)").arg(r.lineNumber()) : QString();
}

QString textOfRuns(const std::string &omml)
{
    QString out;
    QXmlStreamReader r(QByteArray("<root xmlns:m='m' xmlns:w='w'>") + QByteArray::fromStdString(omml) + "</root>");
    bool inText = false;
    while (!r.atEnd()) {
        const auto t = r.readNext();
        if (t == QXmlStreamReader::StartElement && r.name() == QLatin1String("t")) {
            inText = true;
        } else if (t == QXmlStreamReader::EndElement && r.name() == QLatin1String("t")) {
            inText = false;
            out += QLatin1Char('|');
        } else if (t == QXmlStreamReader::Characters && inText) {
            out += r.text();
        }
    }
    return out;
}

const char *kExpressions[] = {
    "R_d = 6,25 tf² fy/γa1",
    "γa1 = 1,10; γa2 = 1,35; γ_a1",
    "λ_p = 1,08 sqrt(E kv / fy)",
    "σ_el = π² E / λ²",
    "ηmax = N_Sd / N_Rd",
    "φ·Mn ≥ M_Sd",
    "Ωx = α_x^2 + β_y",
    "ε_y = fy / E",
    "ação_máx = Ft,Rd / γa2",
    "μ ≤ 0,35 e Δ_máx ≤ L/360",
    "Ψ0 ≥ θ_x",
};
const char *kLatex[] = {
    "\\gamma_{a1}", "γ_{a1}", "γ_a", "α^{2}_{x}", "\\lambda_{p}^{2}", "ε_{y}^{2}", "\\frac{f_y}{γ_{a1}}", "\\sqrt{σ_{el}}", "η_{máx}", "ç_{ã}", "Δ²", "ω_{0}^{ã}",
};

} // namespace

class TestDocxEncoding : public QObject
{
    Q_OBJECT
private slots:
    void greekAndAccentedIdentifiersNeverSplitMultibyteCharacters()
    {
        for (const char *expr : kExpressions) {
            const std::string latex = expressionToLatex(expr);
            QVERIFY2(validUtf8(QByteArray::fromStdString(latex)), qPrintable(QStringLiteral("LaTeX com UTF-8 invalido para: ") + QString::fromUtf8(expr) + QStringLiteral(" | entrada=") + QString::fromLatin1(QByteArray(expr).toHex()) + QStringLiteral(" | latex=") + QString::fromLatin1(QByteArray::fromStdString(latex).toHex())));
            const std::string omml = latexToOmml(latex);
            QVERIFY2(validUtf8(QByteArray::fromStdString(omml)), qPrintable(QStringLiteral("OMML com UTF-8 invalido para: ") + QString::fromUtf8(expr)));
            QVERIFY2(wellFormed(QByteArray("<root xmlns:m='m' xmlns:w='w'>") + QByteArray::fromStdString(omml) + "</root>").isEmpty(), expr);
        }
        for (const char *latex : kLatex) {
            const std::string omml = latexToOmml(latex);
            QVERIFY2(validUtf8(QByteArray::fromStdString(omml)), latex);
            QVERIFY2(wellFormed(QByteArray("<root xmlns:m='m' xmlns:w='w'>") + QByteArray::fromStdString(omml) + "</root>").isEmpty(), latex);
        }
    }

    void greekLettersAndScriptsArePreserved()
    {
        // gamma com subscrito: a letra grega fica inteira e o subscrito fica em <m:sSub>
        const std::string latex = expressionToLatex("γa1");
        const std::string omml = latexToOmml(latex);
        QVERIFY2(omml.find("<m:sSub>") != std::string::npos, latex.c_str());
        const QString text = textOfRuns(omml);
        QVERIFY2(text.contains(QStringLiteral("γ")), qPrintable(text));
        QVERIFY2(QString(text).remove(QLatin1Char('|')).contains(QStringLiteral("a1")), qPrintable(text));
        // identificador comecando por letra acentuada
        QVERIFY(validUtf8(QByteArray::fromStdString(latexToOmml(expressionToLatex("ação_máx")))));
        // sobrescrito e subscrito juntos
        const std::string both = latexToOmml("α^{2}_{x}");
        QVERIFY(both.find("<m:sSubSup>") != std::string::npos);
        QVERIFY(textOfRuns(both).startsWith(QStringLiteral("α")));
        // regressao do defeito original: um identificador grego cru seguido de subscrito nao pode virar byte solto
        const QString t2 = textOfRuns(latexToOmml(expressionToLatex("R_d = 6,25 tf² fy/γa1")));
        QVERIFY2(!t2.contains(QChar::ReplacementCharacter), qPrintable(t2));
    }

    void documentPackageIsStructurallyValidOoxml()
    {
        ReportDocument doc;
        doc.title = "Memorial — ligação à mesa do pilar";
        doc.subtitle = "Verificação de ação e reação, γ e φ";
        doc.project = "Projeto Ç-ã-é";
        doc.author = "Engenheiro Responsável";
        doc.headerText = "Cabeçalho: λ, σ, η";
        doc.heading(1, "Ligações — Seção 1");
        doc.paragraph("Atenção: γa1 = 1,10 e resistência à tração (ação, razão ≤ 1).", ParagraphStyle::Warn);
        for (const char *expr : kExpressions) {
            doc.equation(expressionToLatex(expr));
        }
        for (const char *latex : kLatex) {
            doc.equation(latex);
        }
        Table t;
        t.caption = "Tabela 1 — Resumo de ação";
        t.header = {"Verificação", "Cláusula", "η"};
        t.rows = {{"Efeito de alavanca — mesa do pilar", "NBR 8800:2024, 6.3.5.4", "0,41"}, {"T-stub EN (γM2 = 1,25)", "EN 1993-1-8", "—"}};
        t.widths = {3, 2, 1};
        t.align = {'l', 'l', 'r'};
        doc.table(t);
        // caracteres de controle ilegais em XML 1.0 devem ser descartados, nunca emitidos
        doc.paragraph(std::string("controle:") + '\x01' + '\x0b' + "fim");

        QString error;
        const auto parts = readStoredZip(writeDocx(doc), error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        for (const char *required : {"[Content_Types].xml", "_rels/.rels", "word/document.xml", "word/_rels/document.xml.rels", "word/styles.xml", "word/settings.xml",
                                     "word/header1.xml", "word/footer1.xml", "docProps/core.xml", "docProps/app.xml"}) {
            QVERIFY2(parts.count(QString::fromLatin1(required)) == 1, required);
        }
        for (const auto &kv : parts) {
            if (!kv.first.endsWith(QLatin1String(".xml")) && !kv.first.endsWith(QLatin1String(".rels"))) {
                continue;
            }
            QVERIFY2(validUtf8(kv.second), qPrintable(QStringLiteral("UTF-8 invalido em ") + kv.first));
            const QString problem = wellFormed(kv.second);
            QVERIFY2(problem.isEmpty(), qPrintable(kv.first + QStringLiteral(": ") + problem));
        }
        // todo destino de relacao existe no pacote
        const QByteArray rels = parts.at(QStringLiteral("word/_rels/document.xml.rels"));
        QXmlStreamReader r(rels);
        int checked = 0;
        while (!r.atEnd()) {
            if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("Relationship")) {
                const QString target = QStringLiteral("word/") + r.attributes().value(QLatin1String("Target")).toString();
                QVERIFY2(parts.count(target) == 1, qPrintable(QStringLiteral("destino inexistente: ") + target));
                ++checked;
            }
        }
        QVERIFY(checked >= 4);
        // o texto em portugues atravessa o pacote sem alteracao
        const QString document = QString::fromUtf8(parts.at(QStringLiteral("word/document.xml")));
        QVERIFY(document.contains(QStringLiteral("Atenção: γa1 = 1,10 e resistência à tração (ação, razão ≤ 1).")));
        QVERIFY(document.contains(QStringLiteral("Efeito de alavanca — mesa do pilar")));
        QVERIFY(document.contains(QStringLiteral("controle:fim")));
        const QString core = QString::fromUtf8(parts.at(QStringLiteral("docProps/core.xml")));
        QVERIFY(core.contains(QStringLiteral("Memorial — ligação à mesa do pilar")));
    }
};

QTEST_MAIN(TestDocxEncoding)
#include "TestDocxEncoding.moc"
