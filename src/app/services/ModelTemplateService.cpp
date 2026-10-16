#include "ModelTemplateService.h"

#include "app/ProjectDataTypes.h"
#include "viz/structure/SectionCatalog.h"
#include "viz/structure/SectionShapes.h"

#include <QMap>
#include <QTextStream>
#include <QVector>

#include <cmath>

namespace Structura::App::Services {

namespace {

using Structura::Visualization::SectionCatalog;
using Structura::Visualization::SectionShapes;

constexpr int kHinges = Structura::App::ProjectData::MemberType::kReleaseRotations;

struct SectionDef {
    QString name;
    QString family;
    SectionShapes::Parameters params;
    double area {0}, iz {0}, iy {0}, j {0};
};

struct Node {
    int id;
    double x, y, z;
    bool base;
};

struct Member {
    int id, i, j, section;
};

struct Placement {
    int member;
    double rotation;
    int reference;
    int mask;
    int type;
};

struct Builder {
    QVector<SectionDef> sections;
    QVector<Node> nodes;
    QVector<Member> members;
    QVector<Placement> placements;
    QMap<int, QString> memberGroup;   // membro -> grupo
    QVector<std::tuple<int, QString, double, double, double>> memberLoads;   // membro, caso, qx, qy, qz (GLOBAL)
    QString error;

    int addSection(const QString &family, const QString &name, bool *ok = nullptr)
    {
        for (int i = 0; i < sections.size(); ++i) {
            if (sections[i].name == name && sections[i].family == family) {
                return i + 1;
            }
        }
        SectionDef def;
        def.family = family;
        def.name = name;
        bool found = false;
        for (const auto &p : SectionCatalog::profiles(family)) {
            if (p.name == name) {
                def.params = p.parameters;
                found = true;
                break;
            }
        }
        if (!found) {
            error = QStringLiteral("Perfil \"%1\" não encontrado no catálogo (%2).").arg(name, family);
            if (ok) {
                *ok = false;
            }
            return 0;
        }
        const auto props = SectionShapes::properties(family, def.params);
        if (!props) {
            error = QStringLiteral("Não foi possível calcular as propriedades de \"%1\".").arg(name);
            if (ok) {
                *ok = false;
            }
            return 0;
        }
        def.area = props->area;
        def.iz = props->iz;
        def.iy = props->iy;
        def.j = props->j;
        sections.append(def);
        return sections.size();
    }

    int addCustomSection(const QString &family, const QString &name, const SectionShapes::Parameters &params)
    {
        for (int i = 0; i < sections.size(); ++i) {
            if (sections[i].name == name) {
                return i + 1;
            }
        }
        SectionDef def;
        def.family = family;
        def.name = name;
        def.params = params;
        const auto props = SectionShapes::properties(family, params);
        if (props) {
            def.area = props->area;
            def.iz = props->iz;
            def.iy = props->iy;
            def.j = props->j;
        }
        sections.append(def);
        return sections.size();
    }

    int node(double x, double y, double z, bool base)
    {
        const int id = nodes.size() + 1;
        nodes.append({id, x, y, z, base});
        return id;
    }

    int member(int i, int j, int section, const QString &group = QString())
    {
        const int id = members.size() + 1;
        members.append({id, i, j, section});
        if (!group.isEmpty()) {
            memberGroup.insert(id, group);
        }
        return id;
    }

    void place(int member, double rotation, int reference, int mask, int type)
    {
        placements.append({member, rotation, reference, mask, type});
    }

    QString text(const QString &title, const QStringList &cases, const QStringList &combinations) const
    {
        QString out;
        QTextStream s(&out);
        s << "# " << title << "\n# Unidades: kN, m (E e G em kN/m2). Gerado por Modelos prontos.\n\n";
        s << "[MATERIALS]\n# ID    E (kN/m2)       G (kN/m2)\n1        2.000e+08      7.700e+07\n\n";
        s << "[MATERIAL_PROPS]\n# ID  UnitWeight (kN/m^3)  Nome  Biblioteca  fy(MPa)  fu(MPa)\n1 7.850000e+01 MR250 MR250 250 400\n\n";
        s << "[SECTIONS]\n# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)\n";
        for (int i = 0; i < sections.size(); ++i) {
            s << QString::asprintf("%-8d %14.6e %14.6e %14.6e %14.6e\n", i + 1, sections[i].area, sections[i].iz, sections[i].iy, sections[i].j);
        }
        s << "\n[SECTION_GEOMETRY]\n# ID    Familia(_ no lugar de espaco)  Espelhada(0/1)  parametro=valor (mm) ...\n";
        for (int i = 0; i < sections.size(); ++i) {
            QStringList kv;
            for (auto it = sections[i].params.constBegin(); it != sections[i].params.constEnd(); ++it) {
                kv << QStringLiteral("%1=%2").arg(it.key()).arg(it.value(), 0, 'g', 8);
            }
            QString family = sections[i].family;
            s << QString::asprintf("%-8d ", i + 1) << family.replace(QLatin1Char(' '), QLatin1Char('_')) << " 0 " << kv.join(QLatin1Char(' ')) << "\n";
        }
        s << "\n# Secoes: ";
        for (int i = 0; i < sections.size(); ++i) {
            s << (i ? "; " : "") << i + 1 << " = " << sections[i].name;
        }
        s << "\n\n[NODES]\n# ID    X (m)    Y (m)    Z (m)    UX  UY  UZ  RX  RY  RZ\n";
        for (const Node &n : nodes) {
            s << QString::asprintf("%-8d %10.6f %10.6f %10.6f    %s\n", n.id, n.x, n.y, n.z, n.base ? "1   1   1   1   1   1" : "0   0   0   0   0   0");
        }
        s << "\n[MEMBERS]\n# ID    Node_i   Node_j   Material_ID   Section_ID\n";
        for (const Member &m : members) {
            s << QString::asprintf("%-8d %-8d %-8d 1   %d\n", m.id, m.i, m.j, m.section);
        }
        s << "\n[LOAD_CASES]\n# Nome  Abrev  PesoProprio(0/1)\n";
        for (const QString &c : cases) {
            s << c << "\n";
        }
        s << "\n[COMBINATIONS]\n# Nome  caso=fator ...\n";
        for (const QString &c : combinations) {
            s << c << "\n";
        }
        if (!memberLoads.isEmpty()) {
            s << "\n[MEMBER_LOADS]\n# Member_ID  Sistema  qx (kN/m)  qy (kN/m)  qz (kN/m)  Caso\n";
            for (const auto &l : memberLoads) {
                s << QString::asprintf("%-8d GLOBAL %.4f %.4f %.4f ", std::get<0>(l), std::get<2>(l), std::get<3>(l), std::get<4>(l)) << std::get<1>(l) << "\n";
            }
        }
        s << "\n[MEMBER_PLACEMENT]\n# Member_ID  Giro(graus)  Ponto_ref(0-8)  Desloc_z_I(m)  Desloc_y_I(m)  J_diferente(0/1)  Desloc_z_J(m)  Desloc_y_J(m)  Liberacoes  Tipo\n";
        for (const Placement &p : placements) {
            s << QString::asprintf("%-12d %12.6f %6d %14.6f %14.6f %4d %14.6f %14.6f %6d %3d\n", p.member, p.rotation, p.reference, 0.0, 0.0, 0, 0.0, 0.0, p.mask, p.type);
        }
        QMap<QString, QVector<int>> groups;
        for (auto it = memberGroup.constBegin(); it != memberGroup.constEnd(); ++it) {
            groups[it.value()].append(it.key());
        }
        if (!groups.isEmpty()) {
            s << "\n[GROUPS]\n# ID  Nome  barras...\n";
            int id = 1;
            for (auto it = groups.constBegin(); it != groups.constEnd(); ++it, ++id) {
                QString name = it.key();
                s << id << ' ' << name.replace(QLatin1Char(' '), QLatin1Char('_'));
                for (const int m : it.value()) {
                    s << ' ' << m;
                }
                s << "\n";
            }
        }
        s << "\n[VIEW_OPTIONS]\n# Chave  Valor\ngrid_spacing 1\nsolids 1\n";
        return out.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    }
};

} // namespace

QStringList ModelTemplateService::catalogNames(const QString &family)
{
    QStringList names;
    for (const auto &p : SectionCatalog::profiles(family)) {
        names << p.name;
    }
    return names;
}

QString ModelTemplateService::buildShed(const ShedParams &p, QString *error)
{
    Builder b;
    const int column = b.addSection(QStringLiteral("W Laminado"), p.columnProfile);
    const int rafter = b.addSection(QStringLiteral("W Laminado"), p.rafterProfile);
    SectionShapes::Parameters purlinGeometry;
    purlinGeometry.insert(QStringLiteral("d"), 200.0);
    purlinGeometry.insert(QStringLiteral("bf"), 75.0);
    purlinGeometry.insert(QStringLiteral("c"), 25.0);
    purlinGeometry.insert(QStringLiteral("t"), 2.65);
    const int purlin = b.addCustomSection(QStringLiteral("C Formado"), QStringLiteral("Ue 200x75x25 #2,65"), purlinGeometry);
    SectionShapes::Parameters tieGeometry;
    tieGeometry.insert(QStringLiteral("D"), 20.0);   // barra circular de 20 mm
    int tie = 0;
    if (p.roofBracing) {
        SectionShapes::Parameters round;
        round.insert(QStringLiteral("d"), 20.0);
        tie = b.addCustomSection(QStringLiteral("Barra Circular"), QStringLiteral("Tirante Ø20"), round);
    }
    if (!b.error.isEmpty()) {
        if (error) {
            *error = b.error;
        }
        return {};
    }
    const int n = std::max(2, p.panelsPerSlope);
    const double rise = p.span / 2.0 * p.slopePercent / 100.0;
    const double dx = p.span / 2.0 / n;
    QVector<QVector<int>> roofNodes(p.frames);   // por portico: indices 0..2n (x crescente)
    QVector<int> baseL(p.frames), baseR(p.frames);
    for (int f = 0; f < p.frames; ++f) {
        const double y = f * p.frameSpacing;
        baseL[f] = b.node(0.0, y, 0.0, true);
        baseR[f] = b.node(p.span, y, 0.0, true);
        for (int i = 0; i <= 2 * n; ++i) {
            const double x = i * dx;
            const double z = p.eaveHeight + (i <= n ? i * dx : (2 * n - i) * dx) * p.slopePercent / 100.0;
            roofNodes[f].append(b.node(x, y, z, false));
        }
    }
    (void)rise;
    // cargas: sobrecarga e permanente por area em planta nas tercas
    for (int f = 0; f < p.frames; ++f) {
        const int mL = b.member(baseL[f], roofNodes[f].first(), column, QStringLiteral("Pilares"));
        const int mR = b.member(baseR[f], roofNodes[f].last(), column, QStringLiteral("Pilares"));
        b.place(mL, 90.0, 0, p.pinnedBases ? 0 : 0, 0);
        b.place(mR, 90.0, 0, 0, 0);
        for (int i = 0; i < 2 * n; ++i) {
            b.member(roofNodes[f][i], roofNodes[f][i + 1], rafter, QStringLiteral("Vigas de cobertura"));
        }
    }
    if (p.pinnedBases) {
        // bases articuladas: libera as rotacoes dos pilares na base (extremidade I)
        for (const auto &m : b.members) {
            if (m.section == column) {
                b.place(m.id, 90.0, 0, Structura::App::ProjectData::MemberRelease::kStartRy | Structura::App::ProjectData::MemberRelease::kStartRz, 0);
            }
        }
        // (a primeira colocacao 90/0/0 acima e substituida pela articulada: remove duplicatas mantendo a ultima)
        QMap<int, Placement> last;
        for (const auto &pl : b.placements) {
            last[pl.member] = pl;
        }
        b.placements = last.values().toVector();
    }
    QVector<QVector<int>> purlinMembers(2 * n + 1);
    for (int i = 0; i <= 2 * n; ++i) {
        for (int f = 0; f + 1 < p.frames; ++f) {
            const int m = b.member(roofNodes[f][i], roofNodes[f + 1][i], purlin, QStringLiteral("Terças"));
            purlinMembers[i].append(m);
            // largura de influencia em planta: metade de cada trecho vizinho
            const double width = (i == 0 || i == 2 * n) ? dx / 2.0 : dx;
            const double dead = -p.roofDeadLoad * width;
            const double live = -p.roofLiveLoad * width;
            b.memberLoads.append({m, QStringLiteral("CP"), 0.0, 0.0, dead});
            b.memberLoads.append({m, QStringLiteral("SC"), 0.0, 0.0, live});
        }
    }
    if (p.roofBracing && p.frames >= 2) {
        const int first = 0;
        const int last = p.frames - 2;
        for (const int f : {first, last}) {
            for (int i = 0; i < 2 * n; ++i) {
                const int a = b.member(roofNodes[f][i], roofNodes[f + 1][i + 1], tie, QStringLiteral("Contraventamento"));
                const int c = b.member(roofNodes[f][i + 1], roofNodes[f + 1][i], tie, QStringLiteral("Contraventamento"));
                b.place(a, 0.0, 0, kHinges, Structura::App::ProjectData::MemberType::kTie);
                b.place(c, 0.0, 0, kHinges, Structura::App::ProjectData::MemberType::kTie);
            }
        }
    }
    const QString title = QStringLiteral("Galpao de duas aguas - vao %1 m, %2 porticos a cada %3 m, pilares %4 m, inclinacao %5%")
                              .arg(p.span).arg(p.frames).arg(p.frameSpacing).arg(p.eaveHeight).arg(p.slopePercent);
    return b.text(title, {QStringLiteral("PP PP 1"), QStringLiteral("CP CP 0"), QStringLiteral("SC SC 0")},
                  {QStringLiteral("ELU1 PP=1.25 CP=1.35 SC=1.5"), QStringLiteral("ELS1 PP=1.0 CP=1.0 SC=1.0")});
}

QString ModelTemplateService::buildBuilding(const BuildingParams &p, QString *error)
{
    Builder b;
    const int column = b.addSection(QStringLiteral("W Laminado"), p.columnProfile);
    const int beamX = b.addSection(QStringLiteral("W Laminado"), p.beamXProfile);
    const int beamY = b.addSection(QStringLiteral("W Laminado"), p.beamYProfile);
    SectionShapes::Parameters round;
    round.insert(QStringLiteral("d"), 20.0);
    const int tie = p.bracing ? b.addCustomSection(QStringLiteral("Barra Circular"), QStringLiteral("Tirante Ø20"), round) : 0;
    if (!b.error.isEmpty()) {
        if (error) {
            *error = b.error;
        }
        return {};
    }
    const int nx = p.baysX + 1;
    const int ny = p.baysY + 1;
    const int nz = p.stories + 1;
    auto nodeId = [&](int ix, int iy, int iz) { return 1 + ix + nx * iy + nx * ny * iz; };
    for (int iz = 0; iz < nz; ++iz) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int ix = 0; ix < nx; ++ix) {
                b.node(ix * p.bayX, iy * p.bayY, iz * p.storyHeight, iz == 0);
            }
        }
    }
    for (int iz = 0; iz + 1 < nz; ++iz) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int ix = 0; ix < nx; ++ix) {
                const int m = b.member(nodeId(ix, iy, iz), nodeId(ix, iy, iz + 1), column, QStringLiteral("Pilares"));
                b.place(m, 90.0, 0, 0, 0);
            }
        }
    }
    for (int iz = 1; iz < nz; ++iz) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int ix = 0; ix + 1 < nx; ++ix) {
                const int m = b.member(nodeId(ix, iy, iz), nodeId(ix + 1, iy, iz), beamX, QStringLiteral("Vigas X"));
                const double trib = (iy == 0 || iy == ny - 1) ? p.bayY / 2.0 : p.bayY;
                b.memberLoads.append({m, QStringLiteral("CP"), 0.0, 0.0, -p.floorDeadLoad * trib});
                b.memberLoads.append({m, QStringLiteral("SC"), 0.0, 0.0, -p.floorLiveLoad * trib});
            }
        }
        for (int ix = 0; ix < nx; ++ix) {
            for (int iy = 0; iy + 1 < ny; ++iy) {
                b.member(nodeId(ix, iy, iz), nodeId(ix, iy + 1, iz), beamY, QStringLiteral("Vigas Y"));
            }
        }
    }
    if (p.bracing) {
        for (const int ix : {0, nx - 1}) {
            for (const auto &bay : {std::make_pair(0, 1), std::make_pair(ny - 2, ny - 1)}) {
                for (int iz = 0; iz + 1 < nz; ++iz) {
                    const int a = b.member(nodeId(ix, bay.first, iz), nodeId(ix, bay.second, iz + 1), tie, QStringLiteral("Contraventamento"));
                    const int c = b.member(nodeId(ix, bay.second, iz), nodeId(ix, bay.first, iz + 1), tie, QStringLiteral("Contraventamento"));
                    b.place(a, 0.0, 0, kHinges, Structura::App::ProjectData::MemberType::kTie);
                    b.place(c, 0.0, 0, kHinges, Structura::App::ProjectData::MemberType::kTie);
                }
            }
        }
    }
    const QString title = QStringLiteral("Edificio de porticos - %1x%2 vaos de %3 x %4 m, %5 andares de %6 m")
                              .arg(p.baysX).arg(p.baysY).arg(p.bayX).arg(p.bayY).arg(p.stories).arg(p.storyHeight);
    return b.text(title, {QStringLiteral("PP PP 1"), QStringLiteral("CP CP 0"), QStringLiteral("SC SC 0")},
                  {QStringLiteral("ELU1 PP=1.25 CP=1.35 SC=1.5"), QStringLiteral("ELS1 PP=1.0 CP=1.0 SC=1.0")});
}

} // namespace Structura::App::Services
