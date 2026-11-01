#include "AnalysisService.h"

#include "app/LoadSystemUtils.h"
#include "app/services/IdGeneratorService.h"
#include "app/services/DesignService.h"
#include "app/services/SectionPlacementService.h"
#include "app/services/ShellCouplingService.h"
#include "viz/LocalCoordinateSystem.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

namespace Structura::App::Services {

AnalysisService::BuildResult AnalysisService::buildModel(Structura::App::ISceneModelGateway *sceneController,
                                                         QVector<MaterialInfo> &materials,
                                                         QVector<SectionInfo> &sections,
                                                         const QVector<NodeSupport> &supports,
                                                         const QVector<NodalLoad> &nodalLoads,
                                                         const QVector<MemberLoad> &memberLoads,
                                                         const Structura::App::ProjectData::ProjectExtras &extras,
                                                         const QString &loadCase) const
{
    BuildResult result;
    if (!sceneController) {
        result.errors.append(QStringLiteral("Scene controller indisponivel."));
        return result;
    }

    auto bars = sceneController->bars();
    auto nodes = sceneController->nodeInfos();

    IdGeneratorService::ensureMaterialExternalIds(materials);
    IdGeneratorService::ensureSectionExternalIds(sections);

    IdGeneratorService::ensureBarExternalIds(sceneController);
    bars = sceneController->bars();

    std::sort(nodes.begin(), nodes.end(), [](const Structura::App::SceneNodeInfo &a, const Structura::App::SceneNodeInfo &b) {
        return a.externalId < b.externalId;
    });
    std::sort(bars.begin(), bars.end(), [](const Structura::App::SceneBarInfo &a, const Structura::App::SceneBarInfo &b) {
        return a.externalId < b.externalId;
    });

    QHash<QUuid, Structura::App::SceneNodeInfo> nodeInfoMap;
    for (const auto &node : nodes) {
        nodeInfoMap.insert(node.id, node);
    }

    QHash<int, NodeSupport> supportMap;
    for (const auto &support : supports) {
        supportMap.insert(support.nodeId, support);
    }

    QHash<QUuid, MaterialInfo> materialsById;
    for (const auto &mat : materials) {
        materialsById.insert(mat.uuid, mat);
    }

    QHash<QUuid, SectionInfo> sectionsById;
    for (const auto &sec : sections) {
        sectionsById.insert(sec.uuid, sec);
    }

    auto &model = result.model;
    model.materials.reserve(static_cast<std::size_t>(materials.size()));
    model.sections.reserve(static_cast<std::size_t>(sections.size()));
    model.nodes.reserve(nodes.size());
    model.members.reserve(bars.size());
    model.nodalLoads.reserve(static_cast<std::size_t>(nodalLoads.size()));
    model.memberLoads.reserve(static_cast<std::size_t>(memberLoads.size()));

    for (const auto &mat : materials) {
        Structura::App::SolverBridge::MaterialData data;
        data.id = mat.externalId;
        data.e = mat.youngModulus;
        data.g = mat.shearModulus;
        data.density = mat.unitWeight / 9.80665;   // massa especifica coerente com o sistema de unidades do projeto
        model.materials.push_back(data);
    }

    // Secoes assimetricas (ex.: cantoneiras): o solver recebe as inercias principais e a barra e girada
    // ate os eixos principais (giro do usuario + giro da secao).
    QHash<int, double> principalAngleBySection;
    for (const auto &sec : sections) {
        Structura::App::SolverBridge::SectionData data;
        data.id = sec.externalId;
        data.area = sec.area;
        data.iz = sec.iz;
        data.iy = sec.iy;
        data.j = sec.j;
        const auto principal = Structura::App::Services::SectionPlacementService::principalProperties(sec);
        if (principal.valid && principal.inclined) {
            data.iz = principal.izPrincipal;
            data.iy = principal.iyPrincipal;
            principalAngleBySection.insert(sec.externalId, principal.thetaDeg);
        }
        if (extras.design.shearDeformation) {
            // areas de cisalhamento (alma na direcao da altura, mesas na outra); secoes inclinadas (cantoneiras) ficam rigidas ao cortante
            const double lengthFactor = DesignService::unitFactors(materials).second;   // mm por unidade de comprimento do modelo
            if (!(principal.valid && principal.inclined) && lengthFactor > 0.0) {
                const auto areas = DesignService::shearAreasMm2(DesignService::makeSection(sec, extras.design.rectTubeStandard));
                const bool strongIsZ = data.iz >= data.iy;
                const double unit = 1.0 / (lengthFactor * lengthFactor);
                data.shearAreaY = (strongIsZ ? areas.first : areas.second) * unit;
                data.shearAreaZ = (strongIsZ ? areas.second : areas.first) * unit;
            }
        }
        if (extras.design.warpingTorsion) {
            // empenamento (Vlasov): secoes abertas de paredes finas com linha media conhecida, com o Cw da propria linha media (coerente com a
            // coordenada setorial usada nas tensoes); cantoneiras e T (pernas concorrentes) tem Cw desprezivel e ficam em Saint-Venant.
            // O centro de cisalhamento fora do centroide (U, Z, cartola, I monossimetrico) acopla flexao e torcao na rigidez da barra (eixo a -> z, b -> y);
            // a 2a ordem e as analises de flambagem e modal mantem o eixo do centroide
            const double lengthFactor = DesignService::unitFactors(materials).second;   // mm por unidade de comprimento do modelo
            if (lengthFactor > 0.0) {
                using Structura::Design::SectionKind;
                const auto design = DesignService::makeSection(sec, extras.design.rectTubeStandard);
                const bool concurrentLegs = design.kind == SectionKind::Angle || design.kind == SectionKind::ColdAngle || design.kind == SectionKind::Tee;
                if (design.sectorial.valid && !concurrentLegs && design.sectorial.Cw > 0.0) {
                    data.warpingConstant = design.sectorial.Cw / std::pow(lengthFactor, 6.0);
                    data.shearCenterY = design.y0 / lengthFactor;
                    data.shearCenterZ = design.x0 / lengthFactor;
                }
            }
        }
        model.sections.push_back(data);
    }
    model.shearDeformation = extras.design.shearDeformation;
    model.stabilityFunctions = extras.design.stabilityFunctions;
    model.loadSteps = extras.design.loadSteps;

    for (const auto &node : nodes) {
        Structura::App::SolverBridge::NodeData data;
        data.id = node.externalId;
        data.x = node.x;
        data.y = node.y;
        data.z = node.z;
        const auto supportIt = supportMap.constFind(node.externalId);
        if (supportIt != supportMap.constEnd()) {
            for (int i = 0; i < 6; ++i) {
                data.restraints[static_cast<std::size_t>(i)] = supportIt.value().restraints[i] ? 1 : 0;
            }
        }
        model.nodes.push_back(data);
    }

    for (const auto &bar : bars) {
        const auto startIt = nodeInfoMap.constFind(bar.startNodeId);
        const auto endIt = nodeInfoMap.constFind(bar.endNodeId);
        const auto materialIt = materialsById.constFind(bar.materialId);
        const auto sectionIt = sectionsById.constFind(bar.sectionId);

        if (startIt == nodeInfoMap.constEnd() || endIt == nodeInfoMap.constEnd()) {
            result.errors.append(QStringLiteral("Barra %1 referencia no(s) inexistente(s).").arg(bar.externalId));
            continue;
        }
        if (materialIt == materialsById.constEnd() || materialIt->externalId <= 0) {
            result.errors.append(QStringLiteral("Barra %1 sem material valido.").arg(bar.externalId));
            continue;
        }
        if (sectionIt == sectionsById.constEnd() || sectionIt->externalId <= 0) {
            result.errors.append(QStringLiteral("Barra %1 sem secao valida.").arg(bar.externalId));
            continue;
        }

        Structura::App::SolverBridge::MemberData data;
        data.id = bar.externalId;
        data.nodeI = startIt.value().externalId;
        data.nodeJ = endIt.value().externalId;
        data.materialId = materialIt->externalId;
        data.sectionId = sectionIt->externalId;
        const double principalAngle = principalAngleBySection.value(sectionIt->externalId, 0.0);
        data.sectionRotationDeg = principalAngle;
        for (const auto &placement : extras.placements) {
            if (placement.memberId != bar.externalId) {
                continue;
            }
            using Service = Structura::App::Services::SectionPlacementService;
            const SectionInfo &section = sectionIt.value();
            // Posicao do centroide em relacao a cada no, nos eixos da secao girada pelo usuario.
            auto endOffset = [&](double extraA, double extraB) {
                Structura::App::ProjectData::MemberPlacement end = placement;
                end.offsetA = extraA;
                end.offsetB = extraB;
                return Service::toPrincipalFrame(Service::axisOffset(&section, end), principalAngle);
            };
            const auto startOffset = endOffset(placement.offsetA, placement.offsetB);
            const auto finishOffset = endOffset(placement.endOffsetA(), placement.endOffsetB());
            data.sectionRotationDeg = placement.rotationDeg + principalAngle;
            data.offsetY1 = startOffset.dy;
            data.offsetZ1 = startOffset.dz;
            data.offsetY2 = finishOffset.dy;
            data.offsetZ2 = finishOffset.dz;
            for (int bit = 0; bit < 12; ++bit) {
                data.releases[static_cast<std::size_t>(bit)] = ((placement.releaseMask >> bit) & 1) ? 1 : 0;
            }
            if (placement.memberType != Structura::App::ProjectData::MemberType::kNormal) {
                // trelica e tirante: biarticulados (so esforco axial)
                for (int bit = 0; bit < 12; ++bit) {
                    if ((Structura::App::ProjectData::MemberType::kReleaseRotations >> bit) & 1) {
                        data.releases[static_cast<std::size_t>(bit)] = 1;
                    }
                }
                data.tensionOnly = (placement.memberType == Structura::App::ProjectData::MemberType::kTie);
            }
            break;
        }
        for (const auto &section : model.sections) {
            if (section.id == data.sectionId && section.warpingConstant > 0.0) {
                data.warping = true;
            }
        }
        if (sectionIt->family == QLatin1String("I Alma Variavel")) {
            // Anexo G: alturas d1 (no inicial) e d2 (no final) em unidades do modelo; mesas e alma constantes
            const double lengthFactor = DesignService::unitFactors(materials).second;   // mm por unidade de comprimento do modelo
            if (lengthFactor > 0.0) {
                const auto &g = sectionIt->geometry;
                data.taper = true;
                data.taperD1 = g.value(QStringLiteral("d1")) / lengthFactor;
                data.taperD2 = g.value(QStringLiteral("d2")) / lengthFactor;
                data.taperBf = g.value(QStringLiteral("bf")) / lengthFactor;
                data.taperTw = g.value(QStringLiteral("tw")) / lengthFactor;
                data.taperTf = g.value(QStringLiteral("tf")) / lengthFactor;
                data.warping = false;
            }
        }
        model.members.push_back(data);
    }

    if (extras.design.rigidZoneFactor > 0.0) {
        // Zonas rigidas: em cada extremidade, a barra atravessa o perfil das outras barras do no ate a sua face; o trecho dentro dele e rigido
        // (fator x distancia do eixo ate a face). A distancia e a saida do raio, a partir do eixo da outra barra, do prisma retangular d x bf
        // do seu perfil (eixos locais com o giro da secao). Barras que continuam a propria barra (colineares), trelicas/tirantes e secoes
        // sem dimensoes (ou inclinadas) nao geram zona rigida.
        const double lengthFactor = DesignService::unitFactors(materials).second;   // mm por unidade de comprimento do modelo
        struct Prism {
            std::array<double, 3> x {}, y {}, z {};
            double hy {0.0}, hz {0.0};   // dimensoes (mm) segundo y' e z'
            bool valid {false};
        };
        QHash<int, SectionInfo> sectionByExternal;
        for (const auto &sec : sections) {
            sectionByExternal.insert(sec.externalId, sec);
        }
        QHash<int, std::array<double, 2>> inertiaBySection;   // iz, iy do solver
        for (const auto &data : model.sections) {
            inertiaBySection.insert(data.id, {data.iz, data.iy});
        }
        QHash<int, std::array<double, 3>> nodePosition;
        for (const auto &node : model.nodes) {
            nodePosition.insert(node.id, {node.x, node.y, node.z});
        }
        std::vector<Prism> prism(model.members.size());
        QHash<int, QVector<std::pair<int, bool>>> barsAtNode;   // no -> (indice da barra, ponta final?)
        Structura::Geometry::DefaultLocalAxisProvider provider;
        for (std::size_t i = 0; i < model.members.size(); ++i) {
            const auto &member = model.members[i];
            const auto a = nodePosition.constFind(member.nodeI), b = nodePosition.constFind(member.nodeJ);
            if (a == nodePosition.constEnd() || b == nodePosition.constEnd()) {
                continue;
            }
            barsAtNode[member.nodeI].append({static_cast<int>(i), false});
            barsAtNode[member.nodeJ].append({static_cast<int>(i), true});
            const auto lcs = Structura::Geometry::rotatedAboutX(provider.computeLCS(*a, *b, std::nullopt), member.sectionRotationDeg);
            prism[i].x = lcs.xPrime;
            prism[i].y = lcs.yPrime;
            prism[i].z = lcs.zPrime;
            const auto sectionIt2 = sectionByExternal.constFind(member.sectionId);
            const auto inertia = inertiaBySection.constFind(member.sectionId);
            if (sectionIt2 == sectionByExternal.constEnd() || inertia == inertiaBySection.constEnd()) {
                continue;
            }
            const auto &g = sectionIt2->geometry;
            const double depth = g.value(QStringLiteral("d"), g.value(QStringLiteral("D"), 0.0));
            const double width = g.value(QStringLiteral("bf"), g.value(QStringLiteral("b"), g.value(QStringLiteral("D"), 0.0)));
            if (depth <= 0.0 || width <= 0.0) {
                continue;
            }
            const bool strongIsZ = (*inertia)[0] >= (*inertia)[1];   // flexao forte em torno de z': a altura fica segundo y'
            prism[i].hy = strongIsZ ? depth : width;
            prism[i].hz = strongIsZ ? width : depth;
            prism[i].valid = true;
        }
        auto unitFrom = [&](const Structura::App::SolverBridge::MemberData &member, bool atEnd) {
            const auto from = nodePosition.value(atEnd ? member.nodeJ : member.nodeI);
            const auto to = nodePosition.value(atEnd ? member.nodeI : member.nodeJ);
            std::array<double, 3> u {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
            const double length = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
            for (double &c : u) {
                c = length > 0.0 ? c / length : 0.0;
            }
            return std::make_pair(u, length);
        };
        auto dot = [](const std::array<double, 3> &p, const std::array<double, 3> &q) { return p[0] * q[0] + p[1] * q[1] + p[2] * q[2]; };
        for (std::size_t i = 0; i < model.members.size(); ++i) {
            auto &member = model.members[i];
            bool normalType = true;
            for (const auto &placement : extras.placements) {
                if (placement.memberId == member.id && placement.memberType != Structura::App::ProjectData::MemberType::kNormal) {
                    normalType = false;
                }
            }
            if (!normalType || lengthFactor <= 0.0) {
                continue;
            }
            for (const bool atEnd : {false, true}) {
                const auto [u, length] = unitFrom(member, atEnd);
                double reach = 0.0;   // mm
                for (const auto &other : barsAtNode.value(atEnd ? member.nodeJ : member.nodeI)) {
                    if (other.first == static_cast<int>(i) || !prism[static_cast<std::size_t>(other.first)].valid) {
                        continue;
                    }
                    const auto &pr = prism[static_cast<std::size_t>(other.first)];
                    if (std::abs(dot(u, pr.x)) > 0.985) {
                        continue;   // continuacao (ou diagonal quase alinhada): nao e perfil que a recebe
                    }
                    const double cy = std::abs(dot(u, pr.y)), cz = std::abs(dot(u, pr.z));
                    double exit = 1e300;
                    if (cy > 1e-9) {
                        exit = std::min(exit, 0.5 * pr.hy / cy);
                    }
                    if (cz > 1e-9) {
                        exit = std::min(exit, 0.5 * pr.hz / cz);
                    }
                    if (exit < 1e299) {
                        reach = std::max(reach, exit);
                    }
                }
                const double rigid = std::min(extras.design.rigidZoneFactor * reach / lengthFactor, 0.4 * length);
                (atEnd ? member.rigidEnd : member.rigidStart) = rigid;
            }
        }
    }

    // Com `loadCase` vazio, todas as cargas entram (compatibilidade); senao so as do caso.
    const QString defaultCase = extras.loadCases.isEmpty() ? Structura::App::ProjectData::defaultLoadCaseName()
                                                           : extras.loadCases.first().name;
    auto belongsToCase = [&](const QString &name) {
        if (loadCase.isEmpty()) {
            return true;
        }
        return (name.isEmpty() ? defaultCase : name) == loadCase;
    };

    for (const auto &load : nodalLoads) {
        if (!belongsToCase(load.loadCase)) {
            continue;
        }
        Structura::App::SolverBridge::NodalLoadData data;
        data.nodeId = load.nodeId;
        data.values = {{load.fx, load.fy, load.fz, load.mx, load.my, load.mz}};
        model.nodalLoads.push_back(data);
    }

    for (const auto &load : memberLoads) {
        if (!belongsToCase(load.loadCase)) {
            continue;
        }
        Structura::App::SolverBridge::MemberLoadData data;
        data.memberId = load.memberId;
        const QString system = Structura::App::normalizeMemberLoadSystem(load.system);
        data.coordSystem = (system == QLatin1String("GLOBAL"))
            ? Structura::App::SolverBridge::MemberLoadCoordSystem::Global
            : Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
        data.qx = load.qx;
        data.qy = load.qy;
        data.qz = load.qz;
        model.memberLoads.push_back(data);
    }

    for (const auto &moment : extras.memberMoments) {
        if (!belongsToCase(moment.loadCase)) {
            continue;
        }
        Structura::App::SolverBridge::MemberMomentData data;
        data.memberId = moment.memberId;
        const QString system = Structura::App::normalizeMemberLoadSystem(moment.system);
        data.coordSystem = (system == QLatin1String("GLOBAL"))
            ? Structura::App::SolverBridge::MemberLoadCoordSystem::Global
            : Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
        data.mx = moment.mx;
        data.my = moment.my;
        data.mz = moment.mz;
        model.memberMoments.push_back(data);
    }

    // Cargas gerais de barra com posicao. Temperatura e retracao viram deformacao imposta (eps = alfa dT; curvatura livre pelo gradiente).
    {
        QHash<int, double> alphaByMember;
        for (const auto &bar : bars) {
            const auto materialIt = materialsById.constFind(bar.materialId);
            if (materialIt != materialsById.constEnd()) {
                alphaByMember.insert(bar.externalId, materialIt->thermalExpansion);
            }
        }
        namespace Kind = Structura::App::ProjectData::MemberActionKind;
        for (const auto &action : extras.memberActions) {
            if (!belongsToCase(action.loadCase) || action.isZero()) {
                continue;
            }
            Structura::App::SolverBridge::MemberActionData data;
            data.memberId = action.memberId;
            data.a = action.a;
            data.b = action.b;
            const bool global = Structura::App::normalizeMemberLoadSystem(action.system) == QLatin1String("GLOBAL");
            data.coordSystem = global ? Structura::App::SolverBridge::MemberLoadCoordSystem::Global
                                      : Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
            switch (action.kind) {
            case Kind::kTrapezoid:
                data.kind = 0;
                data.v1 = action.v1;
                data.v2 = action.v2;
                break;
            case Kind::kPointForce:
                data.kind = 1;
                data.v1 = action.v1;
                break;
            case Kind::kPointMoment:
                data.kind = 2;
                data.v1 = action.v1;
                break;
            case Kind::kTemperature: {
                const double alpha = alphaByMember.value(action.memberId, 1.2e-5);
                data.kind = 3;
                data.coordSystem = Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
                data.v1 = {{alpha * action.v1[0], alpha * action.v1[1], alpha * action.v1[2]}};
                break;
            }
            case Kind::kShrinkage:
                data.kind = 3;
                data.coordSystem = Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
                data.v1 = {{action.v1[0], 0.0, 0.0}};
                break;
            default:
                continue;
            }
            model.memberActions.push_back(data);
        }
    }

    // Vinculos, ligacoes semirrigidas, rotulas plasticas, ligacoes entre nos e controle da analise nao linear
    {
        const double toModelForce = 1000.0 / DesignService::unitFactors(materials).first;   // kN -> unidade de forca do modelo
        QSet<int> nodeIds, memberIds;
        for (const auto &node : model.nodes) {
            nodeIds.insert(node.id);
        }
        for (const auto &member : model.members) {
            memberIds.insert(member.id);
        }
        auto requireNode = [&](int id, const QString &what) {
            if (!nodeIds.contains(id)) {
                result.errors.append(QStringLiteral("%1 referencia o no inexistente %2.").arg(what).arg(id));
                return false;
            }
            return true;
        };
        for (const auto &d : extras.diaphragms) {
            Structura::App::SolverBridge::DiaphragmData data;
            data.masterNodeId = d.masterNode;
            data.normalAxis = d.normalAxis;
            bool ok = requireNode(d.masterNode, QStringLiteral("Diafragma %1").arg(d.name));
            for (const int slave : d.slaveNodes) {
                ok = requireNode(slave, QStringLiteral("Diafragma %1").arg(d.name)) && ok;
                data.slaveNodeIds.push_back(slave);
            }
            if (ok) {
                model.diaphragms.push_back(std::move(data));
            }
        }
        for (const auto &l : extras.rigidLinks) {
            if (requireNode(l.masterNode, QStringLiteral("Ligacao rigida")) && requireNode(l.slaveNode, QStringLiteral("Ligacao rigida"))) {
                model.rigidLinks.push_back({l.masterNode, l.slaveNode, l.kind, l.dof});
            }
        }
        for (const auto &s : extras.inclinedSupports) {
            if (requireNode(s.nodeId, QStringLiteral("Apoio inclinado"))) {
                model.inclinedSupports.push_back({s.nodeId, s.axes, s.mask});
            }
        }
        for (const auto &c : extras.constraints) {
            Structura::App::SolverBridge::LinearConstraintData data;
            bool ok = true;
            for (int i = 0; i < c.nodes.size() && i < c.dofs.size() && i < c.coefs.size(); ++i) {
                ok = requireNode(c.nodes[i], QStringLiteral("Vinculo linear")) && ok;
                data.nodeIds.push_back(c.nodes[i]);
                data.dofs.push_back(c.dofs[i]);
                data.coefs.push_back(c.coefs[i]);
            }
            data.reportReaction = c.reportReaction;
            if (ok && !data.nodeIds.empty()) {
                model.constraints.push_back(std::move(data));
            }
        }
        for (const auto &spec : extras.memberConnections) {
            if (!memberIds.contains(spec.memberId)) {
                continue;
            }
            bool anySpring = false;
            Structura::App::SolverBridge::EndSpringData spring;
            spring.memberId = spec.memberId;
            for (std::size_t i = 0; i < 12; ++i) {
                spring.k[i] = spec.springK[i] >= 0.0 ? spec.springK[i] * toModelForce : -1.0;
                anySpring = anySpring || spec.springK[i] >= 0.0;
            }
            if (anySpring) {
                model.endSprings.push_back(spring);
            }
            for (const auto &h : spec.hinges) {
                Structura::App::SolverBridge::PlasticHingeData data;
                data.memberId = spec.memberId;
                data.dof = h.dof;
                data.kElastic = h.kElastic * toModelForce;
                data.yieldMoment = h.yieldMoment * toModelForce;
                data.hardeningRatio = h.hardeningRatio;
                data.axialYield = h.axialYield * toModelForce;
                data.exponent = h.exponent;
                model.plasticHinges.push_back(data);
            }
        }
        for (const auto &link : extras.links) {
            if (!requireNode(link.nodeI, QStringLiteral("Ligacao %1").arg(link.name)) || !requireNode(link.nodeJ, QStringLiteral("Ligacao %1").arg(link.name))) {
                continue;
            }
            Structura::App::SolverBridge::LinkData data;
            data.id = link.id;
            data.nodeI = link.nodeI;
            data.nodeJ = link.nodeJ;
            data.axes = link.axes;
            for (std::size_t i = 0; i < 6; ++i) {
                const auto &law = link.laws[i];
                auto &out = data.laws[i];
                out.kind = law.kind;
                out.k = law.k * toModelForce;
                out.p1 = law.kind == 5 ? law.p1 * toModelForce : law.p1;   // forca de escoamento (kN) ou folga (m/rad)
                out.p2 = law.p2;
                for (const auto &point : law.table) {
                    out.table.emplace_back(point.first, point.second * toModelForce);
                }
            }
            model.links.push_back(std::move(data));
        }
        const auto &nl = extras.nonlinear;
        model.nonlinear.force = nl.force;
        model.nonlinear.steps = nl.steps;
        model.nonlinear.maxIterations = nl.maxIterations;
        model.nonlinear.tolerance = nl.tolerance;
        model.nonlinear.criteria = nl.criteria;
        model.nonlinear.maxCutbacks = nl.maxCutbacks;
        model.nonlinear.controlMode = nl.controlMode;
        model.nonlinear.controlNodeId = nl.controlNode;
        model.nonlinear.controlDof = nl.controlDof;
        model.nonlinear.controlTarget = nl.controlTarget;
        model.nonlinear.loadFactorMax = nl.loadFactorMax;
    }

    // Peso proprio: carga distribuida global -Z = gama (kN/m3) x area da secao (m2).
    bool includeSelfWeight = false;
    for (const auto &candidate : extras.loadCases) {
        if (candidate.name == loadCase && candidate.selfWeight) {
            includeSelfWeight = true;
        }
    }
    if (includeSelfWeight) {
        for (const auto &bar : bars) {
            const auto materialIt = materialsById.constFind(bar.materialId);
            const auto sectionIt = sectionsById.constFind(bar.sectionId);
            if (materialIt == materialsById.constEnd() || sectionIt == sectionsById.constEnd()) {
                continue;
            }
            const double weight = materialIt->unitWeight * sectionIt->area;
            if (weight == 0.0) {
                continue;
            }
            Structura::App::SolverBridge::MemberLoadData data;
            data.memberId = bar.externalId;
            data.coordSystem = Structura::App::SolverBridge::MemberLoadCoordSystem::Global;
            data.qz = -weight;
            model.memberLoads.push_back(data);
        }
    }

    // Cascas e chapas acopladas: entram na mesma analise (nos coincidentes, ligacoes, apoios e cargas do caso)
    if (extras.shell.coupled && !extras.shell.objects.empty() && result.errors.isEmpty()) {
        const double forceUnit = DesignService::unitFactors(materials).first;
        ShellCouplingService::attach(model, extras.shell, (loadCase.isEmpty() ? defaultCase : loadCase).toStdString(), forceUnit);
    } else if (extras.shell.tributary && !extras.shell.objects.empty() && result.errors.isEmpty()) {
        // sem MEF acoplado: quinhoes de carga das chapas apoiadas em barras (cargas distribuidas nas barras)
        const double forceUnit = DesignService::unitFactors(materials).first;
        ShellCouplingService::attachTributary(model, extras.shell, (loadCase.isEmpty() ? defaultCase : loadCase).toStdString(), forceUnit);
    }

    result.success = result.errors.isEmpty();
    return result;
}

QString AnalysisService::formatReport(const Structura::App::SolverBridge::Result &result) const
{
    std::ostringstream report;
    report.setf(std::ios::scientific);
    report << std::setprecision(6);

    report << "ANALISE ESTRUTURAL - RESULTADOS (PROVISORIO)\n";
    report << "==========================================\n\n";
    report << "Nos resolvidos: " << result.nodeResults.size() << "\n";
    report << "Barras resolvidas: " << result.memberResults.size() << "\n\n";

    report << "[DESLOCAMENTOS E REACOES NODAIS]\n";
    report << "# No      UX            UY            UZ            RX            RY            RZ"
           << " | Fx            Fy            Fz            Mx            My            Mz\n";
    for (const auto &nodeResult : result.nodeResults) {
        report << std::setw(6) << nodeResult.nodeId << " ";
        for (const double value : nodeResult.displacements) {
            report << std::setw(13) << value << " ";
        }
        report << "| ";
        for (const double value : nodeResult.reactions) {
            report << std::setw(13) << value << " ";
        }
        report << "\n";
    }

    report << "\n[ESFORCOS LOCAIS EM BARRAS]\n";
    report << "# Barra   N_i           Vy_i          Vz_i          T_i           My_i          Mz_i"
           << "          N_j           Vy_j          Vz_j          T_j           My_j          Mz_j"
           << " | k1            k2            k3\n";
    for (const auto &memberResult : result.memberResults) {
        report << std::setw(7) << memberResult.memberId << " ";
        for (const double value : memberResult.localForces) {
            report << std::setw(13) << value << " ";
        }
        report << "| ";
        for (const double value : memberResult.auxK) {
            report << std::setw(13) << value << " ";
        }
        report << "\n";
    }

    return QString::fromStdString(report.str());
}

} // namespace Structura::App::Services
