#include "app/services/NodeBehaviorService.h"

#include "app/services/JointAnalysisService.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace JE = Structura::Design::JointEngine;
namespace JS = Structura::Design::JointClass;
using SB = Structura::App::SolverBridge;

namespace {

JE::NodeMemberRole roleOf(JS::BarRole role)
{
    switch (role) {
    case JS::BarRole::Beam: return JE::NodeMemberRole::Beam;
    case JS::BarRole::Column: return JE::NodeMemberRole::Column;
    case JS::BarRole::Diagonal: return JE::NodeMemberRole::Diagonal;
    }
    return JE::NodeMemberRole::Unclassified;
}

JE::RotationalCondition conditionOf(const JointAnalysisService::Bar &bar)
{
    if (bar.springKnm) {
        return JE::RotationalCondition::Spring;
    }
    if (bar.rotationalRelease) {
        return JE::RotationalCondition::Released;
    }
    return JE::RotationalCondition::ContinuousImplicit;
}

} // namespace

JE::NodeFacts NodeBehaviorService::collectFacts(const SB::ModelData &model, int nodeId, double forceToKn, const Options &options, std::string *error)
{
    JE::NodeFacts facts;
    facts.nodeId = nodeId;
    facts.standard = options.standard;
    facts.edition = options.edition;
    facts.standardOrigin = options.standardOrigin;
    facts.structuralSystem = options.structuralSystem;
    facts.bracing = options.bracing;
    auto fail = [&](const std::string &message) {
        if (error) {
            *error = message;
        }
        return facts;
    };
    const SB::NodeData *node = nullptr;
    for (const auto &n : model.nodes) {
        if (n.id == nodeId) {
            node = &n;
        }
    }
    if (!node) {
        return fail("Nó " + std::to_string(nodeId) + " não encontrado no modelo.");
    }
    facts.position = {node->x, node->y, node->z};
    facts.restraints = node->restraints;
    std::vector<int> ids;
    for (const auto &m : model.members) {
        if (m.nodeI == nodeId || m.nodeJ == nodeId) {
            ids.push_back(m.id);
        }
    }
    if (ids.size() < 2) {
        return fail("O nó " + std::to_string(nodeId) + " tem menos de duas barras (" + std::to_string(ids.size()) + "): não há ligação entre barras a orientar.");
    }
    const auto base = JointAnalysisService::analyze(model, nodeId, ids[0], ids[1], forceToKn);
    if (!base.valid) {
        return fail(base.error);
    }
    const auto label = [&](int id) {
        const auto it = options.memberLabels.find(id);
        return it == options.memberLabels.end() ? std::string() : it->second;
    };
    for (const auto &bar : base.bars) {
        JE::NodeMemberFacts m;
        m.memberId = bar.memberId;
        m.role = roleOf(bar.role);
        m.label = label(bar.memberId);
        m.lengthM = bar.length;
        m.inertiaM4 = bar.inertia > 0.0 ? bar.inertia : JE::kNaN;
        m.youngKnM2 = bar.youngModulus > 0.0 ? bar.youngModulus : JE::kNaN;
        m.condition = conditionOf(bar);
        m.springKnm = bar.springKnm ? *bar.springKnm : JE::kNaN;
        m.hasEccentricity = bar.hasEccentricity;
        m.direction = bar.direction;
        facts.members.push_back(m);
    }
    facts.modelNotes.push_back("I e a condição de rotação de cada barra, na tabela de barras, referem-se ao plano do par " + std::to_string(ids[0]) + "–" + std::to_string(ids[1]) +
                               "; cada ligação viga–pilar usa o seu próprio plano.");
    // um item por par; so os pares viga-pilar precisam de limites e andar
    for (const auto &pair : base.pairs) {
        JE::PairFacts p;
        p.kind = pair.kind;
        p.beamId = pair.memberA;
        p.otherId = pair.memberB;
        if (pair.kind == JS::JointKind::BeamColumn) {
            const auto roleA = std::find_if(base.bars.begin(), base.bars.end(), [&](const auto &b) { return b.memberId == pair.memberA; });
            if (roleA != base.bars.end() && roleA->role != JS::BarRole::Beam) {
                std::swap(p.beamId, p.otherId);   // a viga e a primeira
            }
            const auto ctx = JointAnalysisService::analyze(model, nodeId, p.beamId, p.otherId, forceToKn);
            if (ctx.valid && ctx.referenceBar >= 0) {
                const auto &beam = ctx.bars[static_cast<std::size_t>(ctx.referenceBar)];
                p.beamCondition = conditionOf(beam);
                p.beamSpringKnm = beam.springKnm ? *beam.springKnm : JE::kNaN;
                p.limitsAvailable = ctx.beamInertia > 0.0 && ctx.beamLength > 0.0 && ctx.youngModulus > 0.0;
                p.limits = ctx.limits;
                p.storey = ctx.storey;
                p.notes = ctx.notes;
            } else {
                p.notes.push_back("Extração do par falhou: " + ctx.error);
            }
        }
        facts.pairs.push_back(p);
    }
    return facts;
}

JE::RequiredBehaviorAssessment NodeBehaviorService::assess(const SB::ModelData &model, int nodeId, double forceToKn, const Options &options)
{
    std::string error;
    const JE::NodeFacts facts = collectFacts(model, nodeId, forceToKn, options, &error);
    if (!error.empty()) {
        JE::RequiredBehaviorAssessment out;
        out.nodeId = nodeId;
        out.status = JE::ValidityStatus::InsufficientData;
        out.standard = options.standard;
        out.edition = options.edition;
        out.nodeGaps.push_back(error);
        return out;
    }
    return JE::assessRequiredBehavior(facts);
}

} // namespace Structura::App::Services
