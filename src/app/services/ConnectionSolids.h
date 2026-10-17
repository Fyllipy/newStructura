#pragma once

#include "app/ProjectDataTypes.h"

#include <array>
#include <vector>

namespace Structura::App::Services {

/**
 * Sólidos esquemáticos das ligações para a vista 3D: chapas, parafusos, soldas, chumbadores e bloco.
 * A geometria é gerada no referencial da ligação (mm): s ao longo da barra, a partir do nó e para dentro dela;
 * t na altura da seção (eixo local y' da barra); u na normal da alma (eixo local z').
 */
class ConnectionSolids
{
public:
    enum class Role { Plate, Bolt, Weld, Concrete, Marker, Member };
    enum class Kind { Box, Cylinder, Prism };

    struct Primitive {
        Kind kind {Kind::Box};
        Role role {Role::Plate};
        std::array<double, 3> a {};      ///< Box: centro; Cylinder: inicio
        std::array<double, 3> b {};      ///< Cylinder: fim
        std::array<double, 3> size {};   ///< Box: dimensoes em s, t, u
        double radius {0.0};
        std::vector<std::array<double, 2>> polygon;   ///< Prism: contorno convexo em (s, t)
        double u0 {0.0}, u1 {0.0};                    ///< Prism: extrusao em u
    };

    struct Frame {
        std::array<double, 3> origin {};
        std::array<double, 3> s {1, 0, 0};
        std::array<double, 3> t {0, 1, 0};
        std::array<double, 3> u {0, 0, 1};
        double scale {0.001};   ///< unidades do modelo por mm
    };

    struct Mesh {
        std::vector<std::array<float, 3>> points;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<unsigned char, 3>> colors;
        std::vector<std::array<int, 3>> triangles;
    };

    /// `context` acrescenta as pecas ligadas (viga, pilar, banzo e diagonais) para vistas isoladas da ligacao.
    [[nodiscard]] static std::vector<Primitive> build(const Structura::App::ProjectData::NodeConnection &connection, bool context = false);
    /// Malha triangulada no referencial global; `eta` > 0 colore chapas pela utilizacao (verde ate 0,7, amarelo ate 1, vermelho acima).
    static void append(Mesh &mesh, const std::vector<Primitive> &primitives, const Frame &frame, double eta);
    /// Maior distancia (mm) de um ponto das pecas a origem da ligacao.
    [[nodiscard]] static double extent(const std::vector<Primitive> &primitives);
    [[nodiscard]] static std::array<unsigned char, 3> color(Role role, double eta);
};

} // namespace Structura::App::Services
