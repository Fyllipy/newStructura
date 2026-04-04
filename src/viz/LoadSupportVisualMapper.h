#pragma once

#include <QVector>

#include "app/ProjectDataTypes.h"
#include "viz/SceneController.h"

namespace Structura::Viz {

class LoadSupportVisualMapper
{
public:
    using NodalLoad = Structura::App::ProjectData::NodalLoad;
    using MemberLoad = Structura::App::ProjectData::MemberLoad;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;

    static QVector<SceneController::NodalLoadVisual> mapNodalLoadVisuals(const SceneController *sceneController,
                                                                          const QVector<NodalLoad> &nodalLoads);

    static QVector<SceneController::MemberLoadVisual> mapMemberLoadVisuals(const SceneController *sceneController,
                                                                            const QVector<MemberLoad> &memberLoads);

    static QVector<SceneController::SupportVisual> mapSupportVisuals(const SceneController *sceneController,
                                                                     const QVector<NodeSupport> &supports);
};

} // namespace Structura::Viz
