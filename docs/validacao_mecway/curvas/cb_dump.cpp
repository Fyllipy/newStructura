// Casca curva: o que o Structura calcula alem da estatica linear (flambagem, modal, segunda ordem) e comparacao com o Mecway 28.
// Pipeline real: Shell::Model -> ShellCouplingService::attach (cascas acopladas, sem barras) -> SolverBridge -> rds.dll.
//   CB  cilindro de parede fina sob compressao axial: R = 1, L = 2, t = 0,01, E = 2,1e11, nu = 0,3, rho = 7850. Base: ux = uy = uz = 0; topo: ux = uy = 0
//       (anel carregado por forcas axiais iguais, linha de 1 N/m no total). O fator critico de flambagem e entao a carga por comprimento (N/m).
//       Referencia classica: N_cl = E t^2 / (R sqrt(3 (1 - nu^2))) (flambagem axissimetrica/de Donnell, cilindro "longo": vale com ressalva para L/R = 2).
//   CM  o mesmo cilindro em vibracao (frequencias naturais).
//   NL  tira em balanco com carga na ponta (PL^2/EI = 1): solucao da elastica (v/L = 0,3017) contra linear e segunda ordem do Structura.
// uso: cb_dump <rds.dll> <pasta_saida> [h]
#include "app/SolverBridge.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellFactory.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"

#include <QCoreApplication>
#include <QString>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using Structura::App::SolverBridge;
using Structura::App::Services::ShellCouplingService;
namespace Sh = Structura::Shell;

namespace cb {

constexpr double kPi = 3.14159265358979323846;
constexpr double kR = 1.0, kL = 2.0, kT = 0.01, kE = 2.1e11, kNu = 0.3, kRho = 7850.0;

inline Sh::Model cylinder(double h, std::vector<std::array<double, 3>> *ring = nullptr)
{
    Sh::Model model;
    model.materials[0].E = kE;
    model.materials[0].nu = kNu;
    model.materials[0].unitWeight = kRho * 9.80665;   // densidade 7850 kg/m3 (o peso proprio so entra se houver carga de peso)
    auto object = Sh::makeRevolution({{kR, 0.0, 0.0}, {kR, 0.0, kL}}, {0, 0, 0}, {0, 0, 1});
    object->id = 1;
    object->name = "Cilindro";
    object->p["t"] = kT;
    object->p["angle"] = 360.0;
    object->meshSize = h;
    model.objects = {*object};
    Sh::Support bottom, top;
    bottom.selector = top.selector = Sh::Support::Plane;
    bottom.axis = top.axis = 2;
    bottom.value = 0.0;
    top.value = kL;
    bottom.tol = top.tol = 1.0e-6;
    bottom.fixed = {1, 1, 1, 0, 0, 0};
    top.fixed = {1, 1, 0, 0, 0, 0};
    model.supports = {bottom, top};
    const Sh::Mesh mesh = Sh::generateMesh(model);
    Sh::Load load;
    load.caseName = "Q";
    load.type = Sh::Load::NodalPoints;
    std::vector<std::array<double, 3>> points;
    for (const auto &p : mesh.nodes) {
        if (std::abs(p[2] - kL) < 1e-6) {
            points.push_back(p);
        }
    }
    const double f0 = 2.0 * kPi * kR / static_cast<double>(points.size());   // linha de 1 N/m no total
    load.v = {0.0, 0.0, -f0, 0.0};
    load.points = points;
    model.loads = {load};
    if (ring) {
        *ring = points;
    }
    return model;
}

struct Solved {
    SolverBridge::ModelData data;
    std::shared_ptr<const ShellCouplingService::Mapping> mapping;
};

inline Solved attach(const Sh::Model &shell)
{
    Solved s;
    s.data.materials.clear();
    s.data.sections.clear();
    Sh::Model coupled = shell;
    coupled.coupled = true;
    coupled.monolithic = true;
    s.mapping = ShellCouplingService::attach(s.data, coupled, "Q", 1.0);
    return s;
}

} // namespace cb

#ifndef CB_NO_MAIN
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "uso: cb_dump <rds.dll> <pasta_saida> [h]\n");
        return 2;
    }
    const SolverBridge solver(args.at(1));
    const std::filesystem::path out = std::filesystem::u8path(args.at(2).toUtf8().toStdString());
    const double h = args.size() > 3 ? args.at(3).toDouble() : 0.1;
    std::filesystem::create_directories(out);

    // ---- cilindro: exporta a malha e roda flambagem e modal ----
    std::vector<std::array<double, 3>> ring;
    const Sh::Model shell = cb::cylinder(h, &ring);
    const Sh::Mesh mesh = Sh::generateMesh(shell);
    const Sh::Job job = Sh::buildJob(shell, mesh, {"Q"});
    {
        std::ofstream meta(out / "meta.txt");
        meta << "E=" << cb::kE << "\nnu=" << cb::kNu << "\nthickness=" << cb::kT << "\nrho=" << cb::kRho << "\nR=" << cb::kR << "\nL=" << cb::kL << "\nring_nodes=" << ring.size()
             << "\nforce_per_node=" << 2.0 * cb::kPi * cb::kR / static_cast<double>(ring.size()) << "\nnodes=" << mesh.nodes.size() << "\nquads=" << mesh.quads.size() << "\n";
        std::ofstream nodes(out / "nodes.csv");
        nodes.precision(17);
        for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
            nodes << i << ',' << mesh.nodes[i][0] << ',' << mesh.nodes[i][1] << ',' << mesh.nodes[i][2] << '\n';
        }
        std::ofstream quads(out / "quads.csv");
        quads.precision(17);
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            quads << e << ',' << mesh.quads[e].n[0] << ',' << mesh.quads[e].n[1] << ',' << mesh.quads[e].n[2] << ',' << mesh.quads[e].n[3] << ',' << mesh.quads[e].thickness << '\n';
        }
        std::ofstream fixed(out / "fixed.csv");
        for (std::size_t i = 0; i < job.nodes.size(); ++i) {
            const auto &f = job.nodes[i].fixed;
            if (f[0] || f[1] || f[2]) {
                fixed << i << ',' << f[0] << ',' << f[1] << ',' << f[2] << '\n';
            }
        }
        std::ofstream loads(out / "loads.csv");
        loads.precision(17);
        for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
            if (std::abs(mesh.nodes[i][2] - cb::kL) < 1e-6) {
                loads << i << ',' << shell.loads[0].v[2] << '\n';
            }
        }
    }
    auto writeShape = [&](const char *file, const SolverBridge::ModelData &data, const std::vector<double> &shape) {
        // forma de flambagem/modal dos nos de casca: 6 valores por no, na ordem de data.shell->nodes
        std::ofstream o(out / file);
        o.precision(17);
        for (std::size_t i = 0; i < data.shell->nodes.size(); ++i) {
            const auto &n = data.shell->nodes[i];
            const std::size_t base = 6 * i;
            if (base + 2 < shape.size()) {
                o << n.x << ',' << n.y << ',' << n.z << ',' << shape[base] << ',' << shape[base + 1] << ',' << shape[base + 2] << '\n';
            }
        }
    };
    {
        cb::Solved s = cb::attach(shell);
        if (!s.mapping) {
            std::fprintf(stderr, "attach falhou\n");
            return 1;
        }
        s.data.bucklingModes = 6;
        const SolverBridge::Result r = solver.solve(s.data);
        std::ofstream o(out / "s_buckling.txt");
        o.precision(10);
        const double ncl = cb::kE * cb::kT * cb::kT / (cb::kR * std::sqrt(3.0 * (1.0 - cb::kNu * cb::kNu)));
        o << "classico=" << ncl << "\n";
        for (std::size_t k = 0; k < r.bucklingFactors.size(); ++k) {
            o << "fator" << k + 1 << '=' << r.bucklingFactors[k] << "\n";
            std::printf("flambagem modo %zu: %.6g N/m  (%.4f do classico %.6g)\n", k + 1, r.bucklingFactors[k], r.bucklingFactors[k] / ncl, ncl);
        }
        for (const QString &w : r.warnings) {
            o << "aviso=" << w.toStdString() << "\n";
        }
        if (!r.bucklingShellShapes.empty()) {
            std::printf("forma de flambagem dos nos de casca: %zu valores para %zu nos\n", r.bucklingShellShapes[0].size(), s.data.shell->nodes.size());
            writeShape("s_buckling_mode1.csv", s.data, r.bucklingShellShapes[0]);
        }
    }
    {
        cb::Solved s = cb::attach(shell);
        s.data.modalModes = 8;
        const SolverBridge::Result r = solver.solve(s.data);
        std::ofstream o(out / "s_modal.txt");
        o.precision(10);
        for (std::size_t k = 0; k < r.modalOmega.size(); ++k) {
            o << "f" << k + 1 << '=' << r.modalOmega[k] / (2.0 * cb::kPi) << "\n";
            std::printf("modal %zu: %.6g Hz\n", k + 1, r.modalOmega[k] / (2.0 * cb::kPi));
        }
        for (const QString &w : r.warnings) {
            o << "aviso=" << w.toStdString() << "\n";
        }
        if (!r.modalShellShapes.empty()) {
            writeShape("s_modal_mode1.csv", s.data, r.modalShellShapes[0]);
        }
    }
    // ---- NL: tira em balanco com carga na ponta, PL^2/EI = 1 (grandes deslocamentos): elastica de referencia v/L = 0,3017 (Timoshenko & Gere), encurtamento 0,0560 L ----
    {
        const double L = 1.0, b = 0.1, t = 0.01, E = 2.1e11;
        const double EI = E * b * t * t * t / 12.0, P = EI / (L * L);
        Sh::Model strip;
        strip.materials[0].E = E;
        strip.materials[0].nu = 0.0;
        strip.materials[0].unitWeight = 0.0;
        Sh::Object plate;
        plate.id = 1;
        plate.name = "Tira";
        plate.kind = Sh::Kind::Plate;
        plate.polygon = {{0, 0, 0}, {L, 0, 0}, {L, b, 0}, {0, b, 0}};
        plate.p = {{"t", t}};
        plate.meshSize = 0.025;
        strip.objects = {plate};
        Sh::Support clamp;
        clamp.selector = Sh::Support::Plane;
        clamp.axis = 0;
        clamp.value = 0.0;
        clamp.fixed = {1, 1, 1, 1, 1, 1};
        strip.supports = {clamp};
        const Sh::Mesh smesh = Sh::generateMesh(strip);
        Sh::Load tip;
        tip.caseName = "Q";
        tip.type = Sh::Load::NodalPoints;
        std::vector<std::array<double, 3>> tipNodes;
        for (const auto &p : smesh.nodes) {
            if (std::abs(p[0] - L) < 1e-9) {
                tipNodes.push_back(p);
            }
        }
        tip.v = {0.0, 0.0, -P / static_cast<double>(tipNodes.size()), 0.0};
        tip.points = tipNodes;
        strip.loads = {tip};
        std::ofstream o(out / "s_nonlinear.txt");
        o.precision(10);
        for (const bool second : {false, true}) {
            cb::Solved s = cb::attach(strip);
            s.data.secondOrder = second;
            const SolverBridge::Result r = solver.solve(s.data);
            double tipW = 0.0, tipU = 0.0;
            for (std::size_t i = 0; i < s.data.shell->nodes.size() && i < r.shellNodeResults.size(); ++i) {
                if (std::abs(s.data.shell->nodes[i].x - L) < 1e-9 && std::abs(s.data.shell->nodes[i].y) < 1e-9) {
                    tipW = r.shellNodeResults[i].displacements[2];
                    tipU = r.shellNodeResults[i].displacements[0];
                }
            }
            o << (second ? "segunda_ordem" : "linear") << "_w_sobre_L=" << tipW / L << "\n" << (second ? "segunda_ordem" : "linear") << "_u_sobre_L=" << tipU / L << "\n";
            for (const QString &w : r.warnings) {
                o << "aviso=" << w.toStdString() << "\n";
            }
            std::printf("balanco PL2/EI=1 (%s): w/L = %.5f (elastica -0.3017), u/L = %.5f (elastica -0.0560)\n", second ? "segunda ordem" : "linear", tipW / L, tipU / L);
        }
    }
    return 0;
}
#endif
