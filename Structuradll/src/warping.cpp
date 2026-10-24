#include "warping.h"

#include <cmath>

namespace Warping {

namespace {

// General solution theta = a0 + a1 x + a2 exp(-k x) + a3 exp(-k (L - x)) (numerically stable for large k L)
struct Basis {
    double k {0.0};
    double L {0.0};

    // rows: theta, theta', theta'', theta''' at x; columns: a0..a3
    Eigen::Matrix<double, 4, 4> at(double x) const
    {
        const double e1 = std::exp(-k * x);
        const double e2 = std::exp(-k * (L - x));
        Eigen::Matrix<double, 4, 4> m;
        m << 1.0, x, e1, e2,
             0.0, 1.0, -k * e1, k * e2,
             0.0, 0.0, k * k * e1, k * k * e2,
             0.0, 0.0, -k * k * k * e1, k * k * k * e2;
        return m;
    }
};

// Rows that give the end forces F = (-T(0), -EIw theta''(0), T(L), EIw theta''(L)) from the derivative rows of `at(0)` and `at(L)`
Eigen::Matrix4d forceRows(const Basis& basis, double EIw, double GJ)
{
    const auto s0 = basis.at(0.0);
    const auto sL = basis.at(basis.L);
    Eigen::Matrix4d rows;
    rows.row(0) = -(GJ * s0.row(1) - EIw * s0.row(3));
    rows.row(1) = -EIw * s0.row(2);
    rows.row(2) = GJ * sL.row(1) - EIw * sL.row(3);
    rows.row(3) = EIw * sL.row(2);
    return rows;
}

Eigen::Matrix4d displacementRows(const Basis& basis)
{
    const auto s0 = basis.at(0.0);
    const auto sL = basis.at(basis.L);
    Eigen::Matrix4d rows;
    rows.row(0) = s0.row(0);
    rows.row(1) = s0.row(1);
    rows.row(2) = sL.row(0);
    rows.row(3) = sL.row(1);
    return rows;
}

} // namespace

Eigen::Matrix4d stiffness(double EIw, double GJ, double L)
{
    Basis basis;
    basis.k = std::sqrt(GJ / EIw);
    basis.L = L;
    const Eigen::Matrix4d c = displacementRows(basis);
    const Eigen::Matrix4d f = forceRows(basis, EIw, GJ);
    Eigen::Matrix4d k = f * c.inverse();
    // the exact stiffness is symmetric: remove round-off asymmetry
    return 0.5 * (k + k.transpose());
}

Eigen::Vector4d uniformTorqueFixedEnd(double m, double EIw, double GJ, double L)
{
    Basis basis;
    basis.k = std::sqrt(GJ / EIw);
    basis.L = L;
    // theta_p = -m x^2 / (2 GJ); clamped ends: theta_h(0) = 0, theta_h'(0) = 0, theta_h(L) = m L^2/(2 GJ), theta_h'(L) = m L/GJ
    const Eigen::Vector4d d(0.0, 0.0, m * L * L / (2.0 * GJ), m * L / GJ);
    const Eigen::Vector4d a = displacementRows(basis).inverse() * d;
    Eigen::Vector4d f = forceRows(basis, EIw, GJ) * a;
    // particular solution: T_p(x) = -m x, E Cw theta_p'' = -m E Cw / GJ
    f(1) += EIw * m / GJ;
    f(2) += -m * L;
    f(3) += -EIw * m / GJ;
    return f;
}

Station evaluate(const Eigen::Vector4d& d, double m, double EIw, double GJ, double L, double x)
{
    Basis basis;
    basis.k = std::sqrt(GJ / EIw);
    basis.L = L;
    // theta = theta_p + theta_h with theta_p = -m x^2/(2 GJ): the homogeneous part takes d - (theta_p, theta_p')
    const Eigen::Vector4d dh(d(0) - 0.0, d(1) - 0.0, d(2) + m * L * L / (2.0 * GJ), d(3) + m * L / GJ);
    const Eigen::Vector4d a = displacementRows(basis).inverse() * dh;
    const auto s = basis.at(x);
    const Eigen::Vector4d v = s * a;   // theta_h, theta_h', theta_h'', theta_h'''
    Station out;
    out.theta = v(0) - m * x * x / (2.0 * GJ);
    out.phi = v(1) - m * x / GJ;
    const double thetaSecond = v(2) - m / GJ;
    const double thetaThird = v(3);
    out.bimoment = -EIw * thetaSecond;
    out.torqueSaintVenant = GJ * out.phi;
    out.torqueWarping = -EIw * thetaThird;
    return out;
}

} // namespace Warping
