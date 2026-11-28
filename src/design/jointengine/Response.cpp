#include "design/jointengine/Response.h"

namespace Structura::Design::JointEngine {

std::string responseKindName(ResponseKind kind)
{
    switch (kind) {
    case ResponseKind::MomentRotation: return "MomentRotation";
    case ResponseKind::ForceDisplacement: return "ForceDisplacement";
    case ResponseKind::GeneralizedVector: return "GeneralizedVector";
    case ResponseKind::ResistanceOnly: return "ResistanceOnly";
    case ResponseKind::StiffnessOnly: return "StiffnessOnly";
    }
    return "Unknown";
}

std::string verdictName(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Atende: return "Atende";
    case Verdict::NaoAtende: return "NaoAtende";
    case Verdict::Indeterminado: return "Indeterminado";
    case Verdict::NaoVerificavel: return "NaoVerificavel";
    }
    return "Unknown";
}

} // namespace Structura::Design::JointEngine
