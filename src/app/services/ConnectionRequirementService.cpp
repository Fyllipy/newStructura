#include "app/services/ConnectionRequirementService.h"

#include "app/services/ConnectionCatalog.h"

namespace Structura::App::Services {

namespace JE = Structura::Design::JointEngine;

namespace {

/// Tipologias que unem DUAS VIGAS (emendas): o par avaliado e viga-viga, para o qual o criterio de 6.1.2 nao se aplica.
bool isBeamBeamTypology(const Structura::App::ProjectData::NodeConnection &c)
{
    using Family = Structura::App::Services::ConnectionCatalog::Family;
    if (c.type == Structura::App::ProjectData::NodeConnection::kSplice) {
        return true;
    }
    if (Structura::App::Services::ConnectionCatalog::isCatalog(c.type)) {
        if (const auto *entry = Structura::App::Services::ConnectionCatalog::find(c.type)) {
            return entry->legacyType == Structura::App::ProjectData::NodeConnection::kSplice
                   || (entry->legacyType < 0 && (entry->family == Family::EndPlateSplice || entry->family == Family::CoverSplice || entry->family == Family::TubeSplice)
                       && entry->preset.value(QStringLiteral("outside"), 0.0) == 0.0);
        }
    }
    return false;
}

} // namespace

JE::ConnectionRequirementComparison ConnectionRequirementService::evaluate(const Structura::App::ProjectData::NodeConnection &connection, const JE::RequiredBehaviorAssessment &assessment,
                                                                           const ConnectionMechanicalResult &mechanical)
{
    const int beam = connection.memberId;
    const int support = static_cast<int>(connection.values.value(QStringLiteral("supMember"), 0.0) + 0.5);
    std::vector<const JE::RequiredBehaviorItem *> candidates;
    const bool beamBeam = isBeamBeamTypology(connection);
    for (const JE::RequiredBehaviorItem &item : assessment.items) {
        if ((item.beamId == beam || item.otherId == beam) && !(beamBeam && item.kind == Structura::Design::JointClass::JointKind::BeamColumn)) {
            candidates.push_back(&item);
        }
    }
    auto indeterminate = [&](const std::string &reason) {
        JE::ConnectionRequirementComparison out;
        out.beamId = beam;
        out.outcome = JE::ComparisonOutcome::Indeterminate;
        out.status = JE::ValidityStatus::InsufficientData;
        out.summary = "Indeterminado: " + reason;
        out.reasons.push_back(out.summary);
        out.gaps.push_back(reason);
        return out;
    };
    if (candidates.empty()) {
        return indeterminate("a viga " + std::to_string(beam) + " não forma par avaliado neste nó (comportamento requerido indisponível).");
    }
    const JE::RequiredBehaviorItem *chosen = nullptr;
    std::string note;
    if (beamBeam) {
        // emenda viga-viga: qualquer par viga-viga da viga tem o mesmo resultado (criterio de 6.1.2 nao aplicavel); nao ha ambiguidade relevante
        chosen = candidates.front();
        note = "Ligação viga–viga (emenda): o critério de rigidez de 6.1.2 vale somente para ligações viga–pilar.";
    } else if (support > 0) {
        for (const auto *c : candidates) {
            if (c->otherId == support || c->beamId == support) {
                chosen = c;
            }
        }
        if (chosen == nullptr) {
            return indeterminate("a barra de apoio vinculada (" + std::to_string(support) + ") não corresponde a nenhum par avaliado da viga " + std::to_string(beam) + ".");
        }
    } else if (candidates.size() == 1) {
        chosen = candidates.front();
        note = "Ligação sem barra de apoio vinculada: adotado o único par avaliado da viga " + std::to_string(beam) + ".";
    } else {
        return indeterminate("a viga " + std::to_string(beam) + " tem mais de um par no nó e a ligação não informa a barra de apoio: o par não pode ser escolhido sem ambiguidade.");
    }
    const bool hasResponse = mechanical.available || mechanical.partial || mechanical.resistanceOnly;
    JE::ConnectionRequirementComparison out = JE::compareRealWithRequired(*chosen, hasResponse ? &mechanical.response : nullptr, mechanical.message, mechanical.directionLabel);
    if (!note.empty()) {
        out.hypotheses.push_back(note);
    }
    if (mechanical.nbrBelowCurve) {
        out.caveats.push_back("A ligação cadastrada NÃO sustenta o Mj,Rd da curva (NBR: " + std::to_string(mechanical.nbrResistanceKnm) + " kN·m): a rigidez classificada pressupõe esse momento resistente.");
    }
    return out;
}

JE::DirectionalComparison ConnectionRequirementService::evaluateBoth(const Structura::App::ProjectData::NodeConnection &connection, const JE::RequiredBehaviorAssessment &assessment,
                                                                    const ConnectionMechanicalBoth &mechanical)
{
    return JE::combineDirections(evaluate(connection, assessment, mechanical.positive), evaluate(connection, assessment, mechanical.negative));
}

} // namespace Structura::App::Services
