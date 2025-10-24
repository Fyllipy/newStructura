#pragma once

#include <QObject>
#include <QVector>
#include <QVector3D>
#include <QUuid>
#include <QString>
#include <optional>

class SceneController;

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
    struct MaterialInfo {
        QUuid uuid;
        int externalId {0};
        QString name;
        double youngModulus {0.0};
        double shearModulus {0.0};
    };

    struct SectionInfo {
        QUuid uuid;
        int externalId {0};
        QString name;
        double area {0.0};
        double iz {0.0};
        double iy {0.0};
        double j {0.0};
    };

    struct NodeSupport {
        int nodeId {0};
        bool restraints[6] {false, false, false, false, false, false};
    };

    struct NodalLoad {
        int nodeId {0};
        double fx {0.0};
        double fy {0.0};
        double fz {0.0};
        double mx {0.0};
        double my {0.0};
        double mz {0.0};
    };

    struct MemberLoad {
        int memberId {0};
        QString system;
        double qx {0.0};
        double qy {0.0};
        double qz {0.0};
    };

    struct NodalLoadPreset {
        double fx {0.0};
        double fy {0.0};
        double fz {0.0};
        double mx {0.0};
        double my {0.0};
        double mz {0.0};
    };

    struct DistributedLoadPreset {
        QString system {QStringLiteral("GLOBAL")};
        double qx {0.0};
        double qy {0.0};
        double qz {0.0};
    };

    struct Dependencies {
        SceneController *sceneController {nullptr};
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
    };

    explicit MainWindowPresenter(const Dependencies &deps, QObject *parent = nullptr);

    [[nodiscard]] SceneController *scene() const noexcept { return m_sceneController; }
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
    SceneController *m_sceneController;
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
