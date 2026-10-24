#include "core/math/Vec3Ops.h"
#include "DesignService.h"
#include "design/CompositeColumn.h"
#include "design/TaperedMember.h"

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
#include <cstdio>
#include <cmath>
#include <unordered_map>

namespace Structura::App::Services {

namespace {

using Structura::Visualization::SectionCatalog;
using Structura::Visualization::SectionShapes;

using Structura::Math::cross;
using Structura::Math::dot;

using Vec3 = std::array<double, 3>;



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

std::pair<double, double> DesignService::shearAreasMm2(const Structura::Design::DesignSection &section)
{
    using Structura::Design::SectionKind;
    const double d = section.d, bf = section.bf, tw = section.tw, tf = section.tf, t = section.t;
    switch (section.kind) {
    case SectionKind::I:
    case SectionKind::Channel:
        return {d * tw, 2.0 * bf * tf};
    case SectionKind::Tee:
        return {d * tw, bf * tf};
    case SectionKind::RectTube: {
        const double h = std::max(0.0, d - 2.0 * t), w = std::max(0.0, bf - 2.0 * t);
        return {2.0 * h * t, 2.0 * w * t};
    }
    case SectionKind::RoundTube:
        return {section.g.area / 2.0, section.g.area / 2.0};
    case SectionKind::DoubleAngleT:
        return {2.0 * section.leg * t, 2.0 * section.leg * t};
    case SectionKind::DoubleChannelBack:
    case SectionKind::DoubleChannelBox:
        return {2.0 * d * tw, 4.0 * 0.5 * (bf - section.gapE) * tf};
    case SectionKind::SolidRound:
        return {0.9 * section.g.area, 0.9 * section.g.area};
    case SectionKind::SolidRect:
        return {5.0 / 6.0 * section.g.area, 5.0 / 6.0 * section.g.area};
    case SectionKind::ColdChannel:
    case SectionKind::ColdLippedChannel:
    case SectionKind::ColdZ:
    case SectionKind::ColdHat:
        return {d * t, 2.0 * bf * t};
    default:
        return {0.0, 0.0};
    }
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

void DesignService::applyB2(QVector<MemberEffects> &effects, const QMap<QString, QMap<int, double>> &b2, double stiffnessFactor)
{
    for (auto &member : effects) {
        for (auto &combo : member.combos) {
            combo.bStiffness = stiffnessFactor;
            const auto it = b2.constFind(QString::fromStdString(combo.name));
            if (it != b2.constEnd()) {
                combo.b2 = it->value(member.memberId, 1.0);
            }
        }
    }
}

void DesignService::tensionConnection(const DesignMemberParams &params, double grossArea, double &netAreaRatio, double &ct, std::vector<std::string> &notes)
{
    auto fmt = [](const char *format, auto... args) {
        char buffer[400];
        std::snprintf(buffer, sizeof(buffer), format, args...);
        return std::string(buffer);
    };
    if (params.holeBoltDiameter > 0.0 && params.holesInChain > 0 && params.holeThickness > 0.0 && grossArea > 0.0) {
        // 5.2.4.1-a: largura do furo = dimensao maxima do furo (6.3.6: db + 1,5 mm) + 2,0 mm; b: + s^2/(4g) para cada linha entre furos em zigue-zague
        const double width = params.holeBoltDiameter + 1.5 + 2.0;
        double an = grossArea - params.holesInChain * width * params.holeThickness;
        std::string chain = fmt("An = Ag - n d t = %.0f - %d x %.1f x %.2f", grossArea, params.holesInChain, width, params.holeThickness);
        if (params.staggerLinks > 0 && params.staggerGauge > 0.0 && params.staggerPitch > 0.0) {
            const double add = params.staggerLinks * params.holeThickness * params.staggerPitch * params.staggerPitch / (4.0 * params.staggerGauge);
            an += add;
            chain += fmt(" + %d t s2/(4g) = + %d x %.2f x %.0f2/(4 x %.0f)", params.staggerLinks, params.staggerLinks, params.holeThickness, params.staggerPitch, params.staggerGauge);
        }
        an = std::clamp(an, 0.0, grossArea);
        netAreaRatio = an / grossArea;
        notes.push_back(fmt("Area liquida (5.2.4): %s = %.0f mm2 (An/Ag = %.3f)", chain.c_str(), an, netAreaRatio));
    }
    switch (params.ctMode) {
    case 1: {
        if (params.ctLength > 0.0) {
            double value = 1.0 - params.ctEccentricity / params.ctLength;
            std::string note = fmt("Ct (5.2.5-c) = 1 - ec/lc = 1 - %.1f/%.0f = %.3f", params.ctEccentricity, params.ctLength, value);
            if (params.ctConnectedAreaRatio > 0.0 && value < params.ctConnectedAreaRatio) {
                value = params.ctConnectedAreaRatio;   // Ct nao precisa ser menor que Ac/Ag
                note += fmt("; adotado Ac/Ag = %.3f (limite inferior de 5.2.5)", value);
            }
            ct = std::clamp(value, 0.05, 1.0);
            notes.push_back(note);
        } else {
            notes.push_back("Ct (5.2.5-c): informe o comprimento da ligacao lc; adotado Ct = 1,0.");
        }
        break;
    }
    case 2:
        if (params.ctConnectedAreaRatio > 0.0) {
            ct = std::clamp(params.ctConnectedAreaRatio, 0.05, 1.0);
            notes.push_back(fmt("Ct (5.2.5-b, soldas transversais) = Ac/Ag = %.3f", ct));
        } else {
            notes.push_back("Ct (5.2.5-b): informe Ac/Ag; adotado Ct = 1,0.");
        }
        break;
    default:
        break;
    }
}

bool DesignService::isExceptionalCombination(const QString &name)
{
    return name.startsWith(QLatin1String("EXC"), Qt::CaseInsensitive);
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
            data.torqueLoad += it->second.isLocalSystem ? it->second.mx : dot(Vec3 {it->second.mx, it->second.my, it->second.mz}, ber.xLocal);
        }
        if (mr.hasWarping) {
            data.warping = true;
            data.psiI = mr.warpingRate[0];
            data.psiJ = mr.warpingRate[1];
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
    // tirantes (so tracao): compressao e folga (barra desativada), nao esforco de projeto: nao passa pela esbeltez de compressao
    QSet<int> ties;
    for (const auto &member : geometry.members) {
        if (member.tensionOnly) {
            ties.insert(member.id);
        }
    }
    // balancos: barras com uma extremidade livre (no sem apoio onde so ela chega)
    QHash<int, int> attached;
    QSet<int> restrainedNodes;
    for (const auto &member : geometry.members) {
        ++attached[member.nodeI];
        ++attached[member.nodeJ];
    }
    for (const auto &node : geometry.nodes) {
        if (std::any_of(node.restraints.begin(), node.restraints.end(), [](int r) { return r != 0; })) {
            restrainedNodes.insert(node.id);
        }
    }
    if (geometry.shell) {
        for (const auto &element : geometry.shell->elements) {
            for (const int id : element.nodes) {
                attached[id] += 2;   // no ligado a casca nao e extremidade livre
            }
        }
    }
    QSet<int> torsionStart, torsionEnd;
    for (const auto &member : geometry.members) {
        if (member.releases[3] != 0) {
            torsionStart.insert(member.id);
        }
        if (member.releases[9] != 0) {
            torsionEnd.insert(member.id);
        }
    }
    QSet<int> cantilevers;
    for (const auto &member : geometry.members) {
        const bool freeI = attached.value(member.nodeI) == 1 && !restrainedNodes.contains(member.nodeI);
        const bool freeJ = attached.value(member.nodeJ) == 1 && !restrainedNodes.contains(member.nodeJ);
        if (freeI != freeJ && !member.tensionOnly) {
            cantilevers.insert(member.id);
        }
    }
    for (const ResultSet &set : sets) {
        // estrutura nt da combinacao (Anexo C.2.1): mesmas cargas e geometria, resultado com os nos dos andares contidos
        std::vector<ElementData> ntElements;
        std::unordered_map<int, const ElementData *> ntById;
        if (set.nt) {
            ResultSet ntSet = set;
            ntSet.result = set.nt;
            ntElements = buildElements(geometry, ntSet);
            for (const ElementData &item : ntElements) {
                ntById[item.element.externalId] = &item;
            }
        }
        for (const ElementData &data : buildElements(geometry, set)) {
            UniformLoadForceEvaluator evaluator(data.element);
            const double length = evaluator.length();
            const auto ntIt = ntById.find(data.element.externalId);
            std::optional<UniformLoadForceEvaluator> ntEvaluator;
            if (ntIt != ntById.end()) {
                ntEvaluator.emplace(ntIt->second->element);
            }
            // Torcao com empenamento (Vlasov): psi = theta' = T(x)/GJ + h(x), h = a exp(-k x) + b exp(-k (L - x)) (solucao da equacao E Cw psi'' - GJ psi = -T(x),
            // com T(x) = T1 - m x e psi nas duas extremidades dados pelo solver); B = -E Cw psi' e Tw = -GJ h
            struct WarpingLaw {
                bool valid {false};
                double k {0.0}, a {0.0}, b {0.0}, m {0.0}, EIw {0.0}, GJ {0.0}, L {0.0};
            } warp;
            if (data.warping) {
                const auto memberIt = std::find_if(geometry.members.begin(), geometry.members.end(), [&](const auto &m) { return m.id == data.element.externalId; });
                if (memberIt != geometry.members.end()) {
                    const auto sectionIt = std::find_if(geometry.sections.begin(), geometry.sections.end(), [&](const auto &s) { return s.id == memberIt->sectionId; });
                    const auto materialIt = std::find_if(geometry.materials.begin(), geometry.materials.end(), [&](const auto &m) { return m.id == memberIt->materialId; });
                    if (sectionIt != geometry.sections.end() && materialIt != geometry.materials.end() && sectionIt->warpingConstant > 0.0 && sectionIt->j > 0.0) {
                        warp.EIw = materialIt->e * sectionIt->warpingConstant;
                        warp.GJ = materialIt->g * sectionIt->j;
                        warp.L = length;
                        warp.k = std::sqrt(warp.GJ / warp.EIw);
                        warp.m = data.torqueLoad;
                        const double t0 = data.element.endForces.Mt1;
                        const double h0 = data.psiI - t0 / warp.GJ;
                        const double hL = data.psiJ - (t0 - warp.m * length) / warp.GJ;
                        const double q = std::exp(-warp.k * length);
                        const double denominator = 1.0 - q * q;
                        warp.a = (h0 - hL * q) / denominator;
                        warp.b = (hL - h0 * q) / denominator;
                        warp.valid = denominator > 1e-12;
                    }
                }
            }
            MemberEffects &me = byMember[data.element.externalId];
            me.memberId = data.element.externalId;
            me.warping = me.warping || warp.valid;
            me.lengthMm = length * lengthFactorToMm;
            me.cantilever = cantilevers.contains(me.memberId);
            me.torsionReleasedStart = torsionStart.contains(me.memberId);
            me.torsionReleasedEnd = torsionEnd.contains(me.memberId);

            Structura::Design::ComboEffects combo;
            combo.name = set.name.toStdString();
            combo.transverseLoadX = std::abs(data.qy) > 1e-9;
            combo.transverseLoadY = std::abs(data.qz) > 1e-9;
            combo.hasNt = ntEvaluator.has_value();
            for (int i = 0; i < stations; ++i) {
                const double x = length * i / std::max(1, stations - 1);
                const InternalForces f = evaluator.evaluate(x);
                Structura::Design::StationEffects st;
                st.x = x * lengthFactorToMm;
                st.e.N = f.N * forceFactorToN;
                if (ties.contains(data.element.externalId) && st.e.N < 0.0) {
                    st.e.N = 0.0;
                }
                st.e.Mx = f.Mz * momentFactor;
                st.e.My = -f.My * momentFactor;
                st.e.Vy = f.Vy * forceFactorToN;
                st.e.Vz = f.Vz * forceFactorToN;
                st.e.T = f.Mt * momentFactor;
                if (warp.valid) {
                    const double eA = std::exp(-warp.k * x), eB = std::exp(-warp.k * (length - x));
                    const double psiPrime = -warp.m / warp.GJ + warp.k * (-warp.a * eA + warp.b * eB);
                    st.e.B = -warp.EIw * psiPrime * forceFactorToN * lengthFactorToMm * lengthFactorToMm;
                    st.e.Tw = -warp.GJ * (warp.a * eA + warp.b * eB) * momentFactor;
                }
                if (ntEvaluator) {
                    const InternalForces g = ntEvaluator->evaluate(x);
                    st.nt.N = g.N * forceFactorToN;
                    st.nt.Mx = g.Mz * momentFactor;
                    st.nt.My = -g.My * momentFactor;
                    st.nt.Vy = g.Vy * forceFactorToN;
                    st.nt.Vz = g.Vz * forceFactorToN;
                    st.nt.T = g.Mt * momentFactor;
                }
                combo.stations.push_back(st);
            }
            me.combos.push_back(std::move(combo));
            EndRecord end;
            end.combo = set.name.toStdString();
            end.x = data.element.xLocal;
            end.y = data.element.yLocal;
            end.z = data.element.zLocal;
            const auto &ef = data.element.endForces;
            const double fN = forceFactorToN;
            end.f = {ef.N1 * fN, ef.Vy1 * fN, ef.Vz1 * fN, ef.Mt1 * momentFactor, ef.My1 * momentFactor, ef.Mz1 * momentFactor,
                     ef.N2 * fN, ef.Vy2 * fN, ef.Vz2 * fN, ef.Mt2 * momentFactor, ef.My2 * momentFactor, ef.Mz2 * momentFactor};
            me.ends.push_back(end);
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
    // "NBR8800" / "NBR14762" forcam a norma; qualquer outro valor (padrao "AUTO") reconhece pela serie do tubo (laminado a quente x formado a frio)
    const Standard tube = rectTubeStandard.compare(QLatin1String("NBR8800"), Qt::CaseInsensitive) == 0 ? Standard::NBR8800
        : (rectTubeStandard.compare(QLatin1String("NBR14762"), Qt::CaseInsensitive) == 0 ? Standard::NBR14762 : automaticRectTubeStandard(spec));
    const double jMm4 = section.j * 1e12;

    // Eixos principais (giro nos mesmos eixos locais do solver): escolhe o sinal que anula Ixy.
    double angle = 0.0;
    const auto principal = SectionPlacementService::principalProperties(section);
    if (principal.valid && principal.inclined) {
        double best = 1e300;
        for (const double candidate : {principal.thetaDeg, -principal.thetaDeg}) {
            const DesignSection trial = makeDesignSection(spec, loops, jMm4, tube, candidate, shape->outerCount);
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
    DesignSection s = makeDesignSection(spec, loops, jMm4, tube, angle, shape->outerCount);
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
    in.cantilever = effects.cantilever;
    in.oneFlangeBraced = params.oneFlangeBraced;
    in.forceAwayFromFreeFlange = params.forceAwayFromFreeFlange;
    in.amplifyB1 = settings.amplifyB1 && !settings.secondOrder;
    in.b2 = params.b2;
    in.slendernessGoverns = settings.slendernessGoverns;
    in.netAreaRatio = params.netAreaRatio;
    in.ct = 1.0;
    tensionConnection(params, ds.g.area, in.netAreaRatio, in.ct, in.tensionNotes);
    in.compositeSpacing = params.compositeSpacing;
    in.compositeWelded = params.compositeWelded;
    in.threadedEnds = params.threadedEnds;
    in.webWeldLeg = params.webWeldLeg;
    in.webWeldFw = params.webWeldFw;
    in.flangeLineLoad = params.flangeLineLoad;
    in.coverB = params.coverB;
    in.coverT = params.coverT;
    in.coverStart = params.coverStart;
    in.coverEnd = params.coverEnd;
    in.coverFlanges = params.coverFlanges;
    in.coverWeldLeg = params.coverWeldLeg;
    in.coverEndWeld = params.coverEndWeld;
    in.plasticAnalysis = params.plasticAnalysis;
    in.warping = effects.warping;
    in.torsionReleasedStart = effects.torsionReleasedStart;
    in.torsionReleasedEnd = effects.torsionReleasedEnd;
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
            static const QRegularExpression variant(QStringLiteral(" [+-][NI][XY]$"));
            if (settings.combinations.contains(name) || settings.combinations.contains(name.remove(variant))) {
                filtered.push_back(c);
            }
        }
        in.combos = std::move(filtered);
    }
    verdict.supported = true;
    // Pilar misto (Anexo M): perfil de aco da propria secao da barra, revestimento/preenchimento de concreto e armadura dos parametros
    Structura::Design::CompositeColumnSection mixed;
    if (params.mixType > 0) {
        using Type = Structura::Design::CompositeColumnSection::Type;
        const Structura::Design::SectionKind kind = ds.kind;
        bool ok = true;
        if (params.mixType == 3 && kind == Structura::Design::SectionKind::RectTube) {
            mixed.type = Type::FilledRectangular;
            mixed.b1 = ds.d;
            mixed.b2 = ds.bf;
            mixed.t = ds.t;
            mixed.innerRadius = ds.t;   // raio interno adotado igual a t (os perfis tubulares da biblioteca nao informam o raio de dobra)
        } else if (params.mixType == 3 && kind == Structura::Design::SectionKind::RoundTube) {
            mixed.type = Type::FilledCircular;
            mixed.b1 = mixed.b2 = ds.D;
            mixed.t = ds.t;
        } else if ((params.mixType == 1 || params.mixType == 2) && kind == Structura::Design::SectionKind::I) {
            mixed.type = params.mixType == 1 ? Type::FullyEncased : Type::PartiallyEncased;
            mixed.d = ds.d;
            mixed.bf = ds.bf;
            mixed.tw = ds.tw;
            mixed.tf = ds.tf;
            mixed.hc = params.mixHc > 0.0 ? params.mixHc : ds.d;
            mixed.bc = params.mixBc > 0.0 ? params.mixBc : ds.bf;
        } else {
            ok = false;
        }
        if (!ok) {
            verdict.output.warnings.push_back("Pilar misto: o tipo escolhido nao corresponde ao perfil da secao (I/H para revestido, tubular para preenchido).");
            return verdict;
        }
        mixed.fck = params.mixFck;
        mixed.fsk = params.mixFsk;
        mixed.alphaE = params.mixAlphaE;
        if (params.mixBarArea > 0.0 && params.mixBarsH >= 2) {
            if (mixed.filled()) {
                // tubo preenchido: armadura em anel dentro do tubo (distancia do eixo da barra a face interna do tubo informada em `mixCover`)
                mixed.bc = mixed.b2 - 2.0 * mixed.t;
                mixed.hc = mixed.b1 - 2.0 * mixed.t;
            }
            if (mixed.circular()) {
                // tubo circular: `mixBarsH` barras igualmente espacadas numa circunferencia a `mixCover` da face interna
                const double ring = mixed.bc / 2.0 - params.mixCover;
                for (int i = 0; i < params.mixBarsH; ++i) {
                    const double angle = 2.0 * Structura::Design::kPi * i / params.mixBarsH;
                    mixed.bars.push_back({params.mixBarArea, ring * std::cos(angle), ring * std::sin(angle)});
                }
            } else {
                mixed.ringOfBars(params.mixBarsH, params.mixBarsV, params.mixBarArea, params.mixCover);
            }
        }
        mixed.finalize();
        in.compositeColumn = &mixed;
        const int model = std::clamp(params.mixModel, 1, 3);
        // as combinacoes filtradas seguem a mesma regra; o gama_c e o gama_s das combinacoes normais (1,4 e 1,15) estao em `designCompositeColumn`
        verdict.output = Structura::Design::designCompositeColumn(in, model);
        return verdict;
    }
    // Barra de secao I de alma variavel (Anexo G): secao a secao, com as propriedades de cada altura
    const bool tapered = section.family == QLatin1String("I Alma Variavel");
    auto runDesign = [&](const Structura::Design::BarDesignInput &input) -> Structura::Design::BarDesignOutput {
        if (!tapered || input.combos.empty() || input.combos.front().stations.empty()) {
            return Structura::Design::designBar(input);
        }
        const auto &g = section.geometry;
        Structura::Design::TaperedGeometry geometry {g.value(QStringLiteral("d1")), g.value(QStringLiteral("d2")), g.value(QStringLiteral("bf")), g.value(QStringLiteral("tw")), g.value(QStringLiteral("tf"))};
        auto sectionAtDepth = [&](double depth) {
            SectionInfo slice = section;
            slice.family = QStringLiteral("W Laminado");
            slice.geometry = {{QStringLiteral("d"), depth}, {QStringLiteral("bf"), geometry.bf}, {QStringLiteral("tw"), geometry.tw}, {QStringLiteral("tf"), geometry.tf}, {QStringLiteral("r"), 0.0}};
            const double web = std::max(depth - 2.0 * geometry.tf, 0.0);
            slice.j = (2.0 * geometry.bf * std::pow(geometry.tf, 3) + web * std::pow(geometry.tw, 3)) / 3.0 * 1e-12;
            return makeSection(slice, settings.rectTubeStandard);
        };
        std::vector<Structura::Design::DesignSection> stations;
        for (const auto &st : input.combos.front().stations) {
            stations.push_back(sectionAtDepth(geometry.depthAt(st.x, input.length)));
        }
        const auto smallest = sectionAtDepth(std::min(geometry.d1, geometry.d2));
        const auto largest = sectionAtDepth(std::max(geometry.d1, geometry.d2));
        return Structura::Design::designTaperedBar(input, geometry, stations, smallest, largest);
    };
    // Tabela 3 da NBR 8800:2024: nas combinacoes excepcionais gama_a1 = 1,00 e gama_a2 = 1,15 (aco laminado e tubulares circulares); cada grupo e verificado com os seus
    // coeficientes e prevalece a pior razao de cada verificacao.
    std::vector<Structura::Design::ComboEffects> normal, exceptional;
    for (const auto &c : in.combos) {
        const bool exc = !ds.supported() ? false : (ds.standard == Structura::Design::Standard::NBR8800 && isExceptionalCombination(QString::fromStdString(c.name)));
        (exc ? exceptional : normal).push_back(c);
    }
    if (exceptional.empty()) {
        verdict.output = runDesign(in);
        return verdict;
    }
    Structura::Design::BarDesignOutput merged;
    bool haveMerged = false;
    if (!normal.empty()) {
        Structura::Design::BarDesignInput a = in;
        a.combos = normal;
        merged = runDesign(a);
        haveMerged = true;
    }
    Structura::Design::BarDesignInput b = in;
    b.combos = exceptional;
    b.opt8800.gammaA1 = 1.00;
    b.opt8800.gammaA2 = 1.15;
    Structura::Design::BarDesignOutput second = runDesign(b);
    if (!haveMerged) {
        merged = std::move(second);
    } else {
        const double secondMax = second.maxRatio;
        const std::string secondGoverning = second.governingCheck, secondCombo = second.governingCombo;
        const double secondX = second.governingX;
        for (auto &check : second.checks) {
            auto it = std::find_if(merged.checks.begin(), merged.checks.end(), [&](const Structura::Design::CheckResult &c) { return c.id == check.id; });
            if (it == merged.checks.end()) {
                merged.checks.push_back(std::move(check));
            } else if (check.ratio > it->ratio) {
                *it = std::move(check);
            }
        }
        for (auto &w : second.warnings) {
            if (std::find(merged.warnings.begin(), merged.warnings.end(), w) == merged.warnings.end()) {
                merged.warnings.push_back(std::move(w));
            }
        }
        // a verificacao governante, a combinacao e a posicao vem do grupo de maior razao
        if (secondMax > merged.maxRatio) {
            merged.maxRatio = secondMax;
            merged.governingCheck = secondGoverning;
            merged.governingCombo = secondCombo;
            merged.governingX = secondX;
        }
    }
    merged.summaryLines.push_back("Combinacoes excepcionais (nome iniciado por EXC): gama_a1 = 1,00 e gama_a2 = 1,15 (NBR 8800:2024, Tabela 3).");
    verdict.output = std::move(merged);
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
