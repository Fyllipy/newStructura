#include <QtTest/QtTest>
#include <QApplication>
#include <QRegularExpression>

#include "design/ColumnFlangeLever.h"
#include "design/Connections.h"
#include "design/EndPlateConnection.h"
#include "design/TStub.h"

#include <cmath>

namespace D = Structura::Design::Connections;
namespace TS = Structura::Design::TStub;

// Nucleo T-stub (EN 1993-1-8) e A2: benchmarks publicados (SCI P398 Exemplo C.1 e verificacao de T-stub da IDEA StatiCa), camada NBR e estados de aprovacao.
class TestTStub : public QObject
{
    Q_OBJECT

    static D::EndPlateInput heb200(int rows, double moment)
    {
        D::EndPlateInput in;
        for (const auto &g : D::boltGrades()) {
            if (g.name.find("A325") != std::string::npos) {
                in.grade = g;
            }
        }
        in.db = 20.0;
        in.colDepth = 200.0;
        in.colFlangeWidth = 200.0;
        in.colFlangeThickness = 15.0;
        in.colWebThickness = 9.0;
        in.colRadius = 18.0;
        in.column = {250.0, 400.0};
        in.gauge = 90.0;
        in.plateWidth = 160.0;
        in.plateThickness = 25.0;
        in.pitch = 70.0;
        in.tensionRows = rows;
        in.moment = moment;
        in.shear = 30.0e3;
        in.columnSource = "HEB200 (teste)";
        return in;
    }
    static const D::Check *find(const std::vector<D::Check> &checks, const char *name)
    {
        for (const auto &c : checks) {
            if (c.name == name) {
                return &c;
            }
        }
        return nullptr;
    }
    static void near(double value, double reference, double tolerance, const char *what)
    {
        QVERIFY2(std::abs(value / reference - 1.0) <= tolerance, qPrintable(QStringLiteral("%1: obtido %2, referencia %3").arg(what).arg(value).arg(reference)));
    }

private slots:
    // SCI P398 Exemplo C.1: UKC 254x254x107 S275, M24 8.8, gM0 = 1,0, gM2 = 1,25 (resistencias publicadas da mesa do pilar)
    void sciP398ColumnFlange()
    {
        const TS::EnCoefficients c;
        TS::ColumnFlangeInput in;
        in.m = 33.4;
        in.e = 79.4;
        in.emin = 75.0;
        in.tf = 20.5;
        in.fy = 265.0;
        in.ew = 39.55 / 4.0;
        in.boltFtRd = TS::boltTensionResistance(800.0, 353.0, c);
        near(in.boltFtRd, 203.0e3, 0.01, "Ft,Rd do parafuso");
        in.rows = 3;
        in.rowSpacings = {100.0, 90.0};
        const auto r = TS::columnFlange(in, c, TS::Method::Two);
        QVERIFY(r.valid);
        near(r.rows[0].isolated.resistance, 398.0e3, 0.02, "fileira 1 isolada");
        QCOMPARE(r.rows[0].isolated.governing, 2);
        auto group = [&](const char *name) {
            for (const auto &g : r.groups) {
                if (g.first == name) {
                    return g.second.resistance;
                }
            }
            return 0.0;
        };
        near(group("1-2"), 697.0e3, 0.02, "grupo 1-2");
        near(group("1-3"), 988.0e3, 0.02, "grupo 1-2-3");
        near(group("2-3"), 691.0e3, 0.02, "grupo 2-3");
        // comprimentos efetivos publicados
        const auto iso = TS::columnFlangeRow(33.4, 79.4);
        near(iso.circular, 210.0, 0.01, "leff,cp isolada");
        near(iso.nonCircular, 233.0, 0.01, "leff,nc isolada");
        // a soma das efetivas respeita o grupo de 3 fileiras: 398 + (697 - 398) + (988 - 697) = 988 (P398: 377/320/291 com a chapa; aqui so a mesa)
        near(r.total, 988.0e3, 0.02, "soma das fileiras = grupo 1-2-3");
    }

    // SCI P398: chapa de extremidade, fileira 1 (extensao): 901 / 377 / 406 kN
    void sciP398EndPlate()
    {
        const TS::EnCoefficients c;
        const auto leff = TS::endPlateExtensionRow(30.4, 50.0, 75.0, 100.0, 250.0);
        near(leff.circular, 191.0, 0.01, "leff,cp da chapa");
        near(leff.nonCircular, 125.0, 0.01, "leff,nc da chapa");
        TS::Geometry g;
        g.m = 30.4;
        g.e = 50.0;
        g.emin = 50.0;
        g.ew = 39.55 / 4.0;
        const auto r = TS::resistance(g, leff, 25.0, {265.0}, {TS::boltTensionResistance(800.0, 353.0, c), 2}, c, TS::Method::Two, "chapa");
        QVERIFY(r.valid);
        near(r.f1, 901.0e3, 0.02, "modo 1");
        near(r.f2, 377.0e3, 0.02, "modo 2");
        near(r.f3, 406.0e3, 0.02, "modo 3");
        QCOMPARE(r.governing, 2);
    }

    // IDEA StatiCa: 26 amostras do metodo dos componentes (T-stub S235, bf = 300, tw = 20, 2 parafusos)
    void ideaStatiCaComponentMethod()
    {
        struct Sample { const char *name; double tf, fub, As, db, w, aw, b, cm; };
        const Sample samples[] = {
            {"tf10", 10, 800, 353, 24, 165, 10, 100, 44}, {"tf12", 12, 800, 353, 24, 165, 10, 100, 63}, {"tf15", 15, 800, 353, 24, 165, 10, 100, 98},
            {"tf20", 20, 800, 353, 24, 165, 10, 100, 174}, {"tf25", 25, 800, 353, 24, 165, 13.5, 100, 279}, {"tf30", 30, 800, 353, 24, 165, 13.5, 100, 305},
            {"tf35", 35, 800, 353, 24, 165, 13.5, 100, 335}, {"tf40", 40, 800, 353, 24, 165, 13.5, 100, 371}, {"tf45", 45, 800, 353, 24, 165, 13.5, 100, 407},
            {"tf50", 50, 800, 353, 24, 165, 13.5, 100, 407},
            {"M16", 25, 800, 157, 16, 165, 10, 100, 152}, {"M20", 25, 800, 245, 20, 165, 10, 100, 205}, {"M24", 25, 800, 353, 24, 165, 10, 100, 270},
            {"M27", 25, 800, 459, 27, 165, 10, 100, 278}, {"4.8", 25, 400, 353, 24, 165, 10, 100, 164}, {"5.8", 25, 500, 353, 24, 165, 10, 100, 190},
            {"6.8", 25, 600, 353, 24, 165, 10, 100, 217}, {"10.9", 25, 1000, 353, 24, 165, 10, 100, 273},
            {"w110", 20, 800, 353, 24, 110, 7, 100, 282}, {"w150", 20, 800, 353, 24, 150, 7, 100, 188}, {"w200", 20, 800, 353, 24, 200, 7, 100, 129},
            {"w240", 20, 800, 353, 24, 240, 7, 100, 107}, {"b100", 20, 1000, 353, 24, 110, 7, 100, 314}, {"b250", 20, 1000, 353, 24, 110, 7, 250, 423},
            {"b300", 20, 1000, 353, 24, 110, 7, 300, 433}, {"b400", 20, 1000, 353, 24, 110, 7, 400, 433},
        };
        const TS::EnCoefficients c;
        int count = 0;
        for (const auto &s : samples) {
            TS::Geometry g;
            g.m = (s.w - 20.0) / 2.0 - 0.8 * s.aw * std::sqrt(2.0);
            g.e = (300.0 - s.w) / 2.0;
            g.ew = (s.db == 24.0 ? 41.6 : TS::boltHeadAcrossPoints(s.db)) / 4.0;   // dw publicado para M24; demais pela tabela do nucleo
            const auto row = TS::columnFlangeRow(g.m, g.e);
            TS::EffectiveLength leff;
            leff.circular = std::min(s.b, row.circular);
            leff.nonCircular = std::min(s.b, row.nonCircular);
            const auto r = TS::resistance(g, leff, s.tf, {235.0}, {TS::boltTensionResistance(s.fub, s.As, c), 2}, c, TS::Method::Two, s.name);
            QVERIFY(r.valid);
            near(r.resistance, s.cm * 1e3, 0.02, s.name);   // tolerancia de 2 % do plano (precisao dos dados de referencia)
            ++count;
        }
        QCOMPARE(count, 26);
    }

    void modeTransitionsAndMonotonicity()
    {
        const TS::EnCoefficients c;
        double previous = 0.0;
        int modes[4] = {0, 0, 0, 0};
        for (double tf : {8.0, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0}) {
            TS::Geometry g;
            g.m = 57.23;
            g.e = 67.5;
            g.ew = 41.6 / 4.0;
            const auto r = TS::resistance(g, {std::min(100.0, 2 * 3.14159265 * g.m), 100.0, ""}, tf, {235.0}, {TS::boltTensionResistance(800.0, 353.0, c), 2}, c, TS::Method::Two, "x");
            QVERIFY(r.valid);
            QVERIFY(r.resistance >= previous - 1e-6);   // nao decresce com a espessura
            QVERIFY(r.resistance <= r.f3 + 1e-6);       // nunca acima dos parafusos
            previous = r.resistance;
            ++modes[r.governing];
        }
        QVERIFY(modes[1] > 0 && modes[2] > 0 && modes[3] > 0);   // os tres modos aparecem
        // metodo 1 nunca excede o metodo 2 no modo 1
        TS::Geometry g;
        g.m = 33.4;
        g.e = 79.4;
        g.emin = 75.0;
        g.ew = 9.9;
        const auto leff = TS::columnFlangeRow(33.4, 79.4);
        const auto m1 = TS::resistance(g, leff, 10.0, {265.0}, {200e3, 2}, c, TS::Method::One, "m1");
        const auto m2 = TS::resistance(g, leff, 10.0, {265.0}, {200e3, 2}, c, TS::Method::Two, "m2");
        QVERIFY(m1.f1 < m2.f1);
        // grupo nunca excede a soma das fileiras isoladas
        TS::ColumnFlangeInput in;
        in.m = 26.1; in.e = 55.0; in.tf = 15.0; in.fy = 250.0; in.ew = 8.0; in.boltFtRd = 141.1e3; in.rows = 2; in.rowSpacings = {70.0};
        const auto two = TS::columnFlange(in, c, TS::Method::Two);
        QVERIFY(two.valid);
        QVERIFY(two.total <= two.rows[0].isolated.resistance + two.rows[1].isolated.resistance + 1e-6);
    }

    void tstubRejectsInvalidInput()
    {
        const TS::EnCoefficients c;
        TS::Geometry g;
        g.m = 0.0;   // invalida
        g.e = 50.0;
        QVERIFY(!TS::resistance(g, {100.0, 100.0, ""}, 10.0, {235.0}, {100e3, 2}, c, TS::Method::Two, "x").valid);
        g.m = 30.0;
        QVERIFY(!TS::resistance(g, {100.0, 100.0, ""}, -1.0, {235.0}, {100e3, 2}, c, TS::Method::Two, "x").valid);
        QVERIFY(!TS::resistance(g, {100.0, 100.0, ""}, 10.0, {0.0}, {100e3, 2}, c, TS::Method::Two, "x").valid);
        TS::ColumnFlangeInput in;   // espacamentos inconsistentes com o numero de fileiras
        in.m = 30; in.e = 50; in.tf = 10; in.fy = 235; in.boltFtRd = 100e3; in.rows = 3; in.rowSpacings = {70.0};
        QVERIFY(!TS::columnFlange(in, c, TS::Method::Two).valid);
    }

    // ---- Camada A (NBR 8800:2024, 6.3.5.4) - conferida com a reimplementacao independente em Python (docs/validacao_alavanca)
    void nbrLayerMatchesIndependentCalculation()
    {
        const double ft = 0.75 * (0.25 * 3.14159265358979 * 400.0) * 830.0 / 1.35;
        const double b = 40.5, p = 3.5 * b;   // HEB200, b literal
        near(D::nbrPryingRowResistance(15.0, b, 55.0, p, 20.0, 22.0, 400.0, 1.10, ft), 256.389e3, 0.002, "HEB200 1 fileira");
        // UKC 254x254x107 (gamma = 1,0; M24 8.8): 406,66 kN
        near(D::nbrPryingRowResistance(20.5, 33.4, 79.4, 3.5 * 33.4, 24.0, 26.0, 410.0, 1.0, 0.9 * 800.0 * 353.0 / 1.25), 406.656e3, 0.002, "UKC 254x254x107");
        // valores intermediarios (Ft,0,Sd = 60 kN)
        const auto r = D::nbrPrying(b, 55.0, p, 20.0, 22.0, 400.0, 1.10, ft, 60.0e3);
        QVERIFY(r.valid && !r.boltInadequate);
        near(r.beta, 2.8114, 0.001, "beta");
        QCOMPARE(r.alpha, 1.0);
        near(r.delta, 0.8448, 0.001, "delta");
        near(r.tRequired, 8.7738, 0.001, "t requerido");
    }

    void nbrLayerEdgeCases()
    {
        const double ft = 144.86e3;
        // beta < 0: parafuso inadequado
        const auto weak = D::nbrPrying(40.5, 55.0, 141.75, 20.0, 22.0, 400.0, 1.10, ft, 1.5 * ft);
        QVERIFY(weak.valid && weak.boltInadequate && weak.beta < 0.0);
        // delta invalido (p menor que o furo)
        QVERIFY(!D::nbrPrying(40.5, 55.0, 20.0, 20.0, 22.0, 400.0, 1.10, ft, 50e3).valid);
        // dimensoes ausentes, nulas ou negativas
        QVERIFY(!D::nbrPrying(0.0, 55.0, 141.75, 20.0, 22.0, 400.0, 1.10, ft, 50e3).valid);
        QVERIFY(!D::nbrPrying(40.5, 55.0, -1.0, 20.0, 22.0, 400.0, 1.10, ft, 50e3).valid);
        QVERIFY(!D::nbrPrying(40.5, 55.0, 141.75, 20.0, 22.0, 0.0, 1.10, ft, 50e3).valid);
        QVERIFY(!D::nbrPrying(8.0, 55.0, 141.75, 20.0, 22.0, 400.0, 1.10, ft, 50e3).valid);   // b <= 0,5 db
        // a > 1,25 b e limitado
        const auto cap = D::nbrPrying(40.5, 500.0, 141.75, 20.0, 22.0, 400.0, 1.10, ft, 50e3);
        near(cap.aEff, 1.25 * 40.5, 1e-9, "a limitado a 1,25 b");
    }

    // ---- integracao: estados de aprovacao
    void singleRowLayerAParticipates()
    {
        const auto checks = D::checkEndPlate(heb200(1, 50.0e6));
        const D::Check *a = find(checks, D::kLeverNbrName);
        QVERIFY(a);
        QVERIFY(a->status == D::CheckStatus::Evaluated);
        QVERIFY(a->minimumRule);
        const D::Check *b = find(checks, D::kLeverEnName);
        QVERIFY(b);
        QVERIFY(b->status == D::CheckStatus::Complementary);
        QVERIFY(b->resistance > 0.0);
        QVERIFY(b->statusLabel().find("COMPLEMENTAR") != std::string::npos);
        // Camada A reprovada pela espessura da mesa: fina demais para a tracao
        auto thin = heb200(1, 60.0e6);
        thin.colFlangeThickness = 6.0;
        const auto weak = D::checkEndPlate(thin);
        const D::Check *aw = find(weak, D::kLeverNbrName);
        QVERIFY(aw && aw->status == D::CheckStatus::Evaluated && aw->ratio() > 1.0);
        QVERIFY(D::summarize(weak).status == D::Summary::Failed);
    }

    void multipleRowsAreNotVerifiableAndNeverApproved()
    {
        const auto checks = D::checkEndPlate(heb200(2, 40.0e6));
        const D::Check *a = find(checks, D::kLeverNbrName);
        QVERIFY(a);
        QVERIFY(a->status == D::CheckStatus::Unverifiable);
        QVERIFY(a->reason.find("MEF independente") != std::string::npos);
        const auto s = D::summarize(checks);
        QVERIFY(s.status == D::Summary::Incomplete);
        QVERIFY(s.worstRatio >= D::kUnverifiableRatio);
        // a camada B existe, mas nao "compensa" a ausencia da camada A
        const D::Check *b = find(checks, D::kLeverEnName);
        QVERIFY(b && b->status == D::CheckStatus::Complementary && b->resistance > 0.0);
        QVERIFY(b->demand < b->resistance);   // camada B "favoravel" ...
        QVERIFY(s.status != D::Summary::Approved);   // ... e a ligacao continua nao aprovada
        QVERIFY(find(checks, D::kLeverWarnName) == nullptr);   // sem camada A valida nao ha advertencia
    }

    void complementaryLayerNeverChangesTheGlobalResult()
    {
        for (const double moment : {20.0e6, 50.0e6, 200.0e6}) {   // aprovada, folgada e muito acima da resistencia da camada B
            const auto checks = D::checkEndPlate(heb200(1, moment));
            std::vector<D::Check> without;
            for (const auto &c : checks) {
                if (c.status != D::CheckStatus::Complementary) {
                    without.push_back(c);
                }
            }
            const auto s1 = D::summarize(checks), s2 = D::summarize(without);
            QVERIFY(s1.status == s2.status);
            QCOMPARE(s1.executed, s2.executed);
            QCOMPARE(s1.failed, s2.failed);
            QCOMPARE(s1.unverifiable, s2.unverifiable);
            QCOMPARE(s1.worstRatio, s2.worstRatio);
            QCOMPARE(D::worstRatio(checks), D::worstRatio(without));
        }
        // camada B reprovada (tracao acima da resistencia EN) nao reprova a ligacao; camada B aprovada nao a aprova (lista sem verificacoes executadas)
        D::Check ok;
        ok.demand = 1.0;
        ok.resistance = 2.0;
        D::Check bFail;
        bFail.status = D::CheckStatus::Complementary;
        bFail.demand = 10.0;
        bFail.resistance = 5.0;
        QVERIFY(D::summarize({ok, bFail}).status == D::Summary::Approved);
        D::Check bOk = bFail;
        bOk.demand = 1.0;
        QVERIFY(D::summarize({bOk}).status == D::Summary::Incomplete);
        QVERIFY(D::summarize({bFail}).status == D::Summary::Incomplete);
    }

    void divergenceWarningAtTenPercent()
    {
        // sem divergencia relevante: HEB200 de referencia (R_A / R_B < 1,10)
        const auto base = D::checkEndPlate(heb200(1, 50.0e6));
        QVERIFY(find(base, D::kLeverWarnName) == nullptr);
        // fu alto do pilar (fy igual): a camada A (usa fu) passa a resistir >= 1,10 da camada B (usa fy)
        auto strong = heb200(1, 50.0e6);
        strong.column = {250.0, 700.0};
        const auto checks = D::checkEndPlate(strong);
        const D::Check *w = find(checks, D::kLeverWarnName);
        QVERIFY(w);
        QVERIFY(w->status == D::CheckStatus::Complementary);
        QVERIFY(w->origin == D::CheckOrigin::DivergenceWarning);
        // I1: nenhuma semantica de demanda/resistencia nem indice de utilizacao; so a mensagem informativa
        QCOMPARE(w->demand, 0.0);
        QCOMPARE(w->resistance, 0.0);
        QCOMPARE(w->ratio(), 0.0);
        const QString message = QString::fromStdString(w->reason);
        QVERIFY2(message.startsWith(QStringLiteral("Divergência entre metodologias: Camada A (NBR) = ")), qPrintable(message));
        QVERIFY(message.contains(QStringLiteral("Camada B (EN) = ")));
        QVERIFY(message.contains(QStringLiteral("superior à da Camada B")));
        QVERIFY(message.contains(QStringLiteral("sem influência na aprovação global")));
        QCOMPARE(QString::fromStdString(w->lines.front()), message);
        // a condicao R_A >= 1,10 R_B continua valida para os numeros mostrados (arredondados a 0,01 kN)
        const QRegularExpression re(QStringLiteral(R"(Camada A \(NBR\) = ([0-9]+,[0-9]+) kN; Camada B \(EN\) = ([0-9]+,[0-9]+) kN\. A estimativa da Camada A é ([0-9]+,[0-9]+)%)"));
        const auto match = re.match(message);
        QVERIFY2(match.hasMatch(), qPrintable(message));
        const double ra = match.captured(1).replace(',', '.').toDouble(), rb = match.captured(2).replace(',', '.').toDouble(), pct = match.captured(3).replace(',', '.').toDouble();
        QVERIFY(ra >= 1.10 * rb - 0.02);
        QVERIFY(std::abs(pct - (ra / rb - 1.0) * 100.0) < 0.2);
        const auto s = D::summarize(checks);
        std::vector<D::Check> without;
        for (const auto &c : checks) {
            if (c.status != D::CheckStatus::Complementary) {
                without.push_back(c);
            }
        }
        QVERIFY(D::summarize(without).status == s.status);
    }

    void boltHeadWidthDefinitionAndFallback()
    {
        // grandeza implementada: largura entre VERTICES = largura entre faces x 2/raiz(3)
        const double hex = 2.0 / std::sqrt(3.0);
        QVERIFY(std::abs(1.1547005 - hex) / hex < 1e-7);   // fator usado
        const std::pair<int, double> flats[] = {{12, 18.0}, {16, 24.0}, {20, 30.0}, {22, 34.0}, {24, 36.0}, {27, 41.0}, {30, 46.0}, {36, 55.0}};
        for (const auto &f : flats) {
            const auto w = TS::boltHeadWidth(f.first);
            QVERIFY(w.tabulated);
            QVERIFY(std::abs(w.acrossPoints - f.second * hex) < 1e-5);   // fator truncado em 1,1547005 (erro relativo 3e-8)
            QVERIFY(w.acrossPoints > f.second);   // vertices > faces
        }
        QVERIFY(std::abs(TS::boltHeadWidth(24).acrossPoints - 41.6) < 0.05);   // dw publicado no benchmark (M24)
        // fallback: s = 1,5 db estimado => dw = 1,5 db x 2/raiz(3) = raiz(3) db = 1,732 db; NAO e 1,5 db
        for (const int db : {14, 18, 33}) {
            const auto w = TS::boltHeadWidth(db);
            QVERIFY(!w.tabulated);
            QVERIFY(std::abs(w.acrossPoints - std::sqrt(3.0) * db) < 1e-5);
            QVERIFY(std::abs(w.acrossPoints - 1.5 * db) > 1.0);
        }
        // a estimativa e identificada na memoria da Camada B com a expressao e o valor de dw
        auto in = heb200(1, 50.0e6);
        in.db = 14.0;
        const auto checks = D::checkEndPlate(in);
        const D::Check *b = find(checks, D::kLeverEnName);
        QVERIFY(b);
        std::string all;
        for (const auto &l : b->lines) {
            all += l + "\n";
        }
        QVERIFY(all.find("VALOR ESTIMADO") != std::string::npos);
        QVERIFY(all.find("1,5 db x 1,1547") != std::string::npos);
        QVERIFY(all.find("24.2") != std::string::npos);   // dw estimado de M14 = 24,25 mm
    }

    void invalidDataIsNeverSilentlyApproved()
    {
        auto bad = heb200(1, 50.0e6);
        bad.colWebThickness = 0.0;   // dado invalido: nada de A2 aprova
        const auto s = D::summarize(D::checkEndPlate(bad));
        QVERIFY(s.status == D::Summary::Incomplete);
        auto noTension = heb200(1, 0.0);
        const auto none = D::checkEndPlate(noTension);
        const D::Check *a = find(none, D::kLeverNbrName);
        QVERIFY(a && a->status == D::CheckStatus::NotApplicable);
        QVERIFY(!a->reason.empty());
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestTStub test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestTStub.moc"
