#include "design/jointengine/EnComponentMethodModel.h"

namespace Structura::Design::JointEngine {

namespace JC = Structura::Design::JointComponents;

const ModelDescriptor &EnComponentMethodModel::descriptor()
{
    static const ModelDescriptor d = [] {
        ModelDescriptor m;
        m.id = kId;
        m.version = kVersion;
        m.formulation = "Metodo dos componentes: rigidez ki (6.3.2), resistencia por componente e por fileira (6.2.4, 6.2.6, 6.2.7), curva M-theta (6.3.1, eq. 6.28)";
        m.references = {"EN 1993-1-8:2005, secoes 6.2, 6.3 e Tabela 6.8 (psi = 2,7)", "implementacao existente: src/design/JointComponents.{h,cpp} (nao alterada)"};
        m.supportedDofs.dofs = {{"M", DofKind::Moment, "z", "kN.m", "rad"}};
        m.parameters = {{"ModelInput", "-", {}, true, "estrutura JointComponents::ModelInput (geometria, acos, parafusos); origem dos valores = responsabilidade do chamador"}};
        m.domain = {{"tipologia", "mesas soldadas ao pilar ou chapa de extremidade estendida com fileiras tracionadas"},
                    {"esforcos", "V e N pequenos (N <= 5% Npl,Rd, 6.2.7.1(2))"},
                    {"curva", "M <= Mj,Rd"}};
        m.hypotheses = {"solda das mesas rigida (penetracao total quando flangeThroat = 0)", "psi = 2,7 (Tabela 6.8)"};
        m.limitations = {"capacidade de rotacao nao calculada: a curva termina em Mj,Rd", "sentido inverso da chapa de extremidade nao calculado",
                         "so duas tipologias; sem comportamento nao linear de componentes individuais"};
        m.resultKind = ResultKind::NormativeResistance;
        return m;
    }();
    return d;
}

MechanicalResponse EnComponentMethodModel::respond(const JC::ModelInput &input, int curvePoints)
{
    MechanicalResponse r;
    r.kind = ResponseKind::MomentRotation;
    r.dofs = descriptor().supportedDofs;
    r.provenance.modelId = kId;
    r.provenance.modelVersion = kVersion;
    r.provenance.standard = "EN 1993-1-8";
    r.provenance.edition = "2005";
    r.provenance.solverStrategy = "S1 (metodo dos componentes, forma fechada)";
    r.provenance.assumptions = descriptor().hypotheses;
    r.provenance.limitations = descriptor().limitations;
    r.hypotheses = descriptor().hypotheses;
    r.limitations = descriptor().limitations;

    if (curvePoints < 2) {
        r.status = ValidityStatus::InsufficientData;
        r.messages.push_back({Severity::Error, "CURVE_POINTS", "numero de pontos da curva deve ser informado (>= 2)"});
        return r;
    }
    const JC::StiffnessResult stiffness = JC::computeStiffness(input);
    if (!stiffness.ok) {
        r.status = ValidityStatus::InsufficientData;
        r.messages.push_back({Severity::Error, "STIFFNESS_NOT_COMPUTABLE", stiffness.error});
        return r;
    }
    const JC::Resistance resistance = JC::computeResistance(input);
    if (!resistance.ok || !(resistance.momentResistanceKnm() > 0.0)) {
        r.status = ValidityStatus::InsufficientData;
        r.messages.push_back({Severity::Error, "RESISTANCE_NOT_COMPUTABLE", resistance.error.empty() ? std::string("Mj,Rd nao determinado") : resistance.error});
        return r;
    }
    const double sIni = stiffness.initialStiffnessKnm();
    const double mjRd = resistance.momentResistanceKnm();
    const JC::CurveMetrics metrics = JC::curveMetrics(sIni, mjRd, JC::kPsiWeldedAndEndPlate);
    const auto curve = JC::momentRotationCurve(sIni, mjRd, JC::kPsiWeldedAndEndPlate, curvePoints);

    r.status = ValidityStatus::ValidWithCaveats;   // sempre com ressalvas: dominio e limitacoes acima
    Curve c;
    c.actionDof = "M";
    c.actionUnit = "kN.m";
    c.dispUnit = "rad";
    for (const auto &p : curve) {
        c.points.emplace_back(p.theta, p.moment);
    }
    r.curves.push_back(std::move(c));
    r.initialStiffness = Stiffness{sIni, "kN.m/rad"};
    r.secantStiffness = Stiffness{metrics.secantAtResistance, "kN.m/rad"};
    r.tangentStiffness = Stiffness{metrics.tangentAtResistance, "kN.m/rad"};
    r.resistances.push_back({"Mj,Rd", mjRd, "kN.m", ResultKind::NormativeResistance, true});
    r.governingMechanism = resistance.capComponent;
    r.provenance.parameters["psi"] = Quantity::make(JC::kPsiWeldedAndEndPlate, "-", Origin::Normative, "EN 1993-1-8:2005 Tabela 6.8");
    r.provenance.parameters["E"] = Quantity::make(input.youngModulus, "MPa", Origin::UserProvided, "ModelInput.youngModulus");
    r.convergence.applicable = false;
    return r;
}

} // namespace Structura::Design::JointEngine
