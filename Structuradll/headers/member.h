#pragma once
#include <algorithm>
#include <memory>
#include <array>
#include <vector>
#include <tuple>
#include <Eigen/Dense>
#include "warping.h"

// Forward declarations
class Node;
class Material;
class Section;

// Rotula plastica bilinear em uma rotacao de extremidade (dof 3,4,5 no no I; 9,10,11 no no J): mola elastica kElastic em serie com o elemento,
// momento de plastificacao yieldMoment (reduzido pelo esforco normal se axialYield > 0: M_y,eff = M_y (1 - |N/N_y|^exponent)) e razao de
// enrijecimento (rigidez pos-escoamento / elastica). O estado plastico e confirmado a cada passo de carga (enrijecimento cinematico).
struct PlasticHinge {
    bool enabled = false;
    double kElastic = 0.0;
    double yieldMoment = 0.0;
    double hardeningRatio = 0.0;
    double axialYield = 0.0;
    double exponent = 2.0;
    double plasticRotation = 0.0;   // confirmada
    double trialPlastic = 0.0;
    double moment = 0.0;            // momento transmitido na ultima avaliacao
    double tangent = 0.0;
    bool yielded = false;
};

class Member {
private:
    int id;
    std::shared_ptr<Node> nodeI;
    std::shared_ptr<Node> nodeJ;
    std::shared_ptr<Material> material;
    std::shared_ptr<Section> section;

    std::vector<double> internalForces;
    std::vector<double> fixedEndForces;

    // Cached local coordinate data to reuse in multiple queries
    mutable bool localAxesComputed = false;
    mutable Eigen::Vector3d cachedXLocal;
    mutable Eigen::Vector3d cachedYLocal;
    mutable Eigen::Vector3d cachedZLocal;
    mutable Eigen::Vector3d auxiliaryVectorK;

    // Section placement (CYPE 3D style): rotation about the member x-axis and
    // rigid offsets of the member (centroid) axis relative to each node,
    // expressed in the rotated local y/z axes.
    double sectionRotationRad = 0.0;
    double offsetY1 = 0.0, offsetZ1 = 0.0;
    double offsetY2 = 0.0, offsetZ2 = 0.0;

    // End releases (local DOF order: ux uy uz rx ry rz at node I, then at node J).
    // Released DOFs are condensed out of the stiffness matrix and fixed-end forces.
    std::array<bool, 12> releases{};
    // Molas de extremidade (rigidez do vinculo entre o no e a barra em cada grau de liberdade): < 0 = sem mola (rigido, salvo liberacao);
    // >= 0 = mola de rigidez dada (0 = articulacao). Entram na condensacao estatica junto com as liberacoes.
    std::array<double, 12> springK = {-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
    std::array<PlasticHinge, 12> hinges{};
    Eigen::VectorXd hingeState = Eigen::VectorXd::Zero(12);   // rotacoes locais dos graus de liberdade condensados (partida do Newton local)
    Eigen::VectorXd rawEquivalent = Eigen::VectorXd::Zero(12);
    std::vector<double> overridden;
    bool hasOverride = false;
    double lastAxial = 0.0;
    int lastHingeIterations = 0;
    mutable bool condensationCached = false;
    // Axial force (tension > 0) used by the geometric stiffness (second-order analysis).
    double geometricAxialForce = 0.0;
    double loadFactor = 1.0;
    // Rigid end zones (m along the member axis from each node): the flexible part has length L - rigidI - rigidJ
    double rigidI = 0.0, rigidJ = 0.0;
    std::vector<double> rigidFixedForces;   // reaction of the rigid end zones to their own loads (axis frame at the nodes)
    bool shearDeformation = false;      // Timoshenko bending (uses the section shear areas)
    bool linearizedGeometric = false;   // ignores stabilityFunctions (linear buckling)
    bool stabilityFunctions = false;    // second order by Livesley stability functions instead of the consistent geometric stiffness
    // Torsion with warping (7th dof at each end, "phi" = rate of twist): enabled when the section has Cw > 0
    bool warpingEnabled = false;
    int warpDofI = -1, warpDofJ = -1;           // global dof indices of the warping parameter at the two ends
    double warpValueI = 0.0, warpValueJ = 0.0;  // current values (set from the displacement vector)
    std::array<double, 2> warpEndForces{};      // generalized forces on the warping dofs after the last internal-forces pass
    double stiffnessFactor = 1.0;   // 1 = active; ~0 = deactivated tension-only member (reports zero internal forces)
    mutable Eigen::MatrixXd cachedCondensedK;
    Eigen::MatrixXd getFullLocalStiffnessMatrix() const;
    Eigen::MatrixXd calculateExtendedNodeToAxisMatrix() const;   // 14x14: node dofs + warping parameters (centroid axis -> shear center axis when the section has an offset)
    bool hasShearCenterOffset() const;                            // warping member whose shear center is not on the centroid
    Eigen::MatrixXd shearCenterMap() const;                       // 14x14 A: [v, w, theta, ry, rz, phi]_shear-center = A [..]_centroid at each end (identity without offset)

    void ensureLocalAxesComputed() const;
    // Graus de liberdade condensados (liberados ou com mola) e rigidez da mola em cada um
    bool isCondensedDof(int i) const { return releases[static_cast<size_t>(i)] || springK[static_cast<size_t>(i)] >= 0.0; }
    double springStiffness(int i) const { return releases[static_cast<size_t>(i)] ? 0.0 : std::max(springK[static_cast<size_t>(i)], 0.0); }

public:
    // Constructor
    Member(int id, std::shared_ptr<Node> nodeI, std::shared_ptr<Node> nodeJ,
        std::shared_ptr<Material> material, std::shared_ptr<Section> section);

    // Getters
    int getId() const;
    std::shared_ptr<Node> getNodeI() const;
    std::shared_ptr<Node> getNodeJ() const;
    std::shared_ptr<Material> getMaterial() const;
    std::shared_ptr<Section> getSection() const;

    // Geometric properties
    double calculateLength() const;
    std::vector<double> getDirectionCosines() const;
    std::tuple<Eigen::Vector3d, Eigen::Vector3d, Eigen::Vector3d> calculateLocalCoordinateSystem() const;
    Eigen::Vector3d getAuxiliaryVectorK() const;

    // Section rotation (degrees, right-hand rule about local x) and end offsets
    void setSectionRotationDegrees(double degrees);
    double getSectionRotationDegrees() const;
    void setEndOffsets(double dy1, double dz1, double dy2, double dz2);
    // Releases: 12 flags (ux uy uz rx ry rz at I, then J). true = released (hinge/slide)
    void setReleases(const std::array<bool, 12>& flags);
    // Verdadeiro se algum grau de liberdade e liberado ou tem mola (a barra precisa de condensacao estatica)
    bool hasReleases() const;
    // Molas de extremidade (semirrigidez): ver springK. `k` com 12 valores (ux uy uz rx ry rz em I e depois em J)
    void setEndSprings(const std::array<double, 12>& k);
    bool hasEndSprings() const;

    void setPlasticHinge(int dof, const PlasticHinge& hinge);
    const PlasticHinge& getPlasticHinge(int dof) const { return hinges[static_cast<size_t>(dof)]; }
    bool hasPlasticHinges() const;
    void commitHinges();
    void resetHinges();
    // Forcas que os nos exercem sobre a barra (eixos locais nas extremidades flexiveis) e rigidez tangente condensada para os deslocamentos
    // `uAxis` (12, eixos locais nas extremidades flexiveis) com as cargas multiplicadas por `loadFactor`; resolve o estado das rotulas.
    // Retorna false se o problema local nao convergiu.
    bool evaluateNonlinear(const Eigen::VectorXd& uAxis, double loadFactor, Eigen::VectorXd& force, Eigen::MatrixXd& tangent);
    // Soma das cargas equivalentes (nao condensadas) da parte flexivel, necessaria ao equilibrio local das barras com rotulas plasticas
    void addRawLoad(const Eigen::VectorXd& flexEquivalent);
    const Eigen::VectorXd& rawLoad() const { return rawEquivalent; }
    // Reacoes das zonas rigidas as suas proprias cargas (eixos do eixo da barra nos nos), ja com sinal de forca do no sobre a barra
    const std::vector<double>& rigidFixed() const { return rigidFixedForces; }
    // Esforcos de extremidade fornecidos pelo motor nao linear (substituem o calculo K u + f0 em calculateInternalForces)
    void overrideInternalForces(const std::vector<double>& forces) { overridden = forces; hasOverride = true; }
    void clearOverride() { hasOverride = false; }
    int hingeIterations() const { return lastHingeIterations; }
    void setLastAxial(double n) { lastAxial = n; }
    double axialForceEstimate() const { return lastAxial; }
    // Second-order: adds the geometric stiffness N/(30L)[...] for the given axial force (tension > 0)
    void setGeometricAxialForce(double axialForce);
    void setStiffnessFactor(double factor);
    // Analysis options: shear deformation (Timoshenko) and stability functions (exact P-delta / P-small-delta in the member)
    // Load factor applied to the fixed-end forces in calculateInternalForces (incremental second-order analysis)
    void setLoadFactor(double factor) { loadFactor = factor; }
    void setShearDeformation(bool enabled);
    // Torsion with warping: the member gets two extra dofs (rate of twist at each end) when the section has Cw > 0
    void setWarpingEnabled(bool enabled);
    bool hasWarping() const;
    int localDofCount() const { return hasWarping() ? 14 : 12; }
    void setWarpDofs(int dofI, int dofJ) { warpDofI = dofI; warpDofJ = dofJ; }
    int getWarpDofI() const { return warpDofI; }
    int getWarpDofJ() const { return warpDofJ; }
    void setWarpValues(double valueI, double valueJ) { warpValueI = valueI; warpValueJ = valueJ; }
    double getWarpValueI() const { return warpValueI; }
    double getWarpValueJ() const { return warpValueJ; }
    const std::array<double, 2>& getWarpEndForces() const { return warpEndForces; }
    // Reactions of clamped (theta = phi = 0) supports to a uniform torque m: (T_i, F_phi_i, T_j, F_phi_j); equivalent nodal loads are the negatives
    Eigen::Vector4d uniformTorqueFixedEnd(double m) const;
    // Rigid end zones: rigid links of the given lengths between each node and the flexible part
    void setRigidZones(double lengthI, double lengthJ);
    bool hasRigidZones() const { return rigidI > 0.0 || rigidJ > 0.0; }
    double getRigidI() const { return rigidI; }
    double getRigidJ() const { return rigidJ; }
    double calculateFlexibleLength() const;
    // 12x12 T with u_flex = T * u_axis_end (rigid links along x): uy += a rz, uz -= a ry at I; uy -= b rz, uz += b ry at J
    Eigen::MatrixXd getRigidZoneMatrix() const;
    // Equivalent nodal loads of the rigid end zones (axis frame at the node ends) for local load intensities q (force/length) and m (moment/length)
    Eigen::VectorXd rigidStubLoads(const Eigen::Vector3d& qLocal, const Eigen::Vector3d& mLocal) const;
    // Loads of a member with rigid zones: forces of the flexible part (clamped ends, local frame) and of the rigid zones (node ends)
    void addFixedEndLocal(const Eigen::VectorXd& flexLocal, const Eigen::VectorXd& rigidAtNodeEnds);
    void setStabilityFunctions(bool enabled);
    // True when the stability functions are applied: they need an Euler-Bernoulli member (no shear deformation)
    bool usesStabilityFunctions() const { return stabilityFunctions && !shearDeformation && !linearizedGeometric; }
    // Forces the consistent (linear in N) geometric stiffness even when stability functions are enabled: the linear buckling
    // eigenproblem (K0 + lambda Kg) needs Kg linear in the axial force
    void setLinearizedGeometric(bool enabled) { linearizedGeometric = enabled; condensationCached = false; }
    double getGeometricAxialForce() const { return geometricAxialForce; }
    // Fixed-end forces of the restrained bar -> forces after static condensation of the releases
    Eigen::VectorXd condenseFixedEndForces(const Eigen::VectorXd& localForces) const;
    bool hasEndOffsets() const;
    // 12x12 matrix T with u_axis = T * u_node (local axes): u += theta x (0, dy, dz)
    Eigen::MatrixXd getRigidOffsetMatrix() const;
    // Node -> bar-axis displacement map in local axes: getRigidOffsetMatrix() * rotation
    Eigen::MatrixXd calculateNodeToAxisMatrix(bool includeRigidZones = true) const;
    // Local forces acting on the bar axis -> equivalent global nodal forces
    Eigen::VectorXd localAxisForcesToGlobalNodal(const Eigen::VectorXd& localForces) const;

    // Structural analysis methods
    Eigen::MatrixXd calculateTransformationMatrix() const;
    Eigen::MatrixXd getLocalStiffnessMatrix() const;
    Eigen::MatrixXd getGlobalStiffnessMatrix() const;
    // Consistent mass matrix (global nodal DOFs) of the bar: rho*A and torsional inertia rho*(Iy+Iz); releases ignored
    Eigen::MatrixXd getGlobalMassMatrix() const;

    // CORRE��O: Novos m�todos para cargas distribu�das
    void addDistributedLoad(const std::vector<double>& globalEquivalentForces);
    void clearDistributedLoads();

    // Internal forces calculation
    void calculateInternalForces();
    const std::vector<double>& getInternalForces() const;
};
