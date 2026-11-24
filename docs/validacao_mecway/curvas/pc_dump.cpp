// Cascas curvas de referencia (MacNeal-Harder) geradas pelo Structura como REVOLUCAO PARCIAL, pipeline real (Model -> generateMesh -> buildJob -> mefs.dll):
//   PC  pinched cylinder (cilindro com duas cargas radiais opostas, diafragmas rigidos nas pontas), modelo de 1/8 por simetria;
//       R = 300, L = 600, t = 3, E = 3e6, nu = 0,3, P = 1; deslocamento radial sob a carga: 1,8248e-5 (referencia conferida na web: serie de Fluegge, 1,8248e-5; Dvorkin e Bathe 1984).
//   HS  hemisferio com furo de 18 graus (quatro cargas radiais alternadas, a 90 graus), modelo de 1/4 por simetria;
//       R = 10, t = 0,04, E = 6,825e7, nu = 0,3; cargas de 1,0 nos nos do modelo de 1/4 (F = 2 no modelo inteiro); deslocamento radial sob a carga: 0,0924 (valor de memoria; fontes
//       conferidas na web citam 0,094 para a casca sem furo com a mesma convencao de cargas e 0,0927 a 0,0935 em malhas finas do caso com furo).
// uso: pc_dump <mefs.dll> [saida.csv]
#include "app/ShellSolverBridge.h"
#include "shell/ShellFactory.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"
#include "shell/ShellMeshQuality.h"
#include "shell/ShellModel.h"
#include "shell/ShellResults.h"

#include <QCoreApplication>
#include <QString>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Structura::Shell;

namespace pc {

constexpr double kPi = 3.14159265358979323846;

struct Case {
    std::string name;
    Model model;
    std::array<double, 3> probe;   // no onde se mede o deslocamento
    int component;                 // componente do deslocamento
    double sign;                   // sentido positivo da referencia
    double reference;
};

inline Support plane(int axis, double value, std::array<int, 6> fixed)
{
    Support s;
    s.selector = Support::Plane;
    s.axis = axis;
    s.value = value;
    s.tol = 1.0e-6;
    s.fixed = fixed;
    return s;
}

/// Pinched cylinder, 1/8: eixo X, x de 0 (meio do comprimento) a 300 (diafragma), arco de 90 graus de +Y (carga) a +Z.
inline Case pinchedCylinder(double h)
{
    const double R = 300.0, L = 300.0;
    Model model;
    model.materials[0].E = 3.0e6;
    model.materials[0].nu = 0.3;
    model.materials[0].unitWeight = 0.0;
    auto object = makeRevolution({{0.0, R, 0.0}, {L, R, 0.0}}, {0, 0, 0}, {1, 0, 0});
    object->id = 1;
    object->name = "Cilindro";
    object->p["t"] = 3.0;
    object->p["angle"] = 90.0;
    object->meshSize = h;
    model.objects = {*object};
    Load load;
    load.caseName = "Q";
    load.type = Load::NodalPoints;
    load.v = {0.0, -0.25, 0.0, 0.0};   // P/4 (o ponto esta em dois planos de simetria), para dentro
    load.points = {{0.0, R, 0.0}};
    model.loads = {load};
    model.supports = {plane(0, 0.0, {1, 0, 0, 0, 1, 1}),    // meio do comprimento: ux, ry, rz
                      plane(2, 0.0, {0, 0, 1, 1, 1, 0}),    // plano da carga (z = 0): uz, rx, ry
                      plane(1, 0.0, {0, 1, 0, 1, 0, 1}),    // plano a 90 graus (y = 0): uy, rx, rz
                      plane(0, L, {0, 1, 1, 0, 0, 0})};     // diafragma rigido no plano: uy, uz
    return {"pinched_cylinder", model, {0.0, R, 0.0}, 1, -1.0, 1.8248e-5};
}

/// Hemisferio com furo de 18 graus, 1/4: eixo Z, do furo (colatitude 18 graus) ao equador, 90 graus de +X a +Y.
inline Case hemisphere(int meridianDivisions)
{
    const double R = 10.0, a0 = 18.0 * kPi / 180.0, a1 = 0.5 * kPi;
    Model model;
    model.materials[0].E = 6.825e7;
    model.materials[0].nu = 0.3;
    model.materials[0].unitWeight = 0.0;
    std::vector<Vec3> profile;
    for (int i = 0; i <= meridianDivisions; ++i) {
        const double a = a0 + (a1 - a0) * i / meridianDivisions;
        profile.push_back({R * std::sin(a), 0.0, R * std::cos(a)});
    }
    auto object = makeRevolution(profile, {0, 0, 0}, {0, 0, 1});
    object->id = 1;
    object->name = "Hemisferio";
    object->p["t"] = 0.04;
    object->p["angle"] = 90.0;
    // um segmento do meridiano por elemento: tamanho um pouco maior que o comprimento do segmento (o arco do equador usa o mesmo tamanho)
    object->meshSize = R * (a1 - a0) / meridianDivisions * 1.001;
    model.objects = {*object};
    Load out, in;
    out.caseName = in.caseName = "Q";
    out.type = in.type = Load::NodalPoints;
    out.v = {1.0, 0.0, 0.0, 0.0};     // carga de 1,0 no no do plano de simetria (convencao do modelo de 1/4 de MacNeal-Harder: F = 2 no modelo inteiro)
    out.points = {{R, 0.0, 0.0}};
    in.v = {0.0, -1.0, 0.0, 0.0};     // idem, para dentro
    in.points = {{0.0, R, 0.0}};
    model.loads = {out, in};
    Support fixZ;
    fixZ.selector = Support::Point;
    fixZ.value = R;
    fixZ.value2 = 0.0;
    fixZ.value3 = 0.0;
    fixZ.fixed = {0, 0, 1, 0, 0, 0};   // movimento de corpo rigido em z
    model.supports = {plane(0, 0.0, {1, 0, 0, 0, 1, 1}), plane(1, 0.0, {0, 1, 0, 1, 0, 1}), fixZ};
    return {"hemisferio_furo18", model, {R, 0.0, 0.0}, 0, 1.0, 0.0924};
}

struct Outcome {
    bool ok {false};
    std::string error;
    double value {0.0};   // deslocamento medido no sentido da referencia
    double distance {0.0};
    std::size_t nodes {0}, quads {0};
    int qualityWarnings {0}, qualityErrors {0};
};

inline Outcome run(const Case &c, const Structura::App::ShellSolverBridge &bridge, Mesh *meshOut = nullptr, Results *resultsOut = nullptr)
{
    Outcome o;
    const Mesh mesh = generateMesh(c.model);
    const Job job = buildJob(c.model, mesh, {"Q"});
    const Results results = bridge.solve(c.model, mesh, job);
    o.nodes = mesh.nodes.size();
    o.quads = mesh.quads.size();
    const MeshQualityReport quality = analyzeMeshQuality(mesh, c.model);
    o.qualityWarnings = quality.warnings;
    o.qualityErrors = quality.errors;
    if (meshOut) {
        *meshOut = mesh;
    }
    if (!results.ok()) {
        o.error = results.error;
        return o;
    }
    const CaseResult &r = results.cases.front();
    double best = 1e300;
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
        const double d = std::sqrt(std::pow(mesh.nodes[i][0] - c.probe[0], 2) + std::pow(mesh.nodes[i][1] - c.probe[1], 2) + std::pow(mesh.nodes[i][2] - c.probe[2], 2));
        if (d < best) {
            best = d;
            o.value = c.sign * r.displacement[i][static_cast<std::size_t>(c.component)];
        }
    }
    o.distance = best;
    o.ok = true;
    if (resultsOut) {
        *resultsOut = results;
    }
    return o;
}

} // namespace pc

#ifndef PC_NO_MAIN
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        std::fprintf(stderr, "uso: pc_dump <mefs.dll> [saida.csv]\n");
        return 2;
    }
    const Structura::App::ShellSolverBridge bridge(args.at(1));
    std::ofstream csv;
    if (args.size() > 2) {
        csv.open(std::filesystem::u8path(args.at(2).toUtf8().toStdString()));
        csv << "caso,divisoes,nos,elementos,deslocamento,referencia,razao,avisos_qualidade,erros_qualidade\n";
        csv.precision(10);
    }
    for (const int n : {4, 8, 16, 32}) {
        for (int which = 0; which < 2; ++which) {
            const pc::Case c = which == 0 ? pc::pinchedCylinder(300.0 / n) : pc::hemisphere(n);
            const pc::Outcome o = pc::run(c, bridge);
            if (!o.ok) {
                std::printf("[%s n=%d] FALHA: %s\n", c.name.c_str(), n, o.error.c_str());
                continue;
            }
            std::printf("[%s n=%2d] %5zu nos %5zu elementos  u = %.6g  referencia %.6g  razao %.4f  (distancia ao no %.1e, avisos %d, erros %d)\n", c.name.c_str(), n, o.nodes, o.quads, o.value, c.reference,
                        o.value / c.reference, o.distance, o.qualityWarnings, o.qualityErrors);
            if (csv.is_open()) {
                csv << c.name << ',' << n << ',' << o.nodes << ',' << o.quads << ',' << o.value << ',' << c.reference << ',' << o.value / c.reference << ',' << o.qualityWarnings << ',' << o.qualityErrors << '\n';
            }
        }
    }
    return 0;
}
#endif
