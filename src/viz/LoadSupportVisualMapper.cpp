#include "LoadSupportVisualMapper.h"

#include "app/LoadSystemUtils.h"
#include "app/LookupUtils.h"
#include "viz/LocalCoordinateSystem.h"

#include <QHash>

#include <algorithm>
#include <cmath>

namespace Structura::Viz {

namespace {

bool inActiveCase(const QString &loadCase, const QString &active, const QString &defaultCase)
{
    if (active.isEmpty()) {
        return true;
    }
    return (loadCase.isEmpty() ? defaultCase : loadCase) == active;
}

QString defaultCaseOf(const QVector<Structura::App::ProjectData::LoadCase> *cases)
{
    return (cases && !cases->isEmpty()) ? cases->first().name : Structura::App::ProjectData::defaultLoadCaseName();
}

bool isCaseShown(const QString &loadCase, const LoadSupportVisualMapper::LoadContext &context, const QString &defaultCase)
{
    // a lista de casos visiveis (janela "Casos visiveis") tem precedencia sobre o caso ativo da barra de vista
    if (context.filterVisibleCases) {
        return context.visibleCases.contains(loadCase.isEmpty() ? defaultCase : loadCase);
    }
    return inActiveCase(loadCase, context.activeCase, defaultCase);
}

} // namespace

QVector<SceneController::NodalLoadVisual> LoadSupportVisualMapper::mapNodalLoadVisuals(const SceneController *sceneController,
                                                                                         const LoadContext &context)
{
    QVector<SceneController::NodalLoadVisual> nodalVisuals;
    if (!sceneController || !context.nodalLoads) {
        return nodalVisuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<int, SceneController::NodeInfo> nodesByExternal =
        Structura::App::buildLookup<int, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.externalId; });

    const QString defaultCase = defaultCaseOf(context.loadCases);
    for (int loadIndex = 0; loadIndex < context.nodalLoads->size(); ++loadIndex) {
        const auto &load = (*context.nodalLoads)[loadIndex];
        if (!isCaseShown(load.loadCase, context, defaultCase)) {
            continue;
        }
        const auto it = nodesByExternal.constFind(load.nodeId);
        if (it == nodesByExternal.constEnd()) {
            continue;
        }

        SceneController::NodalLoadVisual visual;
        visual.position = QVector3D(static_cast<float>(it->x), static_cast<float>(it->y), static_cast<float>(it->z));
        visual.force = QVector3D(static_cast<float>(load.fx), static_cast<float>(load.fy), static_cast<float>(load.fz));
        visual.moment = QVector3D(static_cast<float>(load.mx), static_cast<float>(load.my), static_cast<float>(load.mz));
        visual.sourceKind = 0;
        visual.sourceIndex = loadIndex;
        nodalVisuals.append(visual);
    }

    return nodalVisuals;
}

QVector<SceneController::MemberActionVisual> LoadSupportVisualMapper::mapMemberActionVisuals(const SceneController *sceneController,
                                                                                               const LoadContext &context)
{
    QVector<SceneController::MemberActionVisual> visuals;
    if (!sceneController) {
        return visuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<QUuid, SceneController::NodeInfo> nodesById =
        Structura::App::buildLookup<QUuid, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.id; });

    const auto barInfos = sceneController->bars();
    const QHash<int, SceneController::BarInfo> barsByExternal =
        Structura::App::buildPositiveExternalLookup<SceneController::BarInfo>(
            barInfos,
            [](const SceneController::BarInfo &info) { return info.externalId; },
            [](const SceneController::BarInfo &info) { return info.externalId > 0; });

    const QString defaultCase = defaultCaseOf(context.loadCases);
    Structura::Geometry::DefaultLocalAxisProvider axisProvider;

    // Geometria e eixos locais (mesma convencao do solver) de uma barra.
    auto makeBase = [&](int memberId, SceneController::MemberActionVisual &visual) {
        const auto barIt = barsByExternal.constFind(memberId);
        if (barIt == barsByExternal.constEnd()) {
            return false;
        }
        const auto startIt = nodesById.constFind(barIt->startNodeId);
        const auto endIt = nodesById.constFind(barIt->endNodeId);
        if (startIt == nodesById.constEnd() || endIt == nodesById.constEnd()) {
            return false;
        }
        const std::array<double, 3> a {startIt->x, startIt->y, startIt->z};
        const std::array<double, 3> b {endIt->x, endIt->y, endIt->z};
        const double dx = b[0] - a[0];
        const double dy = b[1] - a[1];
        const double dz = b[2] - a[2];
        if (dx * dx + dy * dy + dz * dz < 1e-12) {
            return false;
        }
        double rotationDeg = 0.0;
        if (context.placements) {
            for (const auto &placement : *context.placements) {
                if (placement.memberId == memberId) {
                    rotationDeg = placement.rotationDeg;
                    break;
                }
            }
        }
        const auto lcs = Structura::Geometry::rotatedAboutX(axisProvider.computeLCS(a, b, std::nullopt), rotationDeg);
        auto toVector = [](const std::array<double, 3> &v) {
            return QVector3D(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
        };
        visual.start = toVector(a);
        visual.end = toVector(b);
        visual.xAxis = toVector(lcs.xPrime);
        visual.yAxis = toVector(lcs.yPrime);
        visual.zAxis = toVector(lcs.zPrime);
        return true;
    };

    if (context.memberLoads) {
        for (int loadIndex = 0; loadIndex < context.memberLoads->size(); ++loadIndex) {
            const auto &load = (*context.memberLoads)[loadIndex];
            if (!isCaseShown(load.loadCase, context, defaultCase)) {
                continue;
            }
            SceneController::MemberActionVisual visual;
            if (!makeBase(load.memberId, visual)) {
                continue;
            }
            visual.values = QVector3D(static_cast<float>(load.qx), static_cast<float>(load.qy), static_cast<float>(load.qz));
            visual.local = Structura::App::isLocalMemberLoadSystem(load.system);
            visual.moment = false;
            visual.sourceKind = 1;
            visual.sourceIndex = loadIndex;
            if (!visual.values.isNull()) {
                visuals.append(visual);
            }
        }
    }

    if (context.memberMoments) {
        for (int loadIndex = 0; loadIndex < context.memberMoments->size(); ++loadIndex) {
            const auto &moment = (*context.memberMoments)[loadIndex];
            if (!isCaseShown(moment.loadCase, context, defaultCase)) {
                continue;
            }
            SceneController::MemberActionVisual visual;
            if (!makeBase(moment.memberId, visual)) {
                continue;
            }
            visual.values = QVector3D(static_cast<float>(moment.mx), static_cast<float>(moment.my), static_cast<float>(moment.mz));
            visual.local = Structura::App::isLocalMemberLoadSystem(moment.system);
            visual.moment = true;
            visual.sourceKind = 2;
            visual.sourceIndex = loadIndex;
            if (!visual.values.isNull()) {
                visuals.append(visual);
            }
        }
    }

    // Cargas de barra com posicao: trapezoidal/parcial, forca pontual, momento pontual, temperatura e retracao
    if (context.memberActions) {
        namespace Kind = Structura::App::ProjectData::MemberActionKind;
        auto format = [](double value, int decimals = 2) {
            QString text = QString::number(value, 'f', decimals);
            while (text.contains(QLatin1Char('.')) && (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
                text.chop(1);
            }
            return text.replace(QLatin1Char('.'), QLatin1Char(','));
        };
        for (int loadIndex = 0; loadIndex < context.memberActions->size(); ++loadIndex) {
            const auto &action = (*context.memberActions)[loadIndex];
            if (!isCaseShown(action.loadCase, context, defaultCase)) {
                continue;
            }
            SceneController::MemberActionVisual visual;
            if (!makeBase(action.memberId, visual)) {
                continue;
            }
            const double length = (visual.end - visual.start).length();
            if (length <= 0.0f) {
                continue;
            }
            auto vec = [](const std::array<double, 3> &v) {
                return QVector3D(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
            };
            visual.local = Structura::App::isLocalMemberLoadSystem(action.system);
            visual.sourceKind = 4;
            visual.sourceIndex = loadIndex;
            const double t0 = std::clamp(action.a / length, 0.0, 1.0);
            const double t1 = std::clamp(action.b / length, 0.0, 1.0);
            switch (action.kind) {
            case Kind::kTrapezoid:
                visual.variant = 1;
                visual.t0 = std::min(t0, t1);
                visual.t1 = std::max(t0, t1);
                if (t1 < t0) {
                    visual.values = vec(action.v2);
                    visual.values2 = vec(action.v1);
                } else {
                    visual.values = vec(action.v1);
                    visual.values2 = vec(action.v2);
                }
                break;
            case Kind::kPointForce:
                visual.variant = 2;
                visual.t0 = visual.t1 = t0;
                visual.values = vec(action.v1);
                visual.values2 = visual.values;
                break;
            case Kind::kPointMoment:
                visual.variant = 3;
                visual.moment = true;
                visual.t0 = visual.t1 = t0;
                visual.values = vec(action.v1);
                visual.values2 = visual.values;
                break;
            case Kind::kTemperature:
                visual.variant = 4;
                visual.local = true;
                visual.values = vec(action.v1);
                visual.note = QStringLiteral("ΔT = %1 °C").arg(format(action.v1[0]));
                if (std::abs(action.v1[1]) > 1e-12 || std::abs(action.v1[2]) > 1e-12) {
                    visual.note += QStringLiteral("  (grad. y %1; z %2 °C/m)").arg(format(action.v1[1])).arg(format(action.v1[2]));
                }
                break;
            case Kind::kShrinkage:
                visual.variant = 5;
                visual.local = true;
                visual.values = QVector3D(static_cast<float>(action.v1[0]), 0.0f, 0.0f);
                visual.note = QStringLiteral("ε_cs = %1 ‰").arg(format(action.v1[0] * 1000.0, 3));
                break;
            default:
                continue;
            }
            if (!action.isZero()) {
                visuals.append(visual);
            }
        }
    }

    // Peso proprio: gama (kN/m3) x area (m2), global -Z, somado por caso que o inclui.
    if (context.loadCases && context.materials && context.sections) {
        int selfWeightCases = 0;
        for (const auto &loadCase : *context.loadCases) {
            if (loadCase.selfWeight && isCaseShown(loadCase.name, context, defaultCase)) {
                ++selfWeightCases;
            }
        }
        if (selfWeightCases > 0) {
            QHash<QUuid, double> gammaByMaterial;
            for (const auto &material : *context.materials) {
                gammaByMaterial.insert(material.uuid, material.unitWeight);
            }
            QHash<QUuid, double> areaBySection;
            for (const auto &section : *context.sections) {
                areaBySection.insert(section.uuid, section.area);
            }
            for (const auto &bar : barInfos) {
                const double weight = gammaByMaterial.value(bar.materialId, 0.0) * areaBySection.value(bar.sectionId, 0.0)
                                      * selfWeightCases;
                if (weight == 0.0 || bar.externalId <= 0) {
                    continue;
                }
                SceneController::MemberActionVisual visual;
                if (!makeBase(bar.externalId, visual)) {
                    continue;
                }
                visual.values = QVector3D(0.0f, 0.0f, static_cast<float>(-weight));
                visual.local = false;
                visual.selfWeight = true;
                visual.sourceKind = 3;
                visual.sourceIndex = bar.externalId;
                visuals.append(visual);
            }
        }
    }

    return visuals;
}

QVector<SceneController::SupportVisual> LoadSupportVisualMapper::mapSupportVisuals(const SceneController *sceneController,
                                                                                    const QVector<NodeSupport> &supports)
{
    QVector<SceneController::SupportVisual> supportVisuals;
    if (!sceneController) {
        return supportVisuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<int, SceneController::NodeInfo> nodesByExternal =
        Structura::App::buildLookup<int, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.externalId; });

    supportVisuals.reserve(supports.size());
    for (const auto &support : supports) {
        const auto it = nodesByExternal.constFind(support.nodeId);
        if (it == nodesByExternal.constEnd()) {
            continue;
        }

        SceneController::SupportVisual visual;
        visual.position = QVector3D(static_cast<float>(it->x), static_cast<float>(it->y), static_cast<float>(it->z));
        for (int i = 0; i < 6; ++i) {
            visual.restraints[i] = support.restraints[i];
        }
        supportVisuals.append(visual);
    }

    return supportVisuals;
}

} // namespace Structura::Viz
