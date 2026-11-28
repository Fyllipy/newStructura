#include "design/jointengine/NbrClassifier.h"

#include "design/jointengine/EnComponentMethodModel.h"

namespace Structura::Design::JointEngine {

namespace JS = Structura::Design::JointClass;

JS::JointAnalysis NbrClassifier::analyze(const MechanicalResponse &response) const
{
    JS::JointBehavior behavior;
    behavior.origin = response.provenance.modelId == EnComponentMethodModel::kId ? JS::DataOrigin::ComponentMethod : JS::DataOrigin::None;
    if (allowsNumbers(response.status) && response.kind == ResponseKind::MomentRotation && !response.curves.empty()) {
        for (const auto &p : response.curves.front().points) {
            behavior.positive.curve.push_back({p.first, p.second});
        }
        for (const ResistanceItem &item : response.resistances) {
            if (item.name == "Mj,Rd") {
                behavior.positive.resistantMoment = item.value;
            }
        }
    }
    return JS::analyzeJoint(behavior, m_limits, m_storey, m_kind);
}

ClassificationResult NbrClassifier::classify(const MechanicalResponse &response, const ClassificationContext &) const
{
    const JS::JointAnalysis analysis = analyze(response);
    ClassificationResult out;
    out.standard = "NBR 8800";
    out.edition = "2024";
    out.criteria = {"6.1.2 (rigidez rotacional)"};
    const JS::DirectionAnalysis *primary = analysis.primary();
    if (primary == nullptr || !primary->hasData || !primary->classification.computed || !primary->classification.applicable) {
        out.verdict = Verdict::NaoVerificavel;
        out.justification = primary == nullptr || !primary->hasData ? "sem dados de comportamento M-theta" : primary->classification.justification;
        return out;
    }
    out.classLabel = JS::className(primary->classification.final);
    out.justification = primary->classification.justification;
    out.verdict = primary->classification.storeyPending || primary->classification.final == JS::StiffnessClass::NotClassified ? Verdict::Indeterminado : Verdict::Atende;
    return out;
}

} // namespace Structura::Design::JointEngine
