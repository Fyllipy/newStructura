#include "DesignService.h"

#include <array>
#include <atomic>
#include <thread>

#include "app/services/MaterialLibrary.h"
#include "design/Nbr14762.h"
#include "app/services/SectionPlacementService.h"
#include "viz/postprocessor/UniformLoadForceEvaluator.h"
#include "viz/structure/SectionCatalog.h"
#include "viz/structure/SectionShapes.h"

#include <QMap>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Structura::App::Services {

namespace {

using Structura::Visualization::SectionCatalog;
using Structura::Visualization::SectionShapes;

using Vec3 = std::array<double, 3>;

Vec3 cross(const Vec3 &a, const Vec3 &b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

Vec3 normalized(const Vec3 &v, const Vec3 &fallback)
{
    const double n = std::sqrt(dot(v, v));
    return n > 1e-9 ? Vec3 {v[0] / n, v[1] / n, v[2] / n} : fallback;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Acos
// ---------------------------------------------------------------------------------------------------------------------

QVector<DesignService::SteelOption> DesignService::steelGrades()
{
    QVector<SteelOption> list;
    for (const auto &e : MaterialLibrary::steels()) {
        list.append({e.name, e.fy, e.fu, e.coldFormed()});
    }
    return list;
}

std::optional<DesignService::SteelOption> DesignService::findSteel(const QString &name)
{
    // busca direta (evita montar a lista inteira de acos a cada verificacao de barra)
    if (const auto *entry = MaterialLibrary::find(name); entry && entry->isSteel()) {
        return SteelOption {entry->name, entry->fy, entry->fu, entry->coldFormed()};
    }
    return std::nullopt;
}

std::pair<double, double> DesignService::unitFactors(const QVector<MaterialInfo> &materials)
{
    double e = 0.0;
    for (const auto &m : materials) {
        e = std::max(e, m.youngModulus);
    }
    // E do aco: 2e11 Pa (forca em N) ou 2e8 kN/m2 (forca em kN); comprimento em m nos dois casos.
    const double forceFactor = (e >= 1e10 || e <= 0.0) ? 1.0 : 1000.0;
    return {forceFactor, 1000.0};
}

void DesignService::applyB2(QVector<MemberEffects> &effects, const QMap<QString, QMap<int, double>> &b2)
{
    for (auto &member : effects) {
        for (auto &combo : member.combos) {
            const auto it = b2.constFind(QString::fromStdString(combo.name));
            if (it != b2.constEnd()) {
                combo.b2 = it->value(member.memberId, 1.0);
            }
        }
    }
}

QStringList DesignService::defaultUltimateCombinations(const QStringList &allNames)
{
    QStringList list;
    for (const QString &name : allNames) {
        if (!name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive)) {
            list.append(name);
        }
    }
    return list;
}

// ---------------------------------------------------------------------------------------------------------------------
// Esforcos
// ---------------------------------------------------------------------------------------------------------------------

std::vector<DesignService::ElementData> DesignService::buildElements(const Structura::App::SolverBridge::ModelData &geometry,
                                                                     const ResultSet &set)
{
    using namespace Structura::Visualization;
    std::vector<ElementData> elements;
    if (!set.result) {
        return elements;
    }
    std::unordered_map<int, Vec3> nodes;
    for (const auto &n : geometry.nodes) {
        nodes[n.id] = {n.x, n.y, n.z};
    }
    std::unordered_map<int, Structura::App::SolverBridge::MemberData> members;
    for (const auto &m : geometry.members) {
        members[m.id] = m;
    }
    std::unordered_multimap<int, UniformDistributedLoad> loads;
    std::unordered_multimap<int, UniformDistributedMoment> moments;
    for (const auto &term : set.terms) {
        if (!term.first) {
            continue;
        }
        for (const auto &ml : term.first->memberLoads) {
            UniformDistributedLoad udl;
            udl.qx = ml.qx * term.second;
            udl.qy = ml.qy * term.second;
            udl.qz = ml.qz * term.second;
            udl.isLocalSystem = (ml.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            loads.emplace(ml.memberId, udl);
        }
        for (const auto &mm : term.first->memberMoments) {
            UniformDistributedMoment udm;
            udm.mx = mm.mx * term.second;
            udm.my = mm.my * term.second;
            udm.mz = mm.mz * term.second;
            udm.isLocalSystem = (mm.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            moments.emplace(mm.memberId, udm);
        }
    }
    for (const auto &mr : set.result->memberResults) {
        const auto mit = members.find(mr.memberId);
        if (mit == members.end()) {
            continue;
        }
        const auto n1 = nodes.find(mit->second.nodeI);
        const auto n2 = nodes.find(mit->second.nodeJ);
        if (n1 == nodes.end() || n2 == nodes.end()) {
            continue;
        }
        ElementData data;
        BeamElementResults &ber = data.element;
        ber.externalId = mr.memberId;
        ber.node1 = n1->second;
        ber.node2 = n2->second;
        data.nodeI = mit->second.nodeI;
        data.nodeJ = mit->second.nodeJ;
        const Vec3 d {ber.node2[0] - ber.node1[0], ber.node2[1] - ber.node1[1], ber.node2[2] - ber.node1[2]};
        ber.xLocal = normalized(d, {1, 0, 0});
        const Vec3 aux {mr.auxK[0], mr.auxK[1], mr.auxK[2]};
        if (aux[0] != 0.0 || aux[1] != 0.0 || aux[2] != 0.0) {
            ber.zLocal = normalized(cross(ber.xLocal, aux), {0, 0, 1});
            ber.yLocal = cross(ber.zLocal, ber.xLocal);
        } else {
            const Vec3 up = std::abs(ber.xLocal[2]) > 0.999 ? Vec3 {1, 0, 0} : Vec3 {0, 0, 1};
            ber.zLocal = normalized(cross(ber.xLocal, up), {0, 0, 1});
            ber.yLocal = cross(ber.zLocal, ber.xLocal);
        }
        const auto &lf = mr.localForces;
        ber.endForces = {-lf[0], -lf[1], -lf[2], -lf[3], -lf[4], -lf[5], lf[6], lf[7], lf[8], lf[9], lf[10], lf[11]};
        const auto lr = loads.equal_range(mr.memberId);
        for (auto it = lr.first; it != lr.second; ++it) {
            ber.distributedLoads.push_back(it->second);
            const Vec3 q {it->second.qx, it->second.qy, it->second.qz};
            data.qy += it->second.isLocalSystem ? it->second.qy : dot(q, ber.yLocal);
            data.qz += it->second.isLocalSystem ? it->second.qz : dot(q, ber.zLocal);
        }
        const auto mrg = moments.equal_range(mr.memberId);
        for (auto it = mrg.first; it != mrg.second; ++it) {
            ber.distributedMoments.push_back(it->second);
        }
        elements.push_back(std::move(data));
    }
    return elements;
}

QVector<DesignService::MemberEffects> DesignService::collectEffects(const Structura::App::SolverBridge::ModelData &geometry,
                                                                    const std::vector<ResultSet> &sets, double forceFactorToN,
                                                                    double lengthFactorToMm, int stations)
{
    using namespace Structura::Visualization;
    QMap<int, MemberEffects> byMember;
    const double momentFactor = forceFactorToN * lengthFactorToMm;
    for (const ResultSet &set : sets) {
        for (const ElementData &data : buildElements(geometry, set)) {
            UniformLoadForceEvaluator evaluator(data.element);
            const double length = evaluator.length();
            MemberEffects &me = byMember[data.element.externalId];
            me.memberId = data.element.externalId;
            me.lengthMm = length * lengthFactorToMm;

            Structura::Design::ComboEffects combo;
            combo.name = set.name.toStdString();
            combo.transverseLoadX = std::abs(data.qy) > 1e-9;
            combo.transverseLoadY = std::abs(data.qz) > 1e-9;
            for (int i = 0; i < stations; ++i) {
                const double x = length * i / std::max(1, stations - 1);
                const InternalForces f = evaluator.evaluate(x);
                Structura::Design::StationEffects st;
                st.x = x * lengthFactorToMm;
                st.e.N = f.N * forceFactorToN;
                st.e.Mx = f.Mz * momentFactor;
                st.e.My = -f.My * momentFactor;
                st.e.Vy = f.Vy * forceFactorToN;
                st.e.Vz = f.Vz * forceFactorToN;
                st.e.T = f.Mt * momentFactor;
                combo.stations.push_back(st);
            }
            me.combos.push_back(std::move(combo));
        }
    }
    QVector<MemberEffects> result;
    for (auto it = byMember.constBegin(); it != byMember.constEnd(); ++it) {
        result.append(it.value());
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// Secao
// ---------------------------------------------------------------------------------------------------------------------

Structura::Design::DesignSection DesignService::makeSection(const SectionInfo &section, const QString &rectTubeStandard)
{
    using namespace Structura::Design;
    DesignSection none;
    if (section.family.isEmpty()) {
        return none;
    }
    const auto shape = SectionShapes::build(section.family, section.geometry, section.mirrored);
    if (!shape) {
        return none;
    }
    ProfileSpec spec;
    spec.family = section.family.toStdString();
    spec.mirrored = section.mirrored;
    for (auto it = section.geometry.constBegin(); it != section.geometry.constEnd(); ++it) {
        spec.params[it.key().toStdString()] = it.value();
    }
    std::vector<Loop> loops;
    for (const auto &loop : shape->loops) {
        loops.emplace_back(loop.begin(), loop.end());
    }
    const Standard tube = rectTubeStandard.compare(QLatin1String("NBR8800"), Qt::CaseInsensitive) == 0 ? Standard::NBR8800 : Standard::NBR14762;
    const double jMm4 = section.j * 1e12;

    // Eixos principais (giro nos mesmos eixos locais do solver): escolhe o sinal que anula Ixy.
    double angle = 0.0;
    const auto principal = SectionPlacementService::principalProperties(section);
    if (principal.valid && principal.inclined) {
        double best = 1e300;
        for (const double candidate : {principal.thetaDeg, -principal.thetaDeg}) {
            const DesignSection trial = makeDesignSection(spec, loops, jMm4, tube, candidate);
            if (!trial.supported()) {
                continue;
            }
            const double residual = std::abs(trial.g.Ixy) / std::max(1.0, std::max(trial.g.Ix, trial.g.Iy));
            if (residual < best) {
                best = residual;
                angle = candidate;
            }
        }
    }
    DesignSection s = makeDesignSection(spec, loops, jMm4, tube, angle);
    s.name = section.name.toStdString();
    return s;
}

std::array<double, 2> DesignService::serviceInertiaRatios(const Structura::Design::DesignSection &ds, const QString &steelName, double mxNmm, double myNmm)
{
    using namespace Structura::Design;
    if (!ds.supported() || ds.standard != Standard::NBR14762) {
        return {1.0, 1.0};
    }
    const std::optional<SteelOption> option = findSteel(steelName);
    SteelGrade steel;
    steel.fy = option ? option->fy : 250.0;
    steel.fu = option ? option->fu : 400.0;
    steel.coldFormed = true;
    return Nbr14762::serviceInertiaRatios(ds, steel, mxNmm, myNmm);
}

// ---------------------------------------------------------------------------------------------------------------------
// Verificacao
// ---------------------------------------------------------------------------------------------------------------------

DesignService::MemberVerdict DesignService::verifyMember(const MemberEffects &effects, const SectionInfo &section,
                                                         const DesignMemberParams &params, const DesignSettings &settings,
                                                         double lengthFactorToMm)
{
    (void)lengthFactorToMm;
    MemberVerdict verdict;
    verdict.memberId = effects.memberId;
    verdict.sectionName = section.name;
    const QString steelName = params.steel.isEmpty() ? settings.defaultSteel : params.steel;
    verdict.steel = steelName;
    verdict.lengthM = effects.lengthMm / 1000.0;

    const Structura::Design::DesignSection ds = makeSection(section, settings.rectTubeStandard);
    if (!ds.supported()) {
        verdict.output.warnings.push_back("Perfil sem geometria de aco reconhecida: nao dimensionado.");
        return verdict;
    }
    const std::optional<SteelOption> steel = findSteel(steelName);
    Structura::Design::BarDesignInput in;
    in.section = &ds;
    in.steel.name = steelName.toStdString();
    in.steel.fy = steel ? steel->fy : 250.0;
    in.steel.fu = steel ? steel->fu : 400.0;
    in.steel.coldFormed = steel ? steel->coldFormed : false;
    in.length = effects.lengthMm;
    in.Kx = params.kx;
    in.Ky = params.ky;
    in.Kz = params.kz;
    in.lxOverride = params.lx * 1000.0;
    in.lyOverride = params.ly * 1000.0;
    in.lzOverride = params.lz * 1000.0;
    in.lateralSegments = std::max(1, params.segments);
    in.compressionFlangeBraced = params.braced;
    in.lbOverride = params.lb * 1000.0;
    in.cbEqualsOne = params.cbOne;
    in.amplifyB1 = settings.amplifyB1 && !settings.secondOrder;
    in.b2 = params.b2;
    in.slendernessGoverns = settings.slendernessGoverns;
    in.netAreaRatio = params.netAreaRatio;
    in.stiffenerSpacing = params.stiffenerSpacing;
    in.panelTensionFlange = params.panelTensionFlange;
    in.coldWork = params.coldWork;
    in.bearingLength = params.bearingLength;
    in.fatigueCategory = params.fatigueCategory;
    in.fatigueCycles = params.fatigueCycles;
    in.openingDiameter = params.openingDiameter;
    in.openingPosition = params.openingPosition;
    in.openingOffset = params.openingOffset;
    in.openingNeighbourD = params.openingNeighbourD;
    in.openingSpacing = params.openingSpacing;
    in.combos = effects.combos;
    // Combinacoes selecionadas
    if (!settings.combinations.isEmpty()) {
        std::vector<Structura::Design::ComboEffects> filtered;
        for (const auto &c : in.combos) {
            QString name = QString::fromStdString(c.name);
            // variantes com forcas nocionais ("ELU1 +NX") seguem a selecao da combinacao de origem
            static const QRegularExpression variant(QStringLiteral(" [+-]N[XY]$"));
            if (settings.combinations.contains(name) || settings.combinations.contains(name.remove(variant))) {
                filtered.push_back(c);
            }
        }
        in.combos = std::move(filtered);
    }
    verdict.supported = true;
    verdict.output = Structura::Design::designBar(in);
    return verdict;
}

QVector<DesignService::MemberVerdict> DesignService::verifyAll(const QVector<MemberEffects> &effects,
                                                               const std::vector<MemberRef> &members,
                                                               const DesignSettings &settings, double lengthFactorToMm,
                                                               const std::function<void(int, int)> &progress)
{
    QMap<int, const MemberEffects *> index;
    for (const auto &e : effects) {
        index.insert(e.memberId, &e);
    }
    struct Job {
        const MemberEffects *effects;
        const SectionInfo *section;
        int memberId;
    };
    std::vector<Job> jobs;
    for (const MemberRef &ref : members) {
        const auto it = index.constFind(ref.memberId);
        if (it == index.constEnd() || !ref.section) {
            continue;
        }
        jobs.push_back({*it, ref.section, ref.memberId});
    }
    std::vector<MemberVerdict> results(jobs.size());
    std::atomic<int> next {0}, done {0};
    const int total = static_cast<int>(jobs.size());
    auto worker = [&]() {
        for (;;) {
            const int i = next.fetch_add(1);
            if (i >= total) {
                break;
            }
            const Job &job = jobs[static_cast<std::size_t>(i)];
            results[static_cast<std::size_t>(i)] = verifyMember(*job.effects, *job.section, settings.paramsFor(job.memberId), settings, lengthFactorToMm);
            const int finished = done.fetch_add(1) + 1;
            if (progress && (finished % 8 == 0 || finished == total)) {
                progress(finished, total);
            }
        }
    };
    const unsigned workers = total < 16 ? 1u : std::min(8u, std::max(1u, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (unsigned w = 1; w < workers; ++w) {
        pool.emplace_back(worker);
    }
    worker();
    for (auto &t : pool) {
        t.join();
    }
    QVector<MemberVerdict> verdicts;
    verdicts.reserve(total);
    for (auto &r : results) {
        verdicts.append(std::move(r));
    }
    return verdicts;
}

// ---------------------------------------------------------------------------------------------------------------------
// Series de perfis e otimizacao
// ---------------------------------------------------------------------------------------------------------------------

namespace {

SectionShapes::Parameters params(std::initializer_list<std::pair<const char *, double>> list)
{
    SectionShapes::Parameters p;
    for (const auto &kv : list) {
        p.insert(QString::fromLatin1(kv.first), kv.second);
    }
    return p;
}

const double kThicknesses[] = {0.95, 1.20, 1.50, 1.90, 2.25, 2.65, 3.00, 3.35, 3.75, 4.25, 4.75, 6.35, 8.0};

QString num(double v) { return QString::number(v, 'g', 5); }

} // namespace

QVector<DesignService::SectionInfo> DesignService::candidateSeries(const SectionInfo &current)
{
    struct Item {
        QString name;
        SectionShapes::Parameters geometry;
    };
    std::vector<Item> items;
    const QString family = current.family;
    if (family.isEmpty() || !SectionShapes::findFamily(family)) {
        return {};
    }
    const auto &catalog = SectionCatalog::profiles(family);
    if (!catalog.isEmpty()) {
        for (const auto &profile : catalog) {
            items.push_back({profile.name, profile.parameters});
        }
    } else if (family == QLatin1String("C Formado") || family == QLatin1String("U Formado") || family == QLatin1String("Z Formado")
               || family == QLatin1String("Cartola Formado")) {
        // Serie Ue da NBR 6355 (d x bf x c)
        static const double sizes[][3] = {{75, 40, 15}, {100, 40, 17}, {100, 50, 17}, {127, 50, 17}, {150, 60, 20}, {200, 75, 25},
                                          {250, 85, 25}, {300, 85, 25}, {350, 100, 25}, {400, 100, 25}};
        for (const auto &sz : sizes) {
            for (const double t : kThicknesses) {
                if (t > 6.5 || t < 1.2 || (sz[0] <= 100 && t > 3.75)) {
                    continue;
                }
                SectionShapes::Parameters g = params({{"d", sz[0]}, {"bf", sz[1]}, {"t", t}});
                const bool hasLip = (family == QLatin1String("C Formado") || family == QLatin1String("Cartola Formado"));
                if (hasLip) {
                    g.insert(QStringLiteral("c"), sz[2]);
                }
                const QString prefix = family == QLatin1String("C Formado") ? QStringLiteral("Ue")
                                     : family == QLatin1String("Z Formado") ? QStringLiteral("Z")
                                     : family == QLatin1String("U Formado") ? QStringLiteral("U")
                                                                             : QStringLiteral("Cartola");
                items.push_back({QStringLiteral("%1 %2x%3x%4").arg(prefix, num(sz[0]), num(sz[1]), num(t)), g});
            }
        }
    } else if (family == QLatin1String("L Formado")) {
        for (const double leg : {25.0, 30.0, 38.0, 40.0, 50.0, 60.0, 70.0, 75.0, 80.0, 100.0}) {
            for (const double t : kThicknesses) {
                if (t > 6.5 || t < 1.5 || t > leg / 8.0 * 1.2 + 1.5) {
                    continue;
                }
                items.push_back({QStringLiteral("L %1x%2").arg(num(leg), num(t)), params({{"d", leg}, {"bf", leg}, {"t", t}})});
            }
        }
    } else if (family == QLatin1String("Tubular Quadrado")) {
        for (const double b : {20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 120.0, 140.0, 150.0, 160.0, 180.0, 200.0, 250.0, 300.0}) {
            for (const double t : {1.5, 2.0, 2.25, 2.65, 3.0, 3.35, 3.75, 4.25, 4.75, 5.0, 6.0, 6.3, 8.0, 10.0, 12.5}) {
                if (t > b / 4.0) {
                    continue;
                }
                items.push_back({QStringLiteral("Q %1x%1x%2").arg(num(b), num(t)), params({{"b", b}, {"t", t}})});
            }
        }
    } else if (family == QLatin1String("Tubular Retangular")) {
        static const double sizes[][2] = {{40, 20}, {50, 30}, {60, 40}, {80, 40}, {80, 60}, {100, 40}, {100, 60}, {100, 80}, {120, 60}, {120, 80},
                                          {140, 80}, {150, 100}, {160, 80}, {200, 100}, {200, 120}, {250, 150}, {300, 200}};
        for (const auto &sz : sizes) {
            for (const double t : {1.5, 2.0, 2.25, 2.65, 3.0, 3.35, 3.75, 4.25, 4.75, 5.0, 6.0, 6.3, 8.0, 10.0}) {
                if (t > sz[1] / 4.0) {
                    continue;
                }
                items.push_back({QStringLiteral("R %1x%2x%3").arg(num(sz[0]), num(sz[1]), num(t)), params({{"b", sz[0]}, {"h", sz[1]}, {"t", t}})});
            }
        }
    } else if (family == QLatin1String("Tubular Circular")) {
        for (const double D : {21.3, 26.7, 33.4, 42.2, 48.3, 60.3, 73.0, 88.9, 101.6, 114.3, 141.3, 168.3, 219.1, 273.0, 323.9}) {
            for (const double t : {2.0, 2.25, 2.65, 3.0, 3.35, 3.75, 4.25, 4.75, 5.0, 5.6, 6.3, 8.0, 10.0}) {
                if (t > D / 8.0) {
                    continue;
                }
                items.push_back({QStringLiteral("C %1x%2").arg(num(D), num(t)), params({{"D", D}, {"t", t}})});
            }
        }
    } else {
        return {};
    }

    QVector<SectionInfo> result;
    for (const auto &item : items) {
        SectionInfo info;
        info.name = item.name;
        info.family = family;
        info.geometry = item.geometry;
        info.mirrored = current.mirrored;
        if (const auto props = SectionShapes::properties(family, item.geometry)) {
            info.area = props->area;
            info.iz = props->iz;
            info.iy = props->iy;
            info.j = props->j;
        } else {
            continue;
        }
        result.append(info);
    }
    std::stable_sort(result.begin(), result.end(), [](const SectionInfo &a, const SectionInfo &b) { return a.area < b.area; });
    return result;
}

DesignService::BestChoice DesignService::chooseBest(const QVector<MemberEffects> &effects, const QVector<int> &members,
                                                    const SectionInfo &current, const DesignSettings &settings,
                                                    double lengthFactorToMm, double maxRatio, double minInertia)
{
    BestChoice choice;
    QMap<int, const MemberEffects *> index;
    for (const auto &e : effects) {
        index.insert(e.memberId, &e);
    }
    const QVector<SectionInfo> series = candidateSeries(current);
    if (series.isEmpty()) {
        choice.note = QStringLiteral("Nao ha serie de perfis cadastrada para a familia %1.").arg(current.family);
        return choice;
    }
    double bestRatio = 1e300;
    for (const SectionInfo &candidate : series) {
        if (std::max(candidate.iz, candidate.iy) < minInertia) {
            continue;
        }
        double worst = 0.0;
        bool ok = true;
        for (const int id : members) {
            const auto it = index.constFind(id);
            if (it == index.constEnd()) {
                continue;
            }
            const MemberVerdict v = verifyMember(**it, candidate, settings.paramsFor(id), settings, lengthFactorToMm);
            if (!v.supported || v.output.checks.empty()) {
                ok = false;
                break;
            }
            worst = std::max(worst, v.output.maxRatio);
            if (worst > maxRatio) {
                ok = false;
                break;
            }
        }
        if (ok) {
            choice.found = true;
            choice.section = candidate;
            choice.maxRatio = worst;
            return choice;
        }
        bestRatio = std::min(bestRatio, worst);
    }
    choice.note = QStringLiteral("Nenhum perfil da serie %1 atende as verificacoes.").arg(current.family);
    return choice;
}

// ---------------------------------------------------------------------------------------------------------------------
// Relatorio
// ---------------------------------------------------------------------------------------------------------------------

QString DesignService::report(const MemberVerdict &v, const DesignSettings &settings)
{
    QString text;
    text += QStringLiteral("MEMORIA DE CALCULO - BARRA %1\n").arg(v.memberId);
    text += QStringLiteral("Perfil: %1   Aco: %2   Comprimento: %3 m\n").arg(v.sectionName, v.steel).arg(v.lengthM, 0, 'f', 3);
    text += QStringLiteral("Norma: %1\n\n").arg(QString::fromStdString(v.output.standard));
    for (const auto &line : v.output.summaryLines) {
        text += QString::fromStdString(line) + QLatin1Char('\n');
    }
    text += QLatin1Char('\n');
    text += QStringLiteral("Aproveitamento maximo: %1  (%2, combinacao %3, x = %4 m)\n\n")
                .arg(v.output.maxRatio, 0, 'f', 3)
                .arg(QString::fromStdString(v.output.governingCheck), QString::fromStdString(v.output.governingCombo))
                .arg(v.output.governingX / 1000.0, 0, 'f', 3);
    for (const auto &check : v.output.checks) {
        text += QStringLiteral("--- %1 [%2]  eta = %3\n").arg(QString::fromStdString(check.title), QString::fromStdString(check.clause)).arg(check.ratio, 0, 'f', 3);
        for (const auto &line : check.lines) {
            text += QStringLiteral("    ") + QString::fromStdString(line) + QLatin1Char('\n');
        }
        text += QLatin1Char('\n');
    }
    for (const auto &w : v.output.warnings) {
        text += QStringLiteral("AVISO: ") + QString::fromStdString(w) + QLatin1Char('\n');
    }
    (void)settings;
    return text;
}

} // namespace Structura::App::Services
