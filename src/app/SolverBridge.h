#pragma once

#include <QString>

#include <array>
#include <stdexcept>
#include <vector>

namespace Structura::App {

class SolverBridgeError : public std::runtime_error
{
public:
    explicit SolverBridgeError(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

class SolverBridge
{
public:
    enum class MemberLoadCoordSystem {
        Local = 0,
        Global = 1
    };

    struct MaterialData {
        int id {0};
        double e {0.0};
        double g {0.0};
    };

    struct SectionData {
        int id {0};
        double area {0.0};
        double iz {0.0};
        double iy {0.0};
        double j {0.0};
    };

    struct NodeData {
        int id {0};
        double x {0.0};
        double y {0.0};
        double z {0.0};
        std::array<int, 6> restraints {{0, 0, 0, 0, 0, 0}};
    };

    struct MemberData {
        int id {0};
        int nodeI {0};
        int nodeJ {0};
        int materialId {0};
        int sectionId {0};
    };

    struct NodalLoadData {
        int nodeId {0};
        std::array<double, 6> values {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    };

    struct MemberLoadData {
        int memberId {0};
        MemberLoadCoordSystem coordSystem {MemberLoadCoordSystem::Local};
        double qx {0.0};
        double qy {0.0};
        double qz {0.0};
    };

    struct ModelData {
        std::vector<MaterialData> materials;
        std::vector<SectionData> sections;
        std::vector<NodeData> nodes;
        std::vector<MemberData> members;
        std::vector<NodalLoadData> nodalLoads;
        std::vector<MemberLoadData> memberLoads;
    };

    struct NodeResult {
        int nodeId {0};
        std::array<double, 6> displacements {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
        std::array<double, 6> reactions {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    };

    struct MemberResult {
        int memberId {0};
        std::array<double, 12> localForces {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
        std::array<double, 3> auxK {{0.0, 0.0, 0.0}};
    };

    struct Result {
        std::vector<NodeResult> nodeResults;
        std::vector<MemberResult> memberResults;
    };

    explicit SolverBridge(QString libraryPath = QString());

    [[nodiscard]] Result solve(const ModelData &model) const;

private:
    QString m_libraryPath;
};

} // namespace Structura::App
