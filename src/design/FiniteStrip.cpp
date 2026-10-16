#include "design/FiniteStrip.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Structura::Design {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Quadratura de Gauss-Legendre de 4 pontos em [0, 1]
const double kGaussX[4] = {0.0694318442029737, 0.3300094782075719, 0.6699905217924281, 0.9305681557970263};
const double kGaussW[4] = {0.1739274225687269, 0.3260725774312731, 0.3260725774312731, 0.1739274225687269};

using Mat8 = Eigen::Matrix<double, 8, 8>;
using Vec8 = Eigen::Matrix<double, 8, 1>;

struct StripMatrices {
    Mat8 K = Mat8::Zero();
    Mat8 G = Mat8::Zero();
};

// Ordem local: u1, v1, w1, th1, u2, v2, w2, th2
StripMatrices stripMatrices(double b, double t, double a, double sigma1, double sigma2, double E, double nu)
{
    StripMatrices m;
    const double k = kPi / a;
    const double dm = E * t / (1.0 - nu * nu);
    const double gm = E * t / (2.0 * (1.0 + nu));
    const double db = E * t * t * t / (12.0 * (1.0 - nu * nu));
    const double halfA = a / 2.0;

    for (int q = 0; q < 4; ++q) {
        const double xi = kGaussX[q];
        const double wgt = kGaussW[q] * b * halfA;

        Vec8 ex = Vec8::Zero();
        Vec8 ey = Vec8::Zero();
        Vec8 gx = Vec8::Zero();
        ex(0) = -1.0 / b;
        ex(4) = 1.0 / b;
        ey(1) = -k * (1.0 - xi);
        ey(5) = -k * xi;
        gx(0) = k * (1.0 - xi);
        gx(4) = k * xi;
        gx(1) = -1.0 / b;
        gx(5) = 1.0 / b;

        // Hermite
        const double n1 = 1.0 - 3.0 * xi * xi + 2.0 * xi * xi * xi;
        const double n2 = b * (xi - 2.0 * xi * xi + xi * xi * xi);
        const double n3 = 3.0 * xi * xi - 2.0 * xi * xi * xi;
        const double n4 = b * (-xi * xi + xi * xi * xi);
        const double d1 = -6.0 * xi + 6.0 * xi * xi;
        const double d2 = b * (1.0 - 4.0 * xi + 3.0 * xi * xi);
        const double d3 = 6.0 * xi - 6.0 * xi * xi;
        const double d4 = b * (-2.0 * xi + 3.0 * xi * xi);
        const double s1 = -6.0 + 12.0 * xi;
        const double s2 = b * (-4.0 + 6.0 * xi);
        const double s3 = 6.0 - 12.0 * xi;
        const double s4 = b * (-2.0 + 6.0 * xi);

        Vec8 kx = Vec8::Zero();
        Vec8 ky = Vec8::Zero();
        Vec8 kxy = Vec8::Zero();
        const int wIdx[4] = {2, 3, 6, 7};
        const double nVals[4] = {n1, n2, n3, n4};
        const double dVals[4] = {d1, d2, d3, d4};
        const double sVals[4] = {s1, s2, s3, s4};
        for (int i = 0; i < 4; ++i) {
            kx(wIdx[i]) = -sVals[i] / (b * b);
            ky(wIdx[i]) = k * k * nVals[i];
            kxy(wIdx[i]) = -2.0 * k * dVals[i] / b;
        }

        // Rigidez de membrana e flexao
        m.K += wgt * (dm * (ex * ex.transpose() + ey * ey.transpose() + nu * (ex * ey.transpose() + ey * ex.transpose())) + gm * gx * gx.transpose());
        m.K += wgt * (db * (kx * kx.transpose() + ky * ky.transpose() + nu * (kx * ky.transpose() + ky * kx.transpose())) +
                      db * (1.0 - nu) / 2.0 * kxy * kxy.transpose());

        // Rigidez geometrica (tensao de compressao positiva em sigma1/sigma2)
        const double sigma = sigma1 * (1.0 - xi) + sigma2 * xi;
        Vec8 nu8 = Vec8::Zero();
        Vec8 nv8 = Vec8::Zero();
        Vec8 nw8 = Vec8::Zero();
        nu8(0) = 1.0 - xi;
        nu8(4) = xi;
        nv8(1) = 1.0 - xi;
        nv8(5) = xi;
        for (int i = 0; i < 4; ++i) {
            nw8(wIdx[i]) = nVals[i];
        }
        m.G += wgt * k * k * t * sigma * (nu8 * nu8.transpose() + nv8 * nv8.transpose() + nw8 * nw8.transpose());
    }
    return m;
}

} // namespace

double finiteStripLoadFactor(const FiniteStripModel &model, double length)
{
    const int nNodes = static_cast<int>(model.nodes.size());
    const int n = 4 * nNodes;
    Eigen::MatrixXd K = Eigen::MatrixXd::Zero(n, n);
    Eigen::MatrixXd G = Eigen::MatrixXd::Zero(n, n);

    for (std::size_t s = 0; s < model.strips.size(); ++s) {
        const int i = model.strips[s][0];
        const int j = model.strips[s][1];
        const double dx = model.nodes[static_cast<std::size_t>(j)][0] - model.nodes[static_cast<std::size_t>(i)][0];
        const double dy = model.nodes[static_cast<std::size_t>(j)][1] - model.nodes[static_cast<std::size_t>(i)][1];
        const double b = std::hypot(dx, dy);
        const double c = dx / b;
        const double sn = dy / b;
        const StripMatrices lm = stripMatrices(b, model.thickness[s], length, model.nodeStress[static_cast<std::size_t>(i)],
                                               model.nodeStress[static_cast<std::size_t>(j)], model.E, model.nu);

        // local = T * global; global por no: [Ua, V, Wb, theta]
        Mat8 T = Mat8::Zero();
        for (int e = 0; e < 2; ++e) {
            const int o = 4 * e;
            T(o + 0, o + 0) = c;    // u = Ua cos + Wb sin
            T(o + 0, o + 2) = sn;
            T(o + 1, o + 1) = 1.0;  // v
            T(o + 2, o + 0) = -sn;  // w = -Ua sin + Wb cos
            T(o + 2, o + 2) = c;
            T(o + 3, o + 3) = 1.0;  // theta
        }
        const Mat8 kg = T.transpose() * lm.K * T;
        const Mat8 gg = T.transpose() * lm.G * T;
        const int dof[8] = {4 * i, 4 * i + 1, 4 * i + 2, 4 * i + 3, 4 * j, 4 * j + 1, 4 * j + 2, 4 * j + 3};
        for (int r = 0; r < 8; ++r) {
            for (int q = 0; q < 8; ++q) {
                K(dof[r], dof[q]) += kg(r, q);
                G(dof[r], dof[q]) += gg(r, q);
            }
        }
    }

    std::vector<int> free;
    for (int d = 0; d < n; ++d) {
        bool fixed = false;
        for (const auto &f : model.fixedDofs) {
            if (4 * f[0] + f[1] == d) {
                fixed = true;
                break;
            }
        }
        if (!fixed) {
            free.push_back(d);
        }
    }
    const int nf = static_cast<int>(free.size());
    Eigen::MatrixXd Kf(nf, nf);
    Eigen::MatrixXd Gf(nf, nf);
    for (int r = 0; r < nf; ++r) {
        for (int q = 0; q < nf; ++q) {
            Kf(r, q) = K(free[static_cast<std::size_t>(r)], free[static_cast<std::size_t>(q)]);
            Gf(r, q) = G(free[static_cast<std::size_t>(r)], free[static_cast<std::size_t>(q)]);
        }
    }

    // G d = mu K d, mu = 1/lambda: o maior mu positivo fornece o menor fator de carga
    Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> solver(Gf, Kf);
    if (solver.info() != Eigen::Success) {
        return std::numeric_limits<double>::infinity();
    }
    const double muMax = solver.eigenvalues()(nf - 1);
    if (muMax <= 1e-30) {
        return std::numeric_limits<double>::infinity();
    }
    return 1.0 / muMax;
}

std::vector<FiniteStripMinimum> finiteStripMinima(const FiniteStripModel &model, double minLength, double maxLength, int samples,
                                                  std::vector<FiniteStripMinimum> *curve)
{
    std::vector<FiniteStripMinimum> values;
    values.reserve(static_cast<std::size_t>(samples));
    const double logMin = std::log(minLength);
    const double logMax = std::log(maxLength);
    for (int i = 0; i < samples; ++i) {
        const double length = std::exp(logMin + (logMax - logMin) * i / (samples - 1));
        values.push_back({length, finiteStripLoadFactor(model, length)});
    }
    if (curve) {
        *curve = values;
    }
    std::vector<FiniteStripMinimum> minima;
    for (std::size_t i = 1; i + 1 < values.size(); ++i) {
        if (values[i].loadFactor < values[i - 1].loadFactor && values[i].loadFactor <= values[i + 1].loadFactor) {
            // refina por parabola nos tres pontos (em log do comprimento)
            minima.push_back(values[i]);
        }
    }
    return minima;
}

} // namespace Structura::Design
