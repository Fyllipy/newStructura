#include "app/services/IfcExporter.h"
#include "app/Product.h"

#include "app/services/FabricationModel.h"
#include "app/services/MemberFabrication.h"

#include <QDateTime>
#include <QUuid>

#include <array>
#include <cmath>

namespace Structura::App::Services {

namespace {

using V3 = std::array<double, 3>;

V3 sub(const V3 &a, const V3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const V3 &a, const V3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V3 cross(const V3 &a, const V3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const V3 &a) { return std::sqrt(dot(a, a)); }
V3 unit(const V3 &a)
{
    const double n = norm(a);
    return n > 1e-12 ? V3 {a[0] / n, a[1] / n, a[2] / n} : V3 {0, 0, 1};
}

/// Texto STEP: aspas simples duplicadas e caracteres fora do ASCII como \X2\...\X0\.
QString step(const QString &text)
{
    QString out;
    bool unicode = false;
    for (const QChar c : text) {
        if (c.unicode() > 126) {
            if (!unicode) {
                out += QStringLiteral("\\X2\\");
                unicode = true;
            }
            out += QStringLiteral("%1").arg(static_cast<uint>(c.unicode()), 4, 16, QLatin1Char('0')).toUpper();
            continue;
        }
        if (unicode) {
            out += QStringLiteral("\\X0\\");
            unicode = false;
        }
        out += c == QLatin1Char('\'') ? QStringLiteral("''") : (c == QLatin1Char('\\') ? QStringLiteral("\\\\") : QString(c));
    }
    if (unicode) {
        out += QStringLiteral("\\X0\\");
    }
    return QStringLiteral("'") + out + QStringLiteral("'");
}

QString real(double v)
{
    QString s = QString::number(v, 'f', 4);
    while (s.endsWith(QLatin1Char('0')) && s.contains(QLatin1Char('.'))) {
        s.chop(1);
    }
    if (s.endsWith(QLatin1Char('.'))) {
        return s;
    }
    return s.contains(QLatin1Char('.')) ? s : s + QLatin1Char('.');
}

QString point(const V3 &p)
{
    return QStringLiteral("IFCCARTESIANPOINT((%1,%2,%3))").arg(real(p[0]), real(p[1]), real(p[2]));
}

QString direction(const V3 &d)
{
    return QStringLiteral("IFCDIRECTION((%1,%2,%3))").arg(real(d[0]), real(d[1]), real(d[2]));
}

class Writer
{
public:
    int add(const QString &entity)
    {
        lines.push_back(QStringLiteral("#%1=%2;").arg(++next).arg(entity));
        return next;
    }
    QString ref(int id) const { return QStringLiteral("#%1").arg(id); }
    QStringList lines;
    int next {0};
};

QString newGuid()
{
    const QUuid id = QUuid::createUuid();
    return IfcExporter::compressGuid(id.toRfc4122());
}

} // namespace

QString IfcExporter::compressGuid(const QByteArray &bytes)
{
    static const char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$";
    if (bytes.size() != 16) {
        return QString();
    }
    // 128 bits do mais significativo ao menos: o primeiro caractere leva 2 bits e os 21 seguintes 6 bits cada
    QString out;
    int bitPos = 0;
    auto take = [&](int count) {
        int value = 0;
        for (int i = 0; i < count; ++i, ++bitPos) {
            const int bit = (static_cast<quint8>(bytes[bitPos / 8]) >> (7 - bitPos % 8)) & 1;
            value = (value << 1) | bit;
        }
        return value;
    };
    out += QLatin1Char(chars[take(2)]);
    for (int i = 0; i < 21; ++i) {
        out += QLatin1Char(chars[take(6)]);
    }
    return out;
}

QString IfcExporter::build(const SheetBuilder::Input &input, const Options &options)
{
    Writer w;
    // ---- contexto, unidades e estrutura espacial
    const int mm = w.add(QStringLiteral("IFCSIUNIT(*,.LENGTHUNIT.,.MILLI.,.METRE.)"));
    const int area = w.add(QStringLiteral("IFCSIUNIT(*,.AREAUNIT.,$,.SQUARE_METRE.)"));
    const int volume = w.add(QStringLiteral("IFCSIUNIT(*,.VOLUMEUNIT.,$,.CUBIC_METRE.)"));
    const int angle = w.add(QStringLiteral("IFCSIUNIT(*,.PLANEANGLEUNIT.,$,.RADIAN.)"));
    const int mass = w.add(QStringLiteral("IFCSIUNIT(*,.MASSUNIT.,.KILO.,.GRAM.)"));
    const int units = w.add(QStringLiteral("IFCUNITASSIGNMENT((%1,%2,%3,%4,%5))").arg(w.ref(mm), w.ref(area), w.ref(volume), w.ref(angle), w.ref(mass)));
    const int origin = w.add(point({0, 0, 0}));
    const int zDir = w.add(direction({0, 0, 1}));
    const int xDir = w.add(direction({1, 0, 0}));
    const int worldAxis = w.add(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,%2,%3)").arg(w.ref(origin), w.ref(zDir), w.ref(xDir)));
    const int context = w.add(QStringLiteral("IFCGEOMETRICREPRESENTATIONCONTEXT($,'Model',3,1.E-05,%1,$)").arg(w.ref(worldAxis)));
    const int body = w.add(QStringLiteral("IFCGEOMETRICREPRESENTATIONSUBCONTEXT('Body','Model',*,*,*,*,%1,$,.MODEL_VIEW.,$)").arg(w.ref(context)));
    const int project = w.add(QStringLiteral("IFCPROJECT('%1',$,%2,$,$,$,$,(%3),%4)").arg(newGuid(), step(options.projectName), w.ref(context), w.ref(units)));

    auto placement = [&](int relativeTo) {
        const int axis = w.add(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)").arg(w.ref(origin)));
        return w.add(QStringLiteral("IFCLOCALPLACEMENT(%1,%2)").arg(relativeTo > 0 ? w.ref(relativeTo) : QStringLiteral("$"), w.ref(axis)));
    };
    const int sitePlacement = placement(0);
    const int site = w.add(QStringLiteral("IFCSITE('%1',$,'Terreno',$,$,%2,$,$,.ELEMENT.,$,$,$,$,$)").arg(newGuid(), w.ref(sitePlacement)));
    const int buildingPlacement = placement(sitePlacement);
    const int building = w.add(QStringLiteral("IFCBUILDING('%1',$,'Edificacao',$,$,%2,$,$,.ELEMENT.,$,$,$)").arg(newGuid(), w.ref(buildingPlacement)));
    const int storeyPlacement = placement(buildingPlacement);
    const int storey = w.add(QStringLiteral("IFCBUILDINGSTOREY('%1',$,'Estrutura',$,$,%2,$,$,.ELEMENT.,0.)").arg(newGuid(), w.ref(storeyPlacement)));
    w.add(QStringLiteral("IFCRELAGGREGATES('%1',$,$,$,%2,(%3))").arg(newGuid(), w.ref(project), w.ref(site)));
    w.add(QStringLiteral("IFCRELAGGREGATES('%1',$,$,$,%2,(%3))").arg(newGuid(), w.ref(site), w.ref(building)));
    w.add(QStringLiteral("IFCRELAGGREGATES('%1',$,$,$,%2,(%3))").arg(newGuid(), w.ref(building), w.ref(storey)));

    // ---- pecas
    QMap<int, SheetBuilder::Node> nodes;
    for (const auto &n : input.nodes) {
        nodes.insert(n.id, n);
    }
    QMap<int, SheetBuilder::Bar> bars;
    for (const auto &b : input.bars) {
        bars.insert(b.id, b);
    }
    QStringList elements;
    QStringList steelElements;
    const int steel = w.add(QStringLiteral("IFCMATERIAL('A\\X2\\00E7\\X0\\o estrutural',$,'Steel')"));
    // conjunto de propriedades por elemento (marca, perfil, comprimento de corte, massa)
    auto addProperties = [&](int element, const QString &psetName, const QVector<std::pair<QString, QString>> &items) {
        QStringList refs;
        for (const auto &item : items) {
            refs << w.ref(w.add(QStringLiteral("IFCPROPERTYSINGLEVALUE(%1,$,%2,$)").arg(step(item.first), item.second)));
        }
        const int pset = w.add(QStringLiteral("IFCPROPERTYSET('%1',$,%2,$,(%3))").arg(newGuid(), step(psetName), refs.join(QLatin1Char(','))));
        w.add(QStringLiteral("IFCRELDEFINESBYPROPERTIES('%1',$,$,$,(%2),%3)").arg(newGuid(), w.ref(element), w.ref(pset)));
    };
    const auto members = MemberFabrication::build(input);
    for (const auto &member : members) {
        for (const int barId : member.bars) {
            const auto &bar = bars.value(barId);
            const auto a = nodes.value(bar.nodeI), b = nodes.value(bar.nodeJ);
            const V3 p0 {a.x * 1000.0, a.y * 1000.0, a.z * 1000.0}, p1 {b.x * 1000.0, b.y * 1000.0, b.z * 1000.0};
            const V3 axis = unit(sub(p1, p0));
            if (norm(sub(p1, p0)) < 1e-6) {
                continue;
            }
            const double offset = bar.trimI + member.gapI;
            const V3 start {p0[0] + axis[0] * offset, p0[1] + axis[1] * offset, p0[2] + axis[2] * offset};
            const bool vertical = std::abs(axis[2]) > 0.999;
            const bool horizontal = std::abs(axis[2]) < 1e-3;
            // eixo x do perfil: nas vigas, up x eixo (a altura do perfil fica na vertical); nos pilares, o eixo X global
            V3 ref = vertical ? V3 {1, 0, 0} : unit(cross({0, 0, 1}, axis));
            if (std::abs(bar.rotationDeg) > 1e-9) {
                const double r = bar.rotationDeg * 3.14159265358979323846 / 180.0;
                const V3 side = cross(axis, ref);
                ref = {ref[0] * std::cos(r) + side[0] * std::sin(r), ref[1] * std::cos(r) + side[1] * std::sin(r), ref[2] * std::cos(r) + side[2] * std::sin(r)};
            }
            const int pOrigin = w.add(point(start));
            const int pAxis = w.add(direction(axis));
            const int pRef = w.add(direction(ref));
            const int pAxisPlacement = w.add(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,%2,%3)").arg(w.ref(pOrigin), w.ref(pAxis), w.ref(pRef)));
            const int elementPlacement = w.add(QStringLiteral("IFCLOCALPLACEMENT(%1,%2)").arg(w.ref(storeyPlacement), w.ref(pAxisPlacement)));

            const double depth = bar.depthMm > 1.0 ? bar.depthMm : 200.0, width = bar.widthMm > 1.0 ? bar.widthMm : depth * 0.5;
            int profile = 0;
            if (bar.iShape && bar.flangeMm > 0.0 && bar.webMm > 0.0) {
                profile = w.add(QStringLiteral("IFCISHAPEPROFILEDEF(.AREA.,%1,$,%2,%3,%4,%5,$,$,$)").arg(step(bar.section), real(width), real(depth), real(bar.webMm), real(bar.flangeMm)));
            } else {
                profile = w.add(QStringLiteral("IFCRECTANGLEPROFILEDEF(.AREA.,%1,$,%2,%3)").arg(step(bar.section), real(width), real(depth)));
            }
            const int solidAxis = w.add(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)").arg(w.ref(origin)));
            const int extrude = w.add(QStringLiteral("IFCEXTRUDEDAREASOLID(%1,%2,%3,%4)").arg(w.ref(profile), w.ref(solidAxis), w.ref(zDir), real(member.cutLength)));
            const int rep = w.add(QStringLiteral("IFCSHAPEREPRESENTATION(%1,'Body','SweptSolid',(%2))").arg(w.ref(body), w.ref(extrude)));
            const int shape = w.add(QStringLiteral("IFCPRODUCTDEFINITIONSHAPE($,$,(%1))").arg(w.ref(rep)));
            const QString kind = vertical && bar.memberType == 0 ? QStringLiteral("IFCCOLUMN") : (horizontal && bar.memberType == 0 ? QStringLiteral("IFCBEAM") : QStringLiteral("IFCMEMBER"));
            const QString predefined = kind == QLatin1String("IFCCOLUMN") ? QStringLiteral(".COLUMN.") : (kind == QLatin1String("IFCBEAM") ? QStringLiteral(".BEAM.") : QStringLiteral(".BRACE."));
            const QString name = QStringLiteral("%1 (barra %2)").arg(member.mark).arg(barId);
            const int element = w.add(QStringLiteral("%1('%2',$,%3,%4,%5,%6,%7,%8,%9)")
                                          .arg(kind, newGuid(), step(name), step(member.section), QStringLiteral("$"), w.ref(elementPlacement), w.ref(shape), step(member.mark), predefined));
            elements << w.ref(element);
            steelElements << w.ref(element);
            addProperties(element, QStringLiteral("Structura_Peca"),
                          {{QStringLiteral("Marca"), QStringLiteral("IFCLABEL(%1)").arg(step(member.mark))},
                           {QStringLiteral("Perfil"), QStringLiteral("IFCLABEL(%1)").arg(step(member.section))},
                           {QStringLiteral("ComprimentoDeCorte"), QStringLiteral("IFCLENGTHMEASURE(%1)").arg(real(member.cutLength))},
                           {QStringLiteral("MassaDoConjunto"), QStringLiteral("IFCMASSMEASURE(%1)").arg(real(member.assemblyMassKg))}});

            // chapas das ligacoes desta extremidade (referencial da ligacao -> global)
            for (const auto &detail : input.connections) {
                if (detail.connection.memberId != barId) {
                    continue;
                }
                const bool atEnd = detail.connection.atEnd;
                const V3 sDir = atEnd ? V3 {-axis[0], -axis[1], -axis[2]} : axis;
                const V3 tDir = unit(cross(axis, ref));   // altura do perfil
                const V3 uDir = unit(cross(sDir, tDir));
                const double outDist = detail.connection.values.value(QStringLiteral("outside"), 0.0) != 0.0 ? detail.connection.values.value(QStringLiteral("outDist"), 0.0) : 0.0;
                V3 base = atEnd ? V3 {p1[0] - axis[0] * bar.trimJ, p1[1] - axis[1] * bar.trimJ, p1[2] - axis[2] * bar.trimJ}
                                : V3 {p0[0] + axis[0] * bar.trimI, p0[1] + axis[1] * bar.trimI, p0[2] + axis[2] * bar.trimI};
                for (std::size_t k = 0; k < 3; ++k) {
                    base[k] += sDir[k] * outDist;   // junta fora do no
                }
                const std::array<V3, 3> dirs {sDir, tDir, uDir};
                auto toGlobal = [&](const V3 &c) {
                    return V3 {base[0] + dirs[0][0] * c[0] + dirs[1][0] * c[1] + dirs[2][0] * c[2], base[1] + dirs[0][1] * c[0] + dirs[1][1] * c[1] + dirs[2][1] * c[2],
                               base[2] + dirs[0][2] * c[0] + dirs[1][2] * c[1] + dirs[2][2] * c[2]};
                };
                const FabAssembly fab = FabricationModel::fromConnection(detail.connection);
                for (const FabPart &plate : fab.plateInstances) {
                    const std::size_t ta = static_cast<std::size_t>(plate.thicknessAxis);
                    const std::size_t pa = static_cast<std::size_t>(plate.planeAxes[0]), pb = static_cast<std::size_t>(plate.planeAxes[1]);
                    V3 zAxis = dirs[ta], xAxis = dirs[pa];
                    const V3 yWanted = dirs[pb];
                    double thicknessStart = plate.midOffset - plate.thickness / 2.0;
                    if (dot(cross(zAxis, xAxis), yWanted) < 0.0) {   // mantem o perfil 2D sem espelhar: inverte a extrusao
                        zAxis = {-zAxis[0], -zAxis[1], -zAxis[2]};
                        thicknessStart = plate.midOffset + plate.thickness / 2.0;
                    }
                    V3 corner {};
                    corner[ta] = thicknessStart;
                    corner[pa] = plate.origin[0];
                    corner[pb] = plate.origin[1];
                    const V3 origin3 = toGlobal(corner);
                    const int po = w.add(point(origin3));
                    const int pz = w.add(direction(zAxis));
                    const int px = w.add(direction(xAxis));
                    const int axis3 = w.add(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,%2,%3)").arg(w.ref(po), w.ref(pz), w.ref(px)));
                    const int plateLocal = w.add(QStringLiteral("IFCLOCALPLACEMENT(%1,%2)").arg(w.ref(storeyPlacement), w.ref(axis3)));
                    QStringList pts;
                    for (const auto &v : plate.outline) {
                        pts << w.ref(w.add(QStringLiteral("IFCCARTESIANPOINT((%1,%2))").arg(real(v[0]), real(v[1]))));
                    }
                    pts << pts.first();
                    const int outer = w.add(QStringLiteral("IFCPOLYLINE((%1))").arg(pts.join(QLatin1Char(','))));
                    int plateProfile = 0;
                    if (plate.holes.empty()) {
                        plateProfile = w.add(QStringLiteral("IFCARBITRARYCLOSEDPROFILEDEF(.AREA.,$,%1)").arg(w.ref(outer)));
                    } else {
                        QStringList voids;
                        for (const auto &hole : plate.holes) {
                            const int centre2 = w.add(QStringLiteral("IFCCARTESIANPOINT((%1,%2))").arg(real(hole.a), real(hole.b)));
                            const int place2 = w.add(QStringLiteral("IFCAXIS2PLACEMENT2D(%1,$)").arg(w.ref(centre2)));
                            voids << w.ref(w.add(QStringLiteral("IFCCIRCLE(%1,%2)").arg(w.ref(place2), real(hole.diameter / 2.0))));
                        }
                        plateProfile = w.add(QStringLiteral("IFCARBITRARYPROFILEDEFWITHVOIDS(.AREA.,$,%1,(%2))").arg(w.ref(outer), voids.join(QLatin1Char(','))));
                    }
                    const int plateAxis = w.add(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)").arg(w.ref(origin)));
                    const int plateSolid = w.add(QStringLiteral("IFCEXTRUDEDAREASOLID(%1,%2,%3,%4)").arg(w.ref(plateProfile), w.ref(plateAxis), w.ref(zDir), real(plate.thickness)));
                    const int plateRep = w.add(QStringLiteral("IFCSHAPEREPRESENTATION(%1,'Body','SweptSolid',(%2))").arg(w.ref(body), w.ref(plateSolid)));
                    const int plateShape = w.add(QStringLiteral("IFCPRODUCTDEFINITIONSHAPE($,$,(%1))").arg(w.ref(plateRep)));
                    const QString plateName = QStringLiteral("L%1-%2").arg(detail.connection.id).arg(plate.mark);
                    const int plateEntity = w.add(QStringLiteral("IFCPLATE('%1',$,%2,%3,$,%4,%5,%6,.NOTDEFINED.)")
                                                      .arg(newGuid(), step(plateName), step(plate.name), w.ref(plateLocal), w.ref(plateShape), step(plate.mark)));
                    elements << w.ref(plateEntity);
                    steelElements << w.ref(plateEntity);
                    addProperties(plateEntity, QStringLiteral("Structura_Chapa"),
                                  {{QStringLiteral("Marca"), QStringLiteral("IFCLABEL(%1)").arg(step(plateName))},
                                   {QStringLiteral("Espessura"), QStringLiteral("IFCLENGTHMEASURE(%1)").arg(real(plate.thickness))},
                                   {QStringLiteral("Furos"), QStringLiteral("IFCINTEGER(%1)").arg(static_cast<int>(plate.holes.size()))},
                                   {QStringLiteral("Massa"), QStringLiteral("IFCMASSMEASURE(%1)").arg(real(plate.massKg))},
                                   {QStringLiteral("SoldadaAoPerfil"), QStringLiteral("IFCBOOLEAN(%1)").arg(plate.weldedToMember ? QStringLiteral(".T.") : QStringLiteral(".F."))}});
                }
            }
        }
    }
    if (!steelElements.isEmpty()) {
        w.add(QStringLiteral("IFCRELASSOCIATESMATERIAL('%1',$,$,$,(%2),%3)").arg(newGuid(), steelElements.join(QLatin1Char(',')), w.ref(steel)));
    }
    if (!elements.isEmpty()) {
        w.add(QStringLiteral("IFCRELCONTAINEDINSPATIALSTRUCTURE('%1',$,$,$,(%2),%3)").arg(newGuid(), elements.join(QLatin1Char(',')), w.ref(storey)));
    }

    QString out;
    out += QStringLiteral("ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('ViewDefinition [ReferenceView_V1.2]'),'2;1');\n");
    out += QStringLiteral("FILE_NAME('modelo.ifc','%1',(%2),(''),'%3','%3','');\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate), step(options.author), Structura::App::Product::displayName());
    out += QStringLiteral("FILE_SCHEMA(('IFC4'));\nENDSEC;\nDATA;\n");
    out += w.lines.join(QLatin1Char('\n'));
    out += QStringLiteral("\nENDSEC;\nEND-ISO-10303-21;\n");
    return out;
}

} // namespace Structura::App::Services
