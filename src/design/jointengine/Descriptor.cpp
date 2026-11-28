#include "design/jointengine/Descriptor.h"

#include <algorithm>
#include <set>

namespace Structura::Design::JointEngine {

std::vector<std::string> validateDescriptor(const ModelDescriptor &d)
{
    std::vector<std::string> issues;
    if (d.id.empty()) {
        issues.push_back("id vazio");
    }
    if (d.version.empty()) {
        issues.push_back("version vazia");
    }
    if (d.formulation.empty()) {
        issues.push_back("formulation vazia");
    }
    if (d.references.empty()) {
        issues.push_back("sem referencias");
    }
    if (d.supportedDofs.dofs.empty()) {
        issues.push_back("sem GDL suportados");
    }
    if (d.limitations.empty()) {
        issues.push_back("sem limitacoes declaradas");
    }
    std::set<std::string> seen;
    for (const Dof &dof : d.supportedDofs.dofs) {
        if (dof.id.empty() || dof.unit.empty() || dof.dispUnit.empty()) {
            issues.push_back("GDL sem id/unidade: " + dof.id);
        }
        if (!seen.insert(dof.id).second) {
            issues.push_back("GDL duplicado: " + dof.id);
        }
    }
    std::set<std::string> names;
    for (const ParameterSpec &p : d.parameters) {
        if (p.name.empty() || p.unit.empty()) {
            issues.push_back("parametro sem nome/unidade: " + p.name);
        }
        if (!names.insert(p.name).second) {
            issues.push_back("parametro duplicado: " + p.name);
        }
    }
    if (d.hasInternalState && !d.nonlinear) {
        issues.push_back("estado interno sem declarar comportamento nao linear");
    }
    return issues;
}

std::vector<Message> checkParameters(const ModelDescriptor &d, const ParameterSet &parameters)
{
    std::vector<Message> out;
    for (const ParameterSpec &spec : d.parameters) {
        const auto it = parameters.find(spec.name);
        if (it == parameters.end() || !std::isfinite(it->second.value)) {
            if (spec.required) {
                out.push_back({Severity::Error, "MISSING_PARAMETER", "parametro obrigatorio ausente: " + spec.name});
            }
            continue;
        }
        const Quantity &q = it->second;
        if (!q.origin.has_value()) {
            out.push_back({Severity::Error, "MISSING_ORIGIN", "parametro sem origem: " + spec.name});
            continue;
        }
        if (q.unit != spec.unit) {
            out.push_back({Severity::Error, "UNIT_MISMATCH", "unidade de " + spec.name + ": esperado " + spec.unit + ", recebido '" + q.unit + "'"});
        }
        if (!spec.allowedOrigins.empty() && std::find(spec.allowedOrigins.begin(), spec.allowedOrigins.end(), *q.origin) == spec.allowedOrigins.end()) {
            out.push_back({Severity::Error, "ORIGIN_NOT_ALLOWED", "origem nao admitida para " + spec.name + ": " + originName(*q.origin)});
        }
    }
    return out;
}

} // namespace Structura::Design::JointEngine
