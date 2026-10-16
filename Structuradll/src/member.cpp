#include "member.h"
#include "node.h"
#include "material.h"
#include "section.h"
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <array>
#include <Eigen/Dense>

namespace {
constexpr double GEOM_TOL = 1e-10;

// Selects a reference vector to define the local x-y plane (point K concept) Sennett book.
Eigen::Vector3d selectReferenceVector(const Eigen::Vector3d& xAxis) {
    const std::array<Eigen::Vector3d, 3> candidates = {
        Eigen::Vector3d(0.0, 0.0, 1.0),  // global Z axis
        Eigen::Vector3d(0.0, 1.0, 0.0),  // global Y axis
        Eigen::Vector3d(1.0, 0.0, 0.0)   // global X axis
    };

    for (const auto& candidate : candidates) {
        if (xAxis.cross(candidate).norm() > GEOM_TOL) {
            return candidate.normalized();
        }
    }

    // Degenerate case: pick a generic vector that is not parallel to the local x-axis.
    Eigen::Vector3d fallback(1.0, 1.0, 0.0);
    if (fallback.cross(xAxis).norm() <= GEOM_TOL) {
        fallback = Eigen::Vector3d(1.0, 0.0, 1.0);
    }
    return fallback.normalized();
}
} // namespace

Member::Member(int id, std::shared_ptr<Node> nodeI, std::shared_ptr<Node> nodeJ,
    std::shared_ptr<Material> material, std::shared_ptr<Section> section)
    : id(id), nodeI(nodeI), nodeJ(nodeJ), material(material), section(section) {

    // Initialize internal forces vector
    internalForces.resize(12, 0.0);
    fixedEndForces.resize(12, 0.0);
    localAxesComputed = false;

    // Validate inputs
    if (!nodeI || !nodeJ || !material || !section) {
        throw std::invalid_argument("All member components must be valid pointers");
    }

    // Check for zero length member
    if (calculateLength() < 1e-12) {
        throw std::invalid_argument("Member length is too small");
    }
}

int Member::getId() const {
    return id;
}

std::shared_ptr<Node> Member::getNodeI() const {
    return nodeI;
}

std::shared_ptr<Node> Member::getNodeJ() const {
    return nodeJ;
}

std::shared_ptr<Material> Member::getMaterial() const {
    return material;
}

std::shared_ptr<Section> Member::getSection() const {
    return section;
}

double Member::calculateLength() const {
    double dx = nodeJ->getX() - nodeI->getX();
    double dy = nodeJ->getY() - nodeI->getY();
    double dz = nodeJ->getZ() - nodeI->getZ();
    return sqrt(dx * dx + dy * dy + dz * dz);
}

std::vector<double> Member::getDirectionCosines() const {
    double L = calculateLength();
    if (L < 1e-12) {
        throw std::runtime_error("Member length is too small to calculate direction cosines");
    }
    double dx = nodeJ->getX() - nodeI->getX();
    double dy = nodeJ->getY() - nodeI->getY();
    double dz = nodeJ->getZ() - nodeI->getZ();
    return { dx / L, dy / L, dz / L };
}

void Member::ensureLocalAxesComputed() const {
    if (localAxesComputed) {
        return;
    }

    // Node I (point A) and node J (point B) of the element
    Eigen::Vector3d pointA(nodeI->getX(), nodeI->getY(), nodeI->getZ());
    Eigen::Vector3d pointB(nodeJ->getX(), nodeJ->getY(), nodeJ->getZ());

    Eigen::Vector3d vectorX = pointB - pointA;
    double length = vectorX.norm();
    if (length < GEOM_TOL) {
        throw std::runtime_error("Member length is too small to define local axes");
    }

    // Local x-axis aligned with the member
    Eigen::Vector3d x_local = vectorX / length;

    // K Vector
    Eigen::Vector3d vectorK = selectReferenceVector(x_local);

    
    Eigen::Vector3d z_local = x_local.cross(vectorK);
    double z_norm = z_local.norm();
    
    if (z_norm < GEOM_TOL) {
        // if K is to close to be colinear
        const std::array<Eigen::Vector3d, 3> alternatives = {
            Eigen::Vector3d(0.0, 1.0, 0.0),
            Eigen::Vector3d(1.0, 0.0, 0.0),
            Eigen::Vector3d(1.0, 1.0, 0.0)
        };

        bool found = false;
        for (const auto& alt : alternatives) {
            Eigen::Vector3d normalizedAlt = alt.normalized();
            Eigen::Vector3d test_z = x_local.cross(normalizedAlt);
            double test_norm = test_z.norm();
            if (test_norm > GEOM_TOL) {
                z_local = test_z;
                z_norm = test_norm;
                vectorK = normalizedAlt;
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error("Unable to determine a valid local z-axis for the member");
        }
    }
    
    z_local = z_local / z_norm;

    Eigen::Vector3d y_local = z_local.cross(x_local);

    double y_norm = y_local.norm();
    if (y_norm < GEOM_TOL) {
        throw std::runtime_error("Unable to determine a valid local y-axis for the member");
    }

    y_local = y_local / y_norm;

    // Section rotation about x (right-hand rule): y' = cos*y + sin*z, z' = -sin*y + cos*z
    if (std::abs(sectionRotationRad) > 0.0) {
        const double c = std::cos(sectionRotationRad);
        const double sn = std::sin(sectionRotationRad);
        const Eigen::Vector3d y0 = y_local;
        const Eigen::Vector3d z0 = z_local;
        y_local = c * y0 + sn * z0;
        z_local = -sn * y0 + c * z0;
        // Report y' as the auxiliary vector: x cross y' = z', so consumers that rebuild
        // the axes with z = x cross aux, y = z cross x recover the rotated frame.
        vectorK = y_local;
    }

    cachedXLocal = x_local;
    cachedYLocal = y_local;
    cachedZLocal = z_local;
    auxiliaryVectorK = vectorK;
    localAxesComputed = true;
}

std::tuple<Eigen::Vector3d, Eigen::Vector3d, Eigen::Vector3d>
Member::calculateLocalCoordinateSystem() const {
    ensureLocalAxesComputed();
    return std::make_tuple(cachedXLocal, cachedYLocal, cachedZLocal);
}

Eigen::Vector3d Member::getAuxiliaryVectorK() const {
    ensureLocalAxesComputed();
    return auxiliaryVectorK;
}

void Member::setSectionRotationDegrees(double degrees) {
    constexpr double kPi = 3.14159265358979323846;
    sectionRotationRad = degrees * kPi / 180.0;
    localAxesComputed = false;
}

double Member::getSectionRotationDegrees() const {
    constexpr double kPi = 3.14159265358979323846;
    return sectionRotationRad * 180.0 / kPi;
}

void Member::setEndOffsets(double dy1, double dz1, double dy2, double dz2) {
    offsetY1 = dy1;
    offsetZ1 = dz1;
    offsetY2 = dy2;
    offsetZ2 = dz2;
}

bool Member::hasEndOffsets() const {
    return std::abs(offsetY1) > 0.0 || std::abs(offsetZ1) > 0.0 ||
           std::abs(offsetY2) > 0.0 || std::abs(offsetZ2) > 0.0;
}

Eigen::MatrixXd Member::getRigidOffsetMatrix() const {
    // u_axis = u_node + theta x e, e = (0, dy, dz) in local axes:
    //   ux += ry*dz - rz*dy ; uy += -rx*dz ; uz += rx*dy
    Eigen::MatrixXd T = Eigen::MatrixXd::Identity(12, 12);
    const double dy[2] = {offsetY1, offsetY2};
    const double dz[2] = {offsetZ1, offsetZ2};
    for (int end = 0; end < 2; ++end) {
        const int r = end * 6;
        T(r + 0, r + 4) = dz[end];
        T(r + 0, r + 5) = -dy[end];
        T(r + 1, r + 3) = -dz[end];
        T(r + 2, r + 3) = dy[end];
    }
    return T;
}

Eigen::MatrixXd Member::calculateNodeToAxisMatrix() const {
    return getRigidOffsetMatrix() * calculateTransformationMatrix();
}

Eigen::VectorXd Member::localAxisForcesToGlobalNodal(const Eigen::VectorXd& localForces) const {
    return calculateNodeToAxisMatrix().transpose() * condenseFixedEndForces(localForces);
}

Eigen::MatrixXd Member::calculateTransformationMatrix() const {
    auto [x_local, y_local, z_local] = calculateLocalCoordinateSystem();

    // 3x3 rotation matrix
    Eigen::Matrix3d R;
    R.row(0) = x_local;
    R.row(1) = y_local;
    R.row(2) = z_local;

    // 12x12 transformation matrix
    Eigen::MatrixXd T = Eigen::MatrixXd::Zero(12, 12);
    for (int i = 0; i < 4; ++i) {
        T.block<3, 3>(i * 3, i * 3) = R;
    }

    return T;
}

Eigen::MatrixXd Member::getFullLocalStiffnessMatrix() const {
    double E = material->getE();
    double G = material->getG();
    double A = section->getA();
    double Iz = section->getIz();
    double Iy = section->getIy();
    double J = section->getJ();
    double L = calculateLength();

    double L2 = L * L;
    double L3 = L * L * L;

    // Pre-calculate common terms
    double EA_L = E * A / L;
    double GJ_L = G * J / L;
    double EIy_L = E * Iy / L;
    double EIz_L = E * Iz / L;
    double EIy_L2 = E * Iy / L2;
    double EIz_L2 = E * Iz / L2;
    double EIy_L3 = E * Iy / L3;
    double EIz_L3 = E * Iz / L3;

    Eigen::MatrixXd K_local = Eigen::MatrixXd::Zero(12, 12);

    // Populate the upper triangle of the matrix
    K_local(0, 0) = EA_L;
    K_local(0, 6) = -EA_L;

    K_local(1, 1) = 12 * EIz_L3;
    K_local(1, 5) = 6 * EIz_L2;
    K_local(1, 7) = -12 * EIz_L3;
    K_local(1, 11) = 6 * EIz_L2;

    K_local(2, 2) = 12 * EIy_L3;
    K_local(2, 4) = -6 * EIy_L2;
    K_local(2, 8) = -12 * EIy_L3;
    K_local(2, 10) = -6 * EIy_L2;

    K_local(3, 3) = GJ_L;
    K_local(3, 9) = -GJ_L;

    K_local(4, 4) = 4 * EIy_L;
    K_local(4, 8) = 6 * EIy_L2;
    K_local(4, 10) = 2 * EIy_L;

    K_local(5, 5) = 4 * EIz_L;
    K_local(5, 7) = -6 * EIz_L2;
    K_local(5, 11) = 2 * EIz_L;

    K_local(6, 6) = EA_L;

    K_local(7, 7) = 12 * EIz_L3;
    K_local(7, 11) = -6 * EIz_L2;

    K_local(8, 8) = 12 * EIy_L3;
    K_local(8, 10) = 6 * EIy_L2;

    K_local(9, 9) = GJ_L;

    K_local(10, 10) = 4 * EIy_L;

    K_local(11, 11) = 4 * EIz_L;

    // Use symmetry to fill the lower triangle
    K_local.triangularView<Eigen::Lower>() = K_local.transpose();

    if (geometricAxialForce != 0.0) {
        // Geometric stiffness (consistent, cubic interpolation): captures P-delta (sway) and P-small-delta.
        const double c = geometricAxialForce / (30.0 * L);
        const double a = 36.0 * c, b = 3.0 * L * c, d = 4.0 * L2 * c, e = -L2 * c;
        // bending in the x-y plane: dofs 1 (uy), 5 (rz), 7 (uy), 11 (rz)
        const int p[4] = {1, 5, 7, 11};
        const double ky[4][4] = {{a, b, -a, b}, {b, d, -b, e}, {-a, -b, a, -b}, {b, e, -b, d}};
        // bending in the x-z plane: dofs 2 (uz), 4 (ry), 8 (uz), 10 (ry); rotations change sign
        const int q[4] = {2, 4, 8, 10};
        const double kz[4][4] = {{a, -b, -a, -b}, {-b, d, b, e}, {-a, b, a, b}, {-b, e, b, d}};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                K_local(p[i], p[j]) += ky[i][j];
                K_local(q[i], q[j]) += kz[i][j];
            }
        }
    }

    if (stiffnessFactor != 1.0) {
        K_local *= stiffnessFactor;
    }
    return K_local;
}

void Member::setReleases(const std::array<bool, 12>& flags) {
    releases = flags;
    condensationCached = false;
}

void Member::setStiffnessFactor(double factor) {
    stiffnessFactor = factor;
    condensationCached = false;
}

void Member::setGeometricAxialForce(double axialForce) {
    geometricAxialForce = axialForce;
    condensationCached = false;
}

bool Member::hasReleases() const {
    for (const bool released : releases) {
        if (released) {
            return true;
        }
    }
    return false;
}

namespace {
// Partition indices of retained (A) and released (R) DOFs
void partitionDofs(const std::array<bool, 12>& releases, std::vector<int>& retained, std::vector<int>& released) {
    for (int i = 0; i < 12; ++i) {
        (releases[static_cast<size_t>(i)] ? released : retained).push_back(i);
    }
}
} // namespace

Eigen::MatrixXd Member::getLocalStiffnessMatrix() const {
    if (!hasReleases()) {
        return getFullLocalStiffnessMatrix();
    }
    if (condensationCached) {
        return cachedCondensedK;
    }

    const Eigen::MatrixXd K = getFullLocalStiffnessMatrix();
    std::vector<int> a, r;
    partitionDofs(releases, a, r);

    Eigen::MatrixXd Kaa(a.size(), a.size()), Kar(a.size(), r.size()), Krr(r.size(), r.size());
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < a.size(); ++j) Kaa(i, j) = K(a[i], a[j]);
        for (size_t j = 0; j < r.size(); ++j) Kar(i, j) = K(a[i], r[j]);
    }
    for (size_t i = 0; i < r.size(); ++i) {
        for (size_t j = 0; j < r.size(); ++j) Krr(i, j) = K(r[i], r[j]);
    }

    Eigen::FullPivLU<Eigen::MatrixXd> lu(Krr);
    if (!lu.isInvertible()) {
        throw std::runtime_error("Invalid end releases on member " + std::to_string(id) +
                                 " (the released DOFs form a mechanism)");
    }
    const Eigen::MatrixXd Kc = Kaa - Kar * lu.solve(Kar.transpose());

    Eigen::MatrixXd result = Eigen::MatrixXd::Zero(12, 12);
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < a.size(); ++j) {
            result(a[i], a[j]) = Kc(i, j);
        }
    }
    cachedCondensedK = result;
    condensationCached = true;
    return result;
}

Eigen::VectorXd Member::condenseFixedEndForces(const Eigen::VectorXd& localForces) const {
    if (!hasReleases()) {
        return localForces;
    }
    const Eigen::MatrixXd K = getFullLocalStiffnessMatrix();
    std::vector<int> a, r;
    partitionDofs(releases, a, r);

    Eigen::MatrixXd Kar(a.size(), r.size()), Krr(r.size(), r.size());
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < r.size(); ++j) Kar(i, j) = K(a[i], r[j]);
    }
    for (size_t i = 0; i < r.size(); ++i) {
        for (size_t j = 0; j < r.size(); ++j) Krr(i, j) = K(r[i], r[j]);
    }
    Eigen::VectorXd fa(a.size()), fr(r.size());
    for (size_t i = 0; i < a.size(); ++i) fa(i) = localForces(a[i]);
    for (size_t i = 0; i < r.size(); ++i) fr(i) = localForces(r[i]);

    Eigen::FullPivLU<Eigen::MatrixXd> lu(Krr);
    if (!lu.isInvertible()) {
        throw std::runtime_error("Invalid end releases on member " + std::to_string(id) +
                                 " (the released DOFs form a mechanism)");
    }
    const Eigen::VectorXd fc = fa - Kar * lu.solve(fr);

    Eigen::VectorXd result = Eigen::VectorXd::Zero(12);
    for (size_t i = 0; i < a.size(); ++i) {
        result(a[i]) = fc(i);
    }
    return result;
}

Eigen::MatrixXd Member::getGlobalStiffnessMatrix() const {
    Eigen::MatrixXd K_local = getLocalStiffnessMatrix();
    Eigen::MatrixXd T = calculateNodeToAxisMatrix();
    return T.transpose() * K_local * T;
}

Eigen::MatrixXd Member::getGlobalMassMatrix() const {
    const double rho = material->getDensity();
    const double L = calculateLength();
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(12, 12);
    if (rho <= 0.0 || L <= 0.0) {
        return m;
    }
    const double A = section->getA();
    const double mass = rho * A * L;
    // axial
    m(0, 0) = m(6, 6) = mass / 3.0;
    m(0, 6) = m(6, 0) = mass / 6.0;
    // torsion
    const double ip = rho * (section->getIy() + section->getIz()) * L;
    m(3, 3) = m(9, 9) = ip / 3.0;
    m(3, 9) = m(9, 3) = ip / 6.0;
    const double c = mass / 420.0;
    // bending in the local xy plane: uy (1,7), rz (5,11)
    const int xy[4] = {1, 5, 7, 11};
    const double bxy[4][4] = {{156, 22 * L, 54, -13 * L}, {22 * L, 4 * L * L, 13 * L, -3 * L * L}, {54, 13 * L, 156, -22 * L}, {-13 * L, -3 * L * L, -22 * L, 4 * L * L}};
    // bending in the local xz plane: uz (2,8), ry (4,10)
    const int xz[4] = {2, 4, 8, 10};
    const double bxz[4][4] = {{156, -22 * L, 54, 13 * L}, {-22 * L, 4 * L * L, -13 * L, -3 * L * L}, {54, -13 * L, 156, 22 * L}, {13 * L, -3 * L * L, 22 * L, 4 * L * L}};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m(xy[i], xy[j]) += c * bxy[i][j];
            m(xz[i], xz[j]) += c * bxz[i][j];
        }
    }
    const Eigen::MatrixXd T = calculateNodeToAxisMatrix();
    return T.transpose() * m * T;
}

void Member::addDistributedLoad(const std::vector<double>& globalEquivalentForces) {
    if (globalEquivalentForces.size() != 12) {
        throw std::invalid_argument("Distributed load vector must have 12 components");
    }

    Eigen::VectorXd F_global(12);
    for (int i = 0; i < 12; ++i) {
        F_global(i) = globalEquivalentForces[i];
    }

    // F_global = (Toff * R)^T * f_axis  ->  f_axis = R * Toff^-T * F_global
    Eigen::VectorXd F_local;
    if (hasEndOffsets()) {
        const Eigen::MatrixXd Toff = getRigidOffsetMatrix();
        const Eigen::MatrixXd R = calculateTransformationMatrix();
        const Eigen::VectorXd rotated = R * F_global;
        F_local = Toff.transpose().fullPivLu().solve(rotated);
    } else {
        F_local = calculateTransformationMatrix() * F_global;
    }

    for (int i = 0; i < 12; ++i) {
        fixedEndForces[i] += -F_local(i);  // f0 = -F_eq (reacao dos engastes perfeitos sobre a barra)
    }
}

void Member::calculateInternalForces() {
    // Get global displacements for nodes I and J
    Eigen::VectorXd U_global = Eigen::VectorXd::Zero(12);
    for (int i = 0; i < 6; ++i) {
        U_global(i) = nodeI->getDisplacement(i);
        U_global(i + 6) = nodeJ->getDisplacement(i);
    }

    Eigen::MatrixXd T = calculateNodeToAxisMatrix();
    Eigen::MatrixXd K_local = getLocalStiffnessMatrix();

    // Calculate local (bar-axis) displacements: u_local = Toff * R * U_global
    Eigen::VectorXd u_local = T * U_global;

    // Calculate local forces due to displacements: f_displacement = K_local * u_local
    Eigen::VectorXd f_displacement = K_local * u_local;

    Eigen::VectorXd f_local = f_displacement;

    for (int i = 0; i < 12; ++i) {
        f_local(i) += fixedEndForces[i];   // f = K u + f0, f0 = fixed-end (clamped) forces = -F_equivalent
    }

    // Store results in the member variable
    for (int i = 0; i < 12; ++i) {
        internalForces[i] = f_local(i);
    }
}

const std::vector<double>& Member::getInternalForces() const {
    return internalForces;
}

void Member::clearDistributedLoads() {
    std::fill(fixedEndForces.begin(), fixedEndForces.end(), 0.0);
}
