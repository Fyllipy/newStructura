#include <QtTest/QtTest>

#include "app/SolverBridge.h"
#include "app/services/BucklingService.h"
#include "app/services/CombinationService.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "shell/ShellTributary.h"

#include <cmath>
#include <map>
#include <stdexcept>

using namespace Structura;
using Structura::App::SolverBridge;
using Structura::App::Services::ShellCouplingService;

namespace {

QString solverLibrary()
{
    const QString enc = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.enc");
    if (QFile::exists(enc)) {
        return enc;
    }
    return QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
}

constexpr double kE = 2.0e11;   // Pa (modelo em N e m)
constexpr double kNu = 0.3;

SolverBridge::ModelData baseModel()
{
    SolverBridge::ModelData model;
    SolverBridge::MaterialData material;
    material.id = 1;
    material.e = kE;
    material.g = kE / (2.0 * (1.0 + kNu));
    model.materials.push_back(material);
    SolverBridge::SectionData section;
    section.id = 1;
    section.area = 1.0e-2;
    section.iy = 2.0e-5;
    section.iz = 2.0e-5;
    section.j = 1.0e-5;
    model.sections.push_back(section);
    return model;
}

SolverBridge::NodeData node(int id, double x, double y, double z, bool fixed = false)
{
    SolverBridge::NodeData n;
    n.id = id;
    n.x = x;
    n.y = y;
    n.z = z;
    if (fixed) {
        n.restraints = {{1, 1, 1, 1, 1, 1}};
    }
    return n;
}

SolverBridge::MemberData member(int id, int a, int b)
{
    SolverBridge::MemberData m;
    m.id = id;
    m.nodeI = a;
    m.nodeJ = b;
    m.materialId = 1;
    m.sectionId = 1;
    return m;
}

Shell::Object plate(int id, const std::vector<Shell::Vec3> &polygon, double t, double mesh)
{
    Shell::Object o;
    o.id = id;
    o.name = "Chapa" + std::to_string(id);
    o.kind = Shell::Kind::Plate;
    o.polygon = polygon;
    o.p = {{"t", t}};
    o.meshSize = mesh;
    return o;
}

} // namespace

class TestShellCoupling : public QObject
{
    Q_OBJECT

private slots:
    // "Acoplar as barras": cargas nodais em nos de casca (que ficam fora de model.nodes) precisam entrar no equilibrio enviado
    // ao servidor de licencas; antes eram descartadas no total de reacoes do cliente e somadas na origem pelo servidor
    // (EQUILIBRIUM_FAIL: calculo bloqueado).
    void licenseEquilibriumIncludesLoadsOnShellNodes()
    {
        auto model = baseModel();
        model.nodes.push_back(node(1, 0.0, 0.0, 0.0, true));
        model.nodes.push_back(node(2, 3.0, 0.0, 0.0));
        SolverBridge::MemberData member;
        member.id = 1;
        member.nodeI = 1;
        member.nodeJ = 2;
        member.materialId = 1;
        member.sectionId = 1;
        model.members.push_back(member);
        auto part = std::make_shared<SolverBridge::ShellPart>();
        SolverBridge::ShellNodeData shellNode;
        shellNode.id = 99;
        shellNode.x = 1.5;
        shellNode.y = 2.0;
        shellNode.z = 0.0;
        part->nodes.push_back(shellNode);
        model.shell = part;
        SolverBridge::NodalLoadData onBar;
        onBar.nodeId = 2;
        onBar.values = {0.0, 0.0, -1000.0, 0.0, 0.0, 0.0};
        SolverBridge::NodalLoadData onShell;
        onShell.nodeId = 99;
        onShell.values = {0.0, 0.0, -500.0, 0.0, 0.0, 0.0};
        model.nodalLoads = {onBar, onShell};

        const auto summary = SolverBridge::licenseSummary(model);
        QVERIFY2(summary.modelJson.find("\"id\":99") != std::string::npos, "no de casca ausente do JSON enviado ao servidor");
        // reacoes = -(cargas): Fz = +1500; Mx = -(2*(-500)) = +1000; My = -(-3*(-1000) + -1.5*(-500)) = -3750
        QCOMPARE(summary.reactionForce[2], 1500.0);
        QVERIFY(std::abs(summary.reactionMoment[0] - 1000.0) < 1e-9);
        QVERIFY(std::abs(summary.reactionMoment[1] + 3750.0) < 1e-9);
    }

    void unilateralShellSpringsForbidSuperposition()
    {
        // D2 (Etapa 3): fundacao so-compressao de casca torna a resposta nao linear => o app nao pode superpor casos nem combinacoes
        SolverBridge::ModelData model = baseModel();
        QVERIFY(!model.hasNonlinearElements());
        auto part = std::make_shared<SolverBridge::ShellPart>();
        SolverBridge::ShellSpringData bilateral;
        bilateral.node = 1;
        bilateral.k = {0.0, 0.0, 1.0e6, 0.0, 0.0, 0.0};
        part->springs.push_back(bilateral);
        model.shell = part;
        QVERIFY(!model.hasNonlinearElements());   // mola bilateral continua linear
        auto unilateral = std::make_shared<SolverBridge::ShellPart>(*part);
        SolverBridge::ShellSpringData compression;
        compression.node = 2;
        compression.k = {1.0e6, 1.0e6, 0.0, 0.0, 0.0, 0.0};
        compression.compressionOnly = true;
        unilateral->springs.push_back(compression);
        model.shell = unilateral;
        QVERIFY(model.hasNonlinearElements());
    }

    void barsAloneAreUnchangedByTheShellExtension()
    {
        // balanco de barra: flecha PL^3/(3EI)
        SolverBridge::ModelData model = baseModel();
        model.nodes = {node(1, 0, 0, 0, true), node(2, 4, 0, 0)};
        model.members = {member(1, 1, 2)};
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{0, 0, -1.0e4, 0, 0, 0}};
        model.nodalLoads.push_back(load);
        const SolverBridge::Result r = SolverBridge(solverLibrary()).solve(model);
        const double expected = -1.0e4 * 64.0 / (3.0 * kE * 2.0e-5);
        QVERIFY(std::abs(r.nodeResults[1].displacements[2] - expected) / std::abs(expected) < 1e-9);
        QVERIFY(r.shellNodeResults.empty() && r.shellResults.empty());
    }

    void barAndPlateShareNodesAndStiffness()
    {
        // viga em balanco (barra) ao longo da borda de uma chapa em balanco: rigidez somada (mesmos nos e mesma linha neutra)
        const double L = 4.0, W = 0.5, t = 0.1, P = 1.0e4;
        SolverBridge::ModelData model = baseModel();
        const int n = 8;   // nos da barra a cada 0.5 m (coincidem com a malha da chapa)
        for (int i = 0; i <= n; ++i) {
            model.nodes.push_back(node(i + 1, L * i / n, 0.0, 0.0, i == 0));
        }
        for (int i = 0; i < n; ++i) {
            model.members.push_back(member(i + 1, i + 1, i + 2));
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {L, 0, 0}, {L, W, 0}, {0, W, 0}}, t, 0.5)};
        Shell::Support clamp;
        clamp.axis = 0;
        clamp.value = 0.0;
        clamp.fixed = {{1, 1, 1, 1, 1, 1}};
        // com apoios definidos em nos da casca sobre barras, so esses nos transferem: a linha da barra (y = 0) apoia as translacoes
        Shell::Support onBar;
        onBar.axis = 1;
        onBar.value = 0.0;
        onBar.fixed = {{1, 1, 1, 0, 0, 0}};
        shell.supports = {clamp, onBar};
        shell.coupled = true;
        // carga na ponta da barra (no da barra = no da malha)
        SolverBridge::NodalLoadData load;
        load.nodeId = n + 1;
        load.values = {{0, 0, -P, 0, 0, 0}};
        model.nodalLoads.push_back(load);
        SolverBridge::ModelData coupled = model;
        const auto mapping = ShellCouplingService::attach(coupled, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        QCOMPARE(mapping->mergedNodes, n + 1);
        QVERIFY(coupled.shell && !coupled.shell->empty());
        const SolverBridge solver(solverLibrary());
        const SolverBridge::Result alone = solver.solve(model);
        const SolverBridge::Result both = solver.solve(coupled);
        QVERIFY(!both.shellResults.empty());
        QVERIFY(!both.shellNodeResults.empty());
        const double wBar = alone.nodeResults[static_cast<std::size_t>(n)].displacements[2];
        const double wBoth = both.nodeResults[static_cast<std::size_t>(n)].displacements[2];
        const double iBar = 2.0e-5, iPlate = W * t * t * t / 12.0;   // faixa estreita com bordas livres: EI = E W t^3 / 12
        // a flecha acoplada aproxima PL^3/3(EI_barra + EI_chapa)
        const double expected = -P * L * L * L / (3.0 * kE * (iBar + iPlate));
        QVERIFY2(std::abs(wBoth - expected) / std::abs(expected) < 0.06, qPrintable(QString::number(wBoth) + " vs " + QString::number(expected)));
        QVERIFY(std::abs(wBoth) < std::abs(wBar));   // a chapa enrijece
        // resultados da casca na malha: reacao total = P
        double reaction = 0.0;
        for (const auto &r : both.nodeResults) {
            reaction += r.reactions[2];
        }
        for (const auto &r : both.shellNodeResults) {
            reaction += r.reactions[2];
        }
        QVERIFY2(std::abs(reaction - P) / P < 1e-4, qPrintable(QString::number(reaction)));   // ligacoes por penalidade: erro ~1e-6
        const auto result = ShellCouplingService::caseResult(*mapping, both, "Q");
        QCOMPARE(result.displacement.size(), mapping->mesh.nodes.size());
        QCOMPARE(result.elements.size(), mapping->mesh.quads.size());
    }

    void columnTiedToThePlateInterior()
    {
        // pilar em um ponto interno de um elemento de chapa (no de barra nao coincide com no da malha): ligacao por interpolacao
        const double a = 2.0, t = 0.1, q = 1.0e4;
        SolverBridge::ModelData model = baseModel();
        model.nodes = {node(1, 0.25, 0.25, -1.0, true), node(2, 0.25, 0.25, 0.0)};
        model.members = {member(1, 1, 2)};
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}}, t, 0.5)};
        Shell::Load pressure;
        pressure.caseName = "Q";
        pressure.type = Shell::Load::Pressure;
        pressure.v = {-q, 0, 0, 0};   // contra a normal (+z): para baixo
        shell.loads = {pressure};
        shell.coupled = true;
        shell.monolithic = true;   // pilar em um unico ponto: precisa de continuidade de rotacao (articulado seria mecanismo)
        const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        QCOMPARE(mapping->mergedNodes, 0);
        QCOMPARE(static_cast<int>(mapping->tiedBarNodes.size()), 1);   // o no 2 fica preso ao elemento
        SolverBridge::Result r;
        try {
            r = SolverBridge(solverLibrary()).solve(model);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
        // o pilar recebe toda a carga da chapa: N = q a^2 (compressao)
        const auto &forces = r.memberResults[0].localForces;
        QVERIFY2(std::abs(std::abs(forces[6]) - q * a * a) / (q * a * a) < 1e-3, qPrintable(QString::number(forces[6])));
        // a chapa deflete para baixo nas bordas livres em relacao ao pilar
        double minW = 0.0;
        for (const auto &nr : r.shellNodeResults) {
            minW = std::min(minW, nr.displacements[2]);
        }
        QVERIFY(minW < -1e-6);
    }

    void tributaryModeConservesTheLoadAndCarriesPointLoadsToTheBars()
    {
        // laje 6x4 m apoiada em 2 vigas (y = 0 e y = 4) divididas a cada 1 m: a carga da laje tem que chegar INTEIRA as barras,
        // seja superficial (Area), por pontos (Fase 4: forca em no, regiao) e nunca em silencio
        SolverBridge::ModelData model = baseModel();
        for (int i = 0; i <= 6; ++i) {
            model.nodes.push_back(node(1 + i, i, 0, 0, i == 0 || i == 6));
        }
        for (int i = 0; i <= 6; ++i) {
            model.nodes.push_back(node(8 + i, i, 4, 0, i == 0 || i == 6));
        }
        for (int i = 0; i < 6; ++i) {
            model.members.push_back(member(1 + i, 1 + i, 2 + i));
            model.members.push_back(member(7 + i, 8 + i, 9 + i));
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {6, 0, 0}, {6, 4, 0}, {0, 4, 0}}, 0.15, 1.0)};
        shell.tributary = true;
        const SolverBridge solver(solverLibrary());
        auto totalReaction = [&](const SolverBridge::ModelData &m) {
            double sum = 0.0;
            for (const auto &n : solver.solve(m).nodeResults) {
                sum += n.reactions[2];
            }
            return sum;
        };
        // 1) carga de area: soma das cargas das barras = q x area (conservacao da integral do quinhao)
        Shell::Load area;
        area.caseName = "Q";
        area.type = Shell::Load::Area;
        area.v = {0.0, 0.0, -5000.0, 0.0};
        shell.loads = {area};
        SolverBridge::ModelData surface = model;
        std::vector<std::string> warnings;
        QVERIFY(ShellCouplingService::attachTributary(surface, shell, "Q", 1.0, &warnings) > 0);
        QVERIFY2(std::abs(totalReaction(surface) - 5000.0 * 24.0) < 1e-9 * 5000.0 * 24.0, qPrintable(QString::number(totalReaction(surface), 'g', 12)));
        // laje apoiada so nas duas vigas (bordas livres): a carga e uniforme (q a / 2 = 10 kN/m) em TODAS as barras, inclusive as dos extremos
        // (antes o ultimo trecho de cada viga recebia 1,7% a menos: a marcha de profundidade saia do poligono sobre o vertice)
        QVERIFY(!surface.memberLoads.empty());
        for (const auto &l : surface.memberLoads) {
            QVERIFY2(std::abs(l.qz + 10000.0) < 1e-6 * 10000.0, qPrintable(QString("barra %1: qz = %2").arg(l.memberId).arg(l.qz, 0, 'g', 10)));
        }
        // 2) regiao por clique em todos os nos: mesma carga total (forcas nodais em cada no)
        Shell::Load region;
        region.caseName = "Q";
        region.type = Shell::Load::RegionPoints;
        region.v = {0.0, 0.0, -5000.0, 0.0};
        for (const auto &p : Shell::generateMesh(shell).nodes) {
            region.points.push_back(p);
        }
        shell.loads = {region};
        SolverBridge::ModelData byClick = model;
        warnings.clear();
        QVERIFY(ShellCouplingService::attachTributary(byClick, shell, "Q", 1.0, &warnings) > 0);
        QVERIFY2(!byClick.nodalLoads.empty(), "carga por clique nao chegou as barras no modo de quinhoes");
        QVERIFY2(std::abs(totalReaction(byClick) - 5000.0 * 24.0) < 1e-9 * 5000.0 * 24.0, qPrintable(QString::number(totalReaction(byClick), 'g', 12)));
        QVERIFY2(warnings.empty(), warnings.empty() ? "" : warnings.front().c_str());
        // as duas vigas recebem metade cada (simetria)
        double beam0 = 0.0, beam4 = 0.0;
        for (const auto &l : byClick.nodalLoads) {
            (l.nodeId <= 7 ? beam0 : beam4) += l.values[2];
        }
        QVERIFY(std::abs(beam0 - beam4) < 1e-9 * std::abs(beam0));
        // 3) forca em um no do meio da laje: vai para a viga mais proxima, sem perder nada
        Shell::Load nodal;
        nodal.caseName = "Q";
        nodal.type = Shell::Load::NodalPoints;
        nodal.v = {0.0, 0.0, -1000.0, 0.0};
        nodal.points = {{2.0, 1.0, 0.0}};
        shell.loads = {nodal};
        SolverBridge::ModelData single = model;
        warnings.clear();
        QVERIFY(ShellCouplingService::attachTributary(single, shell, "Q", 1.0, &warnings) > 0);
        QVERIFY(std::abs(totalReaction(single) - 1000.0) < 1e-9 * 1000.0);
        double onBeam0 = 0.0;
        for (const auto &l : single.nodalLoads) {
            onBeam0 += l.nodeId <= 7 ? l.values[2] : 0.0;
        }
        QVERIFY2(std::abs(onBeam0 + 1000.0) < 1e-9, "o ponto (2;1) esta mais perto da viga y=0");
        // 4) o que o modo de quinhoes NAO sabe levar as barras avisa (temperatura / liquido)
        Shell::Load thermal;
        thermal.caseName = "Q";
        thermal.type = Shell::Load::Thermal;
        thermal.v = {10.0, 0.0, 0.0, 0.0};
        shell.loads = {thermal};
        SolverBridge::ModelData ignored = model;
        warnings.clear();
        ShellCouplingService::attachTributary(ignored, shell, "Q", 1.0, &warnings);
        QVERIFY2(!warnings.empty(), "carga termica descartada em silencio");
        // 5) laje lisa sem nenhuma barra nas arestas: avisa que a carga nao chega
        SolverBridge::ModelData columnsOnly = baseModel();
        columnsOnly.nodes = {node(1, 3, 2, -3, true), node(2, 3, 2, 0)};
        columnsOnly.members = {member(1, 1, 2)};
        shell.loads = {area};
        warnings.clear();
        ShellCouplingService::attachTributary(columnsOnly, shell, "Q", 1.0, &warnings);
        QVERIFY2(!warnings.empty(), "laje lisa sem vigas: a perda da carga nao foi avisada");
        // 6) apoio definido na casca no modo de quinhoes: nao entra na analise do projeto, e isso tem que ser dito
        Shell::Support support;
        support.selector = Shell::Support::Plane;
        support.axis = 0;
        support.value = 0.0;
        shell.supports = {support};
        SolverBridge::ModelData withSupport = model;
        warnings.clear();
        ShellCouplingService::attachTributary(withSupport, shell, "Q", 1.0, &warnings);
        bool warned = false;
        for (const std::string &w : warnings) {
            warned = warned || w.find("apoios definidos nas cascas") != std::string::npos;
        }
        QVERIFY2(warned, "apoio da casca ignorado no modo de quinhoes sem aviso");
        // e a carga das barras nao muda por causa dele
        QVERIFY(std::abs(totalReaction(withSupport) - 5000.0 * 24.0) < 1e-9 * 5000.0 * 24.0);
    }

    void monolithicSlabOnBeamsAndColumnsMatchesMecway()
    {
        // Validacao externa (Mecway 28, mesmo modelo com nos compartilhados; docs/validacao_mecway/laje_vigas): laje 8x4 m, t = 15 cm, E = 30 GPa,
        // nu = 0,2, sobre vigas 30x30 de borda e uma central (x = 4 m) divididas a cada 1 m e 6 pilares 30x30 de 3 m engastados; carga 5 kN/m2.
        // Valores de referencia lidos do Mecway: flecha da laje, reacoes e momentos de base. Aceitacao: 5e-4 (diferenca observada ~1e-4).
        SolverBridge::ModelData model = baseModel();
        model.materials[0].e = 30.0e9;
        model.materials[0].g = 30.0e9 / (2.0 * 1.2);
        model.sections[0].area = 0.09;
        model.sections[0].iy = model.sections[0].iz = 0.3 * 0.3 * 0.3 * 0.3 / 12.0;
        model.sections[0].j = 0.140577 * 0.3 * 0.3 * 0.3 * 0.3;
        std::map<std::array<long long, 3>, int> ids;
        auto at = [&](double x, double y, double z) {
            const std::array<long long, 3> k {std::llround(x * 1e6), std::llround(y * 1e6), std::llround(z * 1e6)};
            const auto it = ids.find(k);
            if (it != ids.end()) {
                return it->second;
            }
            const int id = static_cast<int>(model.nodes.size()) + 1;
            model.nodes.push_back(node(id, x, y, z));
            ids[k] = id;
            return id;
        };
        int memberId = 0;
        auto line = [&](double x0, double y0, double z0, double x1, double y1, double z1, int parts) {
            int prev = at(x0, y0, z0);
            for (int i = 1; i <= parts; ++i) {
                const double t = static_cast<double>(i) / parts;
                const int next = at(x0 + t * (x1 - x0), y0 + t * (y1 - y0), z0 + t * (z1 - z0));
                model.members.push_back(member(++memberId, prev, next));
                prev = next;
            }
        };
        line(0, 0, 3, 8, 0, 3, 8);
        line(8, 0, 3, 8, 4, 3, 4);
        line(8, 4, 3, 0, 4, 3, 8);
        line(0, 4, 3, 0, 0, 3, 4);
        line(4, 0, 3, 4, 4, 3, 4);
        const std::vector<std::array<double, 2>> columns {{0, 0}, {8, 0}, {8, 4}, {0, 4}, {4, 0}, {4, 4}};
        for (const auto &c : columns) {
            line(c[0], c[1], 0, c[0], c[1], 3, 1);
            model.nodes[static_cast<std::size_t>(at(c[0], c[1], 0) - 1)].restraints = {{1, 1, 1, 1, 1, 1}};
        }
        Shell::Model shell;
        shell.materials[0].E = 30.0e9;
        shell.materials[0].nu = 0.2;
        shell.materials[0].unitWeight = 0.0;
        shell.objects = {plate(1, {{0, 0, 3}, {8, 0, 3}, {8, 4, 3}, {0, 4, 3}}, 0.15, 1.0)};
        Shell::Load pressure;
        pressure.caseName = "Q";
        pressure.type = Shell::Load::Pressure;
        pressure.v = {-5000.0, 0, 0, 0};
        shell.loads = {pressure};
        shell.coupled = true;
        shell.monolithic = true;
        const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        SolverBridge::Result r;
        try {
            r = SolverBridge(solverLibrary()).solve(model);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
        auto node_ = [&](double x, double y, double z) -> const SolverBridge::NodeResult & {
            // nos de barras (inclusive os da malha fundidos a eles) e nos proprios da laje, por coordenada
            int id = 0;
            for (const auto &n : model.nodes) {
                if (std::abs(n.x - x) < 1e-9 && std::abs(n.y - y) < 1e-9 && std::abs(n.z - z) < 1e-9) {
                    id = n.id;
                }
            }
            for (const auto &n : model.shell->nodes) {
                if (std::abs(n.x - x) < 1e-9 && std::abs(n.y - y) < 1e-9 && std::abs(n.z - z) < 1e-9) {
                    id = n.id;
                }
            }
            for (const auto &list : {&r.nodeResults, &r.shellNodeResults}) {
                for (const auto &n : *list) {
                    if (n.nodeId == id) {
                        return n;
                    }
                }
            }
            throw std::runtime_error("no nao encontrado");
        };
        auto near = [](double a, double ref) { return std::abs(a - ref) <= 5.0e-4 * std::abs(ref); };
        QVERIFY2(near(node_(2, 2, 3).displacements[2], -8.497366107e-4), qPrintable(QString::number(node_(2, 2, 3).displacements[2], 'g', 10)));
        QVERIFY2(near(node_(4, 2, 3).displacements[2], -7.168653493e-4), qPrintable(QString::number(node_(4, 2, 3).displacements[2], 'g', 10)));
        QVERIFY2(near(node_(4, 0, 0).reactions[2], 44804.19605), qPrintable(QString::number(node_(4, 0, 0).reactions[2], 'g', 10)));
        QVERIFY2(near(node_(0, 0, 0).reactions[2], 17597.90197), qPrintable(QString::number(node_(0, 0, 0).reactions[2], 'g', 10)));
        QVERIFY2(near(node_(4, 0, 0).reactions[3], -5785.719707), qPrintable(QString::number(node_(4, 0, 0).reactions[3], 'g', 10)));
        QVERIFY2(near(node_(0, 0, 0).reactions[4], 2442.769748), qPrintable(QString::number(node_(0, 0, 0).reactions[4], 'g', 10)));
        double sum = 0.0;
        for (const auto &n : r.nodeResults) {
            sum += n.reactions[2];
        }
        for (const auto &n : r.shellNodeResults) {
            sum += n.reactions[2];
        }
        QVERIFY(std::abs(sum - 5000.0 * 32.0) < 1e-6 * 5000.0 * 32.0);
    }

    void splittingPointsAreFoundAlongBars()
    {
        // barra ao longo da borda de uma chapa: nos da malha sobre o eixo (fora das extremidades) pedem a divisao da barra
        SolverBridge::ModelData model = baseModel();
        model.nodes = {node(1, 0, 0, 0, true), node(2, 2, 0, 0)};
        model.members = {member(1, 1, 2)};
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}}, 0.1, 0.5)};
        const Shell::Mesh mesh = Shell::generateMesh(shell);
        const auto cuts = ShellCouplingService::barCuts(mesh, model);
        QCOMPARE(static_cast<int>(cuts.size()), 1);
        QCOMPARE(static_cast<int>(cuts[0].parameters.size()), 3);   // x = 0.5, 1.0, 1.5
        QVERIFY(std::abs(cuts[0].parameters[0] - 0.25) < 1e-9);
        shell.coupled = true;
        const auto mapping = ShellCouplingService::mapping(shell, model, 1.0);
        QCOMPARE(mapping->interiorShellNodes, 3);
        QVERIFY(!mapping->warnings.empty());
    }

    void beamWithRigidOffsetBelowThePlateActsCompositely()
    {
        // barra no eixo da faixa, com o eixo 10 cm abaixo do no (que esta no plano medio da chapa): secao composta pelo deslocamento de secao
        const double L = 4.0, W = 0.5, t = 0.1, P = 1.0e4, e = 0.1;
        const int n = 16;
        SolverBridge::ModelData model = baseModel();
        for (int i = 0; i <= n; ++i) {
            model.nodes.push_back(node(i + 1, L * i / n, W / 2.0, 0.0, i == 0));
        }
        for (int i = 0; i < n; ++i) {
            SolverBridge::MemberData m = member(i + 1, i + 1, i + 2);
            m.offsetY1 = m.offsetY2 = -e;   // y local da barra horizontal = Z global
            model.members.push_back(m);
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {L, 0, 0}, {L, W, 0}, {0, W, 0}}, t, 0.25)};
        Shell::Support clamp;
        clamp.axis = 0;
        clamp.value = 0.0;
        clamp.fixed = {{1, 1, 1, 1, 1, 1}};
        shell.supports = {clamp};
        shell.coupled = true;
        shell.monolithic = true;   // acao composta exige continuidade de rotacao
        SolverBridge::NodalLoadData load;
        load.nodeId = n + 1;
        load.values = {{0, 0, -P, 0, 0, 0}};
        model.nodalLoads.push_back(load);
        ShellCouplingService::attach(model, shell, "Q", 1.0);
        const auto r = SolverBridge(solverLibrary()).solve(model);
        const double ap = W * t, ab = 1.0e-2, ip = W * t * t * t / 12.0, ib = 2.0e-5;
        const double zn = -ab * e / (ap + ab);
        const double ei = kE * (ip + ap * zn * zn + ib + ab * (-e - zn) * (-e - zn));
        const double expected = -P * L * L * L / (3.0 * ei);
        const double w = r.nodeResults[static_cast<std::size_t>(n)].displacements[2];
        QVERIFY2(std::abs(w - expected) / std::abs(expected) < 0.03, qPrintable(QString::number(w) + " vs " + QString::number(expected)));
    }

    void slabLoadReachesTheSupportingBeams()
    {
        // laje 4 x 4 m sobre quatro vigas de borda apoiadas nos cantos: a carga da laje chega as vigas (quinhoes), que fletem
        const double a = 4.0, t = 0.12, q = 1.0e4;
        const int n = 8;   // 0.5 m
        SolverBridge::ModelData model = baseModel();
        model.sections[0].area = 0.05;   // vigas rigidas em relacao a laje: a laje trabalha apoiada nas vigas
        model.sections[0].iy = model.sections[0].iz = 3.0e-3;
        int nextId = 1;
        std::map<std::pair<int, int>, int> ids;
        auto at = [&](int i, int j) {
            const auto key = std::make_pair(i, j);
            if (!ids.count(key)) {
                const bool corner = (i == 0 || i == n) && (j == 0 || j == n);
                ids[key] = nextId;
                SolverBridge::NodeData nd = node(nextId++, a * i / n, a * j / n, 0.0);
                if (corner) {
                    nd.restraints = {{1, 1, 1, 0, 0, 0}};
                }
                model.nodes.push_back(nd);
            }
            return ids[key];
        };
        int memberId = 1;
        for (int k = 0; k < n; ++k) {
            model.members.push_back(member(memberId++, at(k, 0), at(k + 1, 0)));
            model.members.push_back(member(memberId++, at(k, n), at(k + 1, n)));
            model.members.push_back(member(memberId++, at(0, k), at(0, k + 1)));
            model.members.push_back(member(memberId++, at(n, k), at(n, k + 1)));
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}}, t, 0.5)};
        shell.objects[0].material = 0;
        Shell::Load pressure;
        pressure.caseName = "Q";
        pressure.type = Shell::Load::Pressure;
        pressure.v = {-q, 0, 0, 0};
        shell.loads = {pressure};
        shell.coupled = true;
        const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
        QVERIFY(mapping != nullptr);
        QCOMPARE(mapping->mergedNodes, 4 * n);   // os nos de borda da malha sao os nos das vigas
        const auto r = SolverBridge(solverLibrary()).solve(model);
        // flecha no meio de uma viga e momento fletor nela: a viga recebe carga da laje
        double midMoment = 0.0;
        for (const auto &m : r.memberResults) {
            midMoment = std::max(midMoment, std::abs(m.localForces[5]) + std::abs(m.localForces[4]));
            midMoment = std::max(midMoment, std::abs(m.localForces[11]) + std::abs(m.localForces[10]));
        }
        const double triangular = q * a * a * a / 24.0;   // viga carregada por quinhao triangular (pico q a / 2)
        QVERIFY2(midMoment > 0.5 * triangular && midMoment < 1.2 * triangular, qPrintable(QString::number(midMoment) + " vs " + QString::number(triangular)));
        // laje apoiada: nenhum momento concentrado entra nas vigas nos nos (equilibrio de momentos de cada no da viga, sem carga externa)
        {
            std::map<int, std::vector<std::pair<const SolverBridge::MemberData *, const SolverBridge::MemberResult *>>> byNode;
            for (std::size_t i = 0; i < model.members.size(); ++i) {
                byNode[model.members[i].nodeI].push_back({&model.members[i], &r.memberResults[i]});
                byNode[model.members[i].nodeJ].push_back({&model.members[i], &r.memberResults[i]});
            }
            double worst = 0.0, scale = 1.0;
            for (const auto &[nodeId, list] : byNode) {
                if (list.size() != 2) {
                    continue;   // so nos intermediarios de uma mesma viga reta
                }
                const auto &a = list[0], &b = list[1];
                // barras colineares ao longo de x ou de y: eixos locais iguais; soma dos momentos de extremidade = 0
                const bool aEnd = a.first->nodeJ == nodeId, bEnd = b.first->nodeJ == nodeId;
                if (aEnd == bEnd) {
                    continue;
                }
                for (const int k : {3, 4, 5}) {
                    const double ma = a.second->localForces[static_cast<std::size_t>(aEnd ? k + 6 : k)];
                    const double mb = b.second->localForces[static_cast<std::size_t>(bEnd ? k + 6 : k)];
                    worst = std::max(worst, std::abs(ma + mb));
                    scale = std::max(scale, std::abs(ma) + std::abs(mb));
                }
            }
            QVERIFY2(worst < 1e-3 * scale, qPrintable(QString::number(worst) + " vs " + QString::number(scale)));
        }
        // equilibrio: reacoes nos cantos = carga total
        double reaction = 0.0;
        for (const auto &nr : r.nodeResults) {
            reaction += nr.reactions[2];
        }
        QVERIFY(std::abs(reaction - q * a * a) / (q * a * a) < 1e-6);
        // forca cortante total absorvida pelas vigas = reacoes (a carga passa pela viga): soma das forcas verticais nas barras que chegam aos cantos
        qWarning("momento maximo nas vigas %g (quinhao triangular %g)", midMoment, triangular);
    }

    // Casca apoiada nas barras: com apoios definidos em nos da casca sobre barras, so esses nos transferem reacao a barra; sem apoios
    // nesses nos, todos os nos em contato transferem (comportamento padrao).
    void shellSupportsOnBarNodesDecideWhereTheReactionGoesToTheBars()
    {
        const double a = 4.0, t = 0.12, q = 1.0e4;
        const int n = 8;
        auto build = [&](bool cornerSupports) {
            SolverBridge::ModelData model = baseModel();
            model.sections[0].area = 0.05;
            model.sections[0].iy = model.sections[0].iz = 3.0e-3;
            int nextId = 1;
            std::map<std::pair<int, int>, int> ids;
            auto at = [&](int i, int j) {
                const auto key = std::make_pair(i, j);
                if (!ids.count(key)) {
                    const bool corner = (i == 0 || i == n) && (j == 0 || j == n);
                    ids[key] = nextId;
                    SolverBridge::NodeData nd = node(nextId++, a * i / n, a * j / n, 0.0);
                    if (corner) {
                        nd.restraints = {{1, 1, 1, 0, 0, 0}};
                    }
                    model.nodes.push_back(nd);
                }
                return ids[key];
            };
            int memberId = 1;
            for (int k = 0; k < n; ++k) {
                model.members.push_back(member(memberId++, at(k, 0), at(k + 1, 0)));
                model.members.push_back(member(memberId++, at(k, n), at(k + 1, n)));
                model.members.push_back(member(memberId++, at(0, k), at(0, k + 1)));
                model.members.push_back(member(memberId++, at(n, k), at(n, k + 1)));
            }
            Shell::Model shell;
            shell.objects = {plate(1, {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}}, t, 0.5)};
            shell.objects[0].material = 0;
            Shell::Load pressure;
            pressure.caseName = "Q";
            pressure.type = Shell::Load::Pressure;
            pressure.v = {-q, 0, 0, 0};
            shell.loads = {pressure};
            shell.coupled = true;
            if (cornerSupports) {
                Shell::Support s;
                s.selector = Shell::Support::Nodes;
                s.fixed = {{1, 1, 1, 0, 0, 0}};
                s.points = {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}};
                shell.supports = {s};
            }
            const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
            if (!mapping) {
                throw std::runtime_error("attach sem mapeamento");
            }
            const auto r = SolverBridge(solverLibrary()).solve(model);
            double midMoment = 0.0, reaction = 0.0;
            for (const auto &m : r.memberResults) {
                midMoment = std::max(midMoment, std::abs(m.localForces[5]) + std::abs(m.localForces[4]));
                midMoment = std::max(midMoment, std::abs(m.localForces[11]) + std::abs(m.localForces[10]));
            }
            for (const auto &nr : r.nodeResults) {
                reaction += nr.reactions[2];
            }
            return std::make_pair(midMoment, reaction);
        };
        const auto [momentAll, reactionAll] = build(false);
        const auto [momentCorners, reactionCorners] = build(true);
        QVERIFY(std::abs(reactionAll - q * a * a) / (q * a * a) < 1e-6);
        QVERIFY2(std::abs(reactionCorners - q * a * a) / (q * a * a) < 1e-3, qPrintable(QString::number(reactionCorners)));
        // padrao: vigas recebem carga em todos os nos; so cantos apoiados: as vigas so recebem as reacoes dos cantos (que caem direto nos apoios)
        QVERIFY2(momentAll > 1.0e4, qPrintable(QString::number(momentAll)));
        QVERIFY2(momentCorners < 0.05 * momentAll, qPrintable(QString::number(momentCorners) + " vs " + QString::number(momentAll)));
    }

    void tributaryLoadsFollowTheEnvelopeAndGiveParabolicDiagrams()
    {
        // laje 6 x 6 m sobre 4 vigas (cada uma dividida em 6 barras de 1 m): quinhoes triangulares, pico p a / 2
        const double a = 6.0, q = 5000.0;
        const int n = 6;
        SolverBridge::ModelData model = baseModel();
        model.sections[0].area = 4.9e-3;
        model.sections[0].iy = model.sections[0].iz = 1.6e-4;
        int nextId = 1;
        std::map<std::pair<int, int>, int> ids;
        auto at = [&](int i, int j) {
            const auto key = std::make_pair(i, j);
            if (!ids.count(key)) {
                const bool corner = (i == 0 || i == n) && (j == 0 || j == n);
                ids[key] = nextId;
                SolverBridge::NodeData nd = node(nextId++, a * i / n, a * j / n, 0.0);
                if (corner) {
                    nd.restraints = {{1, 1, 1, 0, 0, 0}};
                }
                model.nodes.push_back(nd);
            }
            return ids[key];
        };
        int memberId = 1;
        for (int k = 0; k < n; ++k) {
            model.members.push_back(member(memberId++, at(k, 0), at(k + 1, 0)));
            model.members.push_back(member(memberId++, at(k, n), at(k + 1, n)));
            model.members.push_back(member(memberId++, at(0, k), at(0, k + 1)));
            model.members.push_back(member(memberId++, at(n, k), at(n, k + 1)));
        }
        Shell::Model shell;
        shell.objects = {plate(1, {{0, 0, 0}, {a, 0, 0}, {a, a, 0}, {0, a, 0}}, 0.12, 0.5)};
        Shell::Load pressure;
        pressure.caseName = "Q";
        pressure.type = Shell::Load::Pressure;
        pressure.v = {-q, 0, 0, 0};
        shell.loads = {pressure};
        QCOMPARE(ShellCouplingService::attachTributary(model, shell, "Q", 1.0), 4 * n);
        // a carga total transferida = p x area
        double total = 0.0;
        for (const auto &l : model.memberLoads) {
            total += l.qz * 1.0;   // barras de 1 m
        }
        QVERIFY2(std::abs(total + q * a * a) / (q * a * a) < 0.01, qPrintable(QString::number(total)));
        const auto r = SolverBridge(solverLibrary()).solve(model);
        // a viga de borda y = 0 (membros 1, 5, 9, ...): momento no meio = q_pico L^2 / 12 com q_pico = p a / 2
        const double expected = (q * a / 2.0) * a * a / 12.0;
        const double mid = r.memberResults[static_cast<std::size_t>(4 * (n / 2 - 1))].localForces[11];
        QVERIFY2(std::abs(std::abs(mid) - expected) / expected < 0.06, qPrintable(QString::number(mid) + " vs " + QString::number(expected)));
        // momento nulo nos apoios e simetria
        QVERIFY(std::abs(r.memberResults[0].localForces[5]) < 1e-6 * expected);
        // retangulo 4 x 8: nas vigas longas o quinhao e trapezoidal (plateau = p b / 2 com b = 4)
        Shell::Object rect = plate(1, {{0, 0, 0}, {8, 0, 0}, {8, 4, 0}, {0, 4, 0}}, 0.12, 0.5);
        std::vector<Shell::TributaryBar> bars = {{1, {0, 0, 0}, {8, 0, 0}}, {2, {8, 4, 0}, {0, 4, 0}}, {3, {0, 0, 0}, {0, 4, 0}}, {4, {8, 0, 0}, {8, 4, 0}}};
        const auto loads = Shell::tributaryLoads(rect, {0, 0, -q}, bars, 1e-3);
        QCOMPARE(static_cast<int>(loads.size()), 4);
        double sumLong = 0.0, sumShort = 0.0;
        for (const auto &l : loads) {
            (l.barId <= 2 ? sumLong : sumShort) += -l.q[2] * (l.barId <= 2 ? 8.0 : 4.0);
        }
        QVERIFY(std::abs(sumLong + sumShort - q * 32.0) / (q * 32.0) < 0.01);
        QVERIFY(std::abs(sumLong - 2.0 * (q * 2.0 * (8.0 + 4.0) / 2.0)) / sumLong < 0.01);   // 2 trapezios: p b/2 x (L + L - b)/2
    }

    void plateBucklingUsesTheShellGeometricStiffness()
    {
        // placa quadrada simplesmente apoiada sob compressao uniforme: Ncr = 4 pi^2 D / a^2 por unidade de largura
        const double a = 1.0, t = 0.01, P = 1.0e5;
        const int n = 8;
        SolverBridge::ModelData model = baseModel();
        auto part = std::make_shared<SolverBridge::ShellPart>();
        auto id = [n](int i, int j) { return 1 + j * (n + 1) + i; };
        for (int j = 0; j <= n; ++j) {
            for (int i = 0; i <= n; ++i) {
                SolverBridge::ShellNodeData node;
                node.id = id(i, j);
                node.x = a * i / n;
                node.y = a * j / n;
                node.z = 0.0;
                if (i == 0 || i == n || j == 0 || j == n) {
                    node.restraints[2] = 1;
                }
                if (i == 0) {
                    node.restraints[0] = 1;
                }
                if (i == 0 && j == 0) {
                    node.restraints[1] = 1;
                }
                part->nodes.push_back(node);
            }
        }
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                SolverBridge::ShellElementData e;
                e.id = j * n + i + 1;
                e.nodes = {id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)};
                e.e = kE;
                e.nu = kNu;
                e.thickness = t;
                part->elements.push_back(e);
            }
        }
        model.shell = part;
        model.materials.clear();
        model.sections.clear();
        for (int j = 0; j <= n; ++j) {
            SolverBridge::NodalLoadData load;
            load.nodeId = id(n, j);
            load.values[0] = -P / n * ((j == 0 || j == n) ? 0.5 : 1.0);
            model.nodalLoads.push_back(load);
        }
        model.bucklingModes = 1;
        const SolverBridge::Result r = SolverBridge(solverLibrary()).solve(model);
        QVERIFY(!r.bucklingFactors.empty());
        // a forma de flambagem dos nos de casca chega ao chamador (6 valores por no), nao so a das barras (que aqui nao existem)
        QVERIFY(!r.bucklingShellShapes.empty());
        QCOMPARE(r.bucklingShellShapes.front().size(), part->nodes.size() * 6U);
        double largest = 0.0;
        for (const double v : r.bucklingShellShapes.front()) {
            largest = std::max(largest, std::abs(v));
        }
        QVERIFY2(largest > 0.0, "forma de flambagem dos nos de casca toda nula");
        // a janela desenha o modo: malha de cascas deformada, colorida pelo deslocamento, com a maior translacao igual a pedida
        const Structura::Shell::RenderMesh drawn = Structura::App::Services::BucklingService::shellShapeMesh(model, r.bucklingModes.empty() ? std::vector<double> {} : r.bucklingModes.front(),
                                                                                                           r.bucklingShellShapes.front(), 0.25);
        QVERIFY(!drawn.points.empty());
        QVERIFY(drawn.hasValues);
        QCOMPARE(drawn.points.size(), part->elements.size() * 4U);
        double farthest = 0.0;
        for (std::size_t i = 0; i < drawn.points.size(); ++i) {
            farthest = std::max(farthest, static_cast<double>(std::abs(drawn.points[i][2])));   // a placa estava em z = 0: o deslocamento fora do plano e a flecha do modo
        }
        QVERIFY2(std::abs(farthest - 0.25) < 1e-4, qPrintable(QString::number(farthest)));
        // sem cascas no modelo, nada a desenhar
        SolverBridge::ModelData bare;
        QVERIFY(Structura::App::Services::BucklingService::shellShapeMesh(bare, {}, {}, 1.0).points.empty());
        const double D = kE * t * t * t / (12.0 * (1.0 - kNu * kNu));
        const double critical = 4.0 * 3.14159265358979323846 * 3.14159265358979323846 * D / a;   // N (largura a)
        QVERIFY2(std::abs(r.bucklingFactors[0] * P - critical) / critical < 0.12, qPrintable(QString::number(r.bucklingFactors[0] * P) + " vs " + QString::number(critical)));
    }

    void combinationsSuperposeShellResults()
    {
        SolverBridge::Result a, b;
        for (SolverBridge::Result *r : {&a, &b}) {
            r->nodeResults.resize(1);
            r->shellNodeResults.resize(1);
            r->shellResults.resize(1);
        }
        a.shellNodeResults[0].displacements[2] = 1.0;
        b.shellNodeResults[0].displacements[2] = 3.0;
        a.shellResults[0].values[0] = 10.0;
        b.shellResults[0].values[0] = 20.0;
        const auto combined = App::Services::CombinationService::combine({{&a, 1.5}, {&b, 0.5}});
        QVERIFY(std::abs(combined.shellNodeResults[0].displacements[2] - 3.0) < 1e-12);
        QVERIFY(std::abs(combined.shellResults[0].values[0] - 25.0) < 1e-12);
    }
};

QTEST_GUILESS_MAIN(TestShellCoupling)
#include "TestShellCoupling.moc"
