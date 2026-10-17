#pragma once

#include <array>
#include <vector>

namespace Structura::Drawing {

/// Camera de uma vista 3D desenhada em linhas (eixo Z do modelo na vertical).
struct Camera3D {
    std::array<double, 3> position {1.0, -1.0, 1.0};   ///< posicao do olho (so perspectiva)
    std::array<double, 3> target {0.0, 0.0, 0.0};
    std::array<double, 3> up {0.0, 0.0, 1.0};
    bool perspective {false};
    double fovDeg {30.0};                              ///< abertura vertical (perspectiva)
};

/// Camera ortogonal a partir de azimute e elevacao: o olho fica na direcao
/// (sen az cos el, -cos az cos el, sen el) em relacao ao alvo (az = 0, el = 0: vista frontal, X para a direita).
Camera3D cameraFromAngles(double azimuthDeg, double elevationDeg, const std::array<double, 3> &target, double distance);

using Vec3 = std::array<double, 3>;

struct Triangle3D {
    Vec3 a, b, c;
};

struct Edge3D {
    Vec3 a, b;
    int tag {0};   ///< identificador livre devolvido junto do segmento visivel
};

struct Segment2D {
    double x0, y0, x1, y1;   ///< unidades da camera (ortogonal: mm do modelo; perspectiva: tangentes, x/z e y/z)
    int tag {0};
    bool hidden {false};
};

struct HiddenLineResult {
    std::vector<Segment2D> segments;
    double minX {0}, maxX {0}, minY {0}, maxY {0};   ///< limites de todas as geometrias projetadas
    bool ok {false};
};

/// Remocao de linhas ocultas por buffer de profundidade: so os trechos visiveis das arestas sao devolvidos
/// (e, se `keepHidden`, tambem os ocultos, marcados). `cells` e a resolucao do buffer na maior dimensao.
HiddenLineResult computeHiddenLines(const Camera3D &camera, const std::vector<Triangle3D> &faces, const std::vector<Edge3D> &edges, bool keepHidden, int cells = 1400);

/// Triangulacao de um poligono simples por recorte de orelhas (sentido qualquer). Devolve indices de 3 em 3.
std::vector<int> triangulatePolygon(const std::vector<std::array<double, 2>> &polygon);

} // namespace Structura::Drawing
