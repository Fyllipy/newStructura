#include "design/jointengine/Materials.h"

namespace Structura::Design::JointEngine {

std::string materialKindName(MaterialKind kind)
{
    switch (kind) {
    case MaterialKind::Steel: return "Steel";
    case MaterialKind::Bolt: return "Bolt";
    case MaterialKind::Weld: return "Weld";
    case MaterialKind::Concrete: return "Concrete";
    case MaterialKind::Grout: return "Grout";
    case MaterialKind::Other: return "Other";
    }
    return "Unknown";
}

const Quantity *MaterialSpec::find(const std::string &key) const
{
    const auto it = properties.find(key);
    if (it == properties.end() || !it->second.provided()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> MaterialSpec::missing(const std::vector<std::string> &required) const
{
    std::vector<std::string> out;
    for (const std::string &key : required) {
        if (find(key) == nullptr) {
            out.push_back(key);
        }
    }
    return out;
}

} // namespace Structura::Design::JointEngine
