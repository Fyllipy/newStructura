// Casca curva de referencia (Scordelis-Lo, MacNeal-Harder) gerada pelo Structura como REVOLUCAO PARCIAL de um perfil reto (casca cilindrica), em 4 malhas:
// pipeline real (Model -> generateMesh -> buildJob -> mefs.dll). Exporta malha, apoios e resultados em CSV para o gerador do modelo do Mecway 28 e para a
// comparacao com o valor da literatura (flecha vertical no meio da borda livre; referencia 0,3024 - valor de memoria, nao conferido nesta sessao).
// uso: sl_dump <pasta_saida> <mefs.dll>
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

using namespace Structura::Shell;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kR = 25.0, kL = 50.0, kT = 0.25, kE = 4.32e8, kNu = 0.0, kGravity = 90.0;   // N/m2 por area de casca

Model scordelis(int axialElements)
{
    Model model;
    model.materials[0].E = kE;
    model.materials[0].nu = kNu;
    model.materials[0].unitWeight = 0.0;
    // perfil reto paralelo ao eixo X, num semiplano a 50 graus do eixo Y: o arco de 80 graus (de 50 a 130 graus) fica simetrico em torno da cumeeira (+Z)
    const double a0 = 50.0 * kPi / 180.0;
    const double y = kR * std::cos(a0), z = kR * std::sin(a0);
    auto object = makeRevolution({{0.0, y, z}, {kL, y, z}}, {0, 0, 0}, {1, 0, 0});
    object->id = 1;
    object->name = "Scordelis";
    object->p["t"] = kT;
    object->p["angle"] = 80.0;
    object->meshSize = kL / axialElements;
    model.objects = {*object};
    Load load;
    load.caseName = "Q";
    load.type = Load::Area;
    load.v = {0.0, 0.0, -kGravity, 0.0};
    model.loads = {load};
    // diafragmas rigidos nas extremidades curvas: uy = uz = 0; ux preso so na cumeeira de x = 0 (movimento de corpo rigido)
    Support ends[2];
    for (int i = 0; i < 2; ++i) {
        ends[i].selector = Support::Plane;
        ends[i].axis = 0;
        ends[i].value = i == 0 ? 0.0 : kL;
        ends[i].tol = 1.0e-6;
        ends[i].fixed = {0, 1, 1, 0, 0, 0};
    }
    Support crown;
    crown.selector = Support::Point;
    crown.value = 0.0;
    crown.value2 = 0.0;
    crown.value3 = kR;
    crown.fixed = {1, 0, 0, 0, 0, 0};
    model.supports = {ends[0], ends[1], crown};
    return model;
}

} // namespace

#ifndef SL_NO_MAIN
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "uso: sl_dump <pasta_saida> <mefs.dll>\n");
        return 2;
    }
    const std::filesystem::path out = std::filesystem::u8path(args.at(1).toUtf8().toStdString());
    const Structura::App::ShellSolverBridge bridge(args.at(2));
    for (const int n : {4, 8, 16, 32}) {
        const Model model = scordelis(n);
        const Mesh mesh = generateMesh(model);
        const Job job = buildJob(model, mesh, {"Q"});
        const Results results = bridge.solve(model, mesh, job);
        const std::filesystem::path dir = out / ("scordelis_" + std::to_string(n));
        std::filesystem::create_directories(dir);
        const MeshQualityReport quality = analyzeMeshQuality(mesh, model);
        {
            std::ofstream meta(dir / "meta.txt");
            meta << "name=scordelis_" << n << "\nE=" << kE << "\nnu=" << kNu << "\nthickness=" << kT << "\ngravity=" << kGravity << "\nnodes=" << mesh.nodes.size() << "\nquads=" << mesh.quads.size()
                 << "\nok=" << (results.ok() ? 1 : 0) << "\nerror=" << results.error << "\nquality_warnings=" << quality.warnings << "\nquality_errors=" << quality.errors << "\nmax_warp_over_size=" << quality.maxWarpOverSize
                 << "\nfree_edges_expected_only=1\n";
            for (const std::string &w : job.warnings) {
                meta << "warning=" << w << "\n";
            }
        }
        std::ofstream nodes(dir / "nodes.csv");
        nodes.precision(17);
        for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
            nodes << i << ',' << mesh.nodes[i][0] << ',' << mesh.nodes[i][1] << ',' << mesh.nodes[i][2] << '\n';
        }
        std::ofstream quads(dir / "quads.csv");
        quads.precision(17);
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            const MeshQuad &q = mesh.quads[e];
            quads << e << ',' << q.n[0] << ',' << q.n[1] << ',' << q.n[2] << ',' << q.n[3] << ',' << q.thickness << ',' << q.object << '\n';
        }
        std::ofstream fixed(dir / "fixed.csv");
        for (std::size_t i = 0; i < job.nodes.size(); ++i) {
            const auto &f = job.nodes[i].fixed;
            if (f[0] || f[1] || f[2] || f[3] || f[4] || f[5]) {
                fixed << i << ',' << f[0] << ',' << f[1] << ',' << f[2] << ',' << f[3] << ',' << f[4] << ',' << f[5] << '\n';
            }
        }
        if (!results.ok()) {
            std::fprintf(stderr, "[scordelis_%d] FALHA: %s\n", n, results.error.c_str());
            continue;
        }
        const CaseResult &r = results.cases.front();
        std::ofstream disp(dir / "s_disp.csv");
        disp.precision(17);
        double mid = 0.0, dist = 1e300;
        for (std::size_t i = 0; i < r.displacement.size(); ++i) {
            disp << i;
            for (int k = 0; k < 6; ++k) {
                disp << ',' << r.displacement[i][static_cast<std::size_t>(k)];
            }
            disp << '\n';
            // no mais proximo do meio da borda livre reta (x = 25, theta = 50 graus)
            const double a0 = 50.0 * kPi / 180.0;
            const double d = std::hypot(std::hypot(mesh.nodes[i][0] - 0.5 * kL, mesh.nodes[i][1] - kR * std::cos(a0)), mesh.nodes[i][2] - kR * std::sin(a0));
            if (d < dist) {
                dist = d;
                mid = r.displacement[i][2];
            }
        }
        std::printf("scordelis_%d: %zu nos, %zu elementos, uz(meio da borda livre) = %.6f (distancia ao ponto %.2e), avisos de qualidade %d, erros %d\n", n, mesh.nodes.size(), mesh.quads.size(), mid, dist,
                    quality.warnings, quality.errors);
    }
    return 0;
}
#endif
