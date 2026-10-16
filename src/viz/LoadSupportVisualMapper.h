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
    using MemberMoment = Structura::App::ProjectData::MemberMoment;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using LoadCase = Structura::App::ProjectData::LoadCase;

    /// Dados de projeto necessarios para mapear cargas em glifos.
    struct LoadContext {
        const QVector<NodalLoad> *nodalLoads {nullptr};
        const QVector<MemberLoad> *memberLoads {nullptr};
        const QVector<MemberMoment> *memberMoments {nullptr};
        const QVector<MaterialInfo> *materials {nullptr};
        const QVector<SectionInfo> *sections {nullptr};
        const QVector<Structura::App::ProjectData::MemberPlacement> *placements {nullptr}; ///< Giro de secao por barra
        const QVector<LoadCase> *loadCases {nullptr};
        QString activeCase; ///< Vazio = todos os casos
    };

    static QVector<SceneController::NodalLoadVisual> mapNodalLoadVisuals(const SceneController *sceneController,
                                                                          const LoadContext &context);

    /// Cargas distribuidas, momentos distribuidos e peso proprio das barras.
    static QVector<SceneController::MemberActionVisual> mapMemberActionVisuals(const SceneController *sceneController,
                                                                                const LoadContext &context);

    static QVector<SceneController::SupportVisual> mapSupportVisuals(const SceneController *sceneController,
                                                                     const QVector<NodeSupport> &supports);
};

} // namespace Structura::Viz
