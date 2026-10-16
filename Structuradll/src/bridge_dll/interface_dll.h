#pragma once

#include <cstddef>

#if defined(_WIN32)
#if defined(SOLVER_DLL_EXPORTS)
#define SOLVER_DLL_API __declspec(dllexport)
#else
#define SOLVER_DLL_API __declspec(dllimport)
#endif
#else
#define SOLVER_DLL_API
#endif

extern "C" {

// Global solver options for the following structura_solve* calls (second-order analysis).
//  secondOrder: 0 = first-order (default), non-zero = geometric non-linear (P-delta and P-small-delta)
//  maxIterations <= 0 and tolerance <= 0 select defaults (30, 1e-7).
SOLVER_DLL_API void structura_set_options(int secondOrder, int maxIterations, double tolerance);

// Linear buckling: number of modes extracted by the following structura_solve* calls (0 = off, default).
// Uses the first-order axial forces of the loads given to the solve; the factors multiply those loads.
SOLVER_DLL_API void structura_set_buckling(int modes);
// Copies the results of the last successful solve: factors[count] ascending, modes[count * nodeCount * 6]
// (displacement/rotation shape per node in solve input order, normalized to a maximum component of 1). Returns count.
SOLVER_DLL_API int structura_get_buckling(int maxModes, int nodeCount, double* factors, double* modes);

// Modal (free vibration) analysis for the following structura_solve* calls (0 modes = off, default).
// densities: mass density per material id (force*s2/length4, e.g. unit weight / g); masses: additional lumped
// translational mass per node id. Results via structura_get_modal after a successful solve.
SOLVER_DLL_API void structura_set_modal(int modes, int materialCount, const int* materialIds, const double* densities, int massCount, const int* massNodeIds, const double* masses);
// Copies circular frequencies (rad/s, ascending), effective modal mass fractions (X, Y, Z per mode) and mode shapes
// (6 values per node, normalized to a maximum component of 1). Returns the number of modes copied.
SOLVER_DLL_API int structura_get_modal(int maxModes, int nodeCount, double* omega, double* massRatio3, double* shapes);

// Marks members (by id) as tension-only (ties/cables) for the following structura_solve* calls; count = 0 clears.
SOLVER_DLL_API void structura_set_tension_only(int count, const int* memberIds);

// Returns 0 on success and non-zero on failure.
SOLVER_DLL_API int structura_solve(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer);

// Same as structura_solve plus uniformly distributed member moments (N*m/m).
SOLVER_DLL_API int structura_solve_v2(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    int memberMomentCount,
    const int* memberMomentMemberIds,
    const int* memberMomentCoordSystem,
    const double* memberMomentMx,
    const double* memberMomentMy,
    const double* memberMomentMz,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer);


// Same as structura_solve_v3 plus section placement per member:
//  - memberSectionRotationDeg[memberCount]: rotation of the section about the member
//    x-axis in degrees (right-hand rule); may be null (= 0).
//  - memberOffsets4[memberCount*4]: dy1, dz1, dy2, dz2 in meters. Rigid offset of the
//    member (centroid) axis relative to node I / node J, in the rotated local y/z axes;
//    may be null (= 0). Internal forces are reported on the member axis.
// When a rotation is given, outMemberAuxK3 holds the rotated local y direction
// (z = x cross aux, y = z cross x reproduces the rotated frame).
SOLVER_DLL_API int structura_solve_v3(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    int memberMomentCount,
    const int* memberMomentMemberIds,
    const int* memberMomentCoordSystem,
    const double* memberMomentMx,
    const double* memberMomentMy,
    const double* memberMomentMz,
    const double* memberSectionRotationDeg,
    const double* memberOffsets4,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer);

// Same as structura_solve_v3 plus end releases per member:
//  - memberReleases12[memberCount*12]: non-zero = released DOF, order ux uy uz rx ry rz at node I
//    then node J (local axes). Released DOFs are condensed out (hinges, sliders); may be null.
//  Section placement as in structura_solve_v3:
//  - memberSectionRotationDeg[memberCount]: rotation of the section about the member
//    x-axis in degrees (right-hand rule); may be null (= 0).
//  - memberOffsets4[memberCount*4]: dy1, dz1, dy2, dz2 in meters. Rigid offset of the
//    member (centroid) axis relative to node I / node J, in the rotated local y/z axes;
//    may be null (= 0). Internal forces are reported on the member axis.
// When a rotation is given, outMemberAuxK3 holds the rotated local y direction
// (z = x cross aux, y = z cross x reproduces the rotated frame).
SOLVER_DLL_API int structura_solve_v4(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    int memberMomentCount,
    const int* memberMomentMemberIds,
    const int* memberMomentCoordSystem,
    const double* memberMomentMx,
    const double* memberMomentMy,
    const double* memberMomentMz,
    const double* memberSectionRotationDeg,
    const double* memberOffsets4,
    const int* memberReleases12,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer);

}
