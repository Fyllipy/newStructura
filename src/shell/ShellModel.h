#pragma once

#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

/// Modelo de cascas e chapas (placas, tanques, vasos de pressao): geometria parametrica, materiais, cargas e apoios.
/// Unidades SI: m, N, Pa, graus Celsius. Independente de Qt.
namespace Structura::Shell {

using Vec3 = std::array<double, 3>;

struct Material {
    std::string name {"Aco"};
    bool concrete {false};
    double E {2.0e11};            ///< Pa
    double nu {0.3};
    double unitWeight {77000.0};  ///< N/m3
    double alpha {1.2e-5};        ///< 1/K
    double fy {250.0e6};          ///< Pa (aco) ou fck (concreto)
    double fu {400.0e6};          ///< Pa
};

enum class Kind {
    Plate = 0,       ///< chapa/placa/laje poligonal plana
    RectTank = 1,    ///< reservatorio retangular (paredes, fundo e tampa)
    CylTank = 2,     ///< tanque cilindrico vertical (fundo, virolas, teto)
    Vessel = 3,      ///< vaso de pressao (cilindro com tampos, vertical ou horizontal)
    Disk = 4,        ///< disco ou anel desenhado no modelo
    Extrusion = 5,   ///< extrusao de uma linha/polilinha (parede, casca cilindrica reglada) ou de um circulo (cilindro, cone, tronco)
    Revolution = 6   ///< revolucao de um perfil em torno de um eixo (domos, esferas, tanques, toros)
};

/// Objeto parametrico. Parametros (`p`) por tipo:
///  Plate:    t (espessura, m), polygon (vertices, ordem anti-horaria vista do lado da normal positiva)
///  RectTank: lx ly h (m), tWall tBottom tRoof (m; tRoof = 0 sem tampa), x0 y0 z0 (canto inferior)
///  CylTank:  r h (m), courses (numero de virolas), t1..tN (m), tBottom, roof (0 aberto, 1 plano, 2 conico, 3 domo), roofRise, tRoof, x0 y0 z0
///  Disk:      polygon = {centro}; r, rIn (raio interno, 0 = disco), nx ny nz (normal positiva), t, flip
///  Extrusion: polygon = pontos da linha base (ou {centro} se circle = 1); circle (0/1), closed (0/1), r (circulo), vx vy vz (vetor de extrusao),
///             endScale (escala da extremidade: 1 cilindro, 0 cone, outro tronco; so para circulo), t, flip
///  Revolution: polygon = pontos do perfil (no mesmo semiplano do eixo); ax ay az (ponto do eixo), dx dy dz (direcao do eixo), angle (graus, 360), t, flip
///  Vessel:   r (raio interno medio), length (cilindro, m), t, head (0 hemisferico, 1 elipsoidal 2:1, 2 plano), tHead, horizontal (0/1),
///            skirt (altura da saia, m; 0 = sem), tSkirt, x0 y0 z0 (centro da base do cilindro)
struct Object {
    int id {0};
    std::string name;
    Kind kind {Kind::Plate};
    std::vector<Vec3> polygon;
    std::map<std::string, double> p;
    int material {0};
    double meshSize {0.5};   ///< dimensao alvo dos elementos (m)
    bool visible {true};
    double param(const std::string &key, double fallback = 0.0) const
    {
        const auto it = p.find(key);
        return it == p.end() ? fallback : it->second;
    }
};

struct Load {
    enum Type {
        SelfWeight = 0,    ///< peso proprio (fator em v[0], padrao 1)
        Pressure = 1,      ///< pressao uniforme v[0] (Pa), positiva no sentido da normal (para fora nos tanques e vasos)
        Hydrostatic = 2,   ///< liquido: v[0] = peso especifico (N/m3), v[1] = cota da superficie (m, global), v[2] = pressao do gas (Pa)
        Thermal = 3,       ///< v[0] = variacao uniforme de temperatura, v[1] = gradiente (face + normal menos face -)
        Area = 4           ///< forca por area em direcao global v[0..2] (N/m2); v[3] != 0: por area projetada em planta
    };
    std::string caseName;
    int object {0};   ///< id do objeto; 0 = todos
    Type type {SelfWeight};
    std::array<double, 4> v {{1.0, 0.0, 0.0, 0.0}};
};

struct Support {
    enum Selector {
        Plane = 0,      ///< nos com coordenada `axis` = `value` (+- tol)
        FreeEdge = 1,   ///< nos de bordas livres (aresta com um unico elemento) do objeto
        Point = 2       ///< no mais proximo de (value, value2, value3)
    };
    int object {0};   ///< 0 = todos
    Selector selector {Plane};
    int axis {2};     ///< 0 = x, 1 = y, 2 = z (Plane)
    double value {0.0}, value2 {0.0}, value3 {0.0};
    double tol {1.0e-6};
    int legs {0};     ///< Plane: > 0 limita a `legs` pontos igualmente espacados em angulo (apoios pontuais de vasos e tanques)
    double legStart {0.0};   ///< graus
    int limitAxis {-1};          ///< >= 0: so nos com coordenada `limitAxis` abaixo (ou acima) de `limitValue` (selas de vasos horizontais)
    double limitValue {0.0};
    bool limitBelow {true};
    std::array<int, 6> fixed {{1, 1, 1, 0, 0, 0}};
    double springPerArea {0.0};       ///< > 0: apoio elastico (N/m3) em vez de fixo; usa a area tributaria
    bool compressionOnly {false};     ///< apoio elastico que so comprime (fundacao)
    std::array<double, 3> springNormal {{0.0, 0.0, 1.0}};
};

struct Model {
    std::vector<Material> materials {Material {}};
    std::vector<Object> objects;
    std::vector<Load> loads;
    std::vector<Support> supports;
    /// true: as cascas entram na analise das barras (mesma matriz de rigidez; nos coincidentes e ligacoes por interpolacao).
    bool coupled {false};
    bool empty() const { return objects.empty(); }
};

/// Acrescenta materiais usuais (aco ASTM A572 e A516, concreto C30) se o modelo so tem o material padrao.
inline void ensureDefaultMaterials(Model &model)
{
    if (model.materials.size() > 1) {
        return;
    }
    Material a572;
    a572.name = "Aco ASTM A572 Gr.50";
    a572.fy = 345.0e6;
    a572.fu = 450.0e6;
    Material a516;
    a516.name = "Aco ASTM A516 Gr.70 (vasos)";
    a516.fy = 260.0e6;
    a516.fu = 485.0e6;
    Material c30;
    c30.name = "Concreto C30";
    c30.concrete = true;
    c30.nu = 0.2;
    c30.unitWeight = 25000.0;
    c30.alpha = 1.0e-5;
    c30.fy = 30.0e6;
    c30.fu = 0.0;
    c30.E = 0.85 * 5600.0 * std::sqrt(30.0) * 1.0e6;   // Ecs = alfa_i Eci (NBR 6118, 8.2.8)
    model.materials.push_back(a572);
    model.materials.push_back(a516);
    model.materials.push_back(c30);
}

/// Texto que muda sempre que o modelo muda (impressao digital para o desfazer).
inline std::string fingerprint(const Model &model)
{
    std::string out;
    auto num = [&out](double v) { out += std::to_string(v) + ','; };
    for (const Material &m : model.materials) {
        out += m.name + ';';
        num(m.E); num(m.nu); num(m.unitWeight); num(m.alpha); num(m.fy); num(m.fu);
        out += m.concrete ? '1' : '0';
    }
    out += '|';
    for (const Object &o : model.objects) {
        out += std::to_string(o.id) + o.name + ';' + std::to_string(static_cast<int>(o.kind)) + ',' + std::to_string(o.material) + ',' + (o.visible ? '1' : '0') + ',';
        num(o.meshSize);
        for (const Vec3 &p : o.polygon) {
            num(p[0]); num(p[1]); num(p[2]);
        }
        for (const auto &[key, value] : o.p) {
            out += key + '=';
            num(value);
        }
    }
    out += model.coupled ? "|C|" : "|U|";
    for (const Load &l : model.loads) {
        out += l.caseName + ',' + std::to_string(l.object) + ',' + std::to_string(static_cast<int>(l.type)) + ',';
        for (const double v : l.v) {
            num(v);
        }
    }
    out += '|';
    for (const Support &s : model.supports) {
        out += std::to_string(s.object) + ',' + std::to_string(static_cast<int>(s.selector)) + ',' + std::to_string(s.axis) + ',' + std::to_string(s.legs) + ',' + std::to_string(s.limitAxis) + ',';
        num(s.value); num(s.value2); num(s.value3); num(s.tol); num(s.legStart); num(s.limitValue); num(s.springPerArea);
        for (const int f : s.fixed) {
            out += std::to_string(f);
        }
        out += s.compressionOnly ? 'c' : 'n';
    }
    return out;
}

} // namespace Structura::Shell
