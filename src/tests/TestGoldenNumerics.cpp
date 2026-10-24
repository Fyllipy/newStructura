#include <QtTest/QtTest>

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "app/services/ConnectionSolids.h"
#include "design/BarDesign.h"
#include "design/CompositeFloor.h"
#include "design/DesignSection.h"
#include "viz/structure/SectionShapes.h"

#include <QFile>
#include <QTextStream>

#include <cmath>

using namespace Structura;
using namespace Structura::App::Services;
using Structura::Visualization::SectionShapes;

/// Rede de seguranca das refatoracoes: despeja, em texto, todos os numeros das verificacoes de ligacoes, do dimensionamento de barras e das vigas mistas.
/// Com STRUCTURA_GOLDEN_OUT=<arquivo> grava o despejo; comparando o arquivo de antes com o de depois de uma refatoracao, qualquer mudanca de calculo aparece.
class TestGoldenNumerics : public QObject
{
    Q_OBJECT

private:
    static QString num(double v) { return QString::number(v, 'g', 12); }

    static ConnectionService::BeamInfo beam()
    {
        ConnectionService::BeamInfo b;
        b.name = "W 310 x 38,7";
        b.family = "W Laminado";
        b.depth = 310.0;
        b.bf = 165.0;
        b.tf = 9.7;
        b.tw = 5.8;
        b.geometry = {{"d", 310.0}, {"bf", 165.0}, {"tw", 5.8}, {"tf", 9.7}, {"r", 10.0}};
        b.area = 4900.0;
        b.fy = 345.0;
        b.fu = 450.0;
        return b;
    }

    static ConnectionService::Forces forces()
    {
        ConnectionService::Forces f;
        f.shear = 80e3;
        f.axial = 60e3;
        f.tension = 60e3;
        f.compression = 40e3;
        f.moment = 120e6;
        ConnectionService::ForceCase a;
        a.name = "ELU1";
        a.shear = 80e3;
        a.axial = 60e3;
        a.moment = 120e6;
        a.braceAxial = {80e3, 60e3, 30e3};
        a.braceSigned = {80e3, -60e3, 30e3};
        a.panelShear = 150e3;
        a.columnAxial = 200e3;
        ConnectionService::ForceCase b;
        b.name = "ELU2";
        b.shear = 30e3;
        b.axial = -150e3;
        b.moment = 60e6;
        b.braceAxial = {20e3, 40e3, 10e3};
        b.braceSigned = {-20e3, 40e3, -10e3};
        f.cases = {a, b};
        return f;
    }

    static void dumpConnection(QTextStream &out, int type, const QString &label)
    {
        auto c = ConnectionService::makeDefault(type, beam(), 250.0, 400.0);
        out << "### " << label << " tipo " << type << " campos " << ConnectionService::fields(type).size() << "\n";
        const auto e = ConnectionService::evaluate(c, forces());
        out << "descricao " << e.description << " governa " << e.governing << " caso " << e.governingCase << " max " << num(e.maxRatio) << "\n";
        for (const auto &chk : e.checks) {
            out << "chk " << QString::fromStdString(chk.name) << " | " << QString::fromStdString(chk.clause) << " | " << num(chk.demand) << " " << num(chk.resistance) << " "
                << QString::fromStdString(chk.unit) << " " << num(chk.ratio()) << " " << chk.minimumRule << chk.rangeRule << chk.rangeLower << "\n";
            for (const auto &line : chk.lines) {
                out << "   " << QString::fromStdString(line) << "\n";
            }
        }
        for (const auto &p : e.parts) {
            out << "peca " << QString::fromStdString(p.description) << " " << p.quantity << " " << num(p.massEach) << " " << num(p.length) << "\n";
        }
        const auto solids = ConnectionSolids::build(c, true);
        double sum = 0.0;
        for (const auto &p : solids) {
            sum += static_cast<int>(p.kind) * 7.0 + static_cast<int>(p.role) * 13.0 + p.radius + p.u0 + p.u1;
            for (int i = 0; i < 3; ++i) {
                sum += p.a[i] * (i + 1) + p.b[i] * (i + 2) + p.size[i] * (i + 3);
            }
            for (const auto &pt : p.polygon) {
                sum += pt[0] * 3.0 + pt[1] * 5.0;
            }
        }
        out << "solidos " << solids.size() << " " << num(sum) << "\n";
    }

    static Design::DesignSection makeSection(const QString &family, const QMap<QString, double> &params)
    {
        QString err;
        const auto shape = SectionShapes::build(family, params, false, &err);
        if (!shape) {
            qFatal("build failed: %s", qPrintable(err));
        }
        Design::ProfileSpec spec;
        spec.family = family.toStdString();
        for (auto it = params.begin(); it != params.end(); ++it) {
            spec.params[it.key().toStdString()] = it.value();
        }
        std::vector<Design::Loop> loops;
        for (const auto &l : shape->loops) {
            loops.emplace_back(l.begin(), l.end());
        }
        return Design::makeDesignSection(spec, loops, 0.0);
    }

private slots:
    void dumpAll()
    {
        QString text;
        QTextStream out(&text);

        // ---- ligacoes: tipologias antigas (0..7) e todo o catalogo ----
        for (int type = 0; type <= 7; ++type) {
            dumpConnection(out, type, QStringLiteral("legado"));
        }
        for (const auto &entry : ConnectionCatalog::entries()) {
            dumpConnection(out, entry.id, entry.name);
        }

        // ---- dimensionamento de barras ----
        struct Case {
            QString family;
            QMap<QString, double> params;
            double fy, fu;
            double n, mx, my, vy, vz;
        };
        const std::vector<Case> cases = {
            {"W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}}, 345, 450, -100e3, 80e6, 5e6, 40e3, 5e3},
            {"W Laminado", {{"d", 530}, {"bf", 210}, {"tw", 9}, {"tf", 16}}, 250, 400, 300e3, 250e6, 0, 120e3, 0},
            {"U Formado", {{"d", 150}, {"bf", 60}, {"t", 3.0}}, 345, 450, -20e3, 3e6, 0.4e6, 4e3, 1e3},
            {"Tubular Circular", {{"D", 114.3}, {"t", 4.8}}, 355, 490, -150e3, 8e6, 8e6, 10e3, 10e3},
            {"Tubular Retangular", {{"b", 100}, {"h", 150}, {"t", 4.75}}, 355, 490, -80e3, 20e6, 5e6, 15e3, 5e3},
            {"L Laminado", {{"b", 76}, {"t", 6.35}}, 250, 400, 40e3, 0.2e6, 0.2e6, 1e3, 1e3},
            {"C Formado", {{"d", 200}, {"bf", 75}, {"t", 2.65}, {"c", 20}}, 345, 450, -30e3, 4e6, 0.3e6, 5e3, 1e3},
        };
        for (const auto &c : cases) {
            const auto section = makeSection(c.family, c.params);
            for (int standardCase = 0; standardCase < 2; ++standardCase) {
                Design::BarDesignInput in;
                in.section = &section;
                in.steel = {"aco", c.fy, c.fu};
                in.length = 4000.0 + 1500.0 * standardCase;
                in.Kx = 1.0;
                in.Ky = standardCase ? 0.8 : 1.0;
                in.Kz = 1.0;
                in.lateralSegments = 1 + standardCase;
                in.b2 = standardCase ? 1.08 : 1.0;
                Design::ComboEffects combo;
                combo.name = "ELU1";
                for (int i = 0; i <= 8; ++i) {
                    Design::StationEffects st;
                    st.x = in.length * i / 8.0;
                    const double s = std::sin(3.14159265358979 * i / 8.0);
                    st.e.N = c.n;
                    st.e.Mx = c.mx * s;
                    st.e.My = c.my * s;
                    st.e.Vy = c.vy * (1.0 - i / 8.0);
                    st.e.Vz = c.vz;
                    combo.stations.push_back(st);
                }
                in.combos.push_back(combo);
                out << "### barra " << c.family << " caso " << standardCase << "\n";
                try {
                    const auto r = Design::designBar(in);
                    out << "supported " << r.supported << " " << QString::fromStdString(r.standard) << " max " << num(r.maxRatio) << " " << QString::fromStdString(r.governingCheck)
                        << " " << QString::fromStdString(r.governingCombo) << " x " << num(r.governingX) << "\n";
                    for (const auto &chk : r.checks) {
                        out << "chk " << QString::fromStdString(chk.id) << " | " << QString::fromStdString(chk.title) << " | " << QString::fromStdString(chk.clause) << " | "
                            << num(chk.demand) << " " << num(chk.resistance) << " " << num(chk.ratio) << " " << QString::fromStdString(chk.unit) << " " << chk.applicable << "\n";
                        for (const auto &line : chk.lines) {
                            out << "   " << QString::fromStdString(line) << "\n";
                        }
                    }
                    for (const auto &w : r.warnings) {
                        out << "aviso " << QString::fromStdString(w) << "\n";
                    }
                    for (const auto &l : r.summaryLines) {
                        out << "resumo " << QString::fromStdString(l) << "\n";
                    }
                } catch (const std::exception &ex) {
                    out << "excecao " << ex.what() << "\n";
                }
            }
        }

        // ---- vigas mistas e laje mista ----
        {
            Design::Composite::BeamInput beamInput;
            Design::Composite::BeamDemand demand;
            demand.msd = 400e6;
            demand.vsd = 150e3;
            demand.mGaSd = 80e6;
            demand.mGaS = 60e6;
            demand.mLS = 200e6;
            demand.qBefore = 8.0;
            demand.qPermanent = 6.0;
            demand.qVariable = 10.0;
            const auto r = Design::Composite::checkBeam(beamInput, demand);
            out << "### viga mista max " << num(r.maxRatio) << " " << QString::fromStdString(r.governing) << " mRd " << num(r.mRd) << " " << num(r.mRdFull) << "\n";
            for (const auto &chk : r.checks) {
                out << "chk " << QString::fromStdString(chk.name) << " | " << num(chk.demand) << " " << num(chk.resistance) << " " << num(chk.ratio()) << "\n";
                for (const auto &line : chk.lines) {
                    out << "   " << QString::fromStdString(line) << "\n";
                }
            }
            Design::Composite::DeckInput deck;
            Design::Composite::DeckLoads loads;
            const auto d = Design::Composite::checkDeck(deck, loads);
            out << "### laje mista max " << num(d.maxRatio) << " " << QString::fromStdString(d.governing) << "\n";
            for (const auto &chk : d.checks) {
                out << "chk " << QString::fromStdString(chk.name) << " | " << num(chk.demand) << " " << num(chk.resistance) << " " << num(chk.ratio()) << "\n";
                for (const auto &line : chk.lines) {
                    out << "   " << QString::fromStdString(line) << "\n";
                }
            }
        }

        out.flush();
        QVERIFY(text.size() > 10000);
        const QByteArray path = qgetenv("STRUCTURA_GOLDEN_OUT");
        if (!path.isEmpty()) {
            QFile file(QString::fromLocal8Bit(path));
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            file.write(text.toUtf8());
        }
        // comparacao com o despejo versionado (src/tests/golden/numerics.txt): qualquer mudanca numerica de ligacao, barra ou viga mista reprova.
        // Mudanca intencional de calculo: regrave o arquivo com STRUCTURA_GOLDEN_OUT=src/tests/golden/numerics.txt e revise a diferenca no controle de versao.
        QFile golden(QStringLiteral(STRUCTURA_SOURCE_DIR "/src/tests/golden/numerics.txt"));
        QVERIFY2(golden.open(QIODevice::ReadOnly | QIODevice::Text), "arquivo dourado ausente");
        const QStringList expected = QString::fromUtf8(golden.readAll()).split(QLatin1Char('\n'));
        const QStringList actual = text.split(QLatin1Char('\n'));
        for (int i = 0; i < std::min(expected.size(), actual.size()); ++i) {
            if (expected.at(i).trimmed() != actual.at(i).trimmed()) {
                QFAIL(qPrintable(QStringLiteral("linha %1 difere:\n  esperado: %2\n  obtido:   %3").arg(i + 1).arg(expected.at(i), actual.at(i))));
            }
        }
        QCOMPARE(actual.size(), expected.size());
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestGoldenNumerics test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestGoldenNumerics.moc"
