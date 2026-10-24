#pragma once

#include "app/services/ConnectionCatalog.h"
#include "design/Connections.h"

#include <algorithm>
#include <cmath>

/// Utilitarios internos das familias do catalogo de ligacoes (valores, construtores de solidos) e declaracoes das familias.
namespace Structura::App::Services::catalog {

using Role = ConnectionSolids::Role;
using Kind = ConnectionSolids::Kind;

/// Parafuso pelo nome gravado na ligacao; nome desconhecido = o terceiro da tabela (A325), ou o primeiro se a tabela for menor.
inline Structura::Design::Connections::BoltGrade gradeByName(const QString &name)
{
    const auto &grades = Structura::Design::Connections::boltGrades();
    for (const auto &g : grades) {
        if (QString::fromStdString(g.name) == name) {
            return g;
        }
    }
    return grades.size() > 2 ? grades[2] : grades.front();
}

/// Nome do parafuso padrao das ligacoes novas.
inline QString defaultBoltGradeName()
{
    const auto &grades = Structura::Design::Connections::boltGrades();
    return QString::fromStdString(grades.size() > 2 ? grades[2].name : grades.front().name);
}

inline double val(const Structura::App::ProjectData::NodeConnection &c, const char *key, double fallback = 0.0)
{
    return c.values.value(QString::fromLatin1(key), fallback);
}

inline int count(const Structura::App::ProjectData::NodeConnection &c, const char *key, double fallback)
{
    return std::max(1, static_cast<int>(std::lround(val(c, key, fallback))));
}

/// Construtores de solidos no referencial da ligacao (s ao longo da barra, t na altura, u na normal da alma).
namespace sb {

inline ConnectionSolids::Primitive box(Role role, double s, double t, double u, double ds, double dt, double du)
{
    ConnectionSolids::Primitive p;
    p.kind = Kind::Box;
    p.role = role;
    p.a = {s, t, u};
    p.size = {ds, dt, du};
    return p;
}

inline ConnectionSolids::Primitive cylinder(Role role, std::array<double, 3> a, std::array<double, 3> b, double radius)
{
    ConnectionSolids::Primitive p;
    p.kind = Kind::Cylinder;
    p.role = role;
    p.a = a;
    p.b = b;
    p.radius = radius;
    return p;
}

/// Parafuso com eixo ao longo de `axis` (0 = s, 1 = t, 2 = u): cabeca em `lo`, porca em `hi`.
inline void addBolt(std::vector<ConnectionSolids::Primitive> &out, std::array<double, 3> center, int axis, double lo, double hi, double db)
{
    auto at = [&](double along) {
        std::array<double, 3> p = center;
        p[static_cast<std::size_t>(axis)] = along;
        return p;
    };
    out.push_back(cylinder(Role::Bolt, at(lo - 0.65 * db), at(lo), 0.8 * db));
    out.push_back(cylinder(Role::Bolt, at(lo), at(hi), 0.5 * db));
    out.push_back(cylinder(Role::Bolt, at(hi), at(hi + 0.8 * db), 0.8 * db));
}

/// Chapa retangular no plano (s, t) de s0 a s1, altura `h` centrada em t = 0, extrudada de u0 a u1.
inline void addPlate(std::vector<ConnectionSolids::Primitive> &out, double s0, double s1, double h, double u0, double u1)
{
    ConnectionSolids::Primitive p;
    p.kind = Kind::Prism;
    p.role = Role::Plate;
    p.polygon = {{s0, -h / 2.0}, {s1, -h / 2.0}, {s1, h / 2.0}, {s0, h / 2.0}};
    p.u0 = u0;
    p.u1 = u1;
    out.push_back(std::move(p));
}

} // namespace sb

// ---- familia WebShear -----------------------------------------------------------------------------------------------------------------
QVector<ConnectionCatalog::FieldDef> webShearFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection webShearDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation webShearEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString webShearDescribe(const Structura::App::ProjectData::NodeConnection &c);
void webShearSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
std::vector<ConnectionCatalog::Dim> webShearDims(const Structura::App::ProjectData::NodeConnection &c);
std::vector<std::string> webShearNotes(const Structura::App::ProjectData::NodeConnection &c);
QPair<QString, QString> webShearSteelKeys();

// ---- variantes das tipologias anteriores (nervuras de base, ligacoes mistas, decoracoes 3D) -----------------------------------------------
QVector<ConnectionService::FieldDef> baseRibFields();
void baseRibDefaults(Structura::App::ProjectData::NodeConnection &c, const ConnectionService::BeamInfo &beam);
void variantDecorate(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);

// ---- familia FlexEndPlate (chapa de extremidade flexivel) e Composite (ligacao mista, Anexo O) ---------------------------------------
QVector<ConnectionCatalog::FieldDef> flexEndPlateFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection flexEndPlateDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation flexEndPlateEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString flexEndPlateDescribe(const Structura::App::ProjectData::NodeConnection &c);
void flexEndPlateSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
std::vector<ConnectionCatalog::Dim> flexEndPlateDims(const Structura::App::ProjectData::NodeConnection &c);
std::vector<std::string> flexEndPlateNotes(const Structura::App::ProjectData::NodeConnection &c);
QPair<QString, QString> flexEndPlateSteelKeys();
QVector<ConnectionCatalog::FieldDef> compositeConnFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection compositeConnDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation compositeConnEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString compositeConnDescribe(const Structura::App::ProjectData::NodeConnection &c);
void compositeConnSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
std::vector<std::string> compositeConnNotes(const Structura::App::ProjectData::NodeConnection &c);
QPair<QString, QString> compositeConnSteelKeys(const ConnectionCatalog::Entry &entry);

// ---- familia Moment -------------------------------------------------------------------------------------------------------------------
QVector<ConnectionCatalog::FieldDef> momentFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection momentDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation momentEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString momentDescribe(const Structura::App::ProjectData::NodeConnection &c);
void momentSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
std::vector<std::string> momentNotes(const Structura::App::ProjectData::NodeConnection &c);
QPair<QString, QString> momentSteelKeys();

// ---- familias CoverSplice e TubeSplice ---------------------------------------------------------------------------------------------
QVector<ConnectionCatalog::FieldDef> coverSpliceFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection coverSpliceDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation coverSpliceEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString coverSpliceDescribe(const Structura::App::ProjectData::NodeConnection &c);
void coverSpliceSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
QPair<QString, QString> coverSpliceSteelKeys();
QVector<ConnectionCatalog::FieldDef> tubeSpliceFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection tubeSpliceDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation tubeSpliceEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString tubeSpliceDescribe(const Structura::App::ProjectData::NodeConnection &c);
void tubeSpliceSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
QPair<QString, QString> tubeSpliceSteelKeys();

// ---- familia EndPlateSplice ---------------------------------------------------------------------------------------------------------
QVector<ConnectionCatalog::FieldDef> endPlateSpliceFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection endPlateSpliceDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation endPlateSpliceEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString endPlateSpliceDescribe(const Structura::App::ProjectData::NodeConnection &c);
void endPlateSpliceSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
QPair<QString, QString> endPlateSpliceSteelKeys();

// ---- familia Pin (chapa ligada por pino, 5.2.6 e 6.4) ---------------------------------------------------------------------------------
QVector<ConnectionCatalog::FieldDef> pinFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection pinDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &beam, double plateFy, double plateFu);
ConnectionCatalog::Evaluation pinEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString pinDescribe(const Structura::App::ProjectData::NodeConnection &c);
void pinSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionCatalog::Primitive> &out);
std::vector<std::string> pinNotes(const Structura::App::ProjectData::NodeConnection &c);
QPair<QString, QString> pinSteelKeys();

// ---- familia TrussNode ----------------------------------------------------------------------------------------------------------------
QVector<ConnectionCatalog::FieldDef> trussNodeFields(const ConnectionCatalog::Entry &entry);
Structura::App::ProjectData::NodeConnection trussNodeDefault(const ConnectionCatalog::Entry &entry, const ConnectionCatalog::BeamInfo &chord, double plateFy, double plateFu,
                                                             const QVector<ConnectionCatalog::BraceInfo> &braces);
ConnectionCatalog::Evaluation trussNodeEvaluate(const Structura::App::ProjectData::NodeConnection &c, const ConnectionCatalog::Forces &f);
QString trussNodeDescribe(const Structura::App::ProjectData::NodeConnection &c);
void trussNodeSolids(const Structura::App::ProjectData::NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out);
std::vector<ConnectionCatalog::Dim> trussNodeDims(const Structura::App::ProjectData::NodeConnection &c);
std::vector<std::string> trussNodeNotes(const Structura::App::ProjectData::NodeConnection &c);
QPair<QString, QString> trussNodeSteelKeys();

} // namespace Structura::App::Services::catalog
