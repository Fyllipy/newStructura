#include <QtTest/QtTest>

#include "app/Edition.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

/// Fronteira entre as edicoes: o codigo exclusivo da Comercial nao pode entrar na Demonstrativa.
///  - a lista de fontes compartilhados nao contem nada de dimensionamento/ligacoes/detalhamento;
///  - a edicao nao e consultada por `#ifdef` espalhado: so pelas constantes de app/Edition.h;
///  - o executavel construido contem (Comercial) ou nao contem (Demonstrativa) textos que so existem no codigo comercial.
class TestEditionBoundaries : public QObject
{
    Q_OBJECT

private slots:
    void sharedSourceListsHaveNoCommercialCode()
    {
        QFile file(QStringLiteral(STRUCTURA_SOURCE_DIR "/CMakeLists.txt"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(file.readAll());

        auto listOf = [&](const QString &name) {
            const QRegularExpression re(QStringLiteral("set\\(%1\\n(.*?)\\n\\)").arg(name), QRegularExpression::DotMatchesEverythingOption);
            const auto match = re.match(text);
            QStringList items;
            if (match.hasMatch()) {
                for (const QString &line : match.captured(1).split(QLatin1Char('\n'))) {
                    const QString item = line.trimmed();
                    if (!item.isEmpty() && !item.startsWith(QLatin1Char('#'))) {
                        items << item;
                    }
                }
            }
            return items;
        };
        const QStringList design = listOf(QStringLiteral("STRUCTURA_DESIGN_SOURCES"));
        QVERIFY2(design.size() > 100, "a lista de fontes comerciais deveria existir");
        QVERIFY(design.contains(QStringLiteral("src/app/services/DesignService.cpp")));
        QVERIFY(design.contains(QStringLiteral("src/design/Nbr8800.cpp")));
        QVERIFY(design.contains(QStringLiteral("src/app/services/IfcExporter.cpp")));
        QVERIFY(design.contains(QStringLiteral("src/app/services/DstvExporter.cpp")));
        QVERIFY(design.contains(QStringLiteral("src/app/services/SheetBuilder.cpp")));

        for (const QString &name : {QStringLiteral("CORE_SOURCES"), QStringLiteral("VIZ_SOURCES"), QStringLiteral("APP_SOURCES"), QStringLiteral("UI_SOURCES"),
                                    QStringLiteral("SHELL_SOURCES")}) {
            const QStringList shared = listOf(name);
            QVERIFY2(!shared.isEmpty(), qPrintable(name));
            for (const QString &item : shared) {
                QVERIFY2(!item.startsWith(QStringLiteral("src/design/")) && !item.startsWith(QStringLiteral("src/drawing/")), qPrintable(name + ": " + item));
                QVERIFY2(!design.contains(item), qPrintable(name + " repete fonte comercial: " + item));
            }
        }
    }

    void editionIsNotQueriedByScatteredIfdefs()
    {
        // a edicao e escolhida no CMake (STRUCTURA_EDITION_*); o codigo do produto usa as constantes de app/Edition.h
        QStringList offenders;
        QDirIterator it(QStringLiteral(STRUCTURA_SOURCE_DIR "/src"), {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (path.contains(QStringLiteral("/src/tests/"))) {
                continue;   // os testes podem condicionar casos a edicao
            }
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text) && QString::fromUtf8(file.readAll()).contains(QStringLiteral("STRUCTURA_EDITION_"))) {
                offenders << path;
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1Char('\n'))));
    }

    void executableContainsCommercialCodeOnlyInTheCommercialEdition()
    {
        QFile exe(QStringLiteral(STRUCTURA_EXE_PATH));
        if (!exe.open(QIODevice::ReadOnly)) {
            QSKIP("Executavel ainda nao construido.");
        }
        const QByteArray data = exe.readAll();
        // textos de memoria de calculo, ligacoes e detalhamento: existem so nos modulos exclusivos da Comercial
        const QByteArrayList markers = {
            "Todos os elementos com b/t <= (b/t)lim: Aef = Ag (5.3.4.1)",
            "Area liquida (5.2.4): %s = %.0f mm2 (An/Ag = %.3f)",
            "Secao nao suportada pelo dimensionamento (use perfis de aco do catalogo).",
            "Ligacao de grande comprimento (%.0f mm > 950 mm): Fv,Sd e Fc,Sd multiplicados por 1,20",
            "Binario das mesas: Ft = M/(d - tf) + N/2 ; Fc = M/(d - tf) - N/2",
            "escoamento local da alma (5.7.3)",
            "Solda viga-apoio - mesas (penetracao total)",
            "Chapa ligada por pino - escoamento da secao bruta",
            "Chapa - escoamento ao cisalhamento",
            "Secao sem geometria valida: temperatura do aco igual a 20 C.",
            "PARAFUSOS (porca e 2 arruelas)",
            "CORTE (PELA LINHA DE PARAFUSOS)",
            "Tabela A.4: E60 = 415 MPa, E70 = 485 MPa.",
            "Pilar de quatro cantoneiras sem a cantoneira dos banzos.",
        };
        int found = 0;
        QStringList present;
        for (const QByteArray &marker : markers) {
            if (data.contains(marker)) {
                ++found;
                present << QString::fromLatin1(marker);
            }
        }
        if (Structura::App::Edition::kHasDesign) {
            QCOMPARE(found, static_cast<int>(markers.size()));   // confirma que os marcadores valem
        } else {
            QVERIFY2(found == 0, qPrintable(QStringLiteral("a Demonstrativa contem codigo comercial:\n") + present.join(QLatin1Char('\n'))));
        }
    }
};

QTEST_APPLESS_MAIN(TestEditionBoundaries)
#include "TestEditionBoundaries.moc"
