#pragma once

#include <array>
#include <cmath>

/**
 * Referencial local das barras: definido em um unico lugar e usado pela tela, pela ponte do solver, pelo dimensionamento e pelas ligacoes.
 *
 *  - x local: do no I ao no J.
 *  - Barras nao verticais: vetor auxiliar = Z global; z = x cross aux; y = z cross x. O y fica no plano vertical da barra, sempre com componente
 *    positiva em Z (independe do sentido I->J): Mz positivo = tracao na fibra -y, nas vigas e nas tesouras.
 *  - Barras verticais (inclinacao menor que ~0,5 grau em relacao a vertical: ruido de coordenada nao altera o referencial): auxiliar = +X global,
 *    isto e, y = +X (como no SAP2000). A flexao no plano XZ do portico e Mz nas vigas e nos pilares.
 *  - O solver usa outra base (Z, depois Y, depois X); a ponte compensa com o giro em torno de x (referenceToSolverBaseDeg).
 *  - Arquivos antigos (sem o bloco [FRAME]) usam a base do solver para todas as barras, para que os resultados nao mudem (modo legado).
 *
 * O giro da secao nao altera estes eixos: so a geometria e as propriedades geometricas do perfil.
 */
namespace Structura::App::BarFrame {

using Vec3 = std::array<double, 3>;

constexpr double kVerticalTolerance = 0.0087;   ///< sen(0,5 grau): componente horizontal de x abaixo disso = barra vertical
constexpr int kFileVersion = 2;                  ///< versao gravada no bloco [FRAME]

inline bool &legacyFlag()
{
    static bool legacy = false;
    return legacy;
}
/// true = projeto aberto de arquivo antigo (base do solver em todas as barras).
inline bool legacy() { return legacyFlag(); }
inline void setLegacy(bool value) { legacyFlag() = value; }

inline Vec3 cross(const Vec3 &a, const Vec3 &b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline double norm(const Vec3 &v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
inline double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

/// Regra do solver (Structuradll): Z global, depois Y, depois X, o primeiro que nao for paralelo a barra (norma do produto vetorial > 1e-10).
inline Vec3 solverBaseAux(const Vec3 &x)
{
    const std::array<Vec3, 3> candidates = {{{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}}};
    for (const auto &candidate : candidates) {
        if (norm(cross(x, candidate)) > 1e-10) {
            return candidate;
        }
    }
    return {0.7071067811865476, 0.7071067811865476, 0.0};
}

/// Vetor auxiliar do referencial do programa para uma barra de direcao x (unitario).
inline Vec3 referenceAux(const Vec3 &x)
{
    if (legacy()) {
        return solverBaseAux(x);
    }
    if (std::hypot(x[0], x[1]) < kVerticalTolerance) {
        return {1.0, 0.0, 0.0};
    }
    return {0.0, 0.0, 1.0};
}

/// Eixo y local para o auxiliar dado (z = x cross aux; y = z cross x). Zero se o auxiliar for paralelo a x.
inline Vec3 axisY(const Vec3 &x, const Vec3 &aux)
{
    Vec3 z = cross(x, aux);
    const double n = norm(z);
    if (n < 1e-12) {
        return {0.0, 0.0, 0.0};
    }
    z = {z[0] / n, z[1] / n, z[2] / n};
    return cross(z, x);
}

/// Angulo (graus) a girar em torno de x para ir da base do solver ao referencial do programa: y_prog = cos.y_base + sen.z_base.
/// Soma-se ao giro da secao enviado ao solver. Zero no modo legado e onde as duas regras coincidem.
inline double referenceToSolverBaseDeg(const Vec3 &x)
{
    if (legacy()) {
        return 0.0;
    }
    const Vec3 zBase = [&] {
        Vec3 z = cross(x, solverBaseAux(x));
        const double n = norm(z);
        return Vec3 {z[0] / n, z[1] / n, z[2] / n};
    }();
    const Vec3 yBase = cross(zBase, x);
    const Vec3 yRef = axisY(x, referenceAux(x));
    const double c = dot(yRef, yBase), s = dot(yRef, zBase);
    if (std::abs(s) < 1e-12 && c > 0.0) {
        return 0.0;
    }
    return std::atan2(s, c) * 180.0 / 3.14159265358979323846;
}

} // namespace Structura::App::BarFrame
