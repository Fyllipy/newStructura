#include "shell_q4.h"

#include <cmath>

namespace fem {

namespace {

constexpr double kGauss = 0.57735026918962576451;   // 1/sqrt(3)
constexpr double kDrillingFactor = 1.0;             // gamma de Hughes-Brezzi (k_drill = gamma G t)
const double kSignR[4] = {-1.0, 1.0, 1.0, -1.0};
const double kSignS[4] = {-1.0, -1.0, 1.0, 1.0};

struct Kin {
    double N[4] {};
    double dNdr[4] {};
    double dNds[4] {};
    double dNdx[4] {};
    double dNdy[4] {};
    Eigen::Matrix2d J {Eigen::Matrix2d::Identity()};
    Eigen::Matrix2d Jinv {Eigen::Matrix2d::Identity()};
    double detJ {1.0};
};

Kin kinematics(const ShellFrame &f, double r, double s)
{
    Kin k;
    for (int i = 0; i < 4; ++i) {
        k.N[i] = 0.25 * (1.0 + kSignR[i] * r) * (1.0 + kSignS[i] * s);
        k.dNdr[i] = 0.25 * kSignR[i] * (1.0 + kSignS[i] * s);
        k.dNds[i] = 0.25 * kSignS[i] * (1.0 + kSignR[i] * r);
    }
    k.J.setZero();
    for (int i = 0; i < 4; ++i) {
        k.J(0, 0) += k.dNdr[i] * f.x[i];
        k.J(0, 1) += k.dNdr[i] * f.y[i];
        k.J(1, 0) += k.dNds[i] * f.x[i];
        k.J(1, 1) += k.dNds[i] * f.y[i];
    }
    k.detJ = k.J.determinant();
    k.Jinv = k.J.inverse();
    for (int i = 0; i < 4; ++i) {
        k.dNdx[i] = k.Jinv(0, 0) * k.dNdr[i] + k.Jinv(0, 1) * k.dNds[i];
        k.dNdy[i] = k.Jinv(1, 0) * k.dNdr[i] + k.Jinv(1, 1) * k.dNds[i];
    }
    return k;
}

Eigen::Matrix3d membraneD(const Material &m, double t)
{
    Eigen::Matrix3d d;
    const double c = m.E * t / (1.0 - m.nu * m.nu);
    d << 1.0, m.nu, 0.0, m.nu, 1.0, 0.0, 0.0, 0.0, 0.5 * (1.0 - m.nu);
    return c * d;
}

Eigen::Matrix3d bendingD(const Material &m, double t)
{
    Eigen::Matrix3d d;
    const double c = m.E * t * t * t / (12.0 * (1.0 - m.nu * m.nu));
    d << 1.0, m.nu, 0.0, m.nu, 1.0, 0.0, 0.0, 0.0, 0.5 * (1.0 - m.nu);
    return c * d;
}

/// Membrana compativel: 3 x 8, colunas (u1 v1 ... u4 v4).
Eigen::Matrix<double, 3, 8> membraneB(const Kin &k)
{
    Eigen::Matrix<double, 3, 8> b = Eigen::Matrix<double, 3, 8>::Zero();
    for (int i = 0; i < 4; ++i) {
        b(0, 2 * i) = k.dNdx[i];
        b(1, 2 * i + 1) = k.dNdy[i];
        b(2, 2 * i) = k.dNdy[i];
        b(2, 2 * i + 1) = k.dNdx[i];
    }
    return b;
}

/// Modos incompativeis de Wilson com a correcao de Taylor (jacobiana do centro): 3 x 4, colunas (a1u a2u a1v a2v).
Eigen::Matrix<double, 3, 4> incompatibleB(const Kin &center, const Kin &k, double r, double s)
{
    const double scale = center.detJ / k.detJ;
    const Eigen::Vector2d dP1 = scale * center.Jinv * Eigen::Vector2d(-2.0 * r, 0.0);
    const Eigen::Vector2d dP2 = scale * center.Jinv * Eigen::Vector2d(0.0, -2.0 * s);
    Eigen::Matrix<double, 3, 4> b = Eigen::Matrix<double, 3, 4>::Zero();
    b(0, 0) = dP1(0);
    b(2, 0) = dP1(1);
    b(0, 1) = dP2(0);
    b(2, 1) = dP2(1);
    b(1, 2) = dP1(1);
    b(2, 2) = dP1(0);
    b(1, 3) = dP2(1);
    b(2, 3) = dP2(0);
    return b;
}

/// Linha de deformacao de flexao 3 x 24.
Eigen::Matrix<double, 3, 24> bendingB(const Kin &k)
{
    Eigen::Matrix<double, 3, 24> b = Eigen::Matrix<double, 3, 24>::Zero();
    for (int i = 0; i < 4; ++i) {
        b(0, 6 * i + 4) = k.dNdx[i];
        b(1, 6 * i + 3) = -k.dNdy[i];
        b(2, 6 * i + 3) = -k.dNdx[i];
        b(2, 6 * i + 4) = k.dNdy[i];
    }
    return b;
}

/// Cisalhamento MITC4: 2 x 24 (gamma_xz, gamma_yz) no ponto (r, s).
Eigen::Matrix<double, 2, 24> shearB(const ShellFrame &f, const Kin &k, double r, double s)
{
    auto covariantRow = [&](double tr, double ts, bool alongR) {
        const Kin t = kinematics(f, tr, ts);
        Eigen::Matrix<double, 1, 24> row = Eigen::Matrix<double, 1, 24>::Zero();
        const double xr = t.J(0, 0), yr = t.J(0, 1), xs = t.J(1, 0), ys = t.J(1, 1);
        for (int i = 0; i < 4; ++i) {
            row(6 * i + 2) = alongR ? t.dNdr[i] : t.dNds[i];
            row(6 * i + 3) = -(alongR ? yr : ys) * t.N[i];
            row(6 * i + 4) = (alongR ? xr : xs) * t.N[i];
        }
        return row;
    };
    const Eigen::Matrix<double, 1, 24> grzA = covariantRow(0.0, 1.0, true), grzC = covariantRow(0.0, -1.0, true);
    const Eigen::Matrix<double, 1, 24> gszD = covariantRow(1.0, 0.0, false), gszB = covariantRow(-1.0, 0.0, false);
    Eigen::Matrix<double, 2, 24> nat;
    nat.row(0) = 0.5 * (1.0 + s) * grzA + 0.5 * (1.0 - s) * grzC;
    nat.row(1) = 0.5 * (1.0 + r) * gszD + 0.5 * (1.0 - r) * gszB;
    return k.Jinv * nat;
}

/// Mapeia a membrana (u v por no) para os 24 graus de liberdade.
Eigen::Matrix<double, 8, 24> membraneMap()
{
    Eigen::Matrix<double, 8, 24> m = Eigen::Matrix<double, 8, 24>::Zero();
    for (int i = 0; i < 4; ++i) {
        m(2 * i, 6 * i) = 1.0;
        m(2 * i + 1, 6 * i + 1) = 1.0;
    }
    return m;
}

} // namespace

ShellFrame shellFrame(const std::array<Vec3, 4> &p)
{
    ShellFrame f;
    f.center = 0.25 * (p[0] + p[1] + p[2] + p[3]);
    Vec3 e3 = (p[2] - p[0]).cross(p[3] - p[1]);
    f.area = 0.5 * e3.norm();
    e3.normalize();
    Vec3 e1 = (p[1] - p[0]) + (p[2] - p[3]);
    e1 -= e1.dot(e3) * e3;
    e1.normalize();
    const Vec3 e2 = e3.cross(e1);
    f.R.row(0) = e1.transpose();
    f.R.row(1) = e2.transpose();
    f.R.row(2) = e3.transpose();
    for (int i = 0; i < 4; ++i) {
        const Vec3 d = p[i] - f.center;
        f.x[i] = d.dot(e1);
        f.y[i] = d.dot(e2);
        f.warp = std::max(f.warp, std::abs(d.dot(e3)));
    }
    return f;
}

Mat24 shellTransform(const ShellFrame &frame)
{
    Mat24 t = Mat24::Zero();
    for (int i = 0; i < 8; ++i) {
        t.block<3, 3>(3 * i, 3 * i) = frame.R;
    }
    return t;
}

ShellStiffness shellStiffness(const ShellFrame &f, const Material &mat, double t, double kappa)
{
    ShellStiffness out;
    const Eigen::Matrix3d dm = membraneD(mat, t);
    const Eigen::Matrix3d db = bendingD(mat, t);
    const double gt = mat.G() * t;
    const Kin center = kinematics(f, 0.0, 0.0);
    const Eigen::Matrix<double, 8, 24> map = membraneMap();

    Eigen::Matrix<double, 8, 8> kuu = Eigen::Matrix<double, 8, 8>::Zero();
    Eigen::Matrix<double, 8, 4> kua = Eigen::Matrix<double, 8, 4>::Zero();
    Eigen::Matrix<double, 4, 4> kaa = Eigen::Matrix<double, 4, 4>::Zero();
    Mat24 kLocal = Mat24::Zero();

    for (int gr = 0; gr < 2; ++gr) {
        for (int gs = 0; gs < 2; ++gs) {
            const double r = (gr == 0 ? -kGauss : kGauss), s = (gs == 0 ? -kGauss : kGauss);
            const Kin k = kinematics(f, r, s);
            const double w = k.detJ;
            const auto bu = membraneB(k);
            const auto ba = incompatibleB(center, k, r, s);
            kuu += w * bu.transpose() * dm * bu;
            kua += w * bu.transpose() * dm * ba;
            kaa += w * ba.transpose() * dm * ba;
            // flexao
            const auto bb = bendingB(k);
            kLocal += w * bb.transpose() * db * bb;
            // cisalhamento transversal (MITC4)
            const auto bs = shearB(f, k, r, s);
            kLocal += w * kappa * gt * bs.transpose() * bs;
            // rotacao no plano (penalidade de Hughes-Brezzi): theta_z - (1/2)(dv/dx - du/dy)
            Eigen::Matrix<double, 1, 24> bd = Eigen::Matrix<double, 1, 24>::Zero();
            for (int i = 0; i < 4; ++i) {
                bd(6 * i) = 0.5 * k.dNdy[i];
                bd(6 * i + 1) = -0.5 * k.dNdx[i];
                bd(6 * i + 5) = k.N[i];
            }
            kLocal += w * kDrillingFactor * gt * bd.transpose() * bd;
        }
    }
    const Eigen::Matrix<double, 4, 4> kaaInv = kaa.inverse();
    const Eigen::Matrix<double, 8, 8> kc = kuu - kua * kaaInv * kua.transpose();
    kLocal += map.transpose() * kc * map;
    out.K = 0.5 * (kLocal + kLocal.transpose());
    out.alphaFromU = -kaaInv * kua.transpose();
    return out;
}

Vec24 shellPressureLoad(const ShellFrame &f, const std::array<double, 4> &p)
{
    Vec24 load = Vec24::Zero();
    for (int gr = 0; gr < 2; ++gr) {
        for (int gs = 0; gs < 2; ++gs) {
            const double r = (gr == 0 ? -kGauss : kGauss), s = (gs == 0 ? -kGauss : kGauss);
            const Kin k = kinematics(f, r, s);
            double pressure = 0.0;
            for (int i = 0; i < 4; ++i) {
                pressure += k.N[i] * p[i];
            }
            for (int i = 0; i < 4; ++i) {
                load(6 * i + 2) += k.N[i] * pressure * k.detJ;
            }
        }
    }
    return load;
}

Vec24 shellBodyLoad(const ShellFrame &f, const Vec3 &b, double t)
{
    Vec24 load = Vec24::Zero();
    for (int gr = 0; gr < 2; ++gr) {
        for (int gs = 0; gs < 2; ++gs) {
            const double r = (gr == 0 ? -kGauss : kGauss), s = (gs == 0 ? -kGauss : kGauss);
            const Kin k = kinematics(f, r, s);
            for (int i = 0; i < 4; ++i) {
                for (int c = 0; c < 3; ++c) {
                    load(6 * i + c) += k.N[i] * t * b(c) * k.detJ;
                }
            }
        }
    }
    return load;
}

Vec24 shellThermalLoad(const ShellFrame &f, const Material &mat, double t, double uniform, double gradient)
{
    Vec24 load = Vec24::Zero();
    const Eigen::Matrix3d dm = membraneD(mat, t);
    const Eigen::Matrix3d db = bendingD(mat, t);
    const Kin center = kinematics(f, 0.0, 0.0);
    const Eigen::Matrix<double, 8, 24> map = membraneMap();
    const Eigen::Vector3d eps0 = mat.alpha * uniform * Eigen::Vector3d(1.0, 1.0, 0.0);
    const Eigen::Vector3d kap0 = (mat.alpha * gradient / t) * Eigen::Vector3d(1.0, 1.0, 0.0);
    Eigen::Matrix<double, 8, 8> kuu = Eigen::Matrix<double, 8, 8>::Zero();
    Eigen::Matrix<double, 8, 4> kua = Eigen::Matrix<double, 8, 4>::Zero();
    Eigen::Matrix<double, 4, 4> kaa = Eigen::Matrix<double, 4, 4>::Zero();
    Eigen::Matrix<double, 8, 1> fu = Eigen::Matrix<double, 8, 1>::Zero();
    Eigen::Matrix<double, 4, 1> fa = Eigen::Matrix<double, 4, 1>::Zero();
    for (int gr = 0; gr < 2; ++gr) {
        for (int gs = 0; gs < 2; ++gs) {
            const double r = (gr == 0 ? -kGauss : kGauss), s = (gs == 0 ? -kGauss : kGauss);
            const Kin k = kinematics(f, r, s);
            const auto bu = membraneB(k);
            const auto ba = incompatibleB(center, k, r, s);
            kua += k.detJ * bu.transpose() * dm * ba;
            kaa += k.detJ * ba.transpose() * dm * ba;
            fu += k.detJ * bu.transpose() * dm * eps0;
            fa += k.detJ * ba.transpose() * dm * eps0;
            load += k.detJ * bendingB(k).transpose() * db * kap0;
        }
    }
    const Eigen::Matrix<double, 8, 1> fc = fu - kua * kaa.inverse() * fa;
    load += map.transpose() * fc;
    return load;
}

Mat24 shellGeometricStiffness(const ShellFrame &f, double n11, double n22, double n12)
{
    Mat24 kg = Mat24::Zero();
    Eigen::Matrix2d n;
    n << n11, n12, n12, n22;
    for (int gr = 0; gr < 2; ++gr) {
        for (int gs = 0; gs < 2; ++gs) {
            const double r = (gr == 0 ? -kGauss : kGauss), s = (gs == 0 ? -kGauss : kGauss);
            const Kin k = kinematics(f, r, s);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    const Eigen::Vector2d gi(k.dNdx[i], k.dNdy[i]), gj(k.dNdx[j], k.dNdy[j]);
                    kg(6 * i + 2, 6 * j + 2) += k.detJ * gi.dot(n * gj);
                }
            }
        }
    }
    return kg;
}

ShellResult shellResultants(const ShellFrame &f, const Material &mat, double t, double kappa, const ShellStiffness &stiffness, const Vec24 &u,
                            double uniformTemp, double gradientTemp)
{
    ShellResult out;
    const Eigen::Matrix3d dm = membraneD(mat, t);
    const Eigen::Matrix3d db = bendingD(mat, t);
    const double gt = mat.G() * t;
    const Kin center = kinematics(f, 0.0, 0.0);
    Eigen::Matrix<double, 8, 1> uMem;
    for (int i = 0; i < 4; ++i) {
        uMem(2 * i) = u(6 * i);
        uMem(2 * i + 1) = u(6 * i + 1);
    }
    const Eigen::Matrix<double, 4, 1> alpha = stiffness.alphaFromU * uMem;
    const Eigen::Vector3d eps0 = mat.alpha * uniformTemp * Eigen::Vector3d(1.0, 1.0, 0.0);
    const Eigen::Vector3d kap0 = (mat.alpha * gradientTemp / t) * Eigen::Vector3d(1.0, 1.0, 0.0);
    const double pr[5] = {-1.0, 1.0, 1.0, -1.0, 0.0};
    const double ps[5] = {-1.0, -1.0, 1.0, 1.0, 0.0};
    for (int p = 0; p < ShellResult::kPoints; ++p) {
        const Kin k = kinematics(f, pr[p], ps[p]);
        const Eigen::Vector3d eps = membraneB(k) * uMem + incompatibleB(center, k, pr[p], ps[p]) * alpha;
        const Eigen::Vector3d n = dm * (eps - eps0);
        const Eigen::Vector3d kap = bendingB(k) * u;
        const Eigen::Vector3d m = db * (kap - kap0);
        const Eigen::Vector2d q = kappa * gt * (shearB(f, k, pr[p], ps[p]) * u);
        double *v = &out.values[p * ShellResult::kValues];
        v[0] = n(0);
        v[1] = n(1);
        v[2] = n(2);
        v[3] = m(0);
        v[4] = m(1);
        v[5] = m(2);
        v[6] = q(0);
        v[7] = q(1);
    }
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out.axes[3 * r + c] = f.R(r, c);
        }
    }
    return out;
}

} // namespace fem
