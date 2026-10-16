#pragma once
#include <memory>
#include <array>
#include <vector>
#include <tuple>
#include <Eigen/Dense>

// Forward declarations
class Node;
class Material;
class Section;

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
    mutable bool condensationCached = false;
    // Axial force (tension > 0) used by the geometric stiffness (second-order analysis).
    double geometricAxialForce = 0.0;
    double stiffnessFactor = 1.0;   // 1 = active; ~0 = deactivated tension-only member
    mutable Eigen::MatrixXd cachedCondensedK;
    Eigen::MatrixXd getFullLocalStiffnessMatrix() const;

    void ensureLocalAxesComputed() const;

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
    bool hasReleases() const;
    // Second-order: adds the geometric stiffness N/(30L)[...] for the given axial force (tension > 0)
    void setGeometricAxialForce(double axialForce);
    void setStiffnessFactor(double factor);
    double getGeometricAxialForce() const { return geometricAxialForce; }
    // Fixed-end forces of the restrained bar -> forces after static condensation of the releases
    Eigen::VectorXd condenseFixedEndForces(const Eigen::VectorXd& localForces) const;
    bool hasEndOffsets() const;
    // 12x12 matrix T with u_axis = T * u_node (local axes): u += theta x (0, dy, dz)
    Eigen::MatrixXd getRigidOffsetMatrix() const;
    // Node -> bar-axis displacement map in local axes: getRigidOffsetMatrix() * rotation
    Eigen::MatrixXd calculateNodeToAxisMatrix() const;
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
