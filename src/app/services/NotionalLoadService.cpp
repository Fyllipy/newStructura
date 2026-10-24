#include "NotionalLoadService.h"

#include "ModalService.h"

#include <algorithm>
#include <memory>

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

QVector<ProjectData::LoadCombination> NotionalLoadService::withGeometricVariants(const QVector<ProjectData::LoadCombination> &combinations)
{
    QVector<ProjectData::LoadCombination> out = combinations;
    for (const auto &combination : combinations) {
        if (combination.name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive)) {
            continue;
        }
        for (int axis = 0; axis < 2; ++axis) {
            for (const int sign : {1, -1}) {
                ProjectData::LoadCombination variant = combination;
                variant.name = QStringLiteral("%1 %2I%3").arg(combination.name, sign > 0 ? QStringLiteral("+") : QStringLiteral("-"), axis == 0 ? QStringLiteral("X") : QStringLiteral("Y"));
                variant.imperfectionAxis = axis;
                variant.imperfectionSign = sign;
                out.append(variant);
            }
        }
    }
    return out;
}

QVector<ProjectData::LoadCombination> NotionalLoadService::variantsFor(const ProjectData::DesignSettings &settings, const QVector<ProjectData::LoadCombination> &combinations)
{
    if (settings.geometricActive()) {
        return withGeometricVariants(combinations);
    }
    if (settings.notionalActive()) {
        return withVariants(combinations);
    }
    return combinations;
}

void NotionalLoadService::applyImperfection(SolverBridge::ModelData &model, int axis, int sign, double ratio)
{
    if (model.nodes.empty() || ratio == 0.0) {
        return;
    }
    double base = 1e300, lowest = 1e300;
    for (const auto &node : model.nodes) {
        lowest = std::min(lowest, node.z);
        if (std::any_of(node.restraints.begin(), node.restraints.end(), [](int r) { return r != 0; })) {
            base = std::min(base, node.z);
        }
    }
    if (base > 1e299) {
        base = lowest;
    }
    const std::size_t component = axis == 0 ? 0 : 1;
    auto shift = [&](double &x, double &y, double z) {
        (component == 0 ? x : y) += static_cast<double>(sign) * ratio * (z - base);
    };
    for (auto &node : model.nodes) {
        shift(node.x, node.y, node.z);
    }
    if (model.shell) {
        auto shell = std::make_shared<SolverBridge::ShellPart>(*model.shell);   // a parte de cascas e compartilhada entre os modelos: copia antes de alterar
        for (auto &node : shell->nodes) {
            shift(node.x, node.y, node.z);
        }
        model.shell = std::move(shell);
    }
}

} // namespace Structura::App::Services
