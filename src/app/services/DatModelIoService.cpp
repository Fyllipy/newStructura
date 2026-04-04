#include "DatModelIoService.h"

#include "app/services/IdGeneratorService.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace Structura::App::Services {

bool DatModelIoService::load(const QString &filePath,
                             Structura::App::ISceneModelGateway *sceneController,
                             QVector<MaterialInfo> &materials,
                             QVector<SectionInfo> &sections,
                             QVector<NodeSupport> &supports,
                             QVector<NodalLoad> &nodalLoads,
                             QVector<MemberLoad> &memberLoads,
                             QUuid &lastMaterialId,
                             QUuid &lastSectionId,
                             QString *errorMessage,
                             QStringList *warnings) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Nao foi possivel abrir %1").arg(QDir::toNativeSeparators(filePath));
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setLocale(QLocale::c());

    enum class Section { None, Materials, Sections, Nodes, Members, NodalLoads, MemberLoads };
    Section current = Section::None;

    QVector<MaterialInfo> materialsTmp;
    QVector<SectionInfo> sectionsTmp;
    struct LoadedNode {
        int id;
        double x;
        double y;
        double z;
        std::array<int, 6> restraints;
    };
    QVector<LoadedNode> nodesTmp;
    struct LoadedMember {
        int id;
        int nodeI;
        int nodeJ;
        int materialId;
        int sectionId;
    };
    QVector<LoadedMember> membersTmp;
    QVector<NodeSupport> supportsTmp;
    QVector<NodalLoad> nodalLoadsTmp;
    QVector<MemberLoad> memberLoadsTmp;

    int lineNumber = 0;
    QRegularExpression whitespace(QStringLiteral("\\s+"));

    auto toInt = [](const QString &token, bool *ok = nullptr) {
        bool localOk = false;
        const int value = token.toInt(&localOk);
        if (ok) {
            *ok = localOk;
        }
        return value;
    };

    auto toDouble = [](const QString &token, bool *ok = nullptr) {
        bool localOk = false;
        const double value = token.toDouble(&localOk);
        if (ok) {
            *ok = localOk;
        }
        return value;
    };

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        ++lineNumber;
        const QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (trimmed.startsWith(QLatin1Char('['))) {
            if (trimmed.compare(QStringLiteral("[MATERIALS]"), Qt::CaseInsensitive) == 0) {
                current = Section::Materials;
            } else if (trimmed.compare(QStringLiteral("[SECTIONS]"), Qt::CaseInsensitive) == 0) {
                current = Section::Sections;
            } else if (trimmed.compare(QStringLiteral("[NODES]"), Qt::CaseInsensitive) == 0) {
                current = Section::Nodes;
            } else if (trimmed.compare(QStringLiteral("[MEMBERS]"), Qt::CaseInsensitive) == 0) {
                current = Section::Members;
            } else if (trimmed.compare(QStringLiteral("[NODAL_LOADS]"), Qt::CaseInsensitive) == 0) {
                current = Section::NodalLoads;
            } else if (trimmed.compare(QStringLiteral("[MEMBER_LOADS]"), Qt::CaseInsensitive) == 0) {
                current = Section::MemberLoads;
            } else {
                current = Section::None;
            }
            continue;
        }

        const QStringList parts = trimmed.split(whitespace, Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        bool ok = false;
        switch (current) {
        case Section::Materials: {
            if (parts.size() < 3) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Linha de material invalida (%1)").arg(lineNumber);
                }
                return false;
            }
            MaterialInfo mat;
            mat.uuid = QUuid::createUuid();
            mat.externalId = toInt(parts[0], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("ID de material invalido na linha %1").arg(lineNumber);
                }
                return false;
            }
            mat.name = QStringLiteral("Material %1").arg(mat.externalId);
            mat.youngModulus = toDouble(parts[1], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Valor de E invalido na linha %1").arg(lineNumber);
                }
                return false;
            }
            mat.shearModulus = toDouble(parts[2], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Valor de G invalido na linha %1").arg(lineNumber);
                }
                return false;
            }
            materialsTmp.append(mat);
            break;
        }
        case Section::Sections: {
            if (parts.size() < 5) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Linha de secao invalida (%1)").arg(lineNumber);
                }
                return false;
            }
            SectionInfo sec;
            sec.uuid = QUuid::createUuid();
            sec.externalId = toInt(parts[0], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("ID de secao invalido na linha %1").arg(lineNumber);
                }
                return false;
            }
            sec.name = QStringLiteral("Secao %1").arg(sec.externalId);
            sec.area = toDouble(parts[1], &ok);
            sec.iz = toDouble(parts[2], &ok);
            sec.iy = toDouble(parts[3], &ok);
            sec.j = toDouble(parts[4], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Valores da secao invalidos na linha %1").arg(lineNumber);
                }
                return false;
            }
            sectionsTmp.append(sec);
            break;
        }
        case Section::Nodes: {
            if (parts.size() < 10) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Linha de no invalida (%1)").arg(lineNumber);
                }
                return false;
            }
            LoadedNode node{};
            node.id = toInt(parts[0], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("ID de no invalido na linha %1").arg(lineNumber);
                }
                return false;
            }
            node.x = toDouble(parts[1], &ok);
            node.y = toDouble(parts[2], &ok);
            node.z = toDouble(parts[3], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Coordenadas invalidas na linha %1").arg(lineNumber);
                }
                return false;
            }
            for (int i = 0; i < 6; ++i) {
                node.restraints[static_cast<std::size_t>(i)] = (i + 4 < parts.size()) ? toInt(parts[4 + i], &ok) : 0;
                if (!ok) {
                    node.restraints[static_cast<std::size_t>(i)] = 0;
                }
            }
            nodesTmp.append(node);
            break;
        }
        case Section::Members: {
            if (parts.size() < 5) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Linha de barra invalida (%1)").arg(lineNumber);
                }
                return false;
            }
            LoadedMember member{};
            member.id = toInt(parts[0], &ok);
            member.nodeI = toInt(parts[1], &ok);
            member.nodeJ = toInt(parts[2], &ok);
            member.materialId = toInt(parts[3], &ok);
            member.sectionId = toInt(parts[4], &ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Linha de barra invalida (%1)").arg(lineNumber);
                }
                return false;
            }
            membersTmp.append(member);
            break;
        }
        case Section::NodalLoads: {
            if (parts.size() < 7) {
                break;
            }
            NodalLoad load;
            load.nodeId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            load.fx = toDouble(parts[1], &ok);
            load.fy = toDouble(parts[2], &ok);
            load.fz = toDouble(parts[3], &ok);
            load.mx = toDouble(parts[4], &ok);
            load.my = toDouble(parts[5], &ok);
            load.mz = toDouble(parts[6], &ok);
            nodalLoadsTmp.append(load);
            break;
        }
        case Section::MemberLoads: {
            if (parts.size() < 5) {
                break;
            }
            MemberLoad load;
            load.memberId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            load.system = parts[1].toUpper();
            load.qx = toDouble(parts[2], &ok);
            load.qy = toDouble(parts[3], &ok);
            load.qz = toDouble(parts[4], &ok);
            memberLoadsTmp.append(load);
            break;
        }
        case Section::None:
        default:
            break;
        }
    }

    file.close();

    supportsTmp.clear();
    for (const auto &node : nodesTmp) {
        NodeSupport support;
        support.nodeId = node.id;
        for (int i = 0; i < 6; ++i) {
            support.restraints[i] = node.restraints[static_cast<std::size_t>(i)] != 0;
        }
        supportsTmp.append(support);
    }

    materials = materialsTmp;
    sections = sectionsTmp;
    supports = supportsTmp;
    nodalLoads = nodalLoadsTmp;
    memberLoads = memberLoadsTmp;

    if (!materials.isEmpty()) {
        lastMaterialId = materials.first().uuid;
    }
    if (!sections.isEmpty()) {
        lastSectionId = sections.first().uuid;
    }

    QHash<int, QUuid> materialMap;
    for (MaterialInfo &mat : materials) {
        materialMap.insert(mat.externalId, mat.uuid);
        if (mat.name.trimmed().isEmpty()) {
            mat.name = QStringLiteral("Material %1").arg(mat.externalId);
        }
    }
    QHash<int, QUuid> sectionMap;
    for (SectionInfo &sec : sections) {
        sectionMap.insert(sec.externalId, sec.uuid);
        if (sec.name.trimmed().isEmpty()) {
            sec.name = QStringLiteral("Secao %1").arg(sec.externalId);
        }
    }

    QHash<int, QUuid> nodeUuidMap;
    for (const auto &node : nodesTmp) {
        const QUuid uuid = sceneController->addPointWithId(node.x, node.y, node.z, node.id);
        nodeUuidMap.insert(node.id, uuid);

        Structura::Model::Node *sceneNode = sceneController->findNode(uuid);
        if (sceneNode) {
            for (int i = 0; i < 6; ++i) {
                sceneNode->setRestraint(i, node.restraints[static_cast<std::size_t>(i)] != 0);
            }
        }
    }

    for (const auto &member : membersTmp) {
        const QUuid startId = nodeUuidMap.value(member.nodeI);
        const QUuid endId = nodeUuidMap.value(member.nodeJ);
        if (startId.isNull() || endId.isNull()) {
            if (warnings) {
                warnings->append(QStringLiteral("Barra %1 referencia nos inexistentes").arg(member.id));
            }
            continue;
        }
        const QUuid materialUuid = materialMap.value(member.materialId);
        const QUuid sectionUuid = sectionMap.value(member.sectionId);
        const QUuid barId = sceneController->addBar(startId, endId, materialUuid, sectionUuid);
        if (!barId.isNull()) {
            sceneController->setBarExternalId(barId, member.id);
        }
    }

    return true;
}

bool DatModelIoService::save(const QString &filePath,
                             Structura::App::ISceneModelGateway *sceneController,
                             QVector<MaterialInfo> &materials,
                             QVector<SectionInfo> &sections,
                             const QVector<NodeSupport> &supports,
                             const QVector<NodalLoad> &nodalLoads,
                             const QVector<MemberLoad> &memberLoads,
                             QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Nao foi possivel salvar em %1").arg(QDir::toNativeSeparators(filePath));
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setLocale(QLocale::c());

    IdGeneratorService::ensureBarExternalIds(sceneController);
    auto bars = sceneController->bars();

    auto materialsSorted = materials;
    std::sort(materialsSorted.begin(), materialsSorted.end(), [](const MaterialInfo &a, const MaterialInfo &b) {
        return a.externalId < b.externalId;
    });
    IdGeneratorService::ensureMaterialExternalIds(materialsSorted);

    auto sectionsSorted = sections;
    std::sort(sectionsSorted.begin(), sectionsSorted.end(), [](const SectionInfo &a, const SectionInfo &b) {
        return a.externalId < b.externalId;
    });
    IdGeneratorService::ensureSectionExternalIds(sectionsSorted);

    for (const auto &mat : materialsSorted) {
        for (MaterialInfo &orig : materials) {
            if (orig.uuid == mat.uuid) {
                orig.externalId = mat.externalId;
                break;
            }
        }
    }
    for (const auto &sec : sectionsSorted) {
        for (SectionInfo &orig : sections) {
            if (orig.uuid == sec.uuid) {
                orig.externalId = sec.externalId;
                break;
            }
        }
    }

    QHash<QUuid, int> materialExternalById;
    for (const auto &mat : materials) {
        materialExternalById.insert(mat.uuid, mat.externalId);
    }
    QHash<QUuid, int> sectionExternalById;
    for (const auto &sec : sections) {
        sectionExternalById.insert(sec.uuid, sec.externalId);
    }

    const auto nodes = sceneController->nodeInfos();
    QHash<QUuid, Structura::App::SceneNodeInfo> nodeInfoMap;
    for (const auto &node : nodes) {
        nodeInfoMap.insert(node.id, node);
    }

    auto sortedNodes = nodes;
    std::sort(sortedNodes.begin(), sortedNodes.end(), [](const Structura::App::SceneNodeInfo &a, const Structura::App::SceneNodeInfo &b) {
        return a.externalId < b.externalId;
    });

    QVector<NodeSupport> supportsCopy = supports;
    for (const auto &node : sortedNodes) {
        bool found = false;
        for (const auto &sup : supportsCopy) {
            if (sup.nodeId == node.externalId) {
                found = true;
                break;
            }
        }
        if (!found) {
            NodeSupport sup{};
            sup.nodeId = node.externalId;
            for (bool &restraint : sup.restraints) {
                restraint = false;
            }
            supportsCopy.append(sup);
        }
    }

    auto members = bars;
    std::sort(members.begin(), members.end(), [](const Structura::App::SceneBarInfo &a, const Structura::App::SceneBarInfo &b) {
        return a.externalId < b.externalId;
    });

    stream << "[MATERIALS]\n";
    stream << "# ID    E (Pa)          G (Pa)\n";
    for (const auto &mat : materialsSorted) {
        stream << QString::asprintf("%-8d %14.6e %14.6e\n", mat.externalId, mat.youngModulus, mat.shearModulus);
    }
    stream << "\n";

    stream << "[SECTIONS]\n";
    stream << "# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)\n";
    for (const auto &sec : sectionsSorted) {
        stream << QString::asprintf("%-8d %14.6e %14.6e %14.6e %14.6e\n", sec.externalId, sec.area, sec.iz, sec.iy, sec.j);
    }
    stream << "\n";

    stream << "[NODES]\n";
    stream << "# ID    X (m)    Y (m)    Z (m)    UX  UY  UZ  RX  RY  RZ\n";
    for (const auto &node : sortedNodes) {
        int restraints[6] = {0, 0, 0, 0, 0, 0};
        for (const auto &sup : supportsCopy) {
            if (sup.nodeId == node.externalId) {
                for (int i = 0; i < 6; ++i) {
                    restraints[i] = sup.restraints[i] ? 1 : 0;
                }
                break;
            }
        }
        stream << QString::asprintf("%-8d %10.6f %10.6f %10.6f    %d   %d   %d   %d   %d   %d\n",
                                    node.externalId,
                                    node.x,
                                    node.y,
                                    node.z,
                                    restraints[0], restraints[1], restraints[2], restraints[3], restraints[4], restraints[5]);
    }
    stream << "\n";

    stream << "[MEMBERS]\n";
    stream << "# ID    Node_i   Node_j   Material_ID   Section_ID\n";
    for (const auto &bar : members) {
        const auto startIt = nodeInfoMap.constFind(bar.startNodeId);
        const auto endIt = nodeInfoMap.constFind(bar.endNodeId);
        const int startExternalId = (startIt != nodeInfoMap.constEnd()) ? startIt.value().externalId : 0;
        const int endExternalId = (endIt != nodeInfoMap.constEnd()) ? endIt.value().externalId : 0;
        const int materialId = materialExternalById.value(bar.materialId, 0);
        const int sectionId = sectionExternalById.value(bar.sectionId, 0);

        stream << QString::asprintf("%-8d %8d %8d %10d %12d\n",
                                    bar.externalId,
                                    startExternalId,
                                    endExternalId,
                                    materialId,
                                    sectionId);
    }
    stream << "\n";

    stream << "[NODAL_LOADS]\n";
    stream << "# Node_ID   Fx (N)    Fy (N)   Fz (N)   Mx (Nm)   My (Nm)   Mz (Nm)\n";
    for (const auto &load : nodalLoads) {
        stream << QString::asprintf("%-8d %10.6f %10.6f %10.6f %10.6f %10.6f %10.6f\n",
                                    load.nodeId, load.fx, load.fy, load.fz, load.mx, load.my, load.mz);
    }
    stream << "\n";

    stream << "[MEMBER_LOADS]\n";
    stream << "# Formato: Member_ID  Sistema(Local/Global)  qx (N/m)  qy (N/m)  qz (N/m)\n";
    stream << "# Sistemas aceitos: LOCAL, GLOBAL (ou L/G)\n";
    for (const auto &load : memberLoads) {
        const QByteArray systemBytes = load.system.isEmpty() ? QByteArray("LOCAL") : load.system.toUpper().toLatin1();
        stream << QString::asprintf("%-12d %-10s %12.6f %12.6f %12.6f\n",
                                    load.memberId,
                                    systemBytes.constData(),
                                    load.qx, load.qy, load.qz);
    }

    file.close();
    return true;
}

} // namespace Structura::App::Services
