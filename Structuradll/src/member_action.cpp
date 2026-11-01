#include "load.h"
#include "material.h"
#include "member.h"
#include "section.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

// Cargas gerais de barra com posicao: trapezoidal/parcial, pontual, momento pontual e deformacao imposta (temperatura e retracao).
// As cargas equivalentes da parte flexivel usam as funcoes de forma de Hermite (viga de Euler-Bernoulli, as mesmas das cargas uniformes);
// o trecho de carga dentro de uma zona rigida e levado ao no por estatica.

MemberAction::MemberAction(int memberId, Kind kind, CoordinateSystem system, double a, double b, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2)
    : memberId(memberId), kind(kind), system(system), a(a), b(b), v1(v1), v2(v2) {}

namespace {

// Gauss-Legendre de 5 pontos em [-1, 1]: exato para polinomios ate o grau 9 (carga linear x funcoes de forma cubicas = grau 4)
constexpr std::array<double, 5> kGaussX = {0.0, -0.5384693101056831, 0.5384693101056831, -0.9061798459386640, 0.9061798459386640};
constexpr std::array<double, 5> kGaussW = {0.5688888888888889, 0.4786286704993665, 0.4786286704993665, 0.2369268850561891, 0.2369268850561891};

// Acumulador das cargas equivalentes de uma barra com zonas rigidas: parte flexivel (funcoes de forma) e zonas rigidas (nos).
struct EquivalentLoads {
    double length = 0.0;    // comprimento total sobre o eixo
    double rigidI = 0.0, rigidJ = 0.0;
    double flex = 0.0;      // comprimento flexivel
    Eigen::VectorXd flexLocal = Eigen::VectorXd::Zero(12);
    Eigen::VectorXd rigid = Eigen::VectorXd::Zero(12);

    // Forca P (eixos locais) na posicao s, com peso w (w = ds nas cargas distribuidas, 1 nas concentradas)
    void addForce(double s, const Eigen::Vector3d& P, double w) {
        if (s < rigidI) {
            rigid(0) += P.x() * w;
            rigid(1) += P.y() * w;
            rigid(2) += P.z() * w;
            rigid(4) += -s * P.z() * w;
            rigid(5) += s * P.y() * w;
            return;
        }
        if (s > length - rigidJ) {
            const double d = length - s;
            rigid(6) += P.x() * w;
            rigid(7) += P.y() * w;
            rigid(8) += P.z() * w;
            rigid(10) += d * P.z() * w;
            rigid(11) += -d * P.y() * w;
            return;
        }
        const double xi = (s - rigidI) / flex;
        const double xi2 = xi * xi, xi3 = xi2 * xi;
        const double n1 = 1.0 - 3.0 * xi2 + 2.0 * xi3, n2 = flex * (xi - 2.0 * xi2 + xi3);
        const double n3 = 3.0 * xi2 - 2.0 * xi3, n4 = flex * (-xi2 + xi3);
        flexLocal(0) += (1.0 - xi) * P.x() * w;
        flexLocal(6) += xi * P.x() * w;
        flexLocal(1) += n1 * P.y() * w;
        flexLocal(5) += n2 * P.y() * w;
        flexLocal(7) += n3 * P.y() * w;
        flexLocal(11) += n4 * P.y() * w;
        flexLocal(2) += n1 * P.z() * w;
        flexLocal(4) -= n2 * P.z() * w;
        flexLocal(8) += n3 * P.z() * w;
        flexLocal(10) -= n4 * P.z() * w;
    }

    // Momento concentrado M (eixos locais) na posicao s
    void addMoment(double s, const Eigen::Vector3d& M) {
        if (s < rigidI) {
            rigid(3) += M.x();
            rigid(4) += M.y();
            rigid(5) += M.z();
            return;
        }
        if (s > length - rigidJ) {
            rigid(9) += M.x();
            rigid(10) += M.y();
            rigid(11) += M.z();
            return;
        }
        const double xi = (s - rigidI) / flex;
        const double xi2 = xi * xi;
        // derivadas das funcoes de forma em relacao a x
        const double d1 = (-6.0 * xi + 6.0 * xi2) / flex, d2 = 1.0 - 4.0 * xi + 3.0 * xi2;
        const double d3 = (6.0 * xi - 6.0 * xi2) / flex, d4 = -2.0 * xi + 3.0 * xi2;
        flexLocal(3) += (1.0 - xi) * M.x();
        flexLocal(9) += xi * M.x();
        // plano x-z: theta_y = -dw/dx ; plano x-y: theta_z = dv/dx
        flexLocal(2) += -M.y() * d1;
        flexLocal(4) += M.y() * d2;
        flexLocal(8) += -M.y() * d3;
        flexLocal(10) += M.y() * d4;
        flexLocal(1) += M.z() * d1;
        flexLocal(5) += M.z() * d2;
        flexLocal(7) += M.z() * d3;
        flexLocal(11) += M.z() * d4;
    }
};

} // namespace

void MemberAction::localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const {
    if (!member) {
        throw std::invalid_argument("Member pointer cannot be null");
    }
    EquivalentLoads acc;
    acc.length = member->calculateLength();
    acc.rigidI = member->getRigidI();
    acc.rigidJ = member->getRigidJ();
    acc.flex = member->calculateFlexibleLength();
    if (acc.flex <= 0.0) {
        throw std::invalid_argument("Member length must be positive to compute equivalent forces");
    }

    Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
    if (system == CoordinateSystem::Global && kind != Kind::Strain) {
        auto [x_local, y_local, z_local] = member->calculateLocalCoordinateSystem();
        R.row(0) = x_local;
        R.row(1) = y_local;
        R.row(2) = z_local;
    }
    const double tol = 1e-9 * acc.length;

    switch (kind) {
    case Kind::Trapezoid: {
        const double s0 = std::max(0.0, std::min(a, b)), s1 = std::min(acc.length, std::max(a, b));
        if (s1 - s0 <= tol) {
            throw std::invalid_argument("Trapezoidal load on member " + std::to_string(memberId) + " has no length inside the member");
        }
        const double span = std::max(b - a, tol);
        const Eigen::Vector3d q1 = R * v1, q2 = R * v2;
        // pontos de quebra nas fronteiras das zonas rigidas: em cada trecho o integrando e polinomial
        std::vector<double> cuts = {s0, s1};
        for (const double c : {acc.rigidI, acc.length - acc.rigidJ}) {
            if (c > s0 + tol && c < s1 - tol) {
                cuts.push_back(c);
            }
        }
        std::sort(cuts.begin(), cuts.end());
        for (size_t c = 0; c + 1 < cuts.size(); ++c) {
            const double lo = cuts[c], hi = cuts[c + 1];
            for (size_t g = 0; g < kGaussX.size(); ++g) {
                const double s = 0.5 * (lo + hi) + 0.5 * (hi - lo) * kGaussX[g];
                const double t = (s - a) / span;
                // o ponto de Gauss nunca cai exatamente na fronteira, entao a zona e decidida sem ambiguidade
                acc.addForce(s, q1 + (q2 - q1) * t, 0.5 * (hi - lo) * kGaussW[g]);
            }
        }
        break;
    }
    case Kind::PointForce: {
        if (a < -tol || a > acc.length + tol) {
            throw std::invalid_argument("Point load on member " + std::to_string(memberId) + " is outside the member");
        }
        acc.addForce(std::min(std::max(a, 0.0), acc.length), R * v1, 1.0);
        break;
    }
    case Kind::PointMoment: {
        if (a < -tol || a > acc.length + tol) {
            throw std::invalid_argument("Point moment on member " + std::to_string(memberId) + " is outside the member");
        }
        acc.addMoment(std::min(std::max(a, 0.0), acc.length), R * v1);
        break;
    }
    case Kind::Strain: {
        const double E = member->getMaterial()->getE();
        const double EA = E * member->getSection()->getA();
        // deformacao livre eps(y,z) = e0 + ky y + kz z: forca axial E A e0 e curvaturas livres v'' = -ky, w'' = -kz
        acc.flexLocal(0) = -EA * v1.x();
        acc.flexLocal(6) = EA * v1.x();
        const double EIz = E * member->getSection()->getIz(), EIy = E * member->getSection()->getIy();
        acc.flexLocal(5) = EIz * v1.y();
        acc.flexLocal(11) = -EIz * v1.y();
        acc.flexLocal(4) = -EIy * v1.z();
        acc.flexLocal(10) = EIy * v1.z();
        break;
    }
    }

    if (member->hasWarping() && (acc.flexLocal(3) != 0.0 || acc.flexLocal(9) != 0.0 || acc.rigid(3) != 0.0 || acc.rigid(9) != 0.0)) {
        throw std::invalid_argument("Member " + std::to_string(memberId) + ": point torque on a member with warping torsion is not supported");
    }
    flexLocal = acc.flexLocal;
    rigidAtNodeEnds = acc.rigid;
}
