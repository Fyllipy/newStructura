#include "design/jointengine/Topology.h"

#include <algorithm>
#include <set>

namespace Structura::Design::JointEngine {

std::string partKindName(PartKind kind)
{
    switch (kind) {
    case PartKind::Member: return "Member";
    case PartKind::Plate: return "Plate";
    case PartKind::Angle: return "Angle";
    case PartKind::TStub: return "TStub";
    case PartKind::Stiffener: return "Stiffener";
    case PartKind::Diaphragm: return "Diaphragm";
    case PartKind::Gusset: return "Gusset";
    case PartKind::BasePlate: return "BasePlate";
    case PartKind::Bracket: return "Bracket";
    case PartKind::Tube: return "Tube";
    case PartKind::Concrete: return "Concrete";
    case PartKind::Grout: return "Grout";
    case PartKind::Anchor: return "Anchor";
    case PartKind::Auxiliary: return "Auxiliary";
    }
    return "Unknown";
}

std::string linkKindName(LinkKind kind)
{
    switch (kind) {
    case LinkKind::Bolted: return "Bolted";
    case LinkKind::Welded: return "Welded";
    case LinkKind::Contact: return "Contact";
    case LinkKind::Bearing: return "Bearing";
    case LinkKind::Pin: return "Pin";
    case LinkKind::Embedded: return "Embedded";
    case LinkKind::Rigid: return "Rigid";
    }
    return "Unknown";
}

std::string topologyStatusName(TopologyStatus status)
{
    switch (status) {
    case TopologyStatus::Complete: return "Complete";
    case TopologyStatus::Partial: return "Partial";
    case TopologyStatus::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

std::vector<std::string> validateTopology(const JointTopology &t)
{
    std::vector<std::string> issues;
    std::set<int> partIds;
    for (const PartRef &p : t.parts) {
        if (!partIds.insert(p.id).second) {
            issues.push_back("parte duplicada: " + std::to_string(p.id));
        }
    }
    std::set<int> linkIds;
    for (const Link &l : t.links) {
        if (!linkIds.insert(l.id).second) {
            issues.push_back("uniao duplicada: " + std::to_string(l.id));
        }
        if (l.partIds.size() < 2) {
            issues.push_back("uniao com menos de 2 partes: " + std::to_string(l.id));
        }
        std::set<int> uniq(l.partIds.begin(), l.partIds.end());
        if (uniq.size() != l.partIds.size()) {
            issues.push_back("uniao com parte repetida: " + std::to_string(l.id));
        }
        for (const int id : l.partIds) {
            if (!partIds.count(id)) {
                issues.push_back("uniao " + std::to_string(l.id) + " referencia parte inexistente " + std::to_string(id));
            }
        }
    }
    std::set<int> pathIds;
    for (const LoadPath &lp : t.paths) {
        if (!pathIds.insert(lp.id).second) {
            issues.push_back("caminho duplicado: " + std::to_string(lp.id));
        }
        if (lp.basis.empty()) {
            issues.push_back("caminho de carga sem fundamento (basis): " + std::to_string(lp.id));
        }
        if (lp.linkIds.empty()) {
            issues.push_back("caminho de carga sem uniões: " + std::to_string(lp.id));
        }
        for (const int id : lp.linkIds) {
            if (!linkIds.count(id)) {
                issues.push_back("caminho " + std::to_string(lp.id) + " referencia uniao inexistente " + std::to_string(id));
            }
        }
    }
    switch (t.status) {
    case TopologyStatus::Complete:
        if (!t.gaps.empty()) {
            issues.push_back("Complete com lacunas declaradas");
        }
        if (t.parts.empty()) {
            issues.push_back("Complete sem partes");
        }
        break;
    case TopologyStatus::Partial:
        if (t.gaps.empty()) {
            issues.push_back("Partial sem lacunas declaradas");
        }
        break;
    case TopologyStatus::Unavailable:
        if (t.unavailableReason.empty()) {
            issues.push_back("Unavailable sem justificativa");
        }
        if (!t.parts.empty() || !t.links.empty() || !t.paths.empty()) {
            issues.push_back("Unavailable com conteudo");
        }
        break;
    }
    return issues;
}

} // namespace Structura::Design::JointEngine
