// Validacao Structura x Mecway 28: monta cada caso com o pipeline REAL do Structura (Model -> generateMesh -> buildJob -> mefs.dll)
// e exporta malha, apoios, cargas e resultados em CSV para o gerador do modelo equivalente do Mecway e para a comparacao.
#include "app/ShellSolverBridge.h"
#include "val_cases.h"
#include "shell/ShellFactory.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "shell/ShellResults.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Structura::Shell;
using val::Case;
using val::buildCases;

namespace {

void writeCase(const Case &c, const std::filesystem::path &dir, const Structura::App::ShellSolverBridge &bridge)
{
    std::filesystem::create_directories(dir);
    const Mesh mesh = generateMesh(c.model);
    const Job job = buildJob(c.model, mesh, {"Q"});
    const Results results = bridge.solve(c.model, mesh, job);
    {
        std::ofstream meta(dir / "meta.txt");
        meta << "name=" << c.name << "\ndescription=" << c.description << "\n";
        meta << "E=" << c.model.materials[0].E << "\nnu=" << c.model.materials[0].nu << "\npressure=" << c.pressure << "\n";
        meta << "nodes=" << mesh.nodes.size() << "\nquads=" << mesh.quads.size() << "\nok=" << (results.ok() ? 1 : 0) << "\nerror=" << results.error << "\n";
        for (const std::string &w : job.warnings) {
            meta << "warning=" << w << "\n";
        }
        for (const std::string &w : mesh.warnings) {
            meta << "meshwarning=" << w << "\n";
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
    std::ofstream pressure(dir / "pressure.csv");
    pressure.precision(17);
    for (const auto &p : job.cases.front().pressure) {
        pressure << p.shell << ',' << p.p[0] << ',' << p.p[1] << ',' << p.p[2] << ',' << p.p[3] << '\n';
    }
    if (!results.ok()) {
        std::fprintf(stderr, "[%s] FALHA no solver: %s\n", c.name.c_str(), results.error.c_str());
        return;
    }
    const CaseResult &r = results.cases.front();
    std::ofstream disp(dir / "s_disp.csv");
    disp.precision(17);
    std::ofstream react(dir / "s_react.csv");
    react.precision(17);
    for (std::size_t i = 0; i < r.displacement.size(); ++i) {
        disp << i;
        react << i;
        for (int k = 0; k < 6; ++k) {
            disp << ',' << r.displacement[i][static_cast<std::size_t>(k)];
            react << ',' << r.reaction[i][static_cast<std::size_t>(k)];
        }
        disp << '\n';
        react << '\n';
    }
    // por elemento e canto (0..3) + centro (4): mxx myy mxy (N.m/m, eixos locais), nxx nyy nxy, von Mises na face superior (+normal) e inferior
    std::ofstream elem(dir / "s_elem.csv");
    elem.precision(17);
    for (std::size_t e = 0; e < r.elements.size(); ++e) {
        const ElementState &st = r.elements[e];
        for (int p = 0; p < 5; ++p) {
            const std::size_t b = static_cast<std::size_t>(p) * 8;
            const FaceStress top = faceStress(mesh.quads[e], st, p, +1);
            const FaceStress bot = faceStress(mesh.quads[e], st, p, -1);
            elem << e << ',' << p << ',' << st.v[b + 3] << ',' << st.v[b + 4] << ',' << st.v[b + 5] << ',' << st.v[b + 0] << ',' << st.v[b + 1] << ',' << st.v[b + 2] << ',' << top.vonMises() << ','
                 << bot.vonMises() << ',' << st.v[b + 6] << ',' << st.v[b + 7] << '\n';
        }
    }
    std::printf("[%s] %zu nos, %zu elementos: resolvido\n", c.name.c_str(), mesh.nodes.size(), mesh.quads.size());
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "uso: val_dump <pasta_saida> <mefs.dll>\n");
        return 2;
    }
    const std::filesystem::path out = std::filesystem::u8path(args.at(1).toUtf8().toStdString());
    const Structura::App::ShellSolverBridge bridge(args.at(2));
    QString reason;
    if (!bridge.available(&reason)) {
        std::fprintf(stderr, "solver indisponivel: %s\n", reason.toUtf8().constData());
        return 3;
    }
    for (const Case &c : buildCases()) {
        writeCase(c, out / c.name, bridge);
    }
    return 0;
}
