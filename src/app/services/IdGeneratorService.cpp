#include "IdGeneratorService.h"

#include "app/ISceneModelGateway.h"

namespace Structura::App::Services {

int IdGeneratorService::nextMaterialExternalId(const QVector<MaterialInfo> &materials)
{
    int maxId = 0;
    for (const auto &mat : materials) {
        if (mat.externalId > maxId) {
            maxId = mat.externalId;
        }
    }
    return maxId + 1;
}

int IdGeneratorService::nextSectionExternalId(const QVector<SectionInfo> &sections)
{
    int maxId = 0;
    for (const auto &sec : sections) {
        if (sec.externalId > maxId) {
            maxId = sec.externalId;
        }
    }
    return maxId + 1;
}

int IdGeneratorService::nextBarExternalId(const std::vector<int> &externalIds)
{
    int maxId = 0;
    for (int externalId : externalIds) {
        if (externalId > maxId) {
            maxId = externalId;
        }
    }
    return maxId + 1;
}

void IdGeneratorService::ensureMaterialExternalIds(QVector<MaterialInfo> &materials)
{
    int nextId = nextMaterialExternalId(materials);
    for (auto &mat : materials) {
        if (mat.externalId <= 0) {
            mat.externalId = nextId++;
        }
    }
}

void IdGeneratorService::ensureSectionExternalIds(QVector<SectionInfo> &sections)
{
    int nextId = nextSectionExternalId(sections);
    for (auto &sec : sections) {
        if (sec.externalId <= 0) {
            sec.externalId = nextId++;
        }
    }
}

void IdGeneratorService::ensureBarExternalIds(Structura::App::ISceneModelGateway *sceneController)
{
    if (!sceneController) {
        return;
    }

    const auto bars = sceneController->bars();
    std::vector<int> externalIds;
    externalIds.reserve(bars.size());
    for (const auto &bar : bars) {
        externalIds.push_back(bar.externalId);
    }

    int nextId = nextBarExternalId(externalIds);
    for (const auto &bar : bars) {
        if (bar.externalId <= 0) {
            sceneController->setBarExternalId(bar.id, nextId++);
        }
    }
}

} // namespace Structura::App::Services
