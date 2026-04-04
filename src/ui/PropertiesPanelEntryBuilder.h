#pragma once

#include <QSet>
#include <QVector>
#include <QUuid>

#include "app/ProjectDataTypes.h"
#include "ui/PropertiesPanel.h"

class SceneController;

namespace Structura::UI {

class PropertiesPanelEntryBuilder
{
public:
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;
    using NodalLoad = Structura::App::ProjectData::NodalLoad;
    using MemberLoad = Structura::App::ProjectData::MemberLoad;

    static QVector<PropertiesPanel::NodeEntry> buildNodeEntries(const SceneController *sceneController,
                                                                const QSet<QUuid> &nodeIds,
                                                                const QVector<NodeSupport> &supports,
                                                                const QVector<NodalLoad> &nodalLoads);

    static QVector<PropertiesPanel::BarEntry> buildBarEntries(const SceneController *sceneController,
                                                              const QSet<QUuid> &barIds,
                                                              const QVector<MaterialInfo> &materials,
                                                              const QVector<SectionInfo> &sections,
                                                              const QVector<MemberLoad> &memberLoads,
                                                              const QString &noMaterialLabel,
                                                              const QString &noSectionLabel);
};

} // namespace Structura::UI
