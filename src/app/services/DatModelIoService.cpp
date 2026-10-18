#include "DatModelIoService.h"

#include <QUrl>

#include "app/services/IdGeneratorService.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

namespace Structura::App::Services {

bool DatModelIoService::load(const QString &filePath,
                             Structura::App::ISceneModelGateway *sceneController,
                             QVector<MaterialInfo> &materials,
                             QVector<SectionInfo> &sections,
                             QVector<NodeSupport> &supports,
                             QVector<NodalLoad> &nodalLoads,
                             QVector<MemberLoad> &memberLoads,
                             ProjectExtras &extras,
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

    enum class Section { None, Materials, MaterialProps, Sections, Nodes, Members, NodalLoads, MemberLoads, MemberMoments, Placements, Axes, ViewOptions, LoadCases, Combinations, SectionGeometry, DesignSettings, MemberDesign, Groups, AreaLoads, AnalysisResults, DesignResults, BucklingResults, ModalResults, Connections, TitleBlock, Views3D, SheetLayouts, CompositeFloor, ShellModel };
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
    QVector<Structura::App::ProjectData::MemberMoment> memberMomentsTmp;
    QVector<Structura::App::ProjectData::MemberPlacement> placementsTmp;
    QHash<int, double> unitWeightByMaterial;
    struct MaterialExtra {
        QString name, library;
        double fy {0.0}, fu {0.0};
    };
    QHash<int, MaterialExtra> materialExtras;
    Structura::App::ProjectData::ReferenceAxes axesTmp;
    QMap<QString, double> viewOptionsTmp;
    Structura::App::ProjectData::DesignSettings designTmp;
    QVector<Structura::App::ProjectData::BarGroup> groupsTmp;
    QVector<Structura::App::ProjectData::AreaLoadPanel> areaLoadsTmp;
    Structura::App::ProjectData::StoredResults resultsTmp;
    QVector<Structura::App::ProjectData::NodeConnection> connectionsTmp;
    Structura::App::ProjectData::TitleBlockSettings titleBlockTmp;
    QVector<Structura::App::ProjectData::SavedView3D> views3dTmp;
    QVector<Structura::App::ProjectData::SheetLayoutDef> sheetLayoutsTmp;
    QMap<int, QMap<QString, double>> compositeBeamsTmp;
    QMap<QString, double> deckSlabTmp;
    Structura::Shell::Model shellTmp;
    shellTmp.materials.clear();
    auto decodeName = [](const QString &token) { return QUrl::fromPercentEncoding(token.toLatin1()); };
    struct SectionGeometryRecord {
        QString family;
        bool mirrored {false};
        QMap<QString, double> parameters;
    };
    QHash<int, SectionGeometryRecord> sectionGeometryTmp;
    QVector<Structura::App::ProjectData::LoadCase> loadCasesTmp;
    QVector<Structura::App::ProjectData::LoadCombination> combinationsTmp;

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
            } else if (trimmed.compare(QStringLiteral("[MATERIAL_PROPS]"), Qt::CaseInsensitive) == 0) {
                current = Section::MaterialProps;
            } else if (trimmed.compare(QStringLiteral("[SECTION_GEOMETRY]"), Qt::CaseInsensitive) == 0) {
                current = Section::SectionGeometry;
            } else if (trimmed.compare(QStringLiteral("[LOAD_CASES]"), Qt::CaseInsensitive) == 0) {
                current = Section::LoadCases;
            } else if (trimmed.compare(QStringLiteral("[COMBINATIONS]"), Qt::CaseInsensitive) == 0) {
                current = Section::Combinations;
            } else if (trimmed.compare(QStringLiteral("[AXES]"), Qt::CaseInsensitive) == 0) {
                current = Section::Axes;
            } else if (trimmed.compare(QStringLiteral("[VIEW_OPTIONS]"), Qt::CaseInsensitive) == 0) {
                current = Section::ViewOptions;
            } else if (trimmed.compare(QStringLiteral("[MEMBER_MOMENTS]"), Qt::CaseInsensitive) == 0) {
                current = Section::MemberMoments;
            } else if (trimmed.compare(QStringLiteral("[AREA_LOADS]"), Qt::CaseInsensitive) == 0) {
                current = Section::AreaLoads;
            } else if (trimmed.compare(QStringLiteral("[VIEWS_3D]"), Qt::CaseInsensitive) == 0) {
                current = Section::Views3D;
            } else if (trimmed.compare(QStringLiteral("[SHELL_MODEL]"), Qt::CaseInsensitive) == 0) {
                current = Section::ShellModel;
            } else if (trimmed.compare(QStringLiteral("[COMPOSITE_FLOOR]"), Qt::CaseInsensitive) == 0) {
                current = Section::CompositeFloor;
            } else if (trimmed.compare(QStringLiteral("[SHEET_LAYOUTS]"), Qt::CaseInsensitive) == 0) {
                current = Section::SheetLayouts;
            } else if (trimmed.compare(QStringLiteral("[TITLE_BLOCK]"), Qt::CaseInsensitive) == 0) {
                current = Section::TitleBlock;
            } else if (trimmed.compare(QStringLiteral("[CONNECTIONS]"), Qt::CaseInsensitive) == 0) {
                current = Section::Connections;
            } else if (trimmed.compare(QStringLiteral("[ANALYSIS_RESULTS]"), Qt::CaseInsensitive) == 0) {
                current = Section::AnalysisResults;
            } else if (trimmed.compare(QStringLiteral("[DESIGN_RESULTS]"), Qt::CaseInsensitive) == 0) {
                current = Section::DesignResults;
            } else if (trimmed.compare(QStringLiteral("[BUCKLING_RESULTS]"), Qt::CaseInsensitive) == 0) {
                current = Section::BucklingResults;
            } else if (trimmed.compare(QStringLiteral("[MODAL_RESULTS]"), Qt::CaseInsensitive) == 0) {
                current = Section::ModalResults;
            } else if (trimmed.compare(QStringLiteral("[GROUPS]"), Qt::CaseInsensitive) == 0) {
                current = Section::Groups;
            } else if (trimmed.compare(QStringLiteral("[DESIGN_SETTINGS]"), Qt::CaseInsensitive) == 0) {
                current = Section::DesignSettings;
            } else if (trimmed.compare(QStringLiteral("[MEMBER_DESIGN]"), Qt::CaseInsensitive) == 0) {
                current = Section::MemberDesign;
            } else if (trimmed.compare(QStringLiteral("[MEMBER_PLACEMENT]"), Qt::CaseInsensitive) == 0) {
                current = Section::Placements;
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
        case Section::MaterialProps: {
            if (parts.size() < 2) {
                break;
            }
            const int materialId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            const double unitWeight = toDouble(parts[1], &ok);
            if (ok) {
                unitWeightByMaterial.insert(materialId, unitWeight);
            }
            if (parts.size() >= 3) {
                MaterialExtra extra;
                extra.name = parts[2] == QLatin1String("-") ? QString() : QString(parts[2]).replace(QLatin1Char('_'), QLatin1Char(' '));
                if (parts.size() >= 4) {
                    extra.library = parts[3] == QLatin1String("-") ? QString() : QString(parts[3]).replace(QLatin1Char('_'), QLatin1Char(' '));
                }
                if (parts.size() >= 6) {
                    extra.fy = toDouble(parts[4], &ok);
                    extra.fu = toDouble(parts[5], &ok);
                }
                materialExtras.insert(materialId, extra);
            }
            break;
        }
        case Section::SectionGeometry: {
            if (parts.size() < 3) {
                break;
            }
            const int sectionId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            SectionGeometryRecord record;
            record.family = parts[1];
            record.family.replace(QLatin1Char('_'), QLatin1Char(' '));
            record.mirrored = parts[2].toInt() != 0;
            for (int i = 3; i < parts.size(); ++i) {
                const int separator = parts[i].indexOf(QLatin1Char('='));
                if (separator <= 0) {
                    continue;
                }
                bool valueOk = false;
                const double value = parts[i].mid(separator + 1).toDouble(&valueOk);
                if (valueOk) {
                    record.parameters.insert(parts[i].left(separator), value);
                }
            }
            sectionGeometryTmp.insert(sectionId, record);
            break;
        }
        case Section::LoadCases: {
            Structura::App::ProjectData::LoadCase loadCase;
            loadCase.name = parts[0];
            loadCase.abbreviation = parts.size() > 1 ? parts[1] : parts[0];
            loadCase.selfWeight = parts.size() > 2 && parts[2].toInt() != 0;
            loadCasesTmp.append(loadCase);
            break;
        }
        case Section::Combinations: {
            Structura::App::ProjectData::LoadCombination combination;
            combination.name = parts[0];
            for (int i = 1; i < parts.size(); ++i) {
                const int separator = parts[i].lastIndexOf(QLatin1Char('='));
                if (separator <= 0) {
                    continue;
                }
                bool factorOk = false;
                const double factor = parts[i].mid(separator + 1).toDouble(&factorOk);
                if (factorOk) {
                    combination.factors.append({parts[i].left(separator), factor});
                }
            }
            combinationsTmp.append(combination);
            break;
        }
        case Section::Axes: {
            if (parts.size() < 3) {
                break;
            }
            const QString direction = parts[0].toUpper();
            const double value = toDouble(parts[2], &ok);
            if (!ok) {
                break;
            }
            auto currentLevel = [&axesTmp]() -> Structura::App::ProjectData::AxisLevel & {
                if (axesTmp.levels.isEmpty()) {
                    axesTmp.levels.append({QStringLiteral("N0"), 0.0, {}, {}});
                }
                return axesTmp.levels.last();
            };
            if (direction == QLatin1String("LEVEL")) {
                // LEVEL nome elevacao: abre um novo plano de eixos.
                axesTmp.levels.append({parts[1], value, {}, {}});
            } else if (direction == QLatin1String("X")) {
                currentLevel().x.append({parts[1].toUpper(), value});
            } else if (direction == QLatin1String("Y")) {
                currentLevel().y.append({parts[1].toUpper(), value});
            } else if (direction == QLatin1String("Z")) {
                // Formato antigo: eixo Z = nivel sem eixos proprios.
                if (!axesTmp.levelAt(value)) {
                    axesTmp.levels.append({parts[1].toUpper(), value, {}, {}});
                }
            }
            break;
        }
        case Section::ViewOptions: {
            if (parts.size() < 2) {
                break;
            }
            const double value = toDouble(parts[1], &ok);
            if (ok) {
                viewOptionsTmp.insert(parts[0], value);
            }
            break;
        }
        case Section::AreaLoads: {
            // id caso tipo pressao dx dy dz nPontos x y z ... nBarras b1 b2 ... descricao
            if (parts.size() < 9) {
                break;
            }
            Structura::App::ProjectData::AreaLoadPanel panel;
            panel.id = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            panel.loadCase = QString(parts[1]).replace(QLatin1Char('_'), QLatin1Char(' '));
            panel.kind = parts[2];
            panel.pressure = toDouble(parts[3], &ok);
            panel.direction = {toDouble(parts[4], &ok), toDouble(parts[5], &ok), toDouble(parts[6], &ok)};
            int cursor = 7;
            const int pointCount = toInt(parts[cursor++], &ok);
            for (int i = 0; i < pointCount && cursor + 2 < parts.size(); ++i, cursor += 3) {
                panel.polygon.append({toDouble(parts[cursor], &ok), toDouble(parts[cursor + 1], &ok), toDouble(parts[cursor + 2], &ok)});
            }
            if (cursor < parts.size()) {
                const int memberCount = toInt(parts[cursor++], &ok);
                for (int i = 0; i < memberCount && cursor < parts.size(); ++i, ++cursor) {
                    panel.members.append(toInt(parts[cursor], &ok));
                }
            }
            areaLoadsTmp.append(panel);
            break;
        }
        case Section::AnalysisResults: {
            const QString key = parts[0].toUpper();
            if (key == QLatin1String("FINGERPRINT") && parts.size() >= 2) {
                resultsTmp.fingerprint = parts[1];
            } else if (key == QLatin1String("RUN") && parts.size() >= 2) {
                Structura::App::ProjectData::StoredRun run;
                run.name = decodeName(parts[1]);
                resultsTmp.runs.append(run);
            } else if (key == QLatin1String("N") && parts.size() >= 14 && !resultsTmp.runs.isEmpty()) {
                Structura::App::ProjectData::StoredNodeResult node;
                node.nodeId = toInt(parts[1], &ok);
                for (int i = 0; i < 6; ++i) {
                    node.displacements[static_cast<std::size_t>(i)] = toDouble(parts[2 + i], &ok);
                    node.reactions[static_cast<std::size_t>(i)] = toDouble(parts[8 + i], &ok);
                }
                resultsTmp.runs.last().nodes.append(node);
            } else if (key == QLatin1String("M") && parts.size() >= 17 && !resultsTmp.runs.isEmpty()) {
                Structura::App::ProjectData::StoredMemberResult member;
                member.memberId = toInt(parts[1], &ok);
                for (int i = 0; i < 12; ++i) {
                    member.forces[static_cast<std::size_t>(i)] = toDouble(parts[2 + i], &ok);
                }
                for (int i = 0; i < 3; ++i) {
                    member.auxK[static_cast<std::size_t>(i)] = toDouble(parts[14 + i], &ok);
                }
                resultsTmp.runs.last().members.append(member);
            } else if (key == QLatin1String("SN") && parts.size() >= 14 && !resultsTmp.runs.isEmpty()) {
                Structura::App::ProjectData::StoredNodeResult node;
                node.nodeId = toInt(parts[1], &ok);
                for (int i = 0; i < 6; ++i) {
                    node.displacements[static_cast<std::size_t>(i)] = toDouble(parts[2 + i], &ok);
                    node.reactions[static_cast<std::size_t>(i)] = toDouble(parts[8 + i], &ok);
                }
                resultsTmp.runs.last().shellNodes.append(node);
            } else if (key == QLatin1String("SE") && parts.size() >= 52 && !resultsTmp.runs.isEmpty()) {
                Structura::App::ProjectData::StoredShellResult element;
                element.shellId = toInt(parts[1], &ok);
                for (int i = 0; i < 40; ++i) {
                    element.values[static_cast<std::size_t>(i)] = toDouble(parts[2 + i], &ok);
                }
                for (int i = 0; i < 9; ++i) {
                    element.axes[static_cast<std::size_t>(i)] = toDouble(parts[42 + i], &ok);
                }
                resultsTmp.runs.last().shellElements.append(element);
            }
            break;
        }
        case Section::Views3D: {
            if (parts.size() < 14) {
                break;
            }
            Structura::App::ProjectData::SavedView3D v;
            v.name = parts[0] == QLatin1String("-") ? QString() : decodeName(parts[0]);
            v.perspective = toInt(parts[1], &ok) != 0;
            v.azimuthDeg = toDouble(parts[2], &ok);
            v.elevationDeg = toDouble(parts[3], &ok);
            v.fovDeg = toDouble(parts[4], &ok);
            v.explicitCamera = toInt(parts[5], &ok) != 0;
            for (int i = 0; i < 3; ++i) {
                v.position[static_cast<std::size_t>(i)] = toDouble(parts[6 + i], &ok);
            }
            for (int i = 0; i < 3; ++i) {
                v.target[static_cast<std::size_t>(i)] = toDouble(parts[9 + i], &ok);
            }
            if (parts.size() >= 15) {
                for (int i = 0; i < 3; ++i) {
                    v.up[static_cast<std::size_t>(i)] = toDouble(parts[12 + i], &ok);
                }
            }
            views3dTmp.append(v);
            break;
        }
        case Section::ShellModel: {
            const QString key = parts[0].toUpper();
            auto objectById = [&](int id) -> Structura::Shell::Object * {
                for (auto &o : shellTmp.objects) {
                    if (o.id == id) {
                        return &o;
                    }
                }
                return nullptr;
            };
            if (key == QLatin1String("COUPLED") && parts.size() >= 2) {
                shellTmp.coupled = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("MAT") && parts.size() >= 10) {
                Structura::Shell::Material m;
                m.name = decodeName(parts[2]).toStdString();
                m.concrete = toInt(parts[3], &ok) != 0;
                m.E = toDouble(parts[4], &ok);
                m.nu = toDouble(parts[5], &ok);
                m.unitWeight = toDouble(parts[6], &ok);
                m.alpha = toDouble(parts[7], &ok);
                m.fy = toDouble(parts[8], &ok);
                m.fu = toDouble(parts[9], &ok);
                shellTmp.materials.push_back(m);
            } else if (key == QLatin1String("OBJ") && parts.size() >= 7) {
                Structura::Shell::Object o;
                o.id = toInt(parts[1], &ok);
                o.name = decodeName(parts[2]).toStdString();
                o.kind = static_cast<Structura::Shell::Kind>(toInt(parts[3], &ok));
                o.material = toInt(parts[4], &ok);
                o.meshSize = toDouble(parts[5], &ok);
                o.visible = toInt(parts[6], &ok) != 0;
                shellTmp.objects.push_back(o);
            } else if (key == QLatin1String("PARAM") && parts.size() >= 4) {
                if (auto *o = objectById(toInt(parts[1], &ok))) {
                    o->p[parts[2].toStdString()] = toDouble(parts[3], &ok);
                }
            } else if (key == QLatin1String("POLY") && parts.size() >= 5) {
                if (auto *o = objectById(toInt(parts[1], &ok))) {
                    o->polygon.push_back({toDouble(parts[2], &ok), toDouble(parts[3], &ok), toDouble(parts[4], &ok)});
                }
            } else if (key == QLatin1String("LOAD") && parts.size() >= 8) {
                Structura::Shell::Load l;
                l.caseName = decodeName(parts[1]).toStdString();
                l.object = toInt(parts[2], &ok);
                l.type = static_cast<Structura::Shell::Load::Type>(toInt(parts[3], &ok));
                for (int i = 0; i < 4; ++i) {
                    l.v[static_cast<std::size_t>(i)] = toDouble(parts[4 + i], &ok);
                }
                shellTmp.loads.push_back(l);
            } else if (key == QLatin1String("SUPPORT") && parts.size() >= 24) {
                Structura::Shell::Support s;
                s.object = toInt(parts[1], &ok);
                s.selector = static_cast<Structura::Shell::Support::Selector>(toInt(parts[2], &ok));
                s.axis = toInt(parts[3], &ok);
                s.value = toDouble(parts[4], &ok);
                s.value2 = toDouble(parts[5], &ok);
                s.value3 = toDouble(parts[6], &ok);
                s.tol = toDouble(parts[7], &ok);
                s.legs = toInt(parts[8], &ok);
                s.legStart = toDouble(parts[9], &ok);
                s.limitAxis = toInt(parts[10], &ok);
                s.limitValue = toDouble(parts[11], &ok);
                s.limitBelow = toInt(parts[12], &ok) != 0;
                for (int i = 0; i < 6; ++i) {
                    s.fixed[static_cast<std::size_t>(i)] = toInt(parts[13 + i], &ok);
                }
                s.springPerArea = toDouble(parts[19], &ok);
                s.compressionOnly = toInt(parts[20], &ok) != 0;
                for (int i = 0; i < 3; ++i) {
                    s.springNormal[static_cast<std::size_t>(i)] = toDouble(parts[21 + i], &ok);
                }
                shellTmp.supports.push_back(s);
            }
            break;
        }
        case Section::CompositeFloor: {
            const QString key = parts[0].toUpper();
            if (key == QLatin1String("BEAM") && parts.size() >= 4) {
                compositeBeamsTmp[toInt(parts[1], &ok)].insert(parts[2], toDouble(parts[3], &ok));
            } else if (key == QLatin1String("DECK") && parts.size() >= 3) {
                deckSlabTmp.insert(parts[1], toDouble(parts[2], &ok));
            }
            break;
        }
        case Section::SheetLayouts: {
            const QString key = parts[0].toUpper();
            if (key == QLatin1String("LAYOUT")) {
                Structura::App::ProjectData::SheetLayoutDef layout;
                layout.name = parts.size() > 1 && parts[1] != QLatin1String("-") ? decodeName(parts[1]) : QString();
                sheetLayoutsTmp.append(layout);
            } else if (key == QLatin1String("VIEW") && parts.size() >= 8 && !sheetLayoutsTmp.isEmpty()) {
                Structura::App::ProjectData::SheetLayoutDef::Placement p;
                p.viewId = decodeName(parts[1]);
                p.scale = toDouble(parts[2], &ok);
                p.manual = toInt(parts[3], &ok) != 0;
                p.x = toDouble(parts[4], &ok);
                p.y = toDouble(parts[5], &ok);
                p.w = toDouble(parts[6], &ok);
                p.h = toDouble(parts[7], &ok);
                sheetLayoutsTmp.last().views.append(p);
            }
            break;
        }
        case Section::TitleBlock: {
            const QString key = parts[0].toUpper();
            auto text = [&](int index) { return parts.size() > index && parts[index] != QLatin1String("-") ? decodeName(parts[index]) : QString(); };
            auto &tb = titleBlockTmp;
            if (key == QLatin1String("COMPANY")) tb.company = text(1);
            else if (key == QLatin1String("CLIENT")) tb.client = text(1);
            else if (key == QLatin1String("SITE")) tb.site = text(1);
            else if (key == QLatin1String("PROJECT")) tb.project = text(1);
            else if (key == QLatin1String("AUTHOR")) tb.author = text(1);
            else if (key == QLatin1String("REGISTRATION")) tb.registration = text(1);
            else if (key == QLatin1String("DRAFTER")) tb.drafter = text(1);
            else if (key == QLatin1String("CHECKER")) tb.checker = text(1);
            else if (key == QLatin1String("DATE")) tb.date = text(1);
            else if (key == QLatin1String("REVISION")) tb.revision = text(1);
            else if (key == QLatin1String("PREFIX")) tb.drawingPrefix = text(1);
            else if (key == QLatin1String("NOTES")) tb.notes = text(1);
            else if (key == QLatin1String("REV") && parts.size() >= 5) tb.revisions.append({text(1), text(2), text(3), text(4)});
            else if (key == QLatin1String("SHEET") && parts.size() >= 3) tb.sheetTitles.insert(text(1), text(2));
            break;
        }
        case Section::Connections: {
            if (parts.size() < 9) {
                break;
            }
            Structura::App::ProjectData::NodeConnection c;
            c.id = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            c.type = toInt(parts[1], &ok);
            c.nodeId = toInt(parts[2], &ok);
            c.memberId = toInt(parts[3], &ok);
            c.atEnd = toInt(parts[4], &ok) != 0;
            c.eta = toDouble(parts[5], &ok);
            c.verified = toInt(parts[6], &ok) != 0;
            c.grade = decodeName(parts[7]);
            c.governing = parts[8] == QLatin1String("-") ? QString() : decodeName(parts[8]);
            for (int i = 9; i < parts.size(); ++i) {
                const int eq = parts[i].indexOf(QLatin1Char('='));
                if (eq > 0) {
                    c.values.insert(parts[i].left(eq), toDouble(parts[i].mid(eq + 1), &ok));
                }
            }
            connectionsTmp.append(c);
            break;
        }
        case Section::DesignResults: {
            if (parts.size() < 6) {
                break;
            }
            Structura::App::ProjectData::StoredDesignResult d;
            d.memberId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            d.section = decodeName(parts[1]);
            d.eta = toDouble(parts[2], &ok);
            d.check = decodeName(parts[3]);
            d.combination = decodeName(parts[4]);
            d.lengthM = toDouble(parts[5], &ok);
            resultsTmp.design.append(d);
            break;
        }
        case Section::BucklingResults: {
            const QString key = parts[0].toUpper();
            if (key == QLatin1String("COMBINATION") && parts.size() >= 2) {
                resultsTmp.bucklingCombination = decodeName(parts[1]);
            } else if (key == QLatin1String("FACTOR") && parts.size() >= 2) {
                resultsTmp.bucklingFactors.append(toDouble(parts[1], &ok));
            }
            break;
        }
        case Section::ModalResults: {
            const QString key = parts[0].toUpper();
            if (key == QLatin1String("SOURCE") && parts.size() >= 2) {
                resultsTmp.modalSource = decodeName(parts[1]);
            } else if (key == QLatin1String("MODE") && parts.size() >= 5) {
                resultsTmp.modalOmega.append(toDouble(parts[1], &ok));
                resultsTmp.modalMassRatio.append({toDouble(parts[2], &ok), toDouble(parts[3], &ok), toDouble(parts[4], &ok)});
            }
            break;
        }
        case Section::Groups: {
            if (parts.size() < 3) {
                break;
            }
            Structura::App::ProjectData::BarGroup group;
            group.id = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            group.name = parts[1] == QLatin1String("-") ? QString() : QString(parts[1]).replace(QLatin1Char('_'), QLatin1Char(' '));
            for (int i = 2; i < parts.size(); ++i) {
                const int member = toInt(parts[i], &ok);
                if (ok) {
                    group.members.append(member);
                }
            }
            groupsTmp.append(group);
            break;
        }
        case Section::DesignSettings: {
            if (parts.size() < 2) {
                break;
            }
            const QString key = parts[0].toUpper();
            const QString text = QStringList(parts.mid(1)).join(QLatin1Char(' '));
            auto name = [&]() { return QString(text).replace(QLatin1Char('_'), QLatin1Char(' ')); };
            if (key == QLatin1String("DEFAULT_STEEL")) {
                designTmp.defaultSteel = name();
            } else if (key == QLatin1String("RECT_TUBE_STANDARD")) {
                designTmp.rectTubeStandard = text.toUpper();
            } else if (key == QLatin1String("AMPLIFY_B1")) {
                designTmp.amplifyB1 = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("SLENDERNESS")) {
                designTmp.slendernessGoverns = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("SECOND_ORDER")) {
                designTmp.secondOrder = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("STIFFNESS_REDUCTION")) {
                designTmp.stiffnessReduction = toDouble(parts[1], &ok);
            } else if (key == QLatin1String("AUTO_B2")) {
                designTmp.autoB2 = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("RS")) {
                designTmp.rs = toDouble(parts[1], &ok);
            } else if (key == QLatin1String("SINGLE_STOREY")) {
                designTmp.singleStorey = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("NOTIONAL_LOADS")) {
                designTmp.notionalLoads = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("NOTIONAL_FACTOR")) {
                designTmp.notionalFactor = toDouble(parts[1], &ok);
            } else if (key == QLatin1String("GROUP_BY_FAMILY")) {
                designTmp.groupByFamily = toInt(parts[1], &ok) != 0;
            } else if (key == QLatin1String("TARGET_ETA")) {
                designTmp.targetEta = toDouble(parts[1], &ok);
            } else if (key == QLatin1String("COMBINATION")) {
                designTmp.combinations.append(name());
            } else if (key == QLatin1String("DEFAULTS") && parts.size() >= 10) {
                auto &d = designTmp.defaults;
                d.kx = toDouble(parts[1], &ok);
                d.ky = toDouble(parts[2], &ok);
                d.kz = toDouble(parts[3], &ok);
                d.segments = std::max(1, toInt(parts[4], &ok));
                d.lb = toDouble(parts[5], &ok);
                d.braced = toInt(parts[6], &ok) != 0;
                d.cbOne = toInt(parts[7], &ok) != 0;
                d.b2 = toDouble(parts[8], &ok);
                d.netAreaRatio = toDouble(parts[9], &ok);
                if (parts.size() >= 13) {
                    d.lx = toDouble(parts[10], &ok);
                    d.ly = toDouble(parts[11], &ok);
                    d.lz = toDouble(parts[12], &ok);
                }
                if (parts.size() >= 14) {
                    d.stiffenerSpacing = toDouble(parts[13], &ok);
                }
                if (parts.size() >= 15) {
                    d.panelTensionFlange = toInt(parts[14], &ok);
                }
                if (parts.size() >= 16) {
                    d.coldWork = toInt(parts[15], &ok) != 0;
                }
                if (parts.size() >= 17) {
                    d.bearingLength = toDouble(parts[16], &ok);
                }
                if (parts.size() >= 24) {
                    d.fatigueCategory = toInt(parts[17], &ok);
                    d.fatigueCycles = toDouble(parts[18], &ok);
                    d.openingDiameter = toDouble(parts[19], &ok);
                    d.openingPosition = toDouble(parts[20], &ok);
                    d.openingOffset = toDouble(parts[21], &ok);
                    d.openingNeighbourD = toDouble(parts[22], &ok);
                    d.openingSpacing = toDouble(parts[23], &ok);
                }
            }
            break;
        }
        case Section::MemberDesign: {
            if (parts.size() < 11) {
                break;
            }
            Structura::App::ProjectData::DesignMemberParams p;
            p.memberId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            p.steel = parts[1] == QLatin1String("-") ? QString() : QString(parts[1]).replace(QLatin1Char('_'), QLatin1Char(' '));
            p.kx = toDouble(parts[2], &ok);
            p.ky = toDouble(parts[3], &ok);
            p.kz = toDouble(parts[4], &ok);
            p.segments = std::max(1, toInt(parts[5], &ok));
            p.lb = toDouble(parts[6], &ok);
            p.braced = toInt(parts[7], &ok) != 0;
            p.cbOne = toInt(parts[8], &ok) != 0;
            p.b2 = toDouble(parts[9], &ok);
            p.netAreaRatio = toDouble(parts[10], &ok);
            if (parts.size() >= 12) {
                p.deflDenominator = toInt(parts[11], &ok);
            }
            if (parts.size() >= 15) {
                p.lx = toDouble(parts[12], &ok);
                p.ly = toDouble(parts[13], &ok);
                p.lz = toDouble(parts[14], &ok);
            }
            if (parts.size() >= 16) {
                p.stiffenerSpacing = toDouble(parts[15], &ok);
            }
            if (parts.size() >= 17) {
                p.panelTensionFlange = toInt(parts[16], &ok);
            }
            if (parts.size() >= 18) {
                p.coldWork = toInt(parts[17], &ok) != 0;
            }
            if (parts.size() >= 19) {
                p.bearingLength = toDouble(parts[18], &ok);
            }
            if (parts.size() >= 26) {
                p.fatigueCategory = toInt(parts[19], &ok);
                p.fatigueCycles = toDouble(parts[20], &ok);
                p.openingDiameter = toDouble(parts[21], &ok);
                p.openingPosition = toDouble(parts[22], &ok);
                p.openingOffset = toDouble(parts[23], &ok);
                p.openingNeighbourD = toDouble(parts[24], &ok);
                p.openingSpacing = toDouble(parts[25], &ok);
            }
            designTmp.members.append(p);
            break;
        }
        case Section::Placements: {
            if (parts.size() < 5) {
                break;
            }
            Structura::App::ProjectData::MemberPlacement placement;
            placement.memberId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            placement.rotationDeg = toDouble(parts[1], &ok);
            placement.reference = std::clamp(toInt(parts[2], &ok), 0, 8);
            placement.offsetA = toDouble(parts[3], &ok);
            placement.offsetB = toDouble(parts[4], &ok);
            if (parts.size() >= 8) {
                placement.endDiffers = toInt(parts[5], &ok) != 0;
                placement.offsetA2 = toDouble(parts[6], &ok);
                placement.offsetB2 = toDouble(parts[7], &ok);
            }
            if (parts.size() >= 9) {
                placement.releaseMask = std::clamp(toInt(parts[8], &ok), 0, 4095);
            }
            if (parts.size() >= 10) {
                placement.memberType = std::clamp(toInt(parts[9], &ok), 0, 2);
            }
            if (!placement.isDefault()) {
                placementsTmp.append(placement);
            }
            break;
        }
        case Section::MemberMoments: {
            if (parts.size() < 5) {
                break;
            }
            Structura::App::ProjectData::MemberMoment moment;
            moment.memberId = toInt(parts[0], &ok);
            if (!ok) {
                break;
            }
            moment.system = parts[1].toUpper();
            moment.mx = toDouble(parts[2], &ok);
            moment.my = toDouble(parts[3], &ok);
            moment.mz = toDouble(parts[4], &ok);
            if (parts.size() > 5) {
                moment.loadCase = parts[5];
            }
            memberMomentsTmp.append(moment);
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
            if (parts.size() > 7) {
                load.loadCase = parts[7];
            }
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
            if (parts.size() > 5) {
                load.loadCase = parts[5];
            }
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
    for (SectionInfo &sec : sections) {
        const auto record = sectionGeometryTmp.constFind(sec.externalId);
        if (record != sectionGeometryTmp.constEnd()) {
            sec.family = record->family;
            sec.mirrored = record->mirrored;
            sec.geometry = record->parameters;
        }
    }
    supports = supportsTmp;
    nodalLoads = nodalLoadsTmp;
    memberLoads = memberLoadsTmp;
    extras.memberMoments = memberMomentsTmp;
    extras.placements = placementsTmp;
    std::sort(axesTmp.levels.begin(), axesTmp.levels.end(),
              [](const Structura::App::ProjectData::AxisLevel &a, const Structura::App::ProjectData::AxisLevel &b) {
                  return a.elevation < b.elevation;
              });
    extras.axes = axesTmp;
    extras.viewOptions = viewOptionsTmp;
    extras.design = designTmp;
    extras.groups = groupsTmp;
    extras.areaLoads = areaLoadsTmp;
    extras.results = resultsTmp;
    extras.connections = connectionsTmp;
    extras.titleBlock = titleBlockTmp;
    extras.views3d = views3dTmp;
    extras.sheetLayouts = sheetLayoutsTmp;
    extras.compositeBeams = compositeBeamsTmp;
    extras.deckSlab = deckSlabTmp;
    if (shellTmp.materials.empty()) {
        shellTmp.materials.push_back(Structura::Shell::Material {});
    }
    extras.shell = shellTmp;

    // Projetos antigos nao tem casos: cria o caso padrao e atribui as cargas a ele.
    extras.loadCases = loadCasesTmp;
    extras.combinations = combinationsTmp;
    if (extras.loadCases.isEmpty()) {
        extras.loadCases.append({Structura::App::ProjectData::defaultLoadCaseName(),
                                 Structura::App::ProjectData::defaultLoadCaseName(), false});
    }
    const QString fallbackCase = extras.loadCases.first().name;
    for (auto &load : nodalLoads) {
        if (load.loadCase.isEmpty()) {
            load.loadCase = fallbackCase;
        }
    }
    for (auto &load : memberLoads) {
        if (load.loadCase.isEmpty()) {
            load.loadCase = fallbackCase;
        }
    }
    for (auto &moment : extras.memberMoments) {
        if (moment.loadCase.isEmpty()) {
            moment.loadCase = fallbackCase;
        }
    }

    for (MaterialInfo &mat : materials) {
        mat.unitWeight = unitWeightByMaterial.value(mat.externalId, 0.0);
        const auto extraIt = materialExtras.constFind(mat.externalId);
        if (extraIt != materialExtras.constEnd()) {
            if (!extraIt->name.isEmpty()) {
                mat.name = extraIt->name;
            }
            mat.library = extraIt->library;
            mat.fy = extraIt->fy;
            mat.fu = extraIt->fu;
        }
    }

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
                             const ProjectExtras &extras,
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

    bool anyUnitWeight = false;
    for (const auto &mat : materialsSorted) {
        anyUnitWeight = anyUnitWeight || mat.unitWeight != 0.0 || !mat.library.isEmpty() || mat.fy != 0.0;
    }
    if (anyUnitWeight) {
        stream << "[MATERIAL_PROPS]\n";
        stream << "# ID    UnitWeight (kN/m^3)  Nome  Biblioteca  fy(MPa)  fu(MPa)   ('-' = vazio, '_' = espaco)\n";
        auto token = [](QString text) { return text.isEmpty() ? QStringLiteral("-") : text.replace(QLatin1Char(' '), QLatin1Char('_')); };
        for (const auto &mat : materialsSorted) {
            stream << QString::asprintf("%-8d %14.6e ", mat.externalId, mat.unitWeight) << token(mat.name) << " " << token(mat.library)
                   << QString::asprintf(" %g %g\n", mat.fy, mat.fu);
        }
        stream << "\n";
    }

    stream << "[SECTIONS]\n";
    stream << "# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)\n";
    for (const auto &sec : sectionsSorted) {
        stream << QString::asprintf("%-8d %14.6e %14.6e %14.6e %14.6e\n", sec.externalId, sec.area, sec.iz, sec.iy, sec.j);
    }
    stream << "\n";

    bool anySectionGeometry = false;
    for (const auto &sec : sectionsSorted) {
        anySectionGeometry = anySectionGeometry || !sec.family.isEmpty();
    }
    if (anySectionGeometry) {
        stream << "[SECTION_GEOMETRY]\n";
        stream << "# ID    Familia(_ no lugar de espaco)  Espelhada(0/1)  parametro=valor (mm) ...\n";
        for (const auto &sec : sectionsSorted) {
            if (sec.family.isEmpty()) {
                continue;
            }
            QString family = sec.family;
            family.replace(QLatin1Char(' '), QLatin1Char('_'));
            stream << QString::asprintf("%-8d %s %d", sec.externalId, family.toUtf8().constData(), sec.mirrored ? 1 : 0);
            for (auto it = sec.geometry.constBegin(); it != sec.geometry.constEnd(); ++it) {
                stream << ' ' << it.key() << QString::asprintf("=%g", it.value());
            }
            stream << "\n";
        }
        stream << "\n";
    }

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

    const QString defaultCase = extras.loadCases.isEmpty() ? Structura::App::ProjectData::defaultLoadCaseName()
                                                           : extras.loadCases.first().name;
    auto caseToken = [&defaultCase](const QString &name) {
        return (name.isEmpty() ? defaultCase : name).toLatin1();
    };

    if (!extras.loadCases.isEmpty()) {
        stream << "[LOAD_CASES]\n";
        stream << "# Nome  Sigla  PesoProprio(0/1)\n";
        for (const auto &loadCase : extras.loadCases) {
            stream << QString::asprintf("%-16s %-8s %d\n",
                                        loadCase.name.toLatin1().constData(),
                                        (loadCase.abbreviation.isEmpty() ? loadCase.name : loadCase.abbreviation).toLatin1().constData(),
                                        loadCase.selfWeight ? 1 : 0);
        }
        stream << "\n";
    }
    if (!extras.combinations.isEmpty()) {
        stream << "[COMBINATIONS]\n";
        stream << "# Nome  caso=fator ...\n";
        for (const auto &combination : extras.combinations) {
            stream << combination.name;
            for (const auto &term : combination.factors) {
                stream << ' ' << term.first << QString::asprintf("=%g", term.second);
            }
            stream << "\n";
        }
        stream << "\n";
    }

    stream << "[NODAL_LOADS]\n";
    stream << "# Node_ID   Fx (N)    Fy (N)   Fz (N)   Mx (Nm)   My (Nm)   Mz (Nm)   Caso\n";
    for (const auto &load : nodalLoads) {
        stream << QString::asprintf("%-8d %10.6f %10.6f %10.6f %10.6f %10.6f %10.6f %s\n",
                                    load.nodeId, load.fx, load.fy, load.fz, load.mx, load.my, load.mz,
                                    caseToken(load.loadCase).constData());
    }
    stream << "\n";

    stream << "[MEMBER_LOADS]\n";
    stream << "# Formato: Member_ID  Sistema(Local/Global)  qx (N/m)  qy (N/m)  qz (N/m)  Caso\n";
    stream << "# Sistemas aceitos: LOCAL, GLOBAL (ou L/G)\n";
    for (const auto &load : memberLoads) {
        const QByteArray systemBytes = load.system.isEmpty() ? QByteArray("LOCAL") : load.system.toUpper().toLatin1();
        stream << QString::asprintf("%-12d %-10s %12.6f %12.6f %12.6f %s\n",
                                    load.memberId,
                                    systemBytes.constData(),
                                    load.qx, load.qy, load.qz,
                                    caseToken(load.loadCase).constData());
    }

    if (!extras.memberMoments.isEmpty()) {
        stream << "\n[MEMBER_MOMENTS]\n";
        stream << "# Member_ID  Sistema(Local/Global)  mx (N*m/m)  my (N*m/m)  mz (N*m/m)  Caso\n";
        for (const auto &moment : extras.memberMoments) {
            const QByteArray systemBytes = moment.system.isEmpty() ? QByteArray("LOCAL") : moment.system.toUpper().toLatin1();
            stream << QString::asprintf("%-12d %-10s %12.6f %12.6f %12.6f %s\n",
                                        moment.memberId, systemBytes.constData(),
                                        moment.mx, moment.my, moment.mz,
                                        caseToken(moment.loadCase).constData());
        }
    }

    if (!extras.placements.isEmpty()) {
        stream << "\n[MEMBER_PLACEMENT]\n";
        stream << "# Member_ID  Giro(graus)  Ponto_ref(0-8)  Desloc_z_I(m)  Desloc_y_I(m)  J_diferente(0/1)  Desloc_z_J(m)  Desloc_y_J(m)  Liberacoes(mascara: bit0-5 ux..rz em I, bit6-11 em J)  Tipo(0=normal,1=trelica,2=tirante)\n";
        for (const auto &placement : extras.placements) {
            if (placement.isDefault()) {
                continue;
            }
            stream << QString::asprintf("%-12d %12.6f %6d %14.6f %14.6f %4d %14.6f %14.6f %6d %3d\n",
                                        placement.memberId, placement.rotationDeg, placement.reference,
                                        placement.offsetA, placement.offsetB, placement.endDiffers ? 1 : 0,
                                        placement.offsetA2, placement.offsetB2, placement.releaseMask, placement.memberType);
        }
    }

    if (!extras.axes.isEmpty()) {
        stream << "\n[AXES]\n";
        stream << "# LEVEL nome elevacao (m): abre um plano de eixos; X/Y rotulo coordenada (m) pertencem ao ultimo LEVEL\n";
        for (const auto &level : extras.axes.levels) {
            stream << QString::asprintf("LEVEL %s %12.3f\n", level.name.toLatin1().constData(), level.elevation);
            for (const auto &entry : level.x) {
                stream << QString::asprintf("X %-8s %12.3f\n", entry.label.toLatin1().constData(), entry.value);
            }
            for (const auto &entry : level.y) {
                stream << QString::asprintf("Y %-8s %12.3f\n", entry.label.toLatin1().constData(), entry.value);
            }
        }
    }

    if (!extras.areaLoads.isEmpty()) {
        stream << "\n[AREA_LOADS]\n";
        stream << "# ID Caso('_'=espaco) Tipo(AREA/WIND) Pressao(kN/m2) dirX dirY dirZ nPontos x y z ... nBarras barras...\n";
        for (const auto &panel : extras.areaLoads) {
            stream << QString::asprintf("%-4d ", panel.id) << (panel.loadCase.isEmpty() ? QStringLiteral("-") : QString(panel.loadCase).replace(QLatin1Char(' '), QLatin1Char('_')))
                   << ' ' << panel.kind
                   << QString::asprintf(" %.6g %.6g %.6g %.6g %d", panel.pressure, panel.direction[0], panel.direction[1], panel.direction[2], static_cast<int>(panel.polygon.size()));
            for (const auto &p : panel.polygon) {
                stream << QString::asprintf(" %.6f %.6f %.6f", p[0], p[1], p[2]);
            }
            stream << ' ' << panel.members.size();
            for (const int member : panel.members) {
                stream << ' ' << member;
            }
            stream << "\n";
        }
    }

    if (!extras.groups.isEmpty()) {
        stream << "\n[GROUPS]\n";
        stream << "# ID  Nome('-'=vazio, '_'=espaco)  barras...\n";
        for (const auto &group : extras.groups) {
            stream << QString::asprintf("%-6d ", group.id)
                   << (group.name.isEmpty() ? QStringLiteral("-") : QString(group.name).replace(QLatin1Char(' '), QLatin1Char('_')));
            for (const int member : group.members) {
                stream << ' ' << member;
            }
            stream << "\n";
        }
    }

    if (!extras.design.isDefault()) {
        const auto &ds = extras.design;
        auto token = [](QString text) { return text.isEmpty() ? QStringLiteral("-") : text.replace(QLatin1Char(' '), QLatin1Char('_')); };
        stream << "\n[DESIGN_SETTINGS]\n";
        stream << "# DEFAULT_STEEL nome | RECT_TUBE_STANDARD NBR14762/NBR8800 | AMPLIFY_B1 0/1 | SLENDERNESS 0/1 | COMBINATION nome\n";
        stream << "# DEFAULTS kx ky kz trechos Lb(m) mesa_contida Cb=1 B2 An/Ag Lx Ly Lz (m; 0 = usa K.L)\n";
        stream << "DEFAULT_STEEL " << token(ds.defaultSteel) << "\n";
        stream << "RECT_TUBE_STANDARD " << ds.rectTubeStandard << "\n";
        stream << "AMPLIFY_B1 " << (ds.amplifyB1 ? 1 : 0) << "\n";
        stream << "SLENDERNESS " << (ds.slendernessGoverns ? 1 : 0) << "\n";
        stream << "SECOND_ORDER " << (ds.secondOrder ? 1 : 0) << "\n";
        stream << "STIFFNESS_REDUCTION " << ds.stiffnessReduction << "\n";
        stream << "AUTO_B2 " << (ds.autoB2 ? 1 : 0) << "\n";
        stream << "RS " << ds.rs << "\n";
        stream << "SINGLE_STOREY " << (ds.singleStorey ? 1 : 0) << "\n";
        stream << "NOTIONAL_LOADS " << (ds.notionalLoads ? 1 : 0) << "\n";
        stream << "NOTIONAL_FACTOR " << ds.notionalFactor << "\n";
        stream << "GROUP_BY_FAMILY " << (ds.groupByFamily ? 1 : 0) << "\n";
        stream << "TARGET_ETA " << ds.targetEta << "\n";
        for (const QString &name : ds.combinations) {
            stream << "COMBINATION " << token(name) << "\n";
        }
        stream << QString::asprintf("DEFAULTS %g %g %g %d %g %d %d %g %g %g %g %g %g %d %d %g %d %g %g %g %g %g %g\n", ds.defaults.kx, ds.defaults.ky, ds.defaults.kz,
                                    ds.defaults.segments, ds.defaults.lb, ds.defaults.braced ? 1 : 0, ds.defaults.cbOne ? 1 : 0,
                                    ds.defaults.b2, ds.defaults.netAreaRatio, ds.defaults.lx, ds.defaults.ly, ds.defaults.lz, ds.defaults.stiffenerSpacing,
                                    ds.defaults.panelTensionFlange, ds.defaults.coldWork ? 1 : 0, ds.defaults.bearingLength, ds.defaults.fatigueCategory, ds.defaults.fatigueCycles,
                                    ds.defaults.openingDiameter, ds.defaults.openingPosition, ds.defaults.openingOffset, ds.defaults.openingNeighbourD,
                                    ds.defaults.openingSpacing);
        if (!ds.members.isEmpty()) {
            stream << "\n[MEMBER_DESIGN]\n";
            stream << "# Member_ID  Aco(-=padrao)  kx ky kz  trechos  Lb(m)  mesa_contida  Cb=1  B2  An/Ag  flecha_L/n(0=auto)  Lx Ly Lz (m; 0 = usa K.L)\n";
            for (const auto &p : ds.members) {
                stream << QString::asprintf("%-10d ", p.memberId) << token(p.steel)
                       << QString::asprintf(" %g %g %g %d %g %d %d %g %g %d %g %g %g %g %d %d %g %d %g %g %g %g %g %g\n", p.kx, p.ky, p.kz, p.segments, p.lb,
                                            p.braced ? 1 : 0, p.cbOne ? 1 : 0, p.b2, p.netAreaRatio, p.deflDenominator, p.lx, p.ly, p.lz, p.stiffenerSpacing, p.panelTensionFlange, p.coldWork ? 1 : 0, p.bearingLength, p.fatigueCategory, p.fatigueCycles, p.openingDiameter,
                                            p.openingPosition, p.openingOffset, p.openingNeighbourD, p.openingSpacing);
            }
        }
    }

    if (!extras.views3d.isEmpty()) {
        stream << "\n[VIEWS_3D]\n";
        stream << "# nome perspectiva(0/1) azimute elevacao fov camera_explicita(0/1) pos(x y z, m) alvo(x y z) cima(x y z)\n";
        for (const auto &v : extras.views3d) {
            stream << QString::fromLatin1(QUrl::toPercentEncoding(v.name.isEmpty() ? QStringLiteral("-") : v.name)) << ' ' << (v.perspective ? 1 : 0) << ' ' << QString::number(v.azimuthDeg, 'g', 8)
                   << ' ' << QString::number(v.elevationDeg, 'g', 8) << ' ' << QString::number(v.fovDeg, 'g', 8) << ' ' << (v.explicitCamera ? 1 : 0);
            for (const auto &arr : {v.position, v.target, v.up}) {
                for (const double c : arr) {
                    stream << ' ' << QString::number(c, 'g', 10);
                }
            }
            stream << "\n";
        }
    }
    if (!extras.shell.objects.empty()) {
        auto enc = [](const std::string &s) { return QString::fromLatin1(QUrl::toPercentEncoding(s.empty() ? QStringLiteral("-") : QString::fromStdString(s))); };
        auto num = [](double v) { return QString::number(v, 'g', 12); };
        const auto &sm = extras.shell;
        stream << "\n[SHELL_MODEL]\n";
        stream << "# MAT idx nome concreto E nu pesoEsp alfa fy fu | OBJ id nome tipo material malha visivel | PARAM id chave valor | POLY id x y z\n";
        if (sm.coupled) {
            stream << "COUPLED 1\n";
        }
        stream << "# LOAD caso objeto tipo v0 v1 v2 v3 | SUPPORT objeto seletor eixo v v2 v3 tol pernas inicio limEixo limValor limAbaixo f1..f6 mola so_compressao nx ny nz\n";
        for (std::size_t i = 0; i < sm.materials.size(); ++i) {
            const auto &m = sm.materials[i];
            stream << "MAT " << i << ' ' << enc(m.name) << ' ' << (m.concrete ? 1 : 0) << ' ' << num(m.E) << ' ' << num(m.nu) << ' ' << num(m.unitWeight) << ' ' << num(m.alpha)
                   << ' ' << num(m.fy) << ' ' << num(m.fu) << "\n";
        }
        for (const auto &o : sm.objects) {
            stream << "OBJ " << o.id << ' ' << enc(o.name) << ' ' << static_cast<int>(o.kind) << ' ' << o.material << ' ' << num(o.meshSize) << ' ' << (o.visible ? 1 : 0) << "\n";
            for (const auto &[key, value] : o.p) {
                stream << "PARAM " << o.id << ' ' << QString::fromStdString(key) << ' ' << num(value) << "\n";
            }
            for (const auto &p : o.polygon) {
                stream << "POLY " << o.id << ' ' << num(p[0]) << ' ' << num(p[1]) << ' ' << num(p[2]) << "\n";
            }
        }
        for (const auto &l : sm.loads) {
            stream << "LOAD " << enc(l.caseName) << ' ' << l.object << ' ' << static_cast<int>(l.type) << ' ' << num(l.v[0]) << ' ' << num(l.v[1]) << ' ' << num(l.v[2]) << ' ' << num(l.v[3]) << "\n";
        }
        for (const auto &s : sm.supports) {
            stream << "SUPPORT " << s.object << ' ' << static_cast<int>(s.selector) << ' ' << s.axis << ' ' << num(s.value) << ' ' << num(s.value2) << ' ' << num(s.value3) << ' ' << num(s.tol)
                   << ' ' << s.legs << ' ' << num(s.legStart) << ' ' << s.limitAxis << ' ' << num(s.limitValue) << ' ' << (s.limitBelow ? 1 : 0);
            for (const int f : s.fixed) {
                stream << ' ' << f;
            }
            stream << ' ' << num(s.springPerArea) << ' ' << (s.compressionOnly ? 1 : 0) << ' ' << num(s.springNormal[0]) << ' ' << num(s.springNormal[1]) << ' ' << num(s.springNormal[2]) << "\n";
        }
    }
    if (!extras.compositeBeams.isEmpty() || !extras.deckSlab.isEmpty()) {
        stream << "\n[COMPOSITE_FLOOR]\n";
        stream << "# BEAM id_da_barra chave valor  |  DECK chave valor (entradas das verificacoes de viga mista e laje mista)\n";
        for (auto it = extras.compositeBeams.constBegin(); it != extras.compositeBeams.constEnd(); ++it) {
            for (auto v = it.value().constBegin(); v != it.value().constEnd(); ++v) {
                stream << "BEAM " << it.key() << ' ' << v.key() << ' ' << QString::number(v.value(), 'g', 10) << "\n";
            }
        }
        for (auto v = extras.deckSlab.constBegin(); v != extras.deckSlab.constEnd(); ++v) {
            stream << "DECK " << v.key() << ' ' << QString::number(v.value(), 'g', 10) << "\n";
        }
    }
    if (!extras.sheetLayouts.isEmpty()) {
        stream << "\n[SHEET_LAYOUTS]\n";
        stream << "# LAYOUT nome  |  VIEW id_da_vista escala(0=auto) manual(0/1) x y largura altura (fracoes da area util)\n";
        for (const auto &layout : extras.sheetLayouts) {
            stream << "LAYOUT " << QString::fromLatin1(QUrl::toPercentEncoding(layout.name.isEmpty() ? QStringLiteral("-") : layout.name)) << "\n";
            for (const auto &p : layout.views) {
                stream << "VIEW " << QString::fromLatin1(QUrl::toPercentEncoding(p.viewId)) << ' ' << QString::number(p.scale, 'g', 8) << ' ' << (p.manual ? 1 : 0) << ' '
                       << QString::number(p.x, 'g', 6) << ' ' << QString::number(p.y, 'g', 6) << ' ' << QString::number(p.w, 'g', 6) << ' ' << QString::number(p.h, 'g', 6) << "\n";
            }
        }
    }
    if (!extras.titleBlock.isDefault()) {
        const auto &tb = extras.titleBlock;
        auto enc = [](const QString &text) { return QString::fromLatin1(QUrl::toPercentEncoding(text.isEmpty() ? QStringLiteral("-") : text)); };
        stream << "\n[TITLE_BLOCK]\n";
        stream << "# carimbo das pranchas (valores codificados; - = vazio)\n";
        stream << "COMPANY " << enc(tb.company) << "\nCLIENT " << enc(tb.client) << "\nSITE " << enc(tb.site) << "\nPROJECT " << enc(tb.project) << "\n";
        stream << "AUTHOR " << enc(tb.author) << "\nREGISTRATION " << enc(tb.registration) << "\nDRAFTER " << enc(tb.drafter) << "\nCHECKER " << enc(tb.checker) << "\n";
        stream << "DATE " << enc(tb.date) << "\nREVISION " << enc(tb.revision) << "\nPREFIX " << enc(tb.drawingPrefix) << "\nNOTES " << enc(tb.notes) << "\n";
        for (const auto &r : tb.revisions) {
            stream << "REV " << enc(r.rev) << ' ' << enc(r.date) << ' ' << enc(r.description) << ' ' << enc(r.author) << "\n";
        }
        for (auto it = tb.sheetTitles.constBegin(); it != tb.sheetTitles.constEnd(); ++it) {
            stream << "SHEET " << enc(it.key()) << ' ' << enc(it.value()) << "\n";
        }
    }
    if (!extras.connections.isEmpty()) {
        stream << "\n[CONNECTIONS]\n";
        stream << "# ID tipo(0=chapa de cisalhamento,1=cantoneira dupla) no viga extremidade_final(0/1) aproveitamento verificada(0/1) parafuso governante chave=valor...\n";
        for (const auto &c : extras.connections) {
            stream << c.id << ' ' << c.type << ' ' << c.nodeId << ' ' << c.memberId << ' ' << (c.atEnd ? 1 : 0) << ' ' << QString::number(c.eta, 'g', 8) << ' '
                   << (c.verified ? 1 : 0) << ' ' << QString::fromLatin1(QUrl::toPercentEncoding(c.grade.isEmpty() ? QStringLiteral("-") : c.grade)) << ' '
                   << (c.governing.isEmpty() ? QStringLiteral("-") : QString::fromLatin1(QUrl::toPercentEncoding(c.governing)));
            for (auto it = c.values.constBegin(); it != c.values.constEnd(); ++it) {
                stream << ' ' << it.key() << '=' << QString::number(it.value(), 'g', 10);
            }
            stream << "\n";
        }
    }
    const auto &results = extras.results;
    auto encode = [](const QString &text) { return QString::fromLatin1(QUrl::toPercentEncoding(text.isEmpty() ? QStringLiteral("-") : text)); };
    if (!results.runs.isEmpty()) {
        stream << "\n[ANALYSIS_RESULTS]\n";
        stream << "# FINGERPRINT: impressao digital do modelo analisado (resultados so sao restaurados se o modelo nao mudou)\n";
        stream << "# RUN caso | N no ux uy uz rx ry rz Rux Ruy Ruz Rrx Rry Rrz | M barra f1..f12(locais I,J) k1 k2 k3\n";
        stream << "FINGERPRINT " << results.fingerprint << "\n";
        for (const auto &run : results.runs) {
            stream << "RUN " << encode(run.name) << "\n";
            for (const auto &n : run.nodes) {
                stream << "N " << n.nodeId;
                for (const double v : n.displacements) {
                    stream << ' ' << QString::number(v, 'g', 12);
                }
                for (const double v : n.reactions) {
                    stream << ' ' << QString::number(v, 'g', 12);
                }
                stream << "\n";
            }
            for (const auto &m : run.members) {
                stream << "M " << m.memberId;
                for (const double v : m.forces) {
                    stream << ' ' << QString::number(v, 'g', 12);
                }
                for (const double v : m.auxK) {
                    stream << ' ' << QString::number(v, 'g', 12);
                }
                stream << "\n";
            }
            for (const auto &n : run.shellNodes) {
                stream << "SN " << n.nodeId;
                for (const double v : n.displacements) {
                    stream << ' ' << QString::number(v, 'g', 10);
                }
                for (const double v : n.reactions) {
                    stream << ' ' << QString::number(v, 'g', 10);
                }
                stream << "\n";
            }
            for (const auto &e : run.shellElements) {
                stream << "SE " << e.shellId;
                for (const double v : e.values) {
                    stream << ' ' << QString::number(v, 'g', 10);
                }
                for (const double v : e.axes) {
                    stream << ' ' << QString::number(v, 'g', 9);
                }
                stream << "\n";
            }
        }
    }
    if (!results.design.isEmpty()) {
        stream << "\n[DESIGN_RESULTS]\n";
        stream << "# Member_ID  Perfil  aproveitamento  verificacao_governante  combinacao  comprimento(m)\n";
        for (const auto &d : results.design) {
            stream << QString::asprintf("%-8d ", d.memberId) << encode(d.section) << ' ' << QString::number(d.eta, 'g', 8) << ' ' << encode(d.check)
                   << ' ' << encode(d.combination) << ' ' << QString::number(d.lengthM, 'g', 8) << "\n";
        }
    }
    if (!results.bucklingFactors.isEmpty()) {
        stream << "\n[BUCKLING_RESULTS]\n";
        stream << "COMBINATION " << encode(results.bucklingCombination) << "\n";
        for (const double f : results.bucklingFactors) {
            stream << "FACTOR " << QString::number(f, 'g', 10) << "\n";
        }
    }
    if (!results.modalOmega.isEmpty()) {
        stream << "\n[MODAL_RESULTS]\n";
        stream << "# MODE omega(rad/s) massa_efetiva_X massa_efetiva_Y massa_efetiva_Z\n";
        stream << "SOURCE " << encode(results.modalSource) << "\n";
        for (int i = 0; i < results.modalOmega.size(); ++i) {
            stream << "MODE " << QString::number(results.modalOmega[i], 'g', 10) << ' ' << QString::number(results.modalMassRatio[i][0], 'g', 8) << ' '
                   << QString::number(results.modalMassRatio[i][1], 'g', 8) << ' ' << QString::number(results.modalMassRatio[i][2], 'g', 8) << "\n";
        }
    }

    if (!extras.viewOptions.isEmpty()) {
        stream << "\n[VIEW_OPTIONS]\n";
        stream << "# Chave  Valor (booleanos: 0/1)\n";
        for (auto it = extras.viewOptions.constBegin(); it != extras.viewOptions.constEnd(); ++it) {
            stream << QString::asprintf("%-28s %g\n", it.key().toLatin1().constData(), it.value());
        }
    }

    file.close();
    return true;
}

} // namespace Structura::App::Services
