#pragma once

#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

#include "app/Edition.h"
#include "app/ISceneModelGateway.h"
#include "app/ProjectDataTypes.h"

namespace Structura::App::Services {

class DatModelIoService
{
public:
    /**
     * @param designSections  true: interpreta tambem as secoes de dimensionamento, ligacoes e detalhamento (edicao Comercial);
     *                        false: essas secoes (e as chaves de dimensionamento de [DESIGN_SETTINGS]) sao apenas preservadas e regravadas
     *                        sem alteracao (edicao Demonstrativa). Qualquer secao ou chave desconhecida e preservada nos dois casos.
     */
    explicit DatModelIoService(bool designSections = Structura::App::Edition::kHasDesign) : m_designSections(designSections) {}

    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;
    using NodalLoad = Structura::App::ProjectData::NodalLoad;
    using MemberLoad = Structura::App::ProjectData::MemberLoad;
    using ProjectExtras = Structura::App::ProjectData::ProjectExtras;

    bool load(const QString &filePath,
              Structura::App::ISceneModelGateway *sceneController,
              QVector<MaterialInfo> &materials,
              QVector<SectionInfo> &sections,
              QVector<NodeSupport> &supports,
              QVector<NodalLoad> &nodalLoads,
              QVector<MemberLoad> &memberLoads,
              ProjectExtras &extras,
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
              const ProjectExtras &extras,
              QString *errorMessage) const;

private:
    bool m_designSections;
};

} // namespace Structura::App::Services
