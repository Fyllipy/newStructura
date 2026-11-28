#include "app/services/TypologyCapabilityService.h"

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "app/services/JointTopologyAdapter.h"

#include <QCoreApplication>

namespace Structura::App::Services {

namespace JE = Structura::Design::JointEngine;
using NC = Structura::App::ProjectData::NodeConnection;

namespace {

QString legacyGroup(int type)
{
    switch (type) {
    case NC::kShearTab:
    case NC::kDoubleAngle: return QCoreApplication::translate("TypologyCapabilityService", "Viga–apoio");
    case NC::kGusset: return QCoreApplication::translate("TypologyCapabilityService", "Treliça e contraventamento");
    case NC::kTubular: return QCoreApplication::translate("TypologyCapabilityService", "Treliça tubular");
    case NC::kEndPlate: return QCoreApplication::translate("TypologyCapabilityService", "Viga–pilar (rígida)");
    case NC::kSplice: return QCoreApplication::translate("TypologyCapabilityService", "Continuidade de viga");
    case NC::kBasePlate: return QCoreApplication::translate("TypologyCapabilityService", "Fundação");
    case NC::kColdFormed: return QCoreApplication::translate("TypologyCapabilityService", "Perfis formados a frio");
    }
    return {};
}

TypologyRow build(int type, const QString &name, const QString &group, bool normative, const QMap<QString, double> &preset = {})
{
    TypologyRow r;
    r.type = type;
    r.name = name;
    r.group = group;
    r.configurable = true;   // aparece no configurador existente
    r.normativeDesign = normative;
    NC probe;
    probe.type = type;
    probe.values = preset;   // a variante do catalogo define o modo das mesas (soldadas x perfis T) no preset: sem ele a capacidade seria superestimada
    const JE::MechanicalCapability cap = JointTopologyAdapter::mechanicalCapability(probe);
    r.mechanical = cap.status;
    r.mechanicalModelId = cap.modelId;
    r.justification = cap.justification;
    r.missingFoundations = cap.missingFoundations;
    r.momentRotation = cap.status == JE::ValidityStatus::ValidWithCaveats && cap.modelId == "EnComponentMethod";
    return r;
}

} // namespace

std::vector<TypologyRow> TypologyCapabilityService::list()
{
    std::vector<TypologyRow> rows;
    // mesma lista do configurador (NodeConnectionsDialog::chooseType): tipos legados e depois o catálogo
    for (const int type : {NC::kShearTab, NC::kDoubleAngle, NC::kGusset, NC::kTubular, NC::kEndPlate, NC::kSplice, NC::kBasePlate, NC::kColdFormed}) {
        rows.push_back(build(type, ConnectionService::typeName(type), legacyGroup(type), true));   // verificações em ConnectionService
    }
    for (const auto &entry : ConnectionCatalog::entries()) {
        const bool normative = entry.legacyType >= 0 || static_cast<bool>(ConnectionCatalog::ops(entry.family).evaluate);
        rows.push_back(build(entry.id, entry.name, entry.group, normative, entry.preset));
    }
    return rows;
}

TypologyRow TypologyCapabilityService::row(int type)
{
    for (const TypologyRow &r : list()) {
        if (r.type == type) {
            return r;
        }
    }
    TypologyRow unknown;
    unknown.type = type;
    unknown.configurable = false;
    unknown.mechanical = JE::ValidityStatus::Unsupported;
    unknown.justification = "tipologia desconhecida";
    return unknown;
}

std::vector<DefinedConnection> TypologyCapabilityService::describeAtNode(const QVector<NC> &connections, int nodeId, const std::vector<int> &memberIdsAtNode, double beamYoungModulusMPa)
{
    std::vector<DefinedConnection> out;
    for (const NC &c : connections) {
        if (c.nodeId != nodeId) {
            continue;
        }
        DefinedConnection d;
        d.id = c.id;
        d.memberId = c.memberId;
        d.type = c.type;
        const TypologyRow r = row(c.type);
        d.typeName = r.name.isEmpty() ? ConnectionService::typeName(c.type) : r.name;
        const JE::JointTopology topology = JointTopologyAdapter::derive(c, memberIdsAtNode);
        d.topologyStatus = JE::topologyStatusName(topology.status);
        d.topologyGaps = static_cast<int>(topology.gaps.size());
        d.mechanical = r.mechanical;
        d.momentRotation = r.momentRotation;
        d.mechanicalModelId = r.mechanicalModelId;
        d.fingerprint = JointTopologyAdapter::configurationFingerprint(c, memberIdsAtNode, beamYoungModulusMPa).substr(0, 12);
        out.push_back(d);
    }
    return out;
}

} // namespace Structura::App::Services
