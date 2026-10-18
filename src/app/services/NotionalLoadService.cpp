#include "NotionalLoadService.h"

#include "ModalService.h"

namespace Structura::App::Services {

QString NotionalLoadService::key(int axis, const QString &caseName)
{
    return QStringLiteral("NOC:%1:%2").arg(axis == 0 ? QLatin1Char('X') : QLatin1Char('Y')).arg(caseName);
}

bool NotionalLoadService::isNotionalKey(const QString &key)
{
    return key.startsWith(QLatin1String("NOC:"));
}

SolverBridge::ModelData NotionalLoadService::notionalModel(const SolverBridge::ModelData &caseModel, int axis, double factor)
{
    SolverBridge::ModelData model = caseModel;
    const auto masses = ModalService::massesFromLoads(caseModel);   // massa = carga descendente / g
    model.nodalLoads.clear();
    model.memberLoads.clear();
    model.memberMoments.clear();
    model.shellLoads.clear();
    for (const auto &[nodeId, mass] : masses) {
        SolverBridge::NodalLoadData load;
        load.nodeId = nodeId;
        load.values[static_cast<std::size_t>(axis == 0 ? 0 : 1)] = factor * mass * ModalService::kGravity;
        model.nodalLoads.push_back(load);
    }
    return model;
}

QVector<ProjectData::LoadCombination> NotionalLoadService::withVariants(const QVector<ProjectData::LoadCombination> &combinations)
{
    QVector<ProjectData::LoadCombination> out = combinations;
    for (const auto &combination : combinations) {
        if (combination.name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive)) {
            continue;
        }
        for (int axis = 0; axis < 2; ++axis) {
            for (const double sign : {1.0, -1.0}) {
                ProjectData::LoadCombination variant = combination;
                variant.name = QStringLiteral("%1 %2N%3").arg(combination.name, sign > 0 ? QStringLiteral("+") : QStringLiteral("-"), axis == 0 ? QStringLiteral("X") : QStringLiteral("Y"));
                for (const auto &term : combination.factors) {
                    if (term.second != 0.0) {
                        variant.factors.append({key(axis, term.first), sign * term.second});
                    }
                }
                out.append(variant);
            }
        }
    }
    return out;
}

} // namespace Structura::App::Services
