#pragma once

#include <array>
#include <string>
#include <variant>
#include <vector>

namespace Structura::Drawing {

/// Coordenadas de prancha em milimetros (papel). Origem no canto inferior esquerdo, y para cima.
struct Point {
    double x {0.0};
    double y {0.0};
};

enum class LineStyle { Continuous, Dashed, DashDot, Dotted };

struct Layer {
    std::string name;
    std::array<int, 3> rgb {0, 0, 0};
    double weight {0.25};   ///< espessura de linha no papel (mm)
    LineStyle style {LineStyle::Continuous};
};

struct Line {
    Point a, b;
    int layer {0};
};

struct Polyline {
    std::vector<Point> points;
    bool closed {false};
    bool filled {false};
    int layer {0};
};

struct Circle {
    Point center;
    double radius {1.0};
    bool filled {false};
    int layer {0};
};

enum class TextAlign { Left, Center, Right };

struct Text {
    Point position;
    std::string text;
    double height {2.5};        ///< altura do texto no papel (mm)
    double rotationDeg {0.0};
    TextAlign align {TextAlign::Left};
    bool bold {false};
    int layer {0};
};

using Entity = std::variant<Line, Polyline, Circle, Text>;

/// Uma prancha: papel, camadas e entidades (ja em milimetros do papel).
struct Sheet {
    std::string name;       ///< titulo curto da prancha
    std::string fileStem;   ///< nome sugerido de arquivo (sem extensao)
    double width {420.0};
    double height {297.0};
    std::vector<Layer> layers;
    std::vector<Entity> entities;
    /// Retangulos das vistas de uma folha composta (mm, y para cima), para a edicao interativa; `index` = posicao em SheetLayoutDef::views.
    struct EditCell {
        int index {0};
        double x0 {0.0}, y0 {0.0}, x1 {0.0}, y1 {0.0};
    };
    std::vector<EditCell> cells;
    double areaX0 {0.0}, areaY0 {0.0}, areaX1 {0.0}, areaY1 {0.0};   ///< area util onde as vistas podem ser posicionadas

    int addLayer(const std::string &name, std::array<int, 3> rgb, double weight, LineStyle style = LineStyle::Continuous)
    {
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].name == name) {
                return static_cast<int>(i);
            }
        }
        layers.push_back({name, rgb, weight, style});
        return static_cast<int>(layers.size()) - 1;
    }

    void line(Point a, Point b, int layer) { entities.push_back(Line {a, b, layer}); }
    void polyline(std::vector<Point> points, bool closed, int layer, bool filled = false) { entities.push_back(Polyline {std::move(points), closed, filled, layer}); }
    void rect(Point a, Point b, int layer, bool filled = false)
    {
        polyline({{a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}}, true, layer, filled);
    }
    void circle(Point c, double r, int layer, bool filled = false) { entities.push_back(Circle {c, r, filled, layer}); }
    void text(Point p, const std::string &s, double height, int layer, TextAlign align = TextAlign::Left, double rotation = 0.0, bool bold = false)
    {
        entities.push_back(Text {p, s, height, rotation, align, bold, layer});
    }
};

} // namespace Structura::Drawing
