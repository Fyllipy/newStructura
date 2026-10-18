#include "app/ShellSolverBridge.h"

#include <QCoreApplication>
#include <QLibrary>

#include <vector>

namespace Structura::App {

namespace {

using CreateFn = void *(*) ();
using DestroyFn = void (*)(void *);
using AddMaterialFn = int (*)(void *, int, double, double, double, double);
using AddNodeFn = int (*)(void *, int, double, double, double, const int *);
using AddShellFn = int (*)(void *, int, int, int, int, int, int, double);
using AddSpringFn = int (*)(void *, int, const double *, int, double, double, double);
using AddTieFn = int (*)(void *, int, int, const int *, const double *);
using AddCaseFn = int (*)(void *);
using LoadNodeFn = int (*)(void *, int, int, const double *);
using LoadPressureFn = int (*)(void *, int, int, const double *);
using LoadBodyFn = int (*)(void *, int, int, double, double, double);
using LoadThermalFn = int (*)(void *, int, int, double, double);
using SolveFn = int (*)(void *, int, char *);
using IterationsFn = int (*)(void *);
using WarpFn = double (*)(void *);
using GetFn = int (*)(void *, int, double *);
using GetShellFn = int (*)(void *, int, double *, double *);

struct Api {
    CreateFn create {nullptr};
    DestroyFn destroy {nullptr};
    AddMaterialFn addMaterial {nullptr};
    AddNodeFn addNode {nullptr};
    AddShellFn addShell {nullptr};
    AddSpringFn addSpring {nullptr};
    AddTieFn addTie {nullptr};
    AddCaseFn addCase {nullptr};
    LoadNodeFn loadNode {nullptr};
    LoadPressureFn loadPressure {nullptr};
    LoadBodyFn loadBody {nullptr};
    LoadThermalFn loadThermal {nullptr};
    SolveFn solve {nullptr};
    IterationsFn iterations {nullptr};
    WarpFn warp {nullptr};
    GetFn displacements {nullptr};
    GetFn reactions {nullptr};
    GetShellFn shells {nullptr};

    bool complete() const
    {
        return create && destroy && addMaterial && addNode && addShell && addSpring && addTie && addCase && loadNode && loadPressure && loadBody && loadThermal
            && solve && iterations && warp && displacements && reactions && shells;
    }
};

void configure(QLibrary &library, const QString &path)
{
    if (!path.isEmpty()) {
        library.setFileName(path);
    } else {
        library.setFileName(QStringLiteral("mefs"));
    }
}

Api resolve(QLibrary &library)
{
    Api api;
    api.create = reinterpret_cast<CreateFn>(library.resolve("fem_create"));
    api.destroy = reinterpret_cast<DestroyFn>(library.resolve("fem_destroy"));
    api.addMaterial = reinterpret_cast<AddMaterialFn>(library.resolve("fem_add_material"));
    api.addNode = reinterpret_cast<AddNodeFn>(library.resolve("fem_add_node"));
    api.addShell = reinterpret_cast<AddShellFn>(library.resolve("fem_add_shell"));
    api.addSpring = reinterpret_cast<AddSpringFn>(library.resolve("fem_add_spring"));
    api.addTie = reinterpret_cast<AddTieFn>(library.resolve("fem_add_tie"));
    api.addCase = reinterpret_cast<AddCaseFn>(library.resolve("fem_add_case"));
    api.loadNode = reinterpret_cast<LoadNodeFn>(library.resolve("fem_load_node"));
    api.loadPressure = reinterpret_cast<LoadPressureFn>(library.resolve("fem_load_shell_pressure"));
    api.loadBody = reinterpret_cast<LoadBodyFn>(library.resolve("fem_load_shell_body"));
    api.loadThermal = reinterpret_cast<LoadThermalFn>(library.resolve("fem_load_shell_thermal"));
    api.solve = reinterpret_cast<SolveFn>(library.resolve("fem_solve"));
    api.iterations = reinterpret_cast<IterationsFn>(library.resolve("fem_spring_iterations"));
    api.warp = reinterpret_cast<WarpFn>(library.resolve("fem_max_warp"));
    api.displacements = reinterpret_cast<GetFn>(library.resolve("fem_get_displacements"));
    api.reactions = reinterpret_cast<GetFn>(library.resolve("fem_get_reactions"));
    api.shells = reinterpret_cast<GetShellFn>(library.resolve("fem_get_shell_results"));
    return api;
}

} // namespace

ShellSolverBridge::ShellSolverBridge(QString libraryPath)
    : m_libraryPath(std::move(libraryPath))
{
}

bool ShellSolverBridge::available(QString *reason) const
{
    QLibrary library;
    configure(library, m_libraryPath);
    if (!library.load()) {
        if (reason) {
            *reason = QObject::tr("Nao foi possivel carregar a biblioteca do solver de cascas (mefs): %1").arg(library.errorString());
        }
        return false;
    }
    if (!resolve(library).complete()) {
        if (reason) {
            *reason = QObject::tr("A biblioteca mefs nao exporta todas as funcoes necessarias (versao incompativel).");
        }
        return false;
    }
    return true;
}

Structura::Shell::Results ShellSolverBridge::solve(const Structura::Shell::Model &model, const Structura::Shell::Mesh &mesh,
                                                   const Structura::Shell::Job &job) const
{
    using namespace Structura::Shell;
    Results results;
    QLibrary library;
    configure(library, m_libraryPath);
    if (!library.load()) {
        results.error = QObject::tr("Nao foi possivel carregar a biblioteca do solver de cascas (mefs): %1").arg(library.errorString()).toStdString();
        return results;
    }
    const Api api = resolve(library);
    if (!api.complete()) {
        results.error = QObject::tr("A biblioteca mefs nao exporta todas as funcoes necessarias (versao incompativel).").toStdString();
        return results;
    }
    if (mesh.quads.empty()) {
        results.error = QObject::tr("Nao ha elementos: gere a malha de ao menos um objeto.").toStdString();
        return results;
    }
    void *handle = api.create();
    struct Guard {
        const Api &api;
        void *handle;
        ~Guard() { api.destroy(handle); }
    } guard {api, handle};

    for (std::size_t i = 0; i < model.materials.size(); ++i) {
        const Material &m = model.materials[i];
        const double density = m.unitWeight / 9.80665;
        if (api.addMaterial(handle, static_cast<int>(i), m.E, m.nu, density, m.alpha) != 0) {
            results.error = QObject::tr("Material invalido: %1").arg(QString::fromStdString(m.name)).toStdString();
            return results;
        }
    }
    for (std::size_t n = 0; n < job.nodes.size(); ++n) {
        const Job::Node &node = job.nodes[n];
        api.addNode(handle, static_cast<int>(n), node.x[0], node.x[1], node.x[2], node.fixed.data());
    }
    for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
        const MeshQuad &q = mesh.quads[e];
        if (api.addShell(handle, static_cast<int>(e), q.n[0], q.n[1], q.n[2], q.n[3], q.material, q.thickness) != 0) {
            results.error = QObject::tr("Elemento %1 invalido.").arg(e).toStdString();
            return results;
        }
    }
    for (const Job::SpringDef &s : job.springs) {
        api.addSpring(handle, s.node, s.k.data(), s.compressionOnly ? 1 : 0, s.normal[0], s.normal[1], s.normal[2]);
    }
    for (const Tie &t : job.ties) {
        std::vector<int> masters(t.masters.begin(), t.masters.end());
        api.addTie(handle, t.slave, static_cast<int>(masters.size()), masters.data(), t.weights.data());
    }
    for (const Job::Case &c : job.cases) {
        const int index = api.addCase(handle);
        for (const Job::NodalForce &f : c.nodal) {
            api.loadNode(handle, index, f.node, f.f.data());
        }
        for (const Job::Pressure &p : c.pressure) {
            api.loadPressure(handle, index, p.shell, p.p.data());
        }
        for (const Job::Body &b : c.body) {
            api.loadBody(handle, index, b.shell, b.b[0], b.b[1], b.b[2]);
        }
        for (const Job::Thermal &t : c.thermal) {
            api.loadThermal(handle, index, t.shell, t.uniform, t.gradient);
        }
    }
    std::vector<char> error(512, 0);
    if (api.solve(handle, static_cast<int>(error.size()), error.data()) != 0) {
        results.error = std::string(error.data());
        if (results.error.empty()) {
            results.error = QObject::tr("Falha na solucao do modelo de cascas.").toStdString();
        }
        return results;
    }
    results.springIterations = api.iterations(handle);
    results.maxWarp = api.warp(handle);
    const std::size_t nn = job.nodes.size(), ne = mesh.quads.size();
    for (std::size_t c = 0; c < job.cases.size(); ++c) {
        CaseResult cr;
        cr.name = job.cases[c].name;
        std::vector<double> d(6 * nn), r(6 * nn), s(40 * ne), axes(9 * ne);
        api.displacements(handle, static_cast<int>(c), d.data());
        api.reactions(handle, static_cast<int>(c), r.data());
        api.shells(handle, static_cast<int>(c), s.data(), axes.data());
        cr.displacement.resize(nn);
        cr.reaction.resize(nn);
        for (std::size_t n = 0; n < nn; ++n) {
            for (std::size_t k = 0; k < 6; ++k) {
                cr.displacement[n][k] = d[6 * n + k];
                cr.reaction[n][k] = r[6 * n + k];
            }
        }
        cr.elements.resize(ne);
        for (std::size_t e = 0; e < ne; ++e) {
            for (std::size_t k = 0; k < 40; ++k) {
                cr.elements[e].v[k] = s[40 * e + k];
            }
            for (std::size_t k = 0; k < 9; ++k) {
                cr.elements[e].axes[k] = axes[9 * e + k];
            }
        }
        results.cases.push_back(std::move(cr));
    }
    return results;
}

} // namespace Structura::App
