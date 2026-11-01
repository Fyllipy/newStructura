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
// Shear center per section id, relative to the centroid along the local y and z axes (model length unit). Only used by members with warping: the element
// stiffness (bending and torsion about the shear center line) is brought to the centroid axis with a rigid arm, which couples flexure and torsion in
// sections that are not doubly symmetric. First-order coupling: the geometric stiffness and the buckling/modal analyses keep the centroid axis.
// The torque reported in the member end forces is the one about the shear center. Replaces the previous list; count = 0 clears it.
SOLVER_DLL_API void structura_set_shear_centers(int count, const int* sectionIds, const double* scY, const double* scZ);
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
// Orthotropic shell materials, same order as structura_set_shells (call right after it; shellCount must match): flag (0/1), E2, nu12, G12, bending-stiffness
// multipliers in directions 1 and 2, and the global material axis 1 (3 values per shell). E1 is the shell's E.
SOLVER_DLL_API int structura_set_shell_orthotropy(int shellCount, const int* flags, const double* E2, const double* nu12, const double* G12, const double* bend1,
                                                  const double* bend2, const double* axis3);
// Copies the shell results of the last successful solve in the order of structura_set_shells:
// 40 values per shell (5 points x nxx nyy nxy mxx myy mxy qx qy, local axes) and the axes e1,e2,e3 (9 values). Returns the count.
SOLVER_DLL_API int structura_get_shell_results(int shellCount, double* values40, double* axes9);

// Marks members (by id) as tension-only (ties/cables) for the following structura_solve* calls; count = 0 clears.
SOLVER_DLL_API void structura_set_tension_only(int count, const int* memberIds);

// General member loads with position for the following structura_solve* calls (count = 0 clears). Per load: member id, kind
// (0 = trapezoidal/partial, 1 = point force, 2 = point moment, 3 = imposed strain: temperature/shrinkage), coordinate system (0 = local, 1 = global;
// kind 3 is always local), positions a and b (distance along the member axis from node I; b only for kind 0), and two vectors of 3 values:
//   kind 0: v1 = (qx,qy,qz) at a, v2 = (qx,qy,qz) at b (force per length);   kind 1: v1 = (Fx,Fy,Fz);   kind 2: v1 = (Mx,My,Mz);
//   kind 3: v1 = (free axial strain, d(eps)/dy, d(eps)/dz) over the whole member (alpha*dT for temperature; gradients in 1/length).
SOLVER_DLL_API void structura_set_member_actions(int count, const int* memberIds, const int* kinds, const int* coordSystems, const double* a, const double* b,
                                                 const double* v1_3, const double* v2_3);

// End springs (semi-rigid connections) for the following structura_solve* calls (count = 0 clears): 12 stiffnesses per member
// (ux uy uz rx ry rz at node I, then at node J; force/length or moment/rotation); a value < 0 means "no spring" (rigid, unless released) and 0 a hinge.
SOLVER_DLL_API void structura_set_end_springs(int count, const int* memberIds, const double* k12);

// Bilinear plastic hinges on member end rotations (count = 0 clears): member id, dof (3..5 at node I, 9..11 at node J), elastic rotational stiffness,
// yield moment, hardening ratio (post-yield / elastic stiffness), axial yield force (0 = no N-M interaction; otherwise My,eff = My (1 - |N/Ny|^exponent))
// and the interaction exponent. Their presence runs the incremental nonlinear engine.
SOLVER_DLL_API void structura_set_plastic_hinges(int count, const int* memberIds, const int* dofs, const double* kElastic, const double* yieldMoment,
                                                 const double* hardeningRatio, const double* axialYield, const double* exponent);

// Constraints between nodes (penalty-imposed), replacing the previous list (all counts = 0 clears):
//  - rigid diaphragms: master node, normal axis (0 = X, 1 = Y, 2 = Z), slave count and slave node ids (concatenated)
//  - rigid links: master, slave, kind (0 = rigid body 6 dof, 1 = three translations equal, 2 = three rotations equal, 3 = a single dof equal) and dof (kind 3)
//  - inclined supports: node, 3 direction vectors e1,e2,e3 (9 values, global) and restraint mask (bits 0..2 = translations along e1..e3, bits 3..5 = rotations about them)
//  - general linear constraints: term count per constraint, then node id, dof (0..5) and coefficient of each term (concatenated), sum(coef*u) = 0;
//    reportReaction (0/1) adds the constraint force to the reactions
SOLVER_DLL_API void structura_set_constraints(int diaphragmCount, const int* diaMasters, const int* diaNormals, const int* diaSlaveCounts, const int* diaSlaves,
                                              int linkCount, const int* linkMasters, const int* linkSlaves, const int* linkKinds, const int* linkDofs,
                                              int inclinedCount, const int* incNodes, const double* incAxes9, const int* incMasks,
                                              int genericCount, const int* genTermCounts, const int* genNodes, const int* genDofs, const double* genCoefs,
                                              const int* genReport);

// Two-node links with a force-deformation law per relative dof (count = 0 clears). Per link: id, node I, node J, local axes (9 values; zeros = direction I->J),
// and per dof (6 per link): law kind (0 linear, 1 tension only, 2 compression only, 3 tension gap, 4 compression gap, 5 bilinear elastoplastic,
// 6 multilinear, 7 rigid), stiffness k, parameters p1 (gap or yield force) and p2 (hardening ratio), table point count; the multilinear points
// (deformation, force) of all dofs follow concatenated in tableDef/tableForce.
SOLVER_DLL_API void structura_set_links(int count, const int* ids, const int* nodeI, const int* nodeJ, const double* axes9,
                                        const int* lawKinds6, const double* lawK6, const double* lawP1_6, const double* lawP2_6, const int* tableCounts6,
                                        const double* tableDef, const double* tableForce);

// Incremental nonlinear analysis control. criteria: bit 0 = force residual, bit 1 = displacement increment, bit 2 = energy. controlMode: 0 = load control up
// to loadFactorMax, 1 = displacement control of (controlNode, controlDof) up to controlTarget. force = 1 runs the engine even without nonlinear elements.
SOLVER_DLL_API void structura_set_nonlinear_options(int force, int steps, int maxIterations, double tolerance, int criteria, int maxCutbacks, int controlMode,
                                                    int controlNode, int controlDof, double controlTarget, double loadFactorMax);

// Numerical infrastructure. linearSolver: 0 = direct sparse LDL^T (AMD ordering), 1 = preconditioned conjugate gradient (incomplete Cholesky; falls back to direct if it
// does not converge, and skips the mechanism check). eigenSolver: 0 = automatic (dense up to eigenDenseLimit free DOFs, sparse subspace iteration above), 1 = dense, 2 = sparse.
SOLVER_DLL_API void structura_set_solver_options(int linearSolver, int eigenSolver, int eigenDenseLimit, double iterativeTolerance, int iterativeMaxIterations);

// Results of the last solve: summary (used, converged, load factor, steps, iterations, curve points, hinges, links), capacity curve, hinge states
// (member id, dof, plastic rotation, moment, yielded), link results (id, 6 deformations, 6 forces) and the force of each constraint row.
SOLVER_DLL_API void structura_get_nonlinear_summary(double* out8);
SOLVER_DLL_API int structura_get_nonlinear_curve(int maxCount, double* out2);
SOLVER_DLL_API int structura_get_hinge_results(int maxCount, int* memberDof2, double* out3);
SOLVER_DLL_API int structura_get_link_results(int maxCount, int* ids, double* deformation6, double* force6);
SOLVER_DLL_API int structura_get_constraint_forces(int maxCount, double* forces);

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
