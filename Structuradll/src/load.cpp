#include "load.h"
#include "member.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cctype>

// NodalLoad Implementation
NodalLoad::NodalLoad(int nodeId, const std::vector<double>& forces)
    : nodeId(nodeId), forces(forces) {

    // Ensure forces vector has exactly 6 elements
    if (this->forces.size() != 6) {
        throw std::invalid_argument("Forces vector must have exactly 6 elements");
    }
}

int NodalLoad::getNodeId() const {
    return nodeId;
}

double NodalLoad::getForce(int component) const {
    if (component < 0 || component >= 6) {
        throw std::out_of_range("Force component index must be between 0 and 5");
    }
    return forces[component];
}

const std::vector<double>& NodalLoad::getForces() const {
    return forces;
}


// Distributed load Implementation
DistributedLoad::DistributedLoad(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system)
    : memberId(memberId), intensity(intensity), system(system) {}

int DistributedLoad::getMemberId() const {
    return memberId;
}

const Eigen::Vector3d& DistributedLoad::getIntensity() const {
    return intensity;
}

DistributedLoad::CoordinateSystem DistributedLoad::getCoordinateSystem() const {
    return system;
}

void DistributedLoad::localComponents(const Member* member, Eigen::VectorXd& localForces, Eigen::VectorXd& rigidAtNodeEnds) const {
    if (!member) {
        throw std::invalid_argument("Member pointer cannot be null");
    }

    double L = member->calculateFlexibleLength();   // load of the flexible part; the rigid end zones are treated apart
    if (L <= 0.0) {
        throw std::invalid_argument("Member length must be positive to compute equivalent forces");
    }

    Eigen::Vector3d qLocal;
    if (system == CoordinateSystem::Local) {
        qLocal = intensity;
    }
    else {
        auto [x_local, y_local, z_local] = member->calculateLocalCoordinateSystem();
        Eigen::Matrix3d R;
        R.row(0) = x_local;
        R.row(1) = y_local;
        R.row(2) = z_local;
        qLocal = R * intensity;
    }

    double qx = qLocal.x();
    double qy = qLocal.y();
    double qz = qLocal.z();

    localForces = Eigen::VectorXd(12);
    localForces.setZero();

    // FORÇAS NODAIS EQUIVALENTES (aplicadas nos nós da estrutura)
    localForces(0) = qx * L / 2.0;      // Fx no nó I
    localForces(6) = qx * L / 2.0;      // Fx no nó J

    localForces(1) = qy * L / 2.0;      // Fy no nó I
    localForces(7) = qy * L / 2.0;      // Fy no nó J

    localForces(2) = qz * L / 2.0;      // Fz no nó I
    localForces(8) = qz * L / 2.0;      // Fz no nó J

    localForces(4) = -qz * L * L / 12.0;   // My no nó I
    localForces(10) = qz * L * L / 12.0;   // My no nó J

    localForces(5) = qy * L * L / 12.0;    // Mz no nó I
    localForces(11) = -qy * L * L / 12.0;  // Mz no nó J

    rigidAtNodeEnds = member->rigidStubLoads(qLocal, Eigen::Vector3d::Zero());
}

std::vector<double> DistributedLoad::getEquivalentNodalForces(const Member* member) const {
    Eigen::VectorXd localForces, rigid;
    localComponents(member, localForces, rigid);

    Eigen::VectorXd globalForces = member->localAxisForcesToGlobalNodal(localForces);
    if (member->hasRigidZones()) {
        globalForces.head(12) += member->calculateNodeToAxisMatrix(false).transpose() * rigid;
    }

    // 12 components; 14 when the member has torsion with warping (the last two are the equivalent loads of the warping parameters)
    std::vector<double> equivalentForces(static_cast<size_t>(globalForces.size() == 14 ? 14 : 12));
    for (size_t i = 0; i < equivalentForces.size(); ++i) {
        equivalentForces[i] = globalForces(static_cast<Eigen::Index>(i));
    }

    return equivalentForces;
}

// Distributed moment implementation
DistributedMoment::DistributedMoment(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system)
    : memberId(memberId), intensity(intensity), system(system) {}

int DistributedMoment::getMemberId() const {
    return memberId;
}

void DistributedMoment::localComponents(const Member* member, Eigen::VectorXd& localForces, Eigen::VectorXd& rigidAtNodeEnds) const {
    if (!member) {
        throw std::invalid_argument("Member pointer cannot be null");
    }

    const double L = member->calculateFlexibleLength();
    if (L <= 0.0) {
        throw std::invalid_argument("Member length must be positive to compute equivalent forces");
    }

    Eigen::Vector3d mLocal;
    if (system == CoordinateSystem::Local) {
        mLocal = intensity;
    }
    else {
        auto [x_local, y_local, z_local] = member->calculateLocalCoordinateSystem();
        Eigen::Matrix3d R;
        R.row(0) = x_local;
        R.row(1) = y_local;
        R.row(2) = z_local;
        mLocal = R * intensity;
    }

    // Uniform moment density m: the Hermite shape-function integrals give
    // nodal torsion m*L/2 and a shear couple (+-m) for bending moments.
    localForces = Eigen::VectorXd(12);
    localForces.setZero();

    localForces(3) = mLocal.x() * L / 2.0;
    localForces(9) = mLocal.x() * L / 2.0;

    localForces(2) = mLocal.y();
    localForces(8) = -mLocal.y();

    localForces(1) = -mLocal.z();
    localForces(7) = mLocal.z();

    rigidAtNodeEnds = member->rigidStubLoads(Eigen::Vector3d::Zero(), mLocal);

    if (member->hasWarping() && mLocal.x() != 0.0) {
        // exact equivalent loads of a uniform torque with warping: end torques m L/2 and the bimoment couple on the warping parameters
        const Eigen::Vector4d clamped = member->uniformTorqueFixedEnd(mLocal.x());
        Eigen::VectorXd extended = Eigen::VectorXd::Zero(14);
        extended.head(12) = localForces;
        extended(3) = -clamped(0);
        extended(9) = -clamped(2);
        extended(12) = -clamped(1);
        extended(13) = -clamped(3);
        localForces = extended;
    }
}

std::vector<double> DistributedMoment::getEquivalentNodalForces(const Member* member) const {
    Eigen::VectorXd localForces, rigid;
    localComponents(member, localForces, rigid);

    Eigen::VectorXd globalForces = member->localAxisForcesToGlobalNodal(localForces);
    if (member->hasRigidZones()) {
        globalForces.head(12) += member->calculateNodeToAxisMatrix(false).transpose() * rigid;
    }

    // 12 components; 14 when the member has torsion with warping (the last two are the equivalent loads of the warping parameters)
    std::vector<double> equivalentForces(static_cast<size_t>(globalForces.size() == 14 ? 14 : 12));
    for (size_t i = 0; i < equivalentForces.size(); ++i) {
        equivalentForces[i] = globalForces(static_cast<Eigen::Index>(i));
    }
    return equivalentForces;
}

DistributedLoad::CoordinateSystem DistributedLoad::coordinateSystemFromString(const std::string& token) {
    std::string normalized = token;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (normalized == "LOCAL" || normalized == "L") {
        return CoordinateSystem::Local;
    }
    if (normalized == "GLOBAL" || normalized == "G") {
        return CoordinateSystem::Global;
    }

    throw std::invalid_argument("Unknown distributed load coordinate system: " + token);
}

std::string DistributedLoad::coordinateSystemToString(CoordinateSystem system) {
    return system == CoordinateSystem::Local ? "LOCAL" : "GLOBAL";
}
