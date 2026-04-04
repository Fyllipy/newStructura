#pragma once

#include <QVector>
#include <vector>

#include "app/ProjectDataTypes.h"

namespace Structura::App {
class ISceneModelGateway;
}

namespace Structura::App::Services {

class IdGeneratorService
{
public:
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    static int nextMaterialExternalId(const QVector<MaterialInfo> &materials);
    static int nextSectionExternalId(const QVector<SectionInfo> &sections);
    static int nextBarExternalId(const std::vector<int> &externalIds);
    static void ensureMaterialExternalIds(QVector<MaterialInfo> &materials);
    static void ensureSectionExternalIds(QVector<SectionInfo> &sections);
    static void ensureBarExternalIds(Structura::App::ISceneModelGateway *sceneController);
};

} // namespace Structura::App::Services
