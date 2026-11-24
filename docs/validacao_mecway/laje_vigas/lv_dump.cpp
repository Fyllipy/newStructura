// Validacao laje + vigas + pilares: monta cada caso com o pipeline REAL do Structura (Shell::Model + barras -> ShellCouplingService -> rds.dll),
// em tres modos de ligacao da laje com as barras, e exporta malha, barras, apoios, cargas e resultados em CSV para o gerador do modelo
// equivalente do Mecway 28 e para a comparacao.
//   T = quinhoes de carga (sem acoplamento por MEF; a laje vira cargas distribuidas nas vigas)
//   A = acoplada, laje APOIADA nas barras (so translacoes ligadas)
//   M = acoplada, ligacao monolitica (6 graus compartilhados)  <- equivalente do modelo do Mecway (nos compartilhados)
// uso: lv_dump <pasta_saida> <rds.dll> [calib]
#include "app/SolverBridge.h"
#include "app/services/ShellCouplingService.h"
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
#include <tuple>
#include <vector>

using Structura::App::SolverBridge;
using Structura::App::Services::ShellCouplingService;
namespace Sh = Structura::Shell;

namespace {

constexpr double kE = 30.0e9, kNu = 0.2;   // concreto
constexpr double kSide = 0.30;             // vigas e pilares quadrados (sem ambiguidade de eixo local)

struct Bar {
    std::array<double, 3> a, b;
    double spacing;   // > 0: divide a barra a cada `spacing` (nos coincidentes com a malha da laje)
    bool column;
};

struct Case {
    std::string name, description;
    double lx, ly, z;      // laje no plano z
    double t, mesh;
    double pressure;       // Pa (negativo = para baixo)
    std::vector<Bar> bars;
    bool splitBars {true};
    std::vector<std::array<double, 2>> probesXY;   // pontos da laje (cota z) onde o deslocamento e comparado
    int loadKind {0};      // 0 = Pressure; 1 = RegionPoints em todos os nos (carga por clique da Fase 4)
    double slabNu {kNu};   // coeficiente de Poisson da laje (a rigidez das barras usa kNu)
};

Case slabOnBeams(const std::string &name, const std::string &desc, double lx, double ly, double H, double mesh)
{
    Case c;
    c.name = name;
    c.description = desc;
    c.lx = lx;
    c.ly = ly;
    c.z = H;
    c.t = 0.15;
    c.mesh = mesh;
    c.pressure = -5000.0;
    return c;
}

void addPerimeter(Case &c)
{
    const double m = c.mesh, z = c.z;
    c.bars.push_back({{0, 0, z}, {c.lx, 0, z}, m, false});
    c.bars.push_back({{c.lx, 0, z}, {c.lx, c.ly, z}, m, false});
    c.bars.push_back({{c.lx, c.ly, z}, {0, c.ly, z}, m, false});
    c.bars.push_back({{0, c.ly, z}, {0, 0, z}, m, false});
}

void addColumn(Case &c, double x, double y)
{
    c.bars.push_back({{x, y, 0.0}, {x, y, c.z}, 0.0, true});
}

std::vector<Case> buildCases()
{
    std::vector<Case> out;
    {
        Case c = slabOnBeams("LV1_laje_4vigas_4pilares", "Laje 6x4 m (t=15 cm) sobre 4 vigas de borda 30x30 e 4 pilares de canto 30x30 (H=3 m), carga 5 kN/m2", 6, 4, 3, 1.0);
        addPerimeter(c);
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {6, 0}, {6, 4}, {0, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        c.probesXY = {{3, 2}, {3, 0}, {0, 2}, {1, 1}, {5, 3}};
        out.push_back(c);
    }
    {
        Case c = slabOnBeams("LV2_duas_baias_viga_central", "Laje 8x4 m sobre vigas de borda e viga central em x=4 m, 6 pilares", 8, 4, 3, 1.0);
        addPerimeter(c);
        c.bars.push_back({{4, 0, 3}, {4, 4, 3}, 1.0, false});
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {8, 0}, {8, 4}, {0, 4}, {4, 0}, {4, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        c.probesXY = {{2, 2}, {6, 2}, {4, 2}, {2, 0}, {4, 1}};
        out.push_back(c);
    }
    {
        Case c = slabOnBeams("LV3_laje_unidirecional_2vigas", "Laje 6x4 m apoiada so em 2 vigas (y=0 e y=4) com 4 pilares; bordas x=0 e x=6 livres", 6, 4, 3, 1.0);
        c.bars.push_back({{0, 0, 3}, {6, 0, 3}, 1.0, false});
        c.bars.push_back({{0, 4, 3}, {6, 4, 3}, 1.0, false});
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {6, 0}, {6, 4}, {0, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        c.probesXY = {{3, 2}, {3, 0}, {0, 2}, {6, 2}, {1, 1}};
        out.push_back(c);
    }
    {
        Case c = slabOnBeams("LV4_laje_lisa_4pilares", "Laje lisa 6x6 m sem vigas sobre 4 pilares 30x30 em (1.5;1.5) (4.5;1.5) (4.5;4.5) (1.5;4.5), balancos de 1.5 m", 6, 6, 3, 0.75);
        for (const auto &p : std::vector<std::array<double, 2>>{{1.5, 1.5}, {4.5, 1.5}, {4.5, 4.5}, {1.5, 4.5}}) {
            addColumn(c, p[0], p[1]);
        }
        c.probesXY = {{3, 3}, {0, 0}, {3, 0}, {1.5, 3}, {3, 1.5}};
        out.push_back(c);
    }
    {
        Case c = slabOnBeams("LV5_carga_por_clique", "Mesma geometria do LV1, carga aplicada pela Fase 4 (RegionPoints em todos os nos): 5 kN/m2 por area global", 6, 4, 3, 1.0);
        addPerimeter(c);
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {6, 0}, {6, 4}, {0, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        c.loadKind = 1;
        c.probesXY = {{3, 2}, {3, 0}, {0, 2}, {1, 1}, {5, 3}};
        out.push_back(c);
    }
    {
        Case c = slabOnBeams("LV6_vigas_sem_dividir", "Mesmo modelo do LV1, mas com as vigas NAO divididas nos nos da malha (como o usuario desenha)", 6, 4, 3, 1.0);
        addPerimeter(c);
        for (auto &b : c.bars) {
            b.spacing = 0.0;
        }
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {6, 0}, {6, 4}, {0, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        c.probesXY = {{3, 2}, {3, 0}, {0, 2}, {1, 1}, {5, 3}};
        out.push_back(c);
    }
    // varredura da espessura da laje armada numa direcao (mesma geometria do LV3): quanto mais flexivel a laje em relacao as vigas, mais o acoplamento
    // se aproxima da hipotese de quinhoes (apoios rigidos); nomes LV3t<espessura em mm>
    for (const int tmm : {100, 50, 30, 20, 1020, 1150, 1005}) {
        Case c = slabOnBeams("LV3t" + std::to_string(tmm), "Laje 6x4 m armada numa direcao, apoiada so em 2 vigas (y=0 e y=4) e 4 pilares; espessura " + std::to_string(tmm) + " mm", 6, 4, 3, 1.0);
        c.t = (tmm > 1000 ? tmm - 1000 : tmm) / 1000.0;
        if (tmm > 1000) {
            c.slabNu = 0.0;   // sem efeito de Poisson: elimina a forca de canto da placa com bordas livres
        }
        c.bars.push_back({{0, 0, 3}, {6, 0, 3}, 1.0, false});
        c.bars.push_back({{0, 4, 3}, {6, 4, 3}, 1.0, false});
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {6, 0}, {6, 4}, {0, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        c.probesXY = {{3, 2}, {3, 0}};
        out.push_back(c);
    }
    // independencia de malha da laje espessa (150 mm) e da laje muito fina: malha de 0,5 m e 0,25 m
    for (const double h : {0.5, 0.25}) {
        Case c = slabOnBeams("LV3h" + std::to_string(static_cast<int>(h * 100)) + "_t150", "Laje 6x4 m, 150 mm, armada numa direcao sobre 2 vigas e 4 pilares; malha " + std::to_string(h) + " m", 6, 4, 3, h);
        c.bars.push_back({{0, 0, 3}, {6, 0, 3}, h, false});
        c.bars.push_back({{0, 4, 3}, {6, 4, 3}, h, false});
        for (const auto &p : std::vector<std::array<double, 2>>{{0, 0}, {6, 0}, {6, 4}, {0, 4}}) {
            addColumn(c, p[0], p[1]);
        }
        out.push_back(c);
    }
    return out;
}

struct Key {
    long long x, y, z;
    bool operator<(const Key &o) const { return std::tie(x, y, z) < std::tie(o.x, o.y, o.z); }
};
Key keyOf(double x, double y, double z)
{
    return {std::llround(x * 1e6), std::llround(y * 1e6), std::llround(z * 1e6)};
}

struct Built {
    SolverBridge::ModelData model;
    std::vector<std::array<int, 2>> bars;                 // (no i, no j) de cada barra do modelo
    std::map<int, std::array<double, 3>> position;
    std::vector<int> columnBaseNodes;
    std::vector<char> barIsColumn;
};

Built buildBars(const Case &c)
{
    Built b;
    SolverBridge::MaterialData material;
    material.id = 1;
    material.e = kE;
    material.g = kE / (2.0 * (1.0 + kNu));
    b.model.materials.push_back(material);
    SolverBridge::SectionData section;
    section.id = 1;
    section.area = kSide * kSide;
    section.iy = section.iz = kSide * kSide * kSide * kSide / 12.0;
    section.j = 0.140577 * std::pow(kSide, 4);
    b.model.sections.push_back(section);
    std::map<Key, int> ids;
    auto nodeAt = [&](double x, double y, double z) {
        const Key k = keyOf(x, y, z);
        const auto it = ids.find(k);
        if (it != ids.end()) {
            return it->second;
        }
        SolverBridge::NodeData n;
        n.id = static_cast<int>(b.model.nodes.size()) + 1;
        n.x = x;
        n.y = y;
        n.z = z;
        b.model.nodes.push_back(n);
        ids[k] = n.id;
        b.position[n.id] = {x, y, z};
        return n.id;
    };
    int memberId = 0;
    for (const Bar &bar : c.bars) {
        const double len = std::sqrt(std::pow(bar.b[0] - bar.a[0], 2) + std::pow(bar.b[1] - bar.a[1], 2) + std::pow(bar.b[2] - bar.a[2], 2));
        const int parts = (c.splitBars && bar.spacing > 0.0) ? std::max(1, static_cast<int>(std::lround(len / bar.spacing))) : 1;
        int prev = nodeAt(bar.a[0], bar.a[1], bar.a[2]);
        if (bar.column) {
            b.columnBaseNodes.push_back(prev);
        }
        for (int i = 1; i <= parts; ++i) {
            const double t = static_cast<double>(i) / parts;
            const int next = nodeAt(bar.a[0] + t * (bar.b[0] - bar.a[0]), bar.a[1] + t * (bar.b[1] - bar.a[1]), bar.a[2] + t * (bar.b[2] - bar.a[2]));
            SolverBridge::MemberData m;
            m.id = ++memberId;
            m.nodeI = prev;
            m.nodeJ = next;
            m.materialId = 1;
            m.sectionId = 1;
            b.model.members.push_back(m);
            b.bars.push_back({prev, next});
            b.barIsColumn.push_back(bar.column ? 1 : 0);
            prev = next;
        }
    }
    for (const int id : b.columnBaseNodes) {
        for (auto &n : b.model.nodes) {
            if (n.id == id) {
                n.restraints = {{1, 1, 1, 1, 1, 1}};
            }
        }
    }
    return b;
}

Sh::Model buildShell(const Case &c)
{
    Sh::Model shell;
    shell.materials[0].E = kE;
    shell.materials[0].nu = c.slabNu;
    shell.materials[0].unitWeight = 0.0;
    Sh::Object slab;
    slab.id = 1;
    slab.name = "Laje";
    slab.kind = Sh::Kind::Plate;
    slab.polygon = {{0, 0, c.z}, {c.lx, 0, c.z}, {c.lx, c.ly, c.z}, {0, c.ly, c.z}};
    slab.p = {{"t", c.t}};
    slab.meshSize = c.mesh;
    slab.material = 0;
    shell.objects = {slab};
    Sh::Load load;
    load.caseName = "Q";
    if (c.loadKind == 0) {
        load.type = Sh::Load::Pressure;
        load.v = {c.pressure, 0, 0, 0};
    } else {
        load.type = Sh::Load::RegionPoints;
        load.v = {0.0, 0.0, c.pressure, 0.0};
        const Sh::Mesh mesh = Sh::generateMesh(shell);
        for (const auto &p : mesh.nodes) {
            load.points.push_back(p);
        }
    }
    shell.loads = {load};
    return shell;
}

void writeResults(const std::filesystem::path &dir, const Case &c, const Built &b, const std::string &mode, const Sh::Model &shell, SolverBridge::ModelData model, const SolverBridge &solver)
{
    std::filesystem::create_directories(dir);
    std::vector<std::string> warnings;
    std::shared_ptr<const ShellCouplingService::Mapping> mapping;
    int tributaryBars = 0;
    Sh::Model s = shell;
    if (mode == "T") {
        s.coupled = false;
        s.tributary = true;
        tributaryBars = ShellCouplingService::attachTributary(model, s, "Q", 1.0, &warnings);
    } else {
        s.coupled = true;
        s.monolithic = (mode == "M");
        mapping = ShellCouplingService::attach(model, s, "Q", 1.0);
        if (mapping) {
            warnings.insert(warnings.end(), mapping->warnings.begin(), mapping->warnings.end());
            std::ofstream map(dir / "mapping.txt");
            map << "merged_nodes=" << mapping->mergedNodes << "\ninterior_shell_nodes=" << mapping->interiorShellNodes << "\ntied_bar_nodes=" << mapping->tiedBarNodes.size()
                << "\nmesh_nodes=" << mapping->mesh.nodes.size() << "\n";
            // ligacoes (no da casca -> nos de barras), por coordenada
            std::map<int, std::array<double, 3>> where = b.position;
            if (model.shell) {
                for (const auto &n : model.shell->nodes) {
                    where[n.id] = {n.x, n.y, n.z};
                }
            }
            std::ofstream ties(dir / "ties.csv");
            ties.precision(17);
            if (model.shell) {
                for (const auto &t : model.shell->ties) {
                    const auto &p = where[t.slave];
                    ties << p[0] << ',' << p[1] << ',' << p[2] << ',' << t.dofMask << ',' << t.masters.size();
                    for (const int m : t.masters) {
                        ties << ',' << where[m][0] << ',' << where[m][1] << ',' << where[m][2];
                    }
                    ties << '\n';
                }
            }
        }
    }
    SolverBridge::Result r;
    std::string error;
    try {
        r = solver.solve(model);
    } catch (const std::exception &e) {
        error = e.what();
    }
    std::ofstream meta(dir / "meta.txt");
    meta << "name=" << c.name << "\nmode=" << mode << "\ndescription=" << c.description << "\nerror=" << error << "\ntributary_bars=" << tributaryBars << "\n";
    for (const auto &w : warnings) {
        meta << "warning=" << w << "\n";
    }
    for (const QString &w : r.warnings) {
        meta << "solver_warning=" << w.toStdString() << "\n";
    }
    if (!error.empty()) {
        std::fprintf(stderr, "[%s/%s] FALHA: %s\n", c.name.c_str(), mode.c_str(), error.c_str());
        return;
    }
    // nos do solver por coordenada
    std::map<int, std::array<double, 3>> pos = b.position;
    if (model.shell) {
        for (const auto &n : model.shell->nodes) {
            pos[n.id] = {n.x, n.y, n.z};
        }
    }
    std::ofstream nodes(dir / "s_nodes.csv");
    nodes.precision(17);
    double sumFz = 0.0, applied = 0.0;
    auto writeNode = [&](const SolverBridge::NodeResult &n) {
        const auto &p = pos[n.nodeId];
        nodes << n.nodeId << ',' << p[0] << ',' << p[1] << ',' << p[2];
        for (int k = 0; k < 6; ++k) {
            nodes << ',' << n.displacements[static_cast<std::size_t>(k)];
        }
        for (int k = 0; k < 6; ++k) {
            nodes << ',' << n.reactions[static_cast<std::size_t>(k)];
        }
        nodes << '\n';
        sumFz += n.reactions[2];
    };
    for (const auto &n : r.nodeResults) {
        writeNode(n);
    }
    for (const auto &n : r.shellNodeResults) {
        writeNode(n);
    }
    // carga total aplicada (Z): barras (carga distribuida) + nodais + casca (pressao x area)
    std::map<int, double> length;
    for (const auto &m : model.members) {
        const auto &a = pos[m.nodeI];
        const auto &bb = pos[m.nodeJ];
        length[m.id] = std::sqrt(std::pow(bb[0] - a[0], 2) + std::pow(bb[1] - a[1], 2) + std::pow(bb[2] - a[2], 2));
    }
    for (const auto &l : model.memberLoads) {
        applied += l.qz * length[l.memberId];
    }
    for (const auto &l : model.nodalLoads) {
        applied += l.values[2];
    }
    {
        // cargas que a laje aplicou as barras (quinhoes): distribuidas por barra e forcas nodais
        std::ofstream ml(dir / "bar_loads.csv");
        ml.precision(17);
        for (const auto &l : model.memberLoads) {
            ml << l.memberId << ',' << l.qx << ',' << l.qy << ',' << l.qz << '\n';
        }
        std::ofstream nl(dir / "nodal_loads.csv");
        nl.precision(17);
        for (const auto &l : model.nodalLoads) {
            const auto &p = pos[l.nodeId];
            nl << l.nodeId << ',' << p[0] << ',' << p[1] << ',' << p[2] << ',' << l.values[0] << ',' << l.values[1] << ',' << l.values[2] << '\n';
        }
    }
    std::ofstream bars(dir / "s_bars.csv");
    bars.precision(17);
    for (const auto &m : r.memberResults) {
        bars << m.memberId;
        for (const double v : m.localForces) {
            bars << ',' << v;
        }
        bars << '\n';
    }
    std::ofstream sum(dir / "summary.txt");
    sum.precision(10);
    sum << "sum_reaction_Fz=" << sumFz << "\nbar_loads_Fz=" << applied << "\n";
    std::printf("[%s/%s] %zu nos de barra, %zu nos de casca, soma Rz = %.6g, cargas nas barras = %.6g\n", c.name.c_str(), mode.c_str(), r.nodeResults.size(), r.shellNodeResults.size(), sumFz, applied);
}

void writeCase(const std::filesystem::path &root, const Case &c, const SolverBridge &solver)
{
    const auto dir = root / c.name;
    std::filesystem::create_directories(dir);
    const Built b = buildBars(c);
    const Sh::Model shell = buildShell(c);
    const Sh::Mesh mesh = Sh::generateMesh(shell);
    {
        std::ofstream meta(dir / "meta.txt");
        meta << "name=" << c.name << "\ndescription=" << c.description << "\nE=" << kE << "\nnu=" << kNu << "\nthickness=" << c.t << "\npressure=" << c.pressure
             << "\nside=" << kSide << "\nz=" << c.z << "\nload_kind=" << c.loadKind << "\n";
        std::ofstream n(dir / "nodes.csv");
        n.precision(17);
        for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
            n << i << ',' << mesh.nodes[i][0] << ',' << mesh.nodes[i][1] << ',' << mesh.nodes[i][2] << '\n';
        }
        std::ofstream q(dir / "quads.csv");
        q.precision(17);
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            q << e << ',' << mesh.quads[e].n[0] << ',' << mesh.quads[e].n[1] << ',' << mesh.quads[e].n[2] << ',' << mesh.quads[e].n[3] << ',' << mesh.quads[e].thickness << '\n';
        }
        std::ofstream bb(dir / "bars.csv");
        bb.precision(17);
        for (std::size_t i = 0; i < b.bars.size(); ++i) {
            const auto &p = b.position.at(b.bars[i][0]);
            const auto &q2 = b.position.at(b.bars[i][1]);
            bb << i + 1 << ',' << p[0] << ',' << p[1] << ',' << p[2] << ',' << q2[0] << ',' << q2[1] << ',' << q2[2] << ',' << int(b.barIsColumn[i]) << '\n';
        }
        std::ofstream pr(dir / "probes.csv");
        for (const auto &p : c.probesXY) {
            pr << p[0] << ',' << p[1] << ',' << c.z << '\n';
        }
        std::ofstream bs(dir / "bases.csv");
        for (const int id : b.columnBaseNodes) {
            const auto &p = b.position.at(id);
            bs << p[0] << ',' << p[1] << ',' << p[2] << '\n';
        }
    }
    {
        // nos criados pela laje nas barras: cortes das barras nao divididas pelos nos da malha (e o que o app faz em "Dividir barras nos nos das cascas")
        Case unsplit = c;
        unsplit.splitBars = false;
        const Built bu = buildBars(unsplit);
        const auto cuts = ShellCouplingService::barCuts(mesh, bu.model);
        std::ofstream cu(dir / "cuts.csv");
        cu.precision(17);
        for (const auto &bar : cuts) {
            for (std::size_t k = 0; k < bar.parameters.size(); ++k) {
                cu << bar.memberId << ',' << bar.parameters[k] << ',' << bar.points[k][0] << ',' << bar.points[k][1] << ',' << bar.points[k][2] << '\n';
            }
        }
        std::ofstream bu_csv(dir / "bars_unsplit.csv");
        bu_csv.precision(17);
        for (std::size_t i = 0; i < bu.bars.size(); ++i) {
            const auto &p = bu.position.at(bu.bars[i][0]);
            const auto &q2 = bu.position.at(bu.bars[i][1]);
            bu_csv << i + 1 << ',' << p[0] << ',' << p[1] << ',' << p[2] << ',' << q2[0] << ',' << q2[1] << ',' << q2[2] << ',' << int(bu.barIsColumn[i]) << '\n';
        }
    }
    for (const std::string mode : {"T", "A", "M"}) {
        writeResults(dir / ("modo_" + mode), c, b, mode, shell, b.model, solver);
    }
}

// calibracao: balanco de barra com a secao quadrada (flecha PL^3/3EI) e laje em balanco sobre barra, para conferir unidades e convencoes
void calibrate(const std::filesystem::path &root, const SolverBridge &solver)
{
    SolverBridge::ModelData model;
    SolverBridge::MaterialData material;
    material.id = 1;
    material.e = kE;
    material.g = kE / (2.0 * (1.0 + kNu));
    model.materials.push_back(material);
    SolverBridge::SectionData section;
    section.id = 1;
    section.area = kSide * kSide;
    section.iy = section.iz = std::pow(kSide, 4) / 12.0;
    section.j = 0.140577 * std::pow(kSide, 4);
    model.sections.push_back(section);
    SolverBridge::NodeData a, bnode;
    a.id = 1;
    a.restraints = {{1, 1, 1, 1, 1, 1}};
    bnode.id = 2;
    bnode.x = 2.0;
    model.nodes = {a, bnode};
    SolverBridge::MemberData m;
    m.id = 1;
    m.nodeI = 1;
    m.nodeJ = 2;
    m.materialId = 1;
    m.sectionId = 1;
    model.members = {m};
    SolverBridge::NodalLoadData load;
    load.nodeId = 2;
    load.values = {{0, 0, -1.0e4, 0, 0, 0}};
    model.nodalLoads.push_back(load);
    const auto r = solver.solve(model);
    std::filesystem::create_directories(root);
    std::ofstream o(root / "calib.txt");
    o.precision(17);
    o << "tip_uz=" << r.nodeResults[1].displacements[2] << "\nexpected_Euler=" << -1.0e4 * 8.0 / (3.0 * kE * std::pow(kSide, 4) / 12.0) << "\n";
    std::printf("calibracao: uz=%.9g (Euler %.9g)\n", r.nodeResults[1].displacements[2], -1.0e4 * 8.0 / (3.0 * kE * std::pow(kSide, 4) / 12.0));
}

} // namespace

#ifndef LV_NO_MAIN
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "uso: lv_dump <pasta_saida> <rds.dll> [calib]\n");
        return 2;
    }
    const std::filesystem::path out = std::filesystem::u8path(args.at(1).toUtf8().toStdString());
    const SolverBridge solver(args.at(2));
    if (args.size() > 3 && args.at(3) == QLatin1String("calib")) {
        calibrate(out, solver);
        return 0;
    }
    for (const Case &c : buildCases()) {
        writeCase(out, c, solver);
    }
    return 0;
}
#endif
