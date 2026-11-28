#include "design/jointengine/Types.h"

namespace Structura::Design::JointEngine {

std::string originName(Origin origin)
{
    switch (origin) {
    case Origin::Normative: return "Normative";
    case Origin::ManufacturerDeclared: return "ManufacturerDeclared";
    case Origin::Test: return "Test";
    case Origin::Correlation: return "Correlation";
    case Origin::ModelAssumption: return "ModelAssumption";
    case Origin::UserProvided: return "UserProvided";
    case Origin::Derived: return "Derived";
    }
    return "Unknown";
}

std::string statusName(ValidityStatus status)
{
    switch (status) {
    case ValidityStatus::Valid: return "Valid";
    case ValidityStatus::ValidWithCaveats: return "ValidWithCaveats";
    case ValidityStatus::OutOfDomain: return "OutOfDomain";
    case ValidityStatus::InsufficientData: return "InsufficientData";
    case ValidityStatus::InvalidGeometry: return "InvalidGeometry";
    case ValidityStatus::MissingMaterialProperties: return "MissingMaterialProperties";
    case ValidityStatus::Unsupported: return "Unsupported";
    case ValidityStatus::NotYetAvailable: return "NotYetAvailable";
    }
    return "Unknown";
}

std::string resultKindName(ResultKind kind)
{
    switch (kind) {
    case ResultKind::NormativeResistance: return "NormativeResistance";
    case ResultKind::ModelUltimate: return "ModelUltimate";
    case ResultKind::ModelEstimate: return "ModelEstimate";
    case ResultKind::Reference: return "Reference";
    }
    return "Unknown";
}

} // namespace Structura::Design::JointEngine
