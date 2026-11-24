#pragma once
// Casos de validacao Structura x Mecway (compartilhados entre val_dump e val_shots).
#include "shell/ShellFactory.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"

#include <string>
#include <vector>

namespace val {

using namespace Structura::Shell;

inline Object plateObject(int id, const std::string &name, std::vector<Vec3> polygon, double t, double meshSize)
{
    Object o;
    o.id = id;
    o.name = name;
    o.kind = Kind::Plate;
    o.polygon = std::move(polygon);
    o.p = {{"t", t}};
    o.meshSize = meshSize;
    return o;
}

inline Support fixedAtPoint(double x, double y, double z, std::array<int, 6> fixed)
{
    Support s;
    s.selector = Support::Point;
    s.value = x;
    s.value2 = y;
    s.value3 = z;
    s.fixed = fixed;
    return s;
}

struct Case {
    std::string name;
    std::string description;
    Model model;
    double pressure {0.0};   // Pa, positiva no sentido da normal do elemento
};

inline std::vector<Case> buildCases()
{
    std::vector<Case> cases;
    auto pressureLoad = [](double p) {
        Load l;
        l.caseName = "Q";
        l.type = Load::Pressure;
        l.v = {p, 0.0, 0.0, 0.0};
        return l;
    };
    // 1) placa quadrada simplesmente apoiada (fina), pressao uniforme
    {
        Case c;
        c.name = "ss_plate";
        c.description = "Placa 1x1 m, t=10 mm, apoiada nas 4 bordas (UZ=0), pressao 1 kPa, malha 8x8";
        c.model.objects.push_back(plateObject(1, "Placa", {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, 0.01, 0.125));
        Support edge;
        edge.selector = Support::FreeEdge;
        edge.fixed = {0, 0, 1, 0, 0, 0};
        c.model.supports = {edge, fixedAtPoint(0, 0, 0, {1, 1, 0, 0, 0, 0}), fixedAtPoint(1, 0, 0, {0, 1, 0, 0, 0, 0})};
        c.model.loads = {pressureLoad(1000.0)};
        c.pressure = 1000.0;
        cases.push_back(c);
    }
    // 2) placa quadrada engastada
    {
        Case c;
        c.name = "clamped_plate";
        c.description = "Placa 1x1 m, t=10 mm, engastada nas 4 bordas, pressao 1 kPa, malha 8x8";
        c.model.objects.push_back(plateObject(1, "Placa", {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, 0.01, 0.125));
        Support edge;
        edge.selector = Support::FreeEdge;
        edge.fixed = {1, 1, 1, 1, 1, 1};
        c.model.supports = {edge};
        c.model.loads = {pressureLoad(1000.0)};
        c.pressure = 1000.0;
        cases.push_back(c);
    }
    // 3) placa em balanco (faixa), flexao com pressao
    {
        Case c;
        c.name = "cantilever_plate";
        c.description = "Faixa 4x1 m, t=100 mm, engastada em x=0, pressao 50 kPa, malha 16x4";
        c.model.objects.push_back(plateObject(1, "Balanco", {{0, 0, 0}, {4, 0, 0}, {4, 1, 0}, {0, 1, 0}}, 0.1, 0.25));
        Support wall;
        wall.selector = Support::Plane;
        wall.axis = 0;
        wall.value = 0.0;
        wall.fixed = {1, 1, 1, 1, 1, 1};
        c.model.supports = {wall};
        c.model.loads = {pressureLoad(50000.0)};
        c.pressure = 50000.0;
        cases.push_back(c);
    }
    // 4) cilindro aberto com pressao interna (casca curva, regime de membrana + efeito de borda no engaste)
    {
        Case c;
        c.name = "cylinder_pressure";
        c.description = "Cilindro R=1 m, H=2 m, t=10 mm, engastado em z=0, topo livre, pressao interna 100 kPa";
        auto cyl = makeCylinder({0, 0, 0}, 1.0, {0, 0, 2.0}, 1.0);
        Object o = *cyl;
        o.id = 1;
        o.name = "Cilindro";
        o.p["t"] = 0.01;
        o.meshSize = 0.2;
        c.model.objects.push_back(o);
        Support base;
        base.selector = Support::Plane;
        base.axis = 2;
        base.value = 0.0;
        base.fixed = {1, 1, 1, 1, 1, 1};
        c.model.supports = {base};
        c.model.loads = {pressureLoad(100000.0)};
        c.pressure = 100000.0;
        cases.push_back(c);
    }
    // 5) placa trapezoidal (malha nao estruturada, elementos distorcidos), simplesmente apoiada
    {
        Case c;
        c.name = "trapezoid_plate";
        c.description = "Placa trapezoidal (0,0)-(2,0)-(1.4,1)-(0,1) m, t=20 mm, apoiada nas bordas (UZ=0), pressao 5 kPa, malha 0,125 m";
        c.model.objects.push_back(plateObject(1, "Trapezio", {{0, 0, 0}, {2, 0, 0}, {1.4, 1, 0}, {0, 1, 0}}, 0.02, 0.125));
        Support edge;
        edge.selector = Support::FreeEdge;
        edge.fixed = {0, 0, 1, 0, 0, 0};
        c.model.supports = {edge, fixedAtPoint(0, 0, 0, {1, 1, 0, 0, 0, 0}), fixedAtPoint(2, 0, 0, {0, 1, 0, 0, 0, 0})};
        c.model.loads = {pressureLoad(5000.0)};
        c.pressure = 5000.0;
        cases.push_back(c);
    }
    // 6) mesmo cilindro, malha fina (resolve a camada limite do engaste: comprimento caracteristico ~ sqrt(R t) = 0,1 m)
    {
        Case c = cases[3];
        c.name = "cylinder_pressure_fine";
        c.description = "Cilindro R=1 m, H=2 m, t=10 mm, engastado em z=0, topo livre, pressao interna 100 kPa, malha fina (0,07 m)";
        c.model.objects[0].meshSize = 0.07;
        cases.push_back(c);
    }
    return cases;
}

} // namespace val
