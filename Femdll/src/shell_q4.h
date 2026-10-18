#pragma once

#include "fem_types.h"

namespace fem {

using Mat24 = Eigen::Matrix<double, 24, 24>;
using Vec24 = Eigen::Matrix<double, 24, 1>;

/// Sistema local do elemento: e1 pela bissetriz das arestas 1-2 e 4-3, e3 pela normal media (diagonais), coordenadas planas dos nos.
struct ShellFrame {
    Mat3 R {Mat3::Identity()};      ///< linhas = e1, e2, e3 (global -> local)
    Vec3 center {Vec3::Zero()};
    std::array<double, 4> x {}, y {};
    double area {0.0};
    double warp {0.0};              ///< maior distancia dos nos ao plano medio
};

ShellFrame shellFrame(const std::array<Vec3, 4> &p);

struct ShellStiffness {
    Mat24 K {Mat24::Zero()};                          ///< rigidez local (u v w tx ty tz por no)
    Eigen::Matrix<double, 4, 8> alphaFromU;           ///< recuperacao dos modos incompativeis a partir de (u,v)
};

ShellStiffness shellStiffness(const ShellFrame &frame, const Material &mat, double thickness, double shearCorrection);

/// Vetores de carga locais.
Vec24 shellPressureLoad(const ShellFrame &frame, const std::array<double, 4> &p);
Vec24 shellBodyLoad(const ShellFrame &frame, const Vec3 &bodyLocal, double thickness);
Vec24 shellThermalLoad(const ShellFrame &frame, const Material &mat, double thickness, double uniform, double gradient);

/// Rigidez geometrica local (efeito das forcas de membrana n11 n22 n12, N/m, sobre o deslocamento transversal w; tracao > 0 enrijece).
Mat24 shellGeometricStiffness(const ShellFrame &frame, double n11, double n22, double n12);

/// Resultantes nos 4 cantos e no centro a partir dos deslocamentos locais.
ShellResult shellResultants(const ShellFrame &frame, const Material &mat, double thickness, double shearCorrection, const ShellStiffness &stiffness,
                            const Vec24 &uLocal, double uniformTemp, double gradientTemp);

/// Matriz de transformacao dos 24 graus de liberdade (global -> local).
Mat24 shellTransform(const ShellFrame &frame);

} // namespace fem
