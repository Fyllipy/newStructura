#pragma once

#include <Eigen/Dense>

// Torsion with warping (Vlasov thin-walled open sections), exact solution of E Cw theta'''' - G J theta'' = m(x).
//
// Element dofs (generalized displacements): d = (theta_i, phi_i, theta_j, phi_j), phi = theta' = rate of twist ("warping parameter").
// Generalized end forces, conjugate to d (forces applied to the element by the nodes):
//   F = (-T(0), -E Cw theta''(0), T(L), E Cw theta''(L)),  T = G J theta' - E Cw theta''' (total torque), B = -E Cw theta'' (bimoment)
// so the forces on phi are +B(0) at the start and -B(L) at the end.
namespace Warping {

// 4x4 stiffness of the exact torsion element of length L
Eigen::Matrix4d stiffness(double EIw, double GJ, double L);

// End forces applied by clamped (theta = phi = 0) supports to a bar under a uniform torque m per unit length (+x), same order as the dofs.
// Equivalent nodal loads are the negative of these values.
Eigen::Vector4d uniformTorqueFixedEnd(double m, double EIw, double GJ, double L);

// Values along the bar for given end displacements d and uniform torque m: theta(x), phi(x)=theta', B(x), Tsv(x)=GJ theta', Tw(x)=T-Tsv
struct Station {
    double theta {0.0};
    double phi {0.0};
    double bimoment {0.0};
    double torqueSaintVenant {0.0};
    double torqueWarping {0.0};
};
Station evaluate(const Eigen::Vector4d& d, double m, double EIw, double GJ, double L, double x);

} // namespace Warping
