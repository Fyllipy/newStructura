// SONDA TEMPORARIA (auditoria pos-A2): despeja, para casos representativos, tudo o que a ferramenta momento-rotacao produz a partir de uma ligacao.
// Uso: ProbeMR <arquivo_saida>   (a saida e comparada byte a byte entre as variantes antes / depois / sem filtro)
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "design/JointClassification.h"
#include "design/JointComponents.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace Structura::App::Services;
using Structura::App::ProjectData::NodeConnection;
namespace JC = Structura::Design::JointComponents;
namespace JS = Structura::Design::JointClass;

static ConnectionService::BeamInfo beamOf(const char *name, double d, double bf, double tw, double tf)
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

static void setColumn(NodeConnection &c, double d, double bf, double tf, double tw, double r)
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

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "probe_mr.txt";
    std::FILE *out = std::fopen(path, "w");
    if (!out) {
        return 2;
    }
    const auto ipe300 = beamOf("IPE300", 300.0, 150.0, 7.1, 10.7);
    const auto ipe400 = beamOf("IPE400", 400.0, 180.0, 8.6, 13.5);
    auto make = [&](const ConnectionService::BeamInfo &b, int type = NodeConnection::kEndPlate) {
        NodeConnection c = ConnectionService::makeDefault(type, b, 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;
        return c;
    };
    std::vector<Case> cases;
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        cases.push_back({"C01 IPE300 / col 260x146x12.7 / padrao (2 fileiras)", c});
        c.values["tRows"] = 1;
        cases.push_back({"C02 idem, 1 fileira", c});
        c.values["tRows"] = 3;
        cases.push_back({"C03 idem, 3 fileiras", c});
    }
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 200, 200, 15, 9, 18);
        c.values["db"] = 24;
        c.values["plateT"] = 25;
        c.values["tRows"] = 1;
        cases.push_back({"C04 IPE300 / HEB200 / M24 / chapa 25 / 1 fileira", c});
        c.values["tRows"] = 2;
        cases.push_back({"C05 idem, 2 fileiras", c});
    }
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 200, 200, 8, 6, 10);   // mesa esbelta: a camada A reprovaria
        c.values["tRows"] = 1;
        cases.push_back({"C06 mesa do pilar esbelta tf=8 / 1 fileira (camada A reprova)", c});
        c.values["tRows"] = 2;
        cases.push_back({"C07 mesa do pilar esbelta tf=8 / 2 fileiras", c});
    }
    {
        NodeConnection c = make(ipe400);
        setColumn(c, 300, 300, 19, 11, 27);
        c.values["db"] = 24;
        c.values["plateT"] = 22;
        c.values["plateW"] = 190;
        c.values["gauge"] = 110;
        c.values["tRows"] = 2;
        cases.push_back({"C08 IPE400 / HEB300 / M24 / chapa 22 / 2 fileiras", c});
        c.values["tRows"] = 1;
        cases.push_back({"C09 idem, 1 fileira", c});
        c.values["bothSides"] = 1;
        cases.push_back({"C10 idem, viga dos dois lados", c});
        c.values["bothSides"] = 0;
        c.values["cont"] = 1;
        c.values["doubler"] = 8;
        cases.push_back({"C11 idem, enrijecedores de continuidade + reforco de alma", c});
    }
    {
        NodeConnection c = make(ipe300);
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        c.values["plateT"] = 12;
        c.values["db"] = 16;
        c.values["tRows"] = 1;
        cases.push_back({"C12 chapa fina 12 / M16 / 1 fileira", c});
    }
    {
        NodeConnection c = make(ipe300, 144);
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        cases.push_back({"C13 controle: mesas soldadas (catalogo 144), sem alavanca A2", c});
    }
    {
        // sem geometria do pilar confirmada: estado A1 (nao verificavel)
        NodeConnection c = make(ipe300);
        cases.push_back({"C14 pilar NAO informado (A1): Nao verificavel", c});
    }

    const JS::Limits limits = JS::limitsFor(2.0e8, 2.0e-4, 6.0);
    JS::StoreyStiffness storey;
    storey.status = JS::StoreyStatus::Ok;
    storey.kv = 0.3;
    storey.kp = 1.0;
    storey.ratio = 0.3;
    storey.beams = storey.columns = 1;

    for (const Case &k : cases) {
        const auto r = JointComponentService::evaluate(k.c, 200000.0);
        std::fprintf(out, "=== %s\n", k.label.c_str());
        std::fprintf(out, "supported=%d message=[%s]\n", r.supported ? 1 : 0, r.message.c_str());
        std::fprintf(out, "fingerprint=%s\n", r.fingerprint.c_str());
        std::fprintf(out, "Sj_ini=%.12g MjRd_used=%.12g MjRd_EN=%.12g MjRd_NBR=%.12g nbrAvail=%d nbrBelowCurve=%d\n", r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm,
                     r.momentResistanceEnKnm, r.momentResistanceNbrKnm, r.nbrCheckAvailable ? 1 : 0, r.nbrBelowCurve ? 1 : 0);
        std::fprintf(out, "metrics: onset=%.12g thOnset=%.12g thRes=%.12g secRes=%.12g tanRes=%.12g M_half=%.12g th_half=%.12g\n", r.metrics.onsetOfNonlinearity,
                     r.metrics.rotationAtOnset, r.metrics.rotationAtResistance, r.metrics.secantAtResistance, r.metrics.tangentAtResistance, r.metrics.momentAtHalfStiffness,
                     r.metrics.rotationAtHalfStiffness);
        std::fprintf(out, "curve(%zu pts):", r.behavior.positive.curve.size());
        for (const auto &p : r.behavior.positive.curve) {
            std::fprintf(out, " (%.10g,%.10g)", p.theta, p.moment);
        }
        std::fprintf(out, "\n");
        if (r.supported && r.stiffness.ok && r.momentResistanceKnm > 0.0) {
            const auto cl = JS::classify(r.stiffness.initialStiffnessKnm(), limits, storey, JS::JointKind::BeamColumn);
            std::fprintf(out, "classe: byStiffness=%d final=%d eta=%.12g\n", static_cast<int>(cl.byStiffness), static_cast<int>(cl.final), cl.eta);
            const auto eq = JC::deriveEquivalent(JC::EquivalentModel::SemiRigidBilinear, r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm);
            std::fprintf(out, "equivalente bilinear: valid=%d k=%.12g My=%.12g h=%.12g\n", eq.valid ? 1 : 0, eq.stiffness, eq.yieldMoment, eq.hardeningRatio);
            for (double f : {0.25, 0.5, 2.0 / 3.0, 0.8, 1.0}) {
                const double m = f * r.momentResistanceKnm;
                std::fprintf(out, "  M=%.4f sec=%.10g tan=%.10g\n", m, JC::secantStiffnessAt(m, r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm),
                             JC::tangentStiffnessAt(m, r.stiffness.initialStiffnessKnm(), r.momentResistanceKnm));
            }
        }
        std::fprintf(out, "memoria (%zu linhas):\n", r.memory.size());
        for (const auto &l : r.memory) {
            std::fprintf(out, "| %s\n", l.c_str());
        }
    }
    std::fclose(out);
    return 0;
}
