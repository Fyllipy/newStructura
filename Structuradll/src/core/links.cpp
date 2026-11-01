#include "links.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "node.h"

namespace {

// Piso da rigidez tangente: evita pivos nulos quando o estado e "aberto" (folga, so tracao) sem alterar o equilibrio
constexpr double kStiffnessFloor = 1e-9;

void lawResponse(const LinkLawData& law, double d, double plasticCommitted, double& plasticTrial, double& f, double& kt)
{
    plasticTrial = plasticCommitted;
    const double k = std::abs(law.k);
    switch (law.kind) {
    case 0:
    case 7:
        f = k * d;
        kt = k;
        return;
    case 1:   // so tracao
        f = d > 0.0 ? k * d : 0.0;
        kt = d > 0.0 ? k : k * kStiffnessFloor;
        return;
    case 2:   // so compressao
        f = d < 0.0 ? k * d : 0.0;
        kt = d < 0.0 ? k : k * kStiffnessFloor;
        return;
    case 3: {   // folga de tracao
        const double gap = std::abs(law.p1);
        f = d > gap ? k * (d - gap) : 0.0;
        kt = d > gap ? k : k * kStiffnessFloor;
        return;
    }
    case 4: {   // folga de compressao
        const double gap = std::abs(law.p1);
        f = d < -gap ? k * (d + gap) : 0.0;
        kt = d < -gap ? k : k * kStiffnessFloor;
        return;
    }
    case 5: {   // elastoplastica bilinear, enrijecimento cinematico
        const double ratio = std::clamp(law.p2, 1e-6, 0.999999);
        const double H = ratio * k / (1.0 - ratio);
        const double trial = k * (d - plasticCommitted);
        const double xi = trial - H * plasticCommitted;
        const double yieldForce = std::abs(law.p1);
        const double yieldFunction = std::abs(xi) - yieldForce;
        if (yieldFunction <= 0.0) {
            f = trial;
            kt = k;
        } else {
            const double dgamma = yieldFunction / (k + H);
            plasticTrial = plasticCommitted + (xi >= 0.0 ? dgamma : -dgamma);
            f = k * (d - plasticTrial);
            kt = k * H / (k + H);
        }
        return;
    }
    case 6: {   // multilinear
        const auto& t = law.table;
        if (t.size() < 2) {
            f = k * d;
            kt = k;
            return;
        }
        size_t segment = 0;
        while (segment + 2 < t.size() && d > t[segment + 1].first) {
            ++segment;
        }
        const double dx = t[segment + 1].first - t[segment].first;
        const double slope = dx != 0.0 ? (t[segment + 1].second - t[segment].second) / dx : 0.0;
        f = t[segment].second + slope * (d - t[segment].first);
        double reference = 0.0;
        for (size_t i = 0; i + 1 < t.size(); ++i) {
            const double ddx = t[i + 1].first - t[i].first;
            reference = std::max(reference, ddx != 0.0 ? std::abs((t[i + 1].second - t[i].second) / ddx) : 0.0);
        }
        kt = std::max(slope, reference * kStiffnessFloor);
        return;
    }
    default:
        throw std::runtime_error("Link law: unknown kind " + std::to_string(law.kind));
    }
}

} // namespace

LinkSet::LinkSet(const SolverInput& input, const std::unordered_map<int, int>& nodeIndexById, const std::vector<std::shared_ptr<Node>>& nodes)
{
    for (const LinkData& data : input.links) {
        const auto ni = nodeIndexById.find(data.nodeI), nj = nodeIndexById.find(data.nodeJ);
        if (ni == nodeIndexById.end() || nj == nodeIndexById.end()) {
            throw std::runtime_error("Link " + std::to_string(data.id) + " references unknown node");
        }
        Link link;
        link.id = data.id;
        link.dofI = ni->second * 6;
        link.dofJ = nj->second * 6;
        link.laws = data.laws;
        Eigen::Vector3d ex(data.axes[0], data.axes[1], data.axes[2]);
        Eigen::Vector3d ey(data.axes[3], data.axes[4], data.axes[5]);
        Eigen::Vector3d ez(data.axes[6], data.axes[7], data.axes[8]);
        if (ex.norm() < 1e-12) {
            const auto& a = nodes[static_cast<size_t>(ni->second)];
            const auto& b = nodes[static_cast<size_t>(nj->second)];
            ex = Eigen::Vector3d(b->getX() - a->getX(), b->getY() - a->getY(), b->getZ() - a->getZ());
            if (ex.norm() < 1e-12) {
                ex = Eigen::Vector3d::UnitX();   // nos coincidentes: eixos globais
                ey = Eigen::Vector3d::UnitY();
                ez = Eigen::Vector3d::UnitZ();
            }
        }
        ex.normalize();
        if (ey.norm() < 1e-12 || ez.norm() < 1e-12) {
            // mesma convencao das barras: z = x cross (Z global, ou Y se paralelo), y = z cross x
            Eigen::Vector3d reference = std::abs(ex.dot(Eigen::Vector3d::UnitZ())) > 0.999 ? Eigen::Vector3d::UnitY() : Eigen::Vector3d::UnitZ();
            ez = ex.cross(reference).normalized();
            ey = ez.cross(ex).normalized();
        } else {
            ey.normalize();
            ez.normalize();
        }
        link.axis = {ex, ey, ez};
        for (const LinkLawData& law : link.laws) {
            nonlinear_ = nonlinear_ || (law.kind != 0 && law.kind != 7);
        }
        links_.push_back(std::move(link));
    }
}

void LinkSet::evaluate(const Eigen::VectorXd& u, Eigen::VectorXd& internalForce, std::vector<Eigen::Triplet<double>>& tangent)
{
    for (Link& link : links_) {
        const Eigen::Vector3d dTrans = u.segment<3>(link.dofJ) - u.segment<3>(link.dofI);
        const Eigen::Vector3d dRot = u.segment<3>(link.dofJ + 3) - u.segment<3>(link.dofI + 3);
        for (int k = 0; k < 6; ++k) {
            const Eigen::Vector3d& e = link.axis[static_cast<size_t>(k % 3)];
            const double d = (k < 3 ? dTrans : dRot).dot(e);
            double f = 0.0, kt = 0.0;
            lawResponse(link.laws[static_cast<size_t>(k)], d, link.plastic[static_cast<size_t>(k)], link.trialPlastic[static_cast<size_t>(k)], f, kt);
            link.deformation[static_cast<size_t>(k)] = d;
            link.force[static_cast<size_t>(k)] = f;
            const int offset = k < 3 ? 0 : 3;
            // forca f sobre o no J (e -f sobre o I) ao longo de e
            for (int a = 0; a < 3; ++a) {
                internalForce(link.dofJ + offset + a) += f * e(a);
                internalForce(link.dofI + offset + a) -= f * e(a);
                for (int b = 0; b < 3; ++b) {
                    const double v = kt * e(a) * e(b);
                    tangent.emplace_back(link.dofJ + offset + a, link.dofJ + offset + b, v);
                    tangent.emplace_back(link.dofI + offset + a, link.dofI + offset + b, v);
                    tangent.emplace_back(link.dofJ + offset + a, link.dofI + offset + b, -v);
                    tangent.emplace_back(link.dofI + offset + a, link.dofJ + offset + b, -v);
                }
            }
        }
    }
}

void LinkSet::commit()
{
    for (Link& link : links_) {
        link.plastic = link.trialPlastic;
    }
}

void LinkSet::reset()
{
    for (Link& link : links_) {
        link.plastic.fill(0.0);
        link.trialPlastic.fill(0.0);
    }
}

std::vector<LinkResult> LinkSet::results() const
{
    std::vector<LinkResult> out;
    out.reserve(links_.size());
    for (const Link& link : links_) {
        LinkResult r;
        r.id = link.id;
        r.deformation = link.deformation;
        r.force = link.force;
        out.push_back(r);
    }
    return out;
}
