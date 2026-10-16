#pragma once

#include <array>
#include <utility>
#include <string>
#include <vector>

enum class DistributedLoadSystem {
    Local = 0,
    Global = 1
};

struct MaterialData {
    int id = 0;
    double E = 0.0;
    double G = 0.0;
    double density = 0.0;
};

struct SectionData {
    int id = 0;
    double A = 0.0;
    double Iz = 0.0;
    double Iy = 0.0;
    double J = 0.0;
};

struct NodeData {
    int id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::array<int, 6> restraints{0, 0, 0, 0, 0, 0};
};

struct MemberData {
    int id = 0;
    int nodeI = 0;
    int nodeJ = 0;
    int materialId = 0;
    int sectionId = 0;
    // Section placement: rotation about the member x-axis (degrees) and rigid offsets
    // (m) of the member axis relative to node I / node J, in the rotated local y/z axes.
    double sectionRotationDeg = 0.0;
    double offsetY1 = 0.0;
    double offsetZ1 = 0.0;
    double offsetY2 = 0.0;
    double offsetZ2 = 0.0;
    // End releases: ux uy uz rx ry rz at node I then node J (non-zero = released)
    std::array<int, 12> releases{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

struct NodalLoadData {
    int nodeId = 0;
    std::array<double, 6> values{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct MemberLoadData {
    int memberId = 0;
    DistributedLoadSystem system = DistributedLoadSystem::Local;
    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
};

struct MemberMomentData {
    int memberId = 0;
    DistributedLoadSystem system = DistributedLoadSystem::Local;
    double mx = 0.0;
    double my = 0.0;
    double mz = 0.0;
};

struct SolverInput {
    std::vector<MaterialData> materials;
    std::vector<SectionData> sections;
    std::vector<NodeData> nodes;
    std::vector<MemberData> members;
    std::vector<NodalLoadData> nodalLoads;
    std::vector<MemberLoadData> memberLoads;
    std::vector<MemberMomentData> memberMoments;
    // Second-order (geometric non-linear) analysis by iterating K + Kg(N)
    bool secondOrder = false;
    // Tension-only members (ties/cables): deactivated while shortening, reactivated when elongating
    std::vector<int> tensionOnlyMembers;
    // Linear (elastic) buckling: number of modes to extract (0 = none); uses the first-order axial forces
    int bucklingModes = 0;
    // Modal analysis (free vibration): number of modes and additional lumped nodal masses (translational)
    int modalModes = 0;
    std::vector<std::pair<int, double>> extraNodalMasses;   // node id, mass
    int maxIterations = 30;
    double tolerance = 1e-7;
};

struct NodeResult {
    int id = 0;
    std::array<double, 6> displacements{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 6> reactions{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct MemberResult {
    int id = 0;
    int nodeI = 0;
    int nodeJ = 0;
    std::array<double, 12> localForces{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 3> auxiliaryK{0.0, 0.0, 0.0};
};

struct SolverOutput {
    std::vector<NodeResult> nodes;
    std::vector<MemberResult> members;
    // Critical load factors (ascending) and mode shapes: modes[m] has 6 values per node (input order)
    std::vector<double> bucklingFactors;
    std::vector<std::vector<double>> bucklingModes;
    // Modal results: circular frequencies (rad/s, ascending), mode shapes (6 per node) and effective modal mass fractions (X,Y,Z)
    std::vector<double> modalOmega;
    std::vector<std::vector<double>> modalShapes;
    std::vector<std::array<double, 3>> modalMassRatio;
};
