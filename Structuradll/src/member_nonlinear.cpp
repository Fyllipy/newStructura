#include "member.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Molas de extremidade e rotulas plasticas das barras (nao linearidade de material concentrada nas extremidades).
//
// O vinculo entre o no e a extremidade da barra em um grau de liberdade r e uma mola (rigidez k_r, lei de forca-deformacao s_r(delta)) em serie com
// o elemento. Com theta_n o deslocamento do no e phi o da extremidade da barra, delta = theta_n - phi e o equilibrio local resolve
//     K_rr phi + K_ra u_a + f0_r = s(theta_n - phi)             (phi: incognita local; u_a: graus retidos)
// Forcas dos nos sobre a barra:  f_a = K_aa u_a + K_ar phi + f0_a ;  f_r = s .  Para s linear (k) a condensacao reduz-se a de member.cpp
// e, para k = 0, a liberacao simples. Na rotula plastica, s e bilinear (enrijecimento cinematico) com retorno radial exato.

namespace {

constexpr int kLocalMaxIterations = 40;

// Resposta da mola/rotula: forca s e rigidez tangente kt para a deformacao delta
void springResponse(PlasticHinge& hinge, double delta, double axial, double& s, double& kt)
{
    const double k = hinge.kElastic;
    const double ratio = std::clamp(hinge.hardeningRatio, 1e-6, 0.999999);
    const double H = ratio * k / (1.0 - ratio);   // modulo de enrijecimento cinematico: rigidez pos-escoamento = ratio * k
    double yield = hinge.yieldMoment;
    if (hinge.axialYield > 0.0) {
        yield *= std::max(1e-6, 1.0 - std::pow(std::abs(axial) / hinge.axialYield, hinge.exponent));
    }
    const double trial = k * (delta - hinge.plasticRotation);
    const double xi = trial - H * hinge.plasticRotation;   // momento menos a tensao de retorno
    const double f = std::abs(xi) - yield;
    if (f <= 0.0) {
        hinge.trialPlastic = hinge.plasticRotation;
        hinge.yielded = false;
        s = trial;
        kt = k;
    } else {
        const double dgamma = f / (k + H);
        const double sign = xi >= 0.0 ? 1.0 : -1.0;
        hinge.trialPlastic = hinge.plasticRotation + sign * dgamma;
        hinge.yielded = true;
        s = k * (delta - hinge.trialPlastic);
        kt = k * H / (k + H);
    }
    hinge.moment = s;
    hinge.tangent = kt;
}

} // namespace

void Member::setPlasticHinge(int dof, const PlasticHinge& hinge)
{
    if (dof < 0 || dof >= 12) {
        throw std::out_of_range("Plastic hinge dof out of range");
    }
    hinges[static_cast<size_t>(dof)] = hinge;
    hinges[static_cast<size_t>(dof)].enabled = hinge.enabled && hinge.kElastic > 0.0 && hinge.yieldMoment > 0.0;
    springK[static_cast<size_t>(dof)] = hinges[static_cast<size_t>(dof)].enabled ? hinge.kElastic : springK[static_cast<size_t>(dof)];
    condensationCached = false;
}

bool Member::hasPlasticHinges() const
{
    return std::any_of(hinges.begin(), hinges.end(), [](const PlasticHinge& h) { return h.enabled; });
}

void Member::commitHinges()
{
    for (PlasticHinge& h : hinges) {
        if (h.enabled) {
            h.plasticRotation = h.trialPlastic;
        }
    }
}

void Member::resetHinges()
{
    for (PlasticHinge& h : hinges) {
        h.plasticRotation = 0.0;
        h.trialPlastic = 0.0;
        h.moment = 0.0;
        h.yielded = false;
    }
    hingeState.setZero();
    lastAxial = 0.0;
}

bool Member::evaluateNonlinear(const Eigen::VectorXd& uAxis, double loadFactor, Eigen::VectorXd& force, Eigen::MatrixXd& tangent)
{
    const Eigen::MatrixXd K = getFullLocalStiffnessMatrix();
    if (K.rows() != 12 || uAxis.size() < 12) {
        return false;
    }
    const Eigen::VectorXd f0 = -loadFactor * rawEquivalent;
    std::vector<int> a, r;
    for (int i = 0; i < 12; ++i) {
        (isCondensedDof(i) ? r : a).push_back(i);
    }
    const Eigen::Index na = static_cast<Eigen::Index>(a.size()), nr = static_cast<Eigen::Index>(r.size());
    if (nr == 0) {
        force = K * uAxis.head(12) + f0;
        tangent = K;
        lastAxial = 0.5 * (force(6) - force(0));
        return true;
    }

    Eigen::MatrixXd Kaa(na, na), Kar(na, nr), Krr(nr, nr);
    for (Eigen::Index i = 0; i < na; ++i) {
        for (Eigen::Index j = 0; j < na; ++j) Kaa(i, j) = K(a[static_cast<size_t>(i)], a[static_cast<size_t>(j)]);
        for (Eigen::Index j = 0; j < nr; ++j) Kar(i, j) = K(a[static_cast<size_t>(i)], r[static_cast<size_t>(j)]);
    }
    for (Eigen::Index i = 0; i < nr; ++i) {
        for (Eigen::Index j = 0; j < nr; ++j) Krr(i, j) = K(r[static_cast<size_t>(i)], r[static_cast<size_t>(j)]);
    }
    Eigen::VectorXd ua(na), f0a(na), f0r(nr), theta(nr), phi(nr);
    for (Eigen::Index i = 0; i < na; ++i) {
        ua(i) = uAxis(a[static_cast<size_t>(i)]);
        f0a(i) = f0(a[static_cast<size_t>(i)]);
    }
    for (Eigen::Index i = 0; i < nr; ++i) {
        theta(i) = uAxis(r[static_cast<size_t>(i)]);
        f0r(i) = f0(r[static_cast<size_t>(i)]);
        phi(i) = hingeState(r[static_cast<size_t>(i)]);
    }
    const Eigen::VectorXd krau = Kar.transpose() * ua;

    Eigen::VectorXd s(nr), kt(nr);
    auto respond = [&](const Eigen::VectorXd& p) {
        for (Eigen::Index i = 0; i < nr; ++i) {
            const int dof = r[static_cast<size_t>(i)];
            PlasticHinge& h = hinges[static_cast<size_t>(dof)];
            const double delta = theta(i) - p(i);
            if (h.enabled) {
                springResponse(h, delta, lastAxial, s(i), kt(i));
            } else {
                kt(i) = springStiffness(dof);
                s(i) = kt(i) * delta;
            }
        }
    };

    // Newton local (G e linear por trechos e crescente em phi: converge em poucas iteracoes)
    bool converged = false;
    int iteration = 0;
    for (; iteration < kLocalMaxIterations; ++iteration) {
        respond(phi);
        const Eigen::VectorXd G = Krr * phi + krau + f0r - s;
        const double scale = std::max({1.0, (Krr * phi).cwiseAbs().maxCoeff(), s.cwiseAbs().maxCoeff(), f0r.cwiseAbs().maxCoeff(), krau.cwiseAbs().maxCoeff()});
        if (G.cwiseAbs().maxCoeff() <= 1e-11 * scale) {
            converged = true;
            break;
        }
        const Eigen::MatrixXd J = Krr + Eigen::MatrixXd(kt.asDiagonal());
        Eigen::FullPivLU<Eigen::MatrixXd> lu(J);
        if (!lu.isInvertible()) {
            return false;
        }
        phi -= lu.solve(G);
    }
    lastHingeIterations = iteration;
    if (!converged) {
        return false;
    }
    respond(phi);

    // forcas dos nos sobre a barra
    force = Eigen::VectorXd::Zero(12);
    const Eigen::VectorXd fa = Kaa * ua + Kar * phi + f0a;
    for (Eigen::Index i = 0; i < na; ++i) force(a[static_cast<size_t>(i)]) = fa(i);
    for (Eigen::Index i = 0; i < nr; ++i) {
        force(r[static_cast<size_t>(i)]) = s(i);
        hingeState(r[static_cast<size_t>(i)]) = phi(i);
    }

    // rigidez tangente condensada: Kaa - Kar S^-1 Kra ; Kar S^-1 Kt ; Kt - Kt S^-1 Kt , com S = Krr + Kt
    const Eigen::MatrixXd Kt = Eigen::MatrixXd(kt.asDiagonal());
    Eigen::FullPivLU<Eigen::MatrixXd> lu(Krr + Kt);
    if (!lu.isInvertible()) {
        return false;
    }
    const Eigen::MatrixXd Kc_aa = Kaa - Kar * lu.solve(Kar.transpose());
    const Eigen::MatrixXd Kc_ar = Kar * lu.solve(Kt);
    const Eigen::MatrixXd Kc_rr = Kt - Kt * lu.solve(Kt);
    tangent = Eigen::MatrixXd::Zero(12, 12);
    for (Eigen::Index i = 0; i < na; ++i) {
        for (Eigen::Index j = 0; j < na; ++j) tangent(a[static_cast<size_t>(i)], a[static_cast<size_t>(j)]) = Kc_aa(i, j);
        for (Eigen::Index j = 0; j < nr; ++j) {
            tangent(a[static_cast<size_t>(i)], r[static_cast<size_t>(j)]) = Kc_ar(i, j);
            tangent(r[static_cast<size_t>(j)], a[static_cast<size_t>(i)]) = Kc_ar(i, j);
        }
    }
    for (Eigen::Index i = 0; i < nr; ++i) {
        for (Eigen::Index j = 0; j < nr; ++j) tangent(r[static_cast<size_t>(i)], r[static_cast<size_t>(j)]) = 0.5 * (Kc_rr(i, j) + Kc_rr(j, i));
    }
    lastAxial = 0.5 * (force(6) - force(0));
    return true;
}
