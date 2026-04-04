#pragma once

#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include "app/ISceneModelGateway.h"
#include "app/ProjectDataTypes.h"

namespace Structura::App::Services {

class DatModelIoService
{
public:
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;
    using NodalLoad = Structura::App::ProjectData::NodalLoad;
    using MemberLoad = Structura::App::ProjectData::MemberLoad;

    bool load(const QString &filePath,
              Structura::App::ISceneModelGateway *sceneController,
              QVector<MaterialInfo> &materials,
              QVector<SectionInfo> &sections,
              QVector<NodeSupport> &supports,
              QVector<NodalLoad> &nodalLoads,
              QVector<MemberLoad> &memberLoads,
              QUuid &lastMaterialId,
              QUuid &lastSectionId,
              QString *errorMessage,
              QStringList *warnings) const;

    bool save(const QString &filePath,
              Structura::App::ISceneModelGateway *sceneController,
              QVector<MaterialInfo> &materials,
              QVector<SectionInfo> &sections,
              const QVector<NodeSupport> &supports,
              const QVector<NodalLoad> &nodalLoads,
              const QVector<MemberLoad> &memberLoads,
              QString *errorMessage) const;
};

} // namespace Structura::App::Services
