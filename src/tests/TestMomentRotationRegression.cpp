// Regressao automatizada da ferramenta momento-rotacao (14 casos): a saida completa (rigidez, resistencias, curva, metricas, classificacao, rotula bilinear
// equivalente e memoria de calculo) e comparada TEXTUALMENTE, linha a linha, com a referencia versionada src/tests/golden/momento_rotacao.txt.
// Nao ha tolerancia numerica: os valores sao impressos com 12 algarismos significativos e qualquer diferenca reprova. Nenhum arquivo de producao precisa ser editado.
// Mudanca intencional de calculo: regrave com STRUCTURA_MR_OUT=src/tests/golden/momento_rotacao.txt e justifique a diferenca no controle de versao.
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "design/JointClassification.h"
#include "design/JointComponents.h"

#include <QFile>
#include <QtTest>

using namespace Structura::App::Services;
using Structura::App::ProjectData::NodeConnection;
namespace JC = Structura::Design::JointComponents;
namespace JS = Structura::Design::JointClass;

namespace {

ConnectionService::BeamInfo beamOf(const char *name, double d, double bf, double tw, double tf)
{
    ConnectionService::BeamInfo b;
    b.name = name;
    b.family = "W Laminado";
    b.geometry = {{"d", d}, {"bf", bf}, {"tw", tw}, {"tf", tf}};
    b.depth = d;
    b.bf = bf;
    b.tw = tw;
    b.tf = tf;
    return b;
}

void setColumn(NodeConnection &c, double d, double bf, double tf, double tw, double r)
{
    c.values["supD"] = d;
    c.values["supBf"] = bf;
    c.values["supTf"] = tf;
    c.values["supTw"] = tw;
    c.values["supR"] = r;
    c.values["supFy"] = 250.0;
    c.values["supFu"] = 400.0;
    c.values["supConfirmed"] = 1.0;
}

struct Case {
    std::string label;
    NodeConnection c;
};

std::vector<Case> cases()
{
    const auto ipe300 = beamOf("IPE300", 300.0, 150.0, 7.1, 10.7);
    const auto ipe400 = beamOf("IPE400", 400.0, 180.0, 8.6, 13.5);
    auto make = [&](const ConnectionService::BeamInfo &b, int type = NodeConnection::kEndPlate) {
        NodeConnection c = ConnectionService::makeDefault(type, b, 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;
        return c;
    };
    std::vector<Case> out;
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        out.push_back({"C01 IPE300 / col 260x146x12.7 / padrao (2 fileiras)", c});
        c.values["tRows"] = 1;
        out.push_back({"C02 idem, 1 fileira", c});
        c.values["tRows"] = 3;
        out.push_back({"C03 idem, 3 fileiras", c});
    }
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 200, 200, 15, 9, 18);
        c.values["db"] = 24;
        c.values["plateT"] = 25;
        c.values["tRows"] = 1;
        out.push_back({"C04 IPE300 / HEB200 / M24 / chapa 25 / 1 fileira", c});
        c.values["tRows"] = 2;
        out.push_back({"C05 idem, 2 fileiras", c});
    }
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 200, 200, 8, 6, 10);
        c.values["tRows"] = 1;
        out.push_back({"C06 mesa do pilar esbelta tf=8 / 1 fileira (camada A reprova)", c});
        c.values["tRows"] = 2;
        out.push_back({"C07 mesa do pilar esbelta tf=8 / 2 fileiras", c});
    }
    {
        NodeConnection c = make(ipe400);
        setColumn(c, 300, 300, 19, 11, 27);
        c.values["db"] = 24;
        c.values["plateT"] = 22;
        c.values["plateW"] = 190;
        c.values["gauge"] = 110;
        c.values["tRows"] = 2;
        out.push_back({"C08 IPE400 / HEB300 / M24 / chapa 22 / 2 fileiras", c});
        c.values["tRows"] = 1;
        out.push_back({"C09 idem, 1 fileira", c});
        c.values["bothSides"] = 1;
        out.push_back({"C10 idem, viga dos dois lados", c});
        c.values["bothSides"] = 0;
        c.values["cont"] = 1;
        c.values["doubler"] = 8;
        out.push_back({"C11 idem, enrijecedores de continuidade + reforco de alma", c});
    }
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        c.values["plateT"] = 12;
        c.values["db"] = 16;
        c.values["tRows"] = 1;
        out.push_back({"C12 chapa fina 12 / M16 / 1 fileira", c});
    }
    {
        NodeConnection c = make(ipe300, 144);
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        out.push_back({"C13 controle: mesas soldadas (catalogo 144), sem alavanca A2", c});
    }
    out.push_back({"C14 pilar NAO informado (A1): Nao verificavel", make(ipe300)});
    return out;
}

QString dump()
{
    QString out;
    const JS::Limits limits = JS::limitsFor(2.0e8, 2.0e-4, 6.0);
    JS::StoreyStiffness storey;
    storey.status = JS::StoreyStatus::Ok;
    storey.kv = 0.3;
    storey.kp = 1.0;
    storey.ratio = 0.3;
    storey.beams = storey.columns = 1;
    auto line = [&out](const QString &s) { out += s + QLatin1Char('\n'); };
    for (const Case &k : cases()) {
        const auto r = JointComponentService::evaluate(k.c, 200000.0);
        line(QStringLiteral("=== ") + QString::fromStdString(k.label));
        line(QStringLiteral("supported=%1 message=[%2]").arg(r.supported ? 1 : 0).arg(QString::fromStdString(r.message)));
        line(QStringLiteral("fingerprint=%1").arg(QString::fromStdString(r.fingerprint)));
        line(QStringLiteral("Sj_ini=%1 MjRd_used=%2 MjRd_EN=%3 MjRd_NBR=%4 nbrAvail=%5 nbrBelowCurve=%6")
                 .arg(r.stiffness.initialStiffnessKnm(), 0, 'g', 12).arg(r.momentResistanceKnm, 0, 'g', 12).arg(r.momentResistanceEnKnm, 0, 'g', 12)
                 .arg(r.momentResistanceNbrKnm, 0, 'g', 12).arg(r.nbrCheckAvailable ? 1 : 0).arg(r.nbrBelowCurve ? 1 : 0));
        line(QStringLiteral("metrics: onset=%1 thOnset=%2 thRes=%3 secRes=%4 tanRes=%5 M_half=%6 th_half=%7")
                 .arg(r.metrics.onsetOfNonlinearity, 0, 'g', 12).arg(r.metrics.rotationAtOnset, 0, 'g', 12).arg(r.metrics.rotationAtResistance, 0, 'g', 12)
                 .arg(r.metrics.secantAtResistance, 0, 'g', 12).arg(r.metrics.tangentAtResistance, 0, 'g', 12).arg(r.metrics.momentAtHalfStiffness, 0, 'g', 12)
                 .arg(r.metrics.rotationAtHalfStiffness, 0, 'g', 12));
        QString curve = QStringLiteral("curve(%1 pts):").arg(r.behavior.positive.curve.size());
        for (const auto &p : r.behavior.positive.curve) {
            curve += QStringLiteral(" (%1,%2)").arg(p.theta, 0, 'g', 10).arg(p.moment, 0, 'g', 10);
        }
        line(curve);
        if (r.supported && r.stiffness.ok && r.momentResistanceKnm > 0.0) {
            const auto cl = JS::classify(r.stiffness.initialStiffnessKnm(), limits, storey, JS::JointKind::BeamColumn);
            line(QStringLiteral("classe: byStiffness=%1 final=%2 eta=%3").arg(static_cast<int>(cl.byStiffness)).arg(static_cast<int>(cl.final)).arg(cl.eta, 0, 'g', 12));
            const auto eq = JC::deriveEquivalent(JC::EquivalentModel::SemiRigidBilinear, r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm);
            line(QStringLiteral("equivalente bilinear: valid=%1 k=%2 My=%3 h=%4").arg(eq.valid ? 1 : 0).arg(eq.stiffness, 0, 'g', 12).arg(eq.yieldMoment, 0, 'g', 12).arg(eq.hardeningRatio, 0, 'g', 12));
            for (double f : {0.25, 0.5, 2.0 / 3.0, 0.8, 1.0}) {
                const double m = f * r.momentResistanceKnm;
                line(QStringLiteral("  M=%1 sec=%2 tan=%3").arg(m, 0, 'f', 4)
                         .arg(JC::secantStiffnessAt(m, r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm), 0, 'g', 10)
                         .arg(JC::tangentStiffnessAt(m, r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm), 0, 'g', 10));
            }
        }
        line(QStringLiteral("memoria (%1 linhas):").arg(r.memory.size()));
        for (const auto &l : r.memory) {
            line(QStringLiteral("| ") + QString::fromStdString(l));
        }
    }
    return out;
}

QStringList referenceLines(QString &error)
{
    QFile f(QStringLiteral(STRUCTURA_SOURCE_DIR "/src/tests/golden/momento_rotacao.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("referencia ausente: src/tests/golden/momento_rotacao.txt");
        return {};
    }
    QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    QStringList out;
    for (const QString &l : lines) {
        if (!l.startsWith(QLatin1Char('#'))) {
            out << l;
        }
    }
    return out;
}

} // namespace

class TestMomentRotationRegression : public QObject
{
    Q_OBJECT
private slots:
    void referenceIsComplete()
    {
        QString error;
        const QStringList ref = referenceLines(error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        int headers = 0;
        for (const QString &l : ref) {
            headers += l.startsWith(QLatin1String("=== C")) ? 1 : 0;
        }
        QCOMPARE(headers, 14);
        // cada caso de chapa de extremidade avaliavel traz 42 pontos de curva (a auditoria anterior citou 40 por engano)
        QVERIFY(ref.join(QLatin1Char('\n')).contains(QLatin1String("curve(42 pts):")));
    }

    void momentRotationOutputMatchesTheReferenceExactly()
    {
        const QString current = dump();
        const QByteArray out = qgetenv("STRUCTURA_MR_OUT");
        if (!out.isEmpty()) {
            QFile f(QString::fromLocal8Bit(out));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write("# Referencia de regressao da ferramenta momento-rotacao (14 casos): regravada com STRUCTURA_MR_OUT.\n");
            f.write(current.toUtf8());
            return;
        }
        QString error;
        const QStringList ref = referenceLines(error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QStringList refTrimmed = ref;
        while (!refTrimmed.isEmpty() && refTrimmed.last().isEmpty()) {
            refTrimmed.removeLast();
        }
        QStringList now = current.split(QLatin1Char('\n'));
        while (!now.isEmpty() && now.last().isEmpty()) {
            now.removeLast();
        }
        QString section;
        for (int i = 0; i < std::min(now.size(), refTrimmed.size()); ++i) {
            if (now[i].startsWith(QLatin1String("=== "))) {
                section = now[i];
            }
            QVERIFY2(now[i] == refTrimmed[i], qPrintable(QStringLiteral("diferenca na linha %1 do caso [%2]\n  referencia: %3\n  atual:      %4").arg(i + 1).arg(section, refTrimmed[i].left(300), now[i].left(300))));
        }
        QCOMPARE(now.size(), refTrimmed.size());
    }

    void a2ChecksNeverReachTheResistanceSweep()
    {
        // sem A2 ou com A2 a resistencia da NBR (nivel C) deve ser a mesma: o caso padrao (2 fileiras) tem resistencia determinada mesmo com a camada A NAO VERIFICAVEL
        const auto all = cases();
        const auto r = JointComponentService::evaluate(all.at(0).c, 200000.0);
        QVERIFY(r.nbrCheckAvailable);
        QVERIFY(r.momentResistanceNbrKnm > 0.0);
        QVERIFY(std::abs(r.momentResistanceNbrKnm - 66.280109375) < 1e-6);   // valor registrado antes do A2 (docs/AUDITORIA_DIMENSIONAMENTO_LIGACOES.md: 66,28 kN.m)
        QVERIFY(std::abs(r.momentResistanceEnKnm - 83.9668241333) < 1e-6);   // idem, 83,97 kN.m
    }
};

QTEST_MAIN(TestMomentRotationRegression)
#include "TestMomentRotationRegression.moc"
