#include "IdGeneratorService.h"

#include "app/ExternalIdUtils.h"
#include "app/ISceneModelGateway.h"

namespace Structura::App::Services {

int IdGeneratorService::nextMaterialExternalId(const QVector<MaterialInfo> &materials)
{
    return Structura::App::nextExternalIdFrom(materials, [](const MaterialInfo &mat) {
        return mat.externalId;
    });
}

int IdGeneratorService::nextSectionExternalId(const QVector<SectionInfo> &sections)
{
    return Structura::App::nextExternalIdFrom(sections, [](const SectionInfo &sec) {
        return sec.externalId;
    });
}

int IdGeneratorService::nextBarExternalId(const std::vector<int> &externalIds)
{
    return Structura::App::nextExternalIdFrom(externalIds, [](int externalId) {
        return externalId;
    });
}

void IdGeneratorService::ensureMaterialExternalIds(QVector<MaterialInfo> &materials)
{
    Structura::App::ensurePositiveExternalIds(
        materials,
        [](const MaterialInfo &mat) { return mat.externalId; },
        [](MaterialInfo &mat, int externalId) { mat.externalId = externalId; });
}

void IdGeneratorService::ensureSectionExternalIds(QVector<SectionInfo> &sections)
{
    Structura::App::ensurePositiveExternalIds(
        sections,
        [](const SectionInfo &sec) { return sec.externalId; },
        [](SectionInfo &sec, int externalId) { sec.externalId = externalId; });
}

void IdGeneratorService::ensureBarExternalIds(Structura::App::ISceneModelGateway *sceneController)
{
    if (!sceneController) {
        return;
    }

    const auto bars = sceneController->bars();
    Structura::App::applyMissingExternalIds(
        bars,
        [](const auto &bar) { return bar.externalId; },
        [sceneController](const auto &bar, int externalId) {
            sceneController->setBarExternalId(bar.id, externalId);
        });
}

} // namespace Structura::App::Services
