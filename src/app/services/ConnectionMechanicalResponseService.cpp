#include "app/services/ConnectionMechanicalResponseService.h"

#include "app/services/JointComponentService.h"
#include "app/services/JointTopologyAdapter.h"
#include "design/JointComponents.h"
#include "design/jointengine/EnComponentMethodModel.h"
#include "design/jointengine/Fingerprint.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>

namespace Structura::App::Services {

namespace JE = Structura::Design::JointEngine;
namespace JC = Structura::Design::JointComponents;
namespace JS = Structura::Design::JointClass;

namespace {

std::string directionLabelOf(JS::Direction d)
{
    return d == JS::Direction::Positive ? "positivo (mesa superior tracionada)" : "negativo (mesa inferior tracionada)";
}

ConnectionMechanicalResult unavailable(JS::Direction direction, JE::ValidityStatus status, const std::string &message)
{
    ConnectionMechanicalResult r;
    r.direction = direction;
    r.directionLabel = directionLabelOf(direction);
    r.available = false;
    r.status = status;
    r.message = message;
    r.response.kind = JE::ResponseKind::MomentRotation;
    r.response.status = status;   // sem numeros: a invariante de MechanicalResponse proibe curvas/rigidez com este estado
    r.response.messages.push_back({JE::Severity::Warning, "RESPONSE_UNAVAILABLE", message});
    return r;
}

} // namespace

ConnectionMechanicalResult ConnectionMechanicalResponseService::evaluate(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, int curvePoints)
{
    return evaluate(connection, youngModulusMPa, curvePoints, JS::Direction::Positive);
}

ConnectionMechanicalResult ConnectionMechanicalResponseService::evaluate(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, int curvePoints, JS::Direction direction)
{
    if (!std::isfinite(youngModulusMPa) || youngModulusMPa <= 0.0) {
        return unavailable(direction, JE::ValidityStatus::InsufficientData, "Módulo de elasticidade do aço da viga ausente ou inválido: sem E não há resposta mecânica (nenhum valor padrão é assumido).");
    }
    if (curvePoints < 2) {
        return unavailable(direction, JE::ValidityStatus::InsufficientData, "Número de pontos da curva deve ser informado (>= 2).");
    }
    const JointComponentService::Result service = JointComponentService::evaluate(connection, youngModulusMPa, direction);
    if (!service.supported) {
        return unavailable(direction, JE::ValidityStatus::NotYetAvailable, service.message.empty() ? std::string("Tipologia sem modelo mecânico implementado.") : service.message);
    }

    if (service.resistanceOnly) {
        // INCREMENTO 6: so a resistencia (nivel C da NBR); nada de rigidez, curva ou componentes
        ConnectionMechanicalResult q;
        q.direction = direction;
        q.directionLabel = directionLabelOf(direction);
        q.symmetricGeometry = service.symmetricGeometry;
        q.resistanceOnly = true;
        q.family = service.family;
        q.message = service.message;
        q.memory = service.memory;
        q.status = JE::ValidityStatus::ValidWithCaveats;
        q.resistanceKnm = service.momentResistanceKnm;
        q.nbrResistanceKnm = service.momentResistanceNbrKnm;
        const std::string cfp0 = JointTopologyAdapter::configurationFingerprint(connection, {}, youngModulusMPa);
        JE::AnalysisData an;
        an.models = {{"NbrResistanceOnly", "1"}};
        an.strategy = "resistencia apenas (NBR 8800:2024, verificacoes da ligacao, nivel C)";
        an.parameters["E"] = JE::Quantity::make(youngModulusMPa, "MPa", JE::Origin::Derived, "material do aco da viga no modelo");
        if (direction == JS::Direction::Negative) {
            an.parameters["direction"] = JE::Quantity::make(-1.0, "-", JE::Origin::Derived, "sentido do momento: mesa inferior tracionada");
        }
        const std::string afp0 = JE::analysisFingerprint(cfp0, an);
        q.configurationFingerprint = cfp0.substr(0, 12);
        q.analysisFingerprint = afp0.substr(0, 12);
        q.hypotheses = {service.directionNote, "Mj,Rd = maior momento para o qual todas as verificações da NBR 8800:2024 da ligação (V = N = 0) ficam ≤ 1 (nível C, verificação construtiva); NÃO é o método dos componentes."};
        q.limitations = {"Rigidez inicial Sj,ini INDISPONÍVEL: o método dos componentes para perfis T parafusados não está implementado.",
                         "Curva M–θ INDISPONÍVEL e capacidade de rotação INDISPONÍVEL.",
                         "Lacunas de formulação: espalhamento da compressão do T-stub (a EN 1993-1-8 não dá regra para esta junta), k11/k12 do T com a mesa da viga e benchmark de junta completa."};
        JE::MechanicalResponse &rs = q.response;
        rs.kind = JE::ResponseKind::ResistanceOnly;
        rs.status = JE::ValidityStatus::ValidWithCaveats;
        rs.dofs = JE::EnComponentMethodModel::descriptor().supportedDofs;
        rs.provenance.modelId = "NbrResistanceOnly";
        rs.provenance.modelVersion = "1";
        rs.provenance.standard = "NBR 8800";
        rs.provenance.edition = "2024";
        rs.provenance.solverStrategy = an.strategy;
        rs.provenance.parameters = an.parameters;
        rs.provenance.configurationFingerprint = cfp0;
        rs.provenance.analysisFingerprint = afp0;
        rs.hypotheses = q.hypotheses;
        rs.limitations = q.limitations;
        rs.resistances.push_back({"Mj,Rd,NBR (verificações da NBR 8800:2024 da ligação, nível C)", service.momentResistanceNbrKnm, "kN.m", JE::ResultKind::NormativeResistance, true});
        rs.governingMechanism = service.nbrGoverning;
        q.participants = std::string(direction == JS::Direction::Positive ? "mesa superior" : "mesa inferior") + " tracionada: perfil T parafusado (só resistência)";
        return q;
    }
    ConnectionMechanicalResult r;
    r.direction = direction;
    r.directionLabel = directionLabelOf(direction);
    r.symmetricGeometry = service.symmetricGeometry;
    r.family = service.family;
    // o aviso legado "sentido inverso nao calculado" deixou de valer: cada sentido tem a sua resposta (a nota do sentido vai para as hipoteses)
    r.message = service.message.rfind("Sentido inverso", 0) == 0 ? std::string() : service.message;
    r.memory = service.memory;
    r.initialStiffnessKnm = service.stiffness.initialStiffnessKnm();
    r.leverArmMm = service.stiffness.leverArm;
    r.nbrResistanceKnm = service.momentResistanceNbrKnm;
    r.nbrBelowCurve = service.nbrBelowCurve;

    // proveniencia: o CFP vem do adaptador do JME (derivado da ligacao e do E); o AFP acrescenta modelo, versao e parametros
    const std::string cfp = JointTopologyAdapter::configurationFingerprint(connection, {}, youngModulusMPa);
    JE::AnalysisData analysis;
    analysis.models = {{JE::EnComponentMethodModel::kId, JE::EnComponentMethodModel::kVersion}};
    analysis.strategy = "S1 (metodo dos componentes, forma fechada)";
    analysis.parameters["psi"] = JE::Quantity::make(JC::kPsiWeldedAndEndPlate, "-", JE::Origin::Normative, "EN 1993-1-8:2005 Tabela 6.8");
    analysis.parameters["E"] = JE::Quantity::make(youngModulusMPa, "MPa", JE::Origin::Derived, "material do aco da viga no modelo");
    if (direction == JS::Direction::Negative) {   // o sentido positivo mantem o AFP do Incremento 3; o negativo e distinto por construcao
        analysis.parameters["direction"] = JE::Quantity::make(-1.0, "-", JE::Origin::Derived, "sentido do momento: mesa inferior tracionada");
    }
    const std::string afp = JE::analysisFingerprint(cfp, analysis);
    r.configurationFingerprint = cfp.substr(0, 12);
    r.analysisFingerprint = afp.substr(0, 12);

    JE::MechanicalResponse &resp = r.response;
    resp.dofs = JE::EnComponentMethodModel::descriptor().supportedDofs;
    resp.provenance.modelId = JE::EnComponentMethodModel::kId;
    resp.provenance.modelVersion = JE::EnComponentMethodModel::kVersion;
    resp.provenance.standard = "EN 1993-1-8";
    resp.provenance.edition = "2005";
    resp.provenance.solverStrategy = analysis.strategy;
    resp.provenance.parameters = analysis.parameters;
    resp.provenance.configurationFingerprint = cfp;
    resp.provenance.analysisFingerprint = afp;
    resp.convergence.applicable = false;   // forma fechada: nao ha iteracao
    r.hypotheses = JE::EnComponentMethodModel::descriptor().hypotheses;
    if (!service.directionNote.empty()) {
        r.hypotheses.push_back(service.directionNote);
    }
    for (const std::string &h : service.hypotheses) {
        r.hypotheses.push_back(h);
    }
    r.hypotheses.push_back("E = " + std::to_string(youngModulusMPa) + " MPa informado (aço da viga); Sj,ini = rigidez a 2/3 de Mj,Rd (EN 6.3.1; μ = 1 no trecho elástico).");
    for (const std::string &l : JE::EnComponentMethodModel::descriptor().limitations) {
        if (l.find("sentido inverso") == std::string::npos) {   // o sentido inverso agora e calculado a parte (evaluateBoth)
            r.limitations.push_back(l);
        }
    }
    if (direction == JS::Direction::Negative) {
        r.limitations.push_back("Sentido negativo: as fileiras abaixo da mesa inferior sao as da zona de cisalhamento do cadastro; a ligação não foi dimensionada para trabalhar à tração nessas fileiras (o resultado é do comportamento mecânico, não uma verificação).");
    }
    r.limitations.push_back("Capacidade de rotação INDISPONÍVEL: a EN 1993-1-8, 6.4 não está implementada; a curva termina em Mj,Rd.");
    r.limitations.push_back("Estado individual dos componentes ao longo da curva INDISPONÍVEL: a não linearidade da curva EN 6.3.1 é do nó, não de cada componente (só o estado do nó é informado).");
    r.limitations.push_back("Convergência: não aplicável (forma fechada, sem iteração).");
    resp.hypotheses = r.hypotheses;
    resp.limitations = r.limitations;

    // componentes (k_i, F,Rd) com a flexibilidade de cada fileira
    std::set<int> rowIds;
    for (const JC::Component &c : service.stiffness.components) {
        if (c.row >= 1) {
            rowIds.insert(c.row);
        }
    }
    for (const JC::Component &c : service.stiffness.components) {
        MechanicalComponentRow m;
        m.name = c.name;
        m.symbol = c.symbol;
        m.clause = c.clause;
        m.row = c.row;
        m.rigid = c.infinite || c.k <= 0.0;
        m.stiffnessMm = c.k;
        m.flexibilityPerMm = m.rigid ? 0.0 : 1.0 / c.k;
        m.resistanceKn = c.resistance >= 0.0 ? c.resistance / 1000.0 : -1.0;
        m.failureMode = c.failureMode;
        r.components.push_back(m);
    }
    // fileiras: componentes em SERIE dentro da fileira (keff,r = 1/soma(1/ki)); as fileiras atuam em PARALELO (keq = soma keff,r h_r / zeq);
    // componentes do no (k1, k2...) ficam em serie com keq (EN 1993-1-8, 6.3.1(4)): Sj,ini = E zeq^2 / (soma_no 1/k + 1/keq)
    for (const int row : rowIds) {
        double total = 0.0, maxFlex = 0.0;
        for (const auto &m : r.components) {
            if (m.row == row) {
                total += m.flexibilityPerMm;
                maxFlex = std::max(maxFlex, m.flexibilityPerMm);
            }
        }
        for (auto &m : r.components) {
            if (m.row == row && total > 0.0) {
                m.shareInRow = m.flexibilityPerMm / total;
                m.governingInRow = m.flexibilityPerMm == maxFlex && maxFlex > 0.0;
            }
        }
    }
    double sumKh = 0.0, sumKh2 = 0.0;
    for (const JC::RowResult &sr : service.stiffness.rows) {
        sumKh += sr.keff * sr.h;
        sumKh2 += sr.keff * sr.h * sr.h;
    }
    const double zeq = sumKh > 0.0 ? sumKh2 / sumKh : 0.0;
    const double keq = zeq > 0.0 ? sumKh / zeq : 0.0;
    double nodeFlex = 0.0;
    for (const auto &m : r.components) {
        if (m.row == 0) {
            nodeFlex += m.flexibilityPerMm;
        }
    }
    const double rowsFlex = keq > 0.0 ? 1.0 / keq : 0.0;
    const double totalFlex = nodeFlex + rowsFlex;
    if (totalFlex > 0.0) {
        r.rowsShareOfTotal = rowsFlex / totalFlex;
        double best = rowsFlex;
        r.governingContribution = "fileiras tracionadas (keq)";
        for (auto &m : r.components) {
            if (m.row == 0) {
                m.shareOfTotal = m.flexibilityPerMm / totalFlex;
                if (m.flexibilityPerMm > best) {
                    best = m.flexibilityPerMm;
                    r.governingContribution = m.symbol + " " + m.name;
                }
            }
        }
    }
    if (!service.parts.empty()) {
        // INCREMENTO 6: ligacao composta em serie. As fracoes sao de ROTACAO: f_X = Sj,ini / Sj,ini,X (soma 1). Componentes do no (parte soldada ao pilar): fracao da parte x
        // fracao do componente dentro da parte; fileiras da emenda: fracao da emenda.
        const double sTot = r.initialStiffnessKnm;
        const double fA = sTot / service.parts[0].stiffnessKnm, fB = sTot / service.parts[1].stiffnessKnm;
        double flexA = 0.0;
        for (const auto &m : r.components) {
            flexA += m.row == 0 ? m.flexibilityPerMm : 0.0;
        }
        r.rowsShareOfTotal = fB;
        r.governingContribution = "emenda (fileiras tracionadas)";
        double best = fB;
        for (auto &m : r.components) {
            if (m.row == 0 && flexA > 0.0) {
                m.shareOfTotal = fA * m.flexibilityPerMm / flexA;
                if (m.shareOfTotal > best) {
                    best = m.shareOfTotal;
                    r.governingContribution = m.symbol + " " + m.name;
                }
            }
        }
        r.hypotheses.push_back("Composição em série: " + service.parts[0].label + " (Sj,ini = " + std::to_string(service.parts[0].stiffnessKnm) + " kN·m/rad; Mj,Rd = " + std::to_string(service.parts[0].resistanceKnm) + " kN·m) + "
                               + service.parts[1].label + " (Sj,ini = " + std::to_string(service.parts[1].stiffnessKnm) + " kN·m/rad; Mj,Rd = " + std::to_string(service.parts[1].resistanceKnm) + " kN·m).");
        resp.hypotheses = r.hypotheses;
    }
    std::map<int, JC::RowResult> resistanceByRow;
    for (const JC::RowResult &rr : service.resistance.rows) {
        resistanceByRow[rr.row] = rr;
    }
    for (const JC::RowResult &sr : service.stiffness.rows) {
        MechanicalRowRow m;
        m.row = sr.row;
        m.leverMm = sr.h;
        m.keffMm = sr.keff;
        const auto it = resistanceByRow.find(sr.row);
        if (it != resistanceByRow.end()) {
            m.individualKn = it->second.individual >= 0.0 ? it->second.individual / 1000.0 : -1.0;
            m.effectiveKn = it->second.effective / 1000.0;
            m.limitedBy = it->second.limitedBy;
        }
        r.rows.push_back(m);
    }
    {
        const char *flange = direction == JS::Direction::Positive ? "mesa superior" : "mesa inferior";
        if (r.rows.empty()) {
            r.participants = std::string(flange) + " tracionada: mesa da viga soldada ao pilar (sem fileiras de parafusos)";
        } else {
            r.participants = std::string(flange) + " tracionada; " + std::to_string(r.rows.size()) + " fileira(s) tracionada(s):";
            for (const MechanicalRowRow &m : r.rows) {
                char buf[96];
                std::snprintf(buf, sizeof buf, " %d (h = %.1f mm)", m.row, m.leverMm);
                r.participants += buf;
            }
        }
    }

    const double sIni = r.initialStiffnessKnm;
    const double mjRd = service.momentResistanceKnm;
    const JS::DirectionBehavior &computedBehavior = direction == JS::Direction::Negative ? service.behavior.negative : service.behavior.positive;
    if (!(mjRd > 0.0) || !computedBehavior.resistantMoment) {
        // so rigidez: Mj,Rd nao determinado => sem curva (nao se inventa resistencia)
        r.available = false;
        r.partial = true;
        r.status = JE::ValidityStatus::ValidWithCaveats;
        resp.kind = JE::ResponseKind::StiffnessOnly;
        resp.status = JE::ValidityStatus::ValidWithCaveats;
        resp.initialStiffness = JE::Stiffness{sIni, "kN.m/rad"};
        resp.messages.push_back({JE::Severity::Warning, "RESISTANCE_NOT_DETERMINED", service.message.empty() ? std::string("Mj,Rd não determinado: sem curva M–θ.") : service.message});
        r.message = service.message.empty() ? "Mj,Rd não determinado: só a rigidez inicial está disponível." : service.message;
        return r;
    }
    r.available = true;
    r.status = JE::ValidityStatus::ValidWithCaveats;   // sempre com ressalvas: dominio e limitacoes
    r.resistanceKnm = mjRd;
    resp.kind = JE::ResponseKind::MomentRotation;
    resp.status = JE::ValidityStatus::ValidWithCaveats;
    JE::Curve curve;
    curve.actionDof = "M";
    curve.actionUnit = "kN.m";
    curve.dispUnit = "rad";
    for (const auto &p : JC::momentRotationCurve(sIni, mjRd, JC::kPsiWeldedAndEndPlate, curvePoints)) {
        curve.points.emplace_back(p.theta, p.moment);
    }
    resp.curves.push_back(std::move(curve));
    resp.initialStiffness = JE::Stiffness{sIni, "kN.m/rad"};
    resp.secantStiffness = JE::Stiffness{service.metrics.secantAtResistance, "kN.m/rad"};
    resp.tangentStiffness = JE::Stiffness{service.metrics.tangentAtResistance, "kN.m/rad"};
    resp.resistances.push_back({"Mj,Rd", mjRd, "kN.m", JE::ResultKind::NormativeResistance, true});
    if (service.nbrCheckAvailable) {
        resp.resistances.push_back({"Mj,Rd,NBR (verificações da NBR 8800 da ligação cadastrada)", service.momentResistanceNbrKnm, "kN.m", JE::ResultKind::NormativeResistance, false});
    }
    resp.governingMechanism = service.resistance.capComponent;
    if (service.nbrBelowCurve) {
        r.limitations.push_back("ATENÇÃO: a ligação cadastrada resiste, pela NBR, a menos que o Mj,Rd da curva: ela NÃO sustenta o momento da curva; redimensione antes de adotar a curva como representativa.");
        resp.limitations = r.limitations;
        resp.messages.push_back({JE::Severity::Warning, "NBR_BELOW_CURVE", "A ligação cadastrada não sustenta o Mj,Rd da curva (verificações da NBR)."});
    }
    for (const double ratio : {0.25, 0.5, 2.0 / 3.0, 0.85, 1.0}) {
        MechanicalLevel l;
        l.ratioToResistance = ratio;
        l.momentKnm = ratio * mjRd;
        l.secantKnmPerRad = JC::secantStiffnessAt(l.momentKnm, sIni, mjRd, JC::kPsiWeldedAndEndPlate);
        l.rotationRad = l.secantKnmPerRad > 0.0 ? l.momentKnm / l.secantKnmPerRad : 0.0;
        l.state = ratio <= 2.0 / 3.0 + 1e-12 ? "elástico (M ≤ 2/3 Mj,Rd)" : "não linear (EN 6.3.1: 2/3 Mj,Rd < M ≤ Mj,Rd)";
        r.levels.push_back(l);
    }
    return r;
}

} // namespace Structura::App::Services

namespace Structura::App::Services {

ConnectionMechanicalBoth ConnectionMechanicalResponseService::evaluateBoth(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, int curvePoints)
{
    ConnectionMechanicalBoth both;
    both.positive = evaluate(connection, youngModulusMPa, curvePoints, JS::Direction::Positive);
    both.negative = evaluate(connection, youngModulusMPa, curvePoints, JS::Direction::Negative);
    both.symmetryDemonstrated = (both.positive.available || both.positive.resistanceOnly) && (both.negative.available || both.negative.resistanceOnly) && both.positive.symmetricGeometry && both.negative.symmetricGeometry;
    if (both.symmetryDemonstrated) {
        both.note = "Simetria DEMONSTRADA pela configuração: a entrada física do sentido negativo é idêntica à do positivo (mesmas fileiras em cada lado / mesas soldadas), logo as respostas coincidem. Não é presunção: cada sentido foi calculado.";
        both.positive.hypotheses.push_back(both.note);
        both.negative.hypotheses.push_back(both.note);
        both.positive.response.hypotheses = both.positive.hypotheses;
        both.negative.response.hypotheses = both.negative.hypotheses;
    } else if ((both.positive.available || both.positive.resistanceOnly) && (both.negative.available || both.negative.resistanceOnly)) {
        both.note = "Sentidos ASSIMÉTRICOS: as fileiras/componentes de cada sentido diferem; as respostas são independentes e não se presume simetria.";
    } else if (both.positive.available || both.negative.available) {
        both.note = "Só um dos sentidos pôde ser calculado; o outro está INDISPONÍVEL (sem números).";
    }
    return both;
}

} // namespace Structura::App::Services
