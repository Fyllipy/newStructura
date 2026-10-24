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

// Advanced analysis options for the following structura_solve* calls (defaults: all off, so the results do not change):
//  shearDeformation: non-zero = Timoshenko bending using the shear areas given by structura_set_shear_areas
//  stabilityFunctions: non-zero = second order by Livesley stability functions (exact P-delta / P-small-delta in each member, Euler-Bernoulli);
//                      with shear deformation the consistent geometric stiffness is used instead
//  loadSteps: second order, 0 = direct iteration with automatic load increments if it fails; n > 1 = always n load steps
SOLVER_DLL_API void structura_set_analysis_options(int shearDeformation, int stabilityFunctions, int loadSteps);
// Shear areas (same length unit squared as the section area) per section id; replaces the previous list. count = 0 clears it.
SOLVER_DLL_API void structura_set_shear_areas(int count, const int* sectionIds, const double* shearAreaY, const double* shearAreaZ);
// Rigid end zones per member id (lengths along the member axis from node I and node J, in the model length unit); replaces the previous list.
// The flexible part is the remaining length; end forces are reported at the nodes. count = 0 clears the list.
SOLVER_DLL_API void structura_set_rigid_zones(int count, const int* memberIds, const double* lengthI, const double* lengthJ);
// Torsion with warping (Vlasov) for open thin-walled sections. Sections: warping constant Cw (length^6, same unit as the section data) per section id;
// members: ids that use it, with the warping condition at each end (0 = automatic: restrained at fully fixed nodes, shared by collinear members, free
// elsewhere; 1 = free (zero bimoment); 2 = restrained (theta' = 0)). Both lists replace the previous ones; count = 0 clears them.
// Not combined with buckling or modal analysis (St. Venant torsion is used there, with a warning).
SOLVER_DLL_API void structura_set_warping_sections(int count, const int* sectionIds, const double* warpingConstants);
SOLVER_DLL_API void structura_set_warping_members(int count, const int* memberIds, const int* modeStart, const int* modeEnd);
// Results of the last successful solve per requested member id: out4 = (theta' at I, theta' at J, generalized force at I, at J);
// the forces on theta' are the bimoments applied to the bar (B at I; -B at J, B = -E Cw theta''). Returns the number of members with warping.
SOLVER_DLL_API int structura_get_warping(int count, const int* memberIds, double* out4);
// Barras de secao I de alma variavel (NBR 8800:2024, Anexo G): alturas nas extremidades I e J, largura e espessura das mesas, espessura da alma (unidades do modelo)
// e numero de trechos prismaticos em que o solver subdivide a barra (nos internos; os esforcos devolvidos sao os das extremidades da barra). Substitui a lista anterior.
SOLVER_DLL_API void structura_set_taper(int count, const int* memberIds, const double* d1, const double* d2, const double* bf, const double* tw, const double* tf, const int* segments);
// Warnings of the last successful solve, one per line (UTF-8/ASCII). Returns the number of warnings; copies up to bufferSize - 1 characters.
SOLVER_DLL_API int structura_get_warnings(char* buffer, int bufferSize);

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

// Shells and plates coupled to the frame (Q4 element of Femdll: membrane with incompatible modes + Mindlin bending with MITC4).
// The shell nodes are ordinary nodes of the following structura_solve* calls (same ids, coordinates and restraints); members
// may share them or be tied to them. State kept until structura_clear_shells.
//  shells: ids, 4 node ids each (counter-clockwise seen from the positive normal), E, nu, density, thickness.
//  loads: per shell, kind 0 = nodal pressure (4 values, positive along the normal), 1 = global body force (3 values),
//         2 = temperature (uniform, gradient).
//  ties: slave node tied to up to 8 master nodes (weights sum to 1); tieDofMasks (may be null = all): bits 0..5 = DOFs tied (7 = translations only, a hinged support).
//  springs: nodal elastic supports; compressionOnly != 0: k6[0] normal, k6[1] tangential, normal (3 values) from support to structure.
SOLVER_DLL_API void structura_clear_shells();
// Thermal expansion coefficient (1/K) of each shell, same order as structura_set_shells (call right after it). Default 1.2e-5.
SOLVER_DLL_API int structura_set_shell_alpha(int shellCount, const double* alphas);
SOLVER_DLL_API int structura_set_shells(
    int shellCount,
    const int* shellIds,
    const int* shellNodeIds4,
    const double* shellE,
    const double* shellNu,
    const double* shellDensity,
    const double* shellThickness,
    int loadCount,
    const int* loadShellIds,
    const int* loadKinds,
    const double* loadValues4,
    int tieCount,
    const int* tieSlaveIds,
    const int* tieMasterCounts,
    const int* tieMasterIds,
    const double* tieWeights,
    const int* tieDofMasks,
    int springCount,
    const int* springNodeIds,
    const double* springK6,
    const int* springCompressionOnly,
    const double* springNormals3);
// Copies the shell results of the last successful solve in the order of structura_set_shells:
// 40 values per shell (5 points x nxx nyy nxy mxx myy mxy qx qy, local axes) and the axes e1,e2,e3 (9 values). Returns the count.
SOLVER_DLL_API int structura_get_shell_results(int shellCount, double* values40, double* axes9);

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
