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

std::vector<double> DistributedLoad::getEquivalentNodalForces(const Member* member) const {
    if (!member) {
        throw std::invalid_argument("Member pointer cannot be null");
    }

    double L = member->calculateLength();
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

    Eigen::VectorXd localForces(12);
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


    Eigen::VectorXd globalForces = member->localAxisForcesToGlobalNodal(localForces);

    std::vector<double> equivalentForces(12);
    for (int i = 0; i < 12; ++i) {
        equivalentForces[i] = globalForces(i);
    }

    return equivalentForces;
}

// Distributed moment implementation
DistributedMoment::DistributedMoment(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system)
    : memberId(memberId), intensity(intensity), system(system) {}

int DistributedMoment::getMemberId() const {
    return memberId;
}

std::vector<double> DistributedMoment::getEquivalentNodalForces(const Member* member) const {
    if (!member) {
        throw std::invalid_argument("Member pointer cannot be null");
    }

    const double L = member->calculateLength();
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
    Eigen::VectorXd localForces(12);
    localForces.setZero();

    localForces(3) = mLocal.x() * L / 2.0;
    localForces(9) = mLocal.x() * L / 2.0;

    localForces(2) = mLocal.y();
    localForces(8) = -mLocal.y();

    localForces(1) = -mLocal.z();
    localForces(7) = mLocal.z();

    Eigen::VectorXd globalForces = member->localAxisForcesToGlobalNodal(localForces);

    std::vector<double> equivalentForces(12);
    for (int i = 0; i < 12; ++i) {
        equivalentForces[i] = globalForces(i);
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
