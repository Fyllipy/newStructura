#pragma once

#include <QObject>
#include <QVector>
#include <QVector3D>
#include <QUuid>
#include <QString>
#include <functional>
#include <optional>

#include "app/ISceneEditGateway.h"
#include "app/ProjectDataTypes.h"

namespace Structura {
class SelectionModel;
}

namespace Structura::App {
class UndoRedoService;
}

namespace Structura::UI {

class MainWindowPresenter : public QObject
{
    Q_OBJECT

public:
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using NodeSupport = Structura::App::ProjectData::NodeSupport;
    using NodalLoad = Structura::App::ProjectData::NodalLoad;
    using MemberLoad = Structura::App::ProjectData::MemberLoad;
    using NodalLoadPreset = Structura::App::ProjectData::NodalLoadPreset;
    using DistributedLoadPreset = Structura::App::ProjectData::DistributedLoadPreset;

    struct Dependencies {
        Structura::App::ISceneEditGateway *sceneController {nullptr};
        Structura::SelectionModel *selectionModel {nullptr};
        Structura::App::UndoRedoService *undoService {nullptr};
        QVector<MaterialInfo> *materials {nullptr};
        QVector<SectionInfo> *sections {nullptr};
        QUuid *lastMaterialId {nullptr};
        QUuid *lastSectionId {nullptr};
        QVector<NodeSupport> *supports {nullptr};
        QVector<NodalLoad> *nodalLoads {nullptr};
        QVector<MemberLoad> *memberLoads {nullptr};
        NodalLoadPreset *lastNodalPreset {nullptr};
        DistributedLoadPreset *lastDistributedPreset {nullptr};
        /// Amplia uma lista de barras com os demais membros dos grupos a que pertencem.
        std::function<QVector<QUuid>(const QVector<QUuid> &)> expandIds;
    };

    explicit MainWindowPresenter(const Dependencies &deps, QObject *parent = nullptr);

    [[nodiscard]] Structura::App::ISceneEditGateway *scene() const noexcept { return m_sceneController; }
    [[nodiscard]] Structura::SelectionModel *selectionModel() const noexcept { return m_selectionModel; }
    [[nodiscard]] Structura::App::UndoRedoService *undoService() const noexcept { return m_undoService; }

    [[nodiscard]] QVector<MaterialInfo> &materials() noexcept { return *m_materials; }
    [[nodiscard]] const QVector<MaterialInfo> &materials() const noexcept { return *m_materials; }
    [[nodiscard]] QVector<SectionInfo> &sections() noexcept { return *m_sections; }
    [[nodiscard]] const QVector<SectionInfo> &sections() const noexcept { return *m_sections; }

    [[nodiscard]] MaterialInfo *findMaterial(const QUuid &id);
    [[nodiscard]] const MaterialInfo *findMaterial(const QUuid &id) const;
    [[nodiscard]] SectionInfo *findSection(const QUuid &id);
    [[nodiscard]] const SectionInfo *findSection(const QUuid &id) const;

    [[nodiscard]] const QUuid &lastMaterialId() const noexcept { return *m_lastMaterialId; }
    void setLastMaterialId(const QUuid &id) noexcept { *m_lastMaterialId = id; }
    [[nodiscard]] const QUuid &lastSectionId() const noexcept { return *m_lastSectionId; }
    void setLastSectionId(const QUuid &id) noexcept { *m_lastSectionId = id; }

    [[nodiscard]] QVector<NodeSupport> &supports() noexcept { return *m_supports; }
    [[nodiscard]] const QVector<NodeSupport> &supports() const noexcept { return *m_supports; }
    [[nodiscard]] QVector<NodalLoad> &nodalLoads() noexcept { return *m_nodalLoads; }
    [[nodiscard]] const QVector<NodalLoad> &nodalLoads() const noexcept { return *m_nodalLoads; }
    [[nodiscard]] QVector<MemberLoad> &memberLoads() noexcept { return *m_memberLoads; }
    [[nodiscard]] const QVector<MemberLoad> &memberLoads() const noexcept { return *m_memberLoads; }

    [[nodiscard]] NodalLoadPreset &lastNodalPreset() noexcept { return *m_lastNodalPreset; }
    [[nodiscard]] const NodalLoadPreset &lastNodalPreset() const noexcept { return *m_lastNodalPreset; }
    [[nodiscard]] DistributedLoadPreset &lastDistributedPreset() noexcept { return *m_lastDistributedPreset; }
    [[nodiscard]] const DistributedLoadPreset &lastDistributedPreset() const noexcept { return *m_lastDistributedPreset; }

    void handleNodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value);
    void handleBarMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId);
    void handleBarSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId);

private:
    std::function<QVector<QUuid>(const QVector<QUuid> &)> m_expandIds;
    Structura::App::ISceneEditGateway *m_sceneController;
    Structura::SelectionModel *m_selectionModel;
    Structura::App::UndoRedoService *m_undoService;

    QVector<MaterialInfo> *m_materials;
    QVector<SectionInfo> *m_sections;
    QUuid *m_lastMaterialId;
    QUuid *m_lastSectionId;

    QVector<NodeSupport> *m_supports;
    QVector<NodalLoad> *m_nodalLoads;
    QVector<MemberLoad> *m_memberLoads;
    NodalLoadPreset *m_lastNodalPreset;
    DistributedLoadPreset *m_lastDistributedPreset;
};

} // namespace Structura::UI
