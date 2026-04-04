#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "app/ISceneModelGateway.h"
#include "app/SolverBridge.h"
#include "app/ProjectDataTypes.h"

namespace Structura::App::Services {

class AnalysisService
{
public:
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;
    using NodalLoad = Structura::App::ProjectData::NodalLoad;
    using MemberLoad = Structura::App::ProjectData::MemberLoad;

    struct BuildResult {
        bool success {false};
        Structura::App::SolverBridge::ModelData model;
        QStringList errors;
    };

    BuildResult buildModel(Structura::App::ISceneModelGateway *sceneController,
                           QVector<MaterialInfo> &materials,
                           QVector<SectionInfo> &sections,
                           const QVector<NodeSupport> &supports,
                           const QVector<NodalLoad> &nodalLoads,
                           const QVector<MemberLoad> &memberLoads) const;

    QString formatReport(const Structura::App::SolverBridge::Result &result) const;
};

} // namespace Structura::App::Services
