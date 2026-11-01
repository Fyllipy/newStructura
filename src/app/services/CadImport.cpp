#include "app/services/CadImport.h"

#include <QHash>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

namespace Structura::App::Services {

namespace {

using V3 = std::array<double, 3>;

double lengthOf(const V3 &a, const V3 &b)
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

// ------------------------------------------------------------------------------------------------------------------------------------------------
// DXF
// ------------------------------------------------------------------------------------------------------------------------------------------------

double insUnitsToMeters(int code)
{
    switch (code) {
    case 1: return 0.0254;    // polegada
    case 2: return 0.3048;    // pe
    case 4: return 0.001;     // milimetro
    case 5: return 0.01;      // centimetro
    case 6: return 1.0;       // metro
    case 7: return 1000.0;    // quilometro
    default: return 0.0;      // nao especificado
    }
}

struct DxfPair {
    int code {0};
    QString value;
};

} // namespace

ImportResult DxfImporter::parse(const QString &text, double unitScale)
{
    ImportResult result;
    QVector<DxfPair> pairs;
    {
        const QStringList lines = text.split(QLatin1Char('\n'));
        pairs.reserve(lines.size() / 2);
        for (int i = 0; i + 1 < lines.size(); i += 2) {
            bool ok = false;
            const int code = lines[i].trimmed().toInt(&ok);
            if (!ok) {
                result.warnings << QStringLiteral("DXF: código de grupo inválido na linha %1; leitura interrompida.").arg(i + 1);
                break;
            }
            pairs.push_back({code, lines[i + 1].trimmed()});
        }
    }
    if (pairs.isEmpty()) {
        result.warnings << QStringLiteral("DXF: arquivo vazio ou fora do formato ASCII.");
        return result;
    }
    // cabecalho: $INSUNITS
    double scale = unitScale;
    if (scale <= 0.0) {
        for (int i = 0; i + 1 < pairs.size(); ++i) {
            if (pairs[i].code == 9 && pairs[i].value == QLatin1String("$INSUNITS") && pairs[i + 1].code == 70) {
                scale = insUnitsToMeters(pairs[i + 1].value.toInt());
                break;
            }
            if (pairs[i].code == 0 && pairs[i].value == QLatin1String("ENDSEC") && i > 0) {
                break;   // fim do cabecalho
            }
        }
        if (scale <= 0.0) {
            scale = 1.0;
            result.warnings << QStringLiteral("DXF: unidade não informada ($INSUNITS ausente ou 0); adotado metro.");
        }
    }

    // percorre a secao ENTITIES
    int i = 0;
    bool inEntities = false;
    QMap<QString, int> ignored;
    auto point = [&](double x, double y, double z) { return V3 {x * scale, y * scale, z * scale}; };
    auto addSegment = [&](const V3 &a, const V3 &b, const QString &layer) {
        if (lengthOf(a, b) > 1e-9) {
            ImportedSegment s;
            s.a = a;
            s.b = b;
            s.layer = layer;
            result.segments.push_back(s);
        }
    };
    auto isEntityStart = [&](int index) { return pairs[index].code == 0; };
    while (i < pairs.size()) {
        const DxfPair &p = pairs[i];
        if (p.code == 0 && p.value == QLatin1String("SECTION") && i + 1 < pairs.size() && pairs[i + 1].code == 2) {
            inEntities = pairs[i + 1].value == QLatin1String("ENTITIES");
            i += 2;
            continue;
        }
        if (p.code == 0 && p.value == QLatin1String("ENDSEC")) {
            inEntities = false;
            ++i;
            continue;
        }
        if (!inEntities || p.code != 0) {
            ++i;
            continue;
        }
        const QString type = p.value;
        int j = i + 1;
        if (type == QLatin1String("LINE")) {
            double v[6] = {0, 0, 0, 0, 0, 0};
            QString layer;
            for (; j < pairs.size() && !isEntityStart(j); ++j) {
                const int code = pairs[j].code;
                if (code == 8) {
                    layer = pairs[j].value;
                } else if (code >= 10 && code <= 13 && code != 12) {
                    // 10/20/30: ponto inicial; 11/21/31: ponto final
                    v[0] = code == 10 ? pairs[j].value.toDouble() : v[0];
                    v[3] = code == 11 ? pairs[j].value.toDouble() : v[3];
                } else if (code == 20) {
                    v[1] = pairs[j].value.toDouble();
                } else if (code == 21) {
                    v[4] = pairs[j].value.toDouble();
                } else if (code == 30) {
                    v[2] = pairs[j].value.toDouble();
                } else if (code == 31) {
                    v[5] = pairs[j].value.toDouble();
                }
            }
            addSegment(point(v[0], v[1], v[2]), point(v[3], v[4], v[5]), layer);
        } else if (type == QLatin1String("LWPOLYLINE")) {
            QString layer;
            bool closed = false;
            double elevation = 0.0;
            QVector<std::array<double, 2>> vertices;
            for (; j < pairs.size() && !isEntityStart(j); ++j) {
                const int code = pairs[j].code;
                if (code == 8) {
                    layer = pairs[j].value;
                } else if (code == 70) {
                    closed = (pairs[j].value.toInt() & 1) != 0;
                } else if (code == 38) {
                    elevation = pairs[j].value.toDouble();
                } else if (code == 10) {
                    vertices.push_back({pairs[j].value.toDouble(), 0.0});
                } else if (code == 20 && !vertices.isEmpty()) {
                    vertices.back()[1] = pairs[j].value.toDouble();
                }
            }
            for (int k = 0; k + 1 < vertices.size(); ++k) {
                addSegment(point(vertices[k][0], vertices[k][1], elevation), point(vertices[k + 1][0], vertices[k + 1][1], elevation), layer);
            }
            if (closed && vertices.size() > 2) {
                addSegment(point(vertices.back()[0], vertices.back()[1], elevation), point(vertices.front()[0], vertices.front()[1], elevation), layer);
            }
        } else if (type == QLatin1String("POLYLINE")) {
            QString layer;
            int flags = 0;
            for (; j < pairs.size() && !isEntityStart(j); ++j) {
                if (pairs[j].code == 8) {
                    layer = pairs[j].value;
                } else if (pairs[j].code == 70) {
                    flags = pairs[j].value.toInt();
                }
            }
            QVector<V3> vertices;
            while (j < pairs.size() && pairs[j].code == 0 && pairs[j].value == QLatin1String("VERTEX")) {
                double x = 0, y = 0, z = 0;
                int vertexFlags = 0;
                for (++j; j < pairs.size() && !isEntityStart(j); ++j) {
                    switch (pairs[j].code) {
                    case 10: x = pairs[j].value.toDouble(); break;
                    case 20: y = pairs[j].value.toDouble(); break;
                    case 30: z = pairs[j].value.toDouble(); break;
                    case 70: vertexFlags = pairs[j].value.toInt(); break;
                    default: break;
                    }
                }
                if ((vertexFlags & 128) == 0 || (vertexFlags & 64) != 0) {   // 128 sem 64: indices de face da malha (nao e ponto)
                    vertices.push_back(point(x, y, z));
                }
            }
            if (j < pairs.size() && pairs[j].code == 0 && pairs[j].value == QLatin1String("SEQEND")) {
                ++j;
                while (j < pairs.size() && !isEntityStart(j)) {
                    ++j;
                }
            }
            if ((flags & (16 | 64)) != 0) {
                ++ignored[QStringLiteral("POLYLINE (malha)")];
            } else {
                for (int k = 0; k + 1 < vertices.size(); ++k) {
                    addSegment(vertices[k], vertices[k + 1], layer);
                }
                if ((flags & 1) != 0 && vertices.size() > 2) {
                    addSegment(vertices.back(), vertices.front(), layer);
                }
            }
        } else {
            for (; j < pairs.size() && !isEntityStart(j); ++j) {
            }
            if (type != QLatin1String("TEXT") && type != QLatin1String("MTEXT") && type != QLatin1String("POINT") && type != QLatin1String("ATTDEF") && type != QLatin1String("ATTRIB")) {
                ++ignored[type];
            }
        }
        i = j;
    }
    for (auto it = ignored.constBegin(); it != ignored.constEnd(); ++it) {
        result.warnings << QStringLiteral("DXF: %1 entidade(s) %2 ignorada(s) (só LINE, LWPOLYLINE e POLYLINE viram barras).").arg(it.value()).arg(it.key());
    }
    if (result.segments.isEmpty()) {
        result.warnings << QStringLiteral("DXF: nenhuma linha encontrada na seção ENTITIES.");
    }
    return result;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------
// IFC (STEP)
// ------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

struct Value {
    enum class Kind { Null, Ref, String, Number, Enum, List, Typed };
    Kind kind {Kind::Null};
    int ref {0};
    double number {0.0};
    QString text;          // string, enum, nome do tipo (Typed)
    QVector<Value> list;   // lista ou argumentos do tipo (Typed)
    [[nodiscard]] bool isRef() const { return kind == Kind::Ref; }
    [[nodiscard]] bool isList() const { return kind == Kind::List; }
};

struct Entity {
    QString type;   // maiusculas
    QVector<Value> args;
};

class StepParser
{
public:
    explicit StepParser(const QString &text) : m_text(text) {}

    bool parse(QHash<int, Entity> &entities)
    {
        const int data = m_text.indexOf(QLatin1String("DATA;"));
        if (data < 0) {
            return false;
        }
        m_pos = data + 5;
        while (m_pos < m_text.size()) {
            skipSpace();
            if (m_text.mid(m_pos, 6) == QLatin1String("ENDSEC")) {
                break;
            }
            if (m_pos >= m_text.size() || m_text[m_pos] != QLatin1Char('#')) {
                ++m_pos;
                continue;
            }
            ++m_pos;
            int id = 0;
            while (m_pos < m_text.size() && m_text[m_pos].isDigit()) {
                id = id * 10 + m_text[m_pos].digitValue();
                ++m_pos;
            }
            skipSpace();
            if (m_pos >= m_text.size() || m_text[m_pos] != QLatin1Char('=')) {
                continue;
            }
            ++m_pos;
            skipSpace();
            Entity entity;
            int start = m_pos;
            while (m_pos < m_text.size() && (m_text[m_pos].isLetterOrNumber() || m_text[m_pos] == QLatin1Char('_'))) {
                ++m_pos;
            }
            entity.type = m_text.mid(start, m_pos - start).toUpper();
            skipSpace();
            if (m_pos < m_text.size() && m_text[m_pos] == QLatin1Char('(')) {
                Value list = parseList();
                entity.args = list.list;
            }
            // avanca ate o ';' do fim da entidade
            while (m_pos < m_text.size() && m_text[m_pos] != QLatin1Char(';')) {
                ++m_pos;
            }
            ++m_pos;
            entities.insert(id, std::move(entity));
        }
        return !entities.isEmpty();
    }

private:
    void skipSpace()
    {
        while (m_pos < m_text.size()) {
            if (m_text[m_pos].isSpace()) {
                ++m_pos;
            } else if (m_text.mid(m_pos, 2) == QLatin1String("/*")) {
                const int end = m_text.indexOf(QLatin1String("*/"), m_pos + 2);
                m_pos = end < 0 ? m_text.size() : end + 2;
            } else {
                break;
            }
        }
    }

    Value parseList()
    {
        Value list;
        list.kind = Value::Kind::List;
        ++m_pos;   // '('
        while (m_pos < m_text.size()) {
            skipSpace();
            if (m_pos >= m_text.size()) {
                break;
            }
            const QChar c = m_text[m_pos];
            if (c == QLatin1Char(')')) {
                ++m_pos;
                break;
            }
            if (c == QLatin1Char(',')) {
                ++m_pos;
                continue;
            }
            list.list.push_back(parseValue());
        }
        return list;
    }

    Value parseValue()
    {
        Value v;
        const QChar c = m_text[m_pos];
        if (c == QLatin1Char('$') || c == QLatin1Char('*')) {
            ++m_pos;
            return v;
        }
        if (c == QLatin1Char('#')) {
            ++m_pos;
            int id = 0;
            while (m_pos < m_text.size() && m_text[m_pos].isDigit()) {
                id = id * 10 + m_text[m_pos].digitValue();
                ++m_pos;
            }
            v.kind = Value::Kind::Ref;
            v.ref = id;
            return v;
        }
        if (c == QLatin1Char('\'')) {
            ++m_pos;
            QString s;
            while (m_pos < m_text.size()) {
                if (m_text[m_pos] == QLatin1Char('\'')) {
                    if (m_pos + 1 < m_text.size() && m_text[m_pos + 1] == QLatin1Char('\'')) {
                        s += QLatin1Char('\'');
                        m_pos += 2;
                        continue;
                    }
                    ++m_pos;
                    break;
                }
                s += m_text[m_pos++];
            }
            v.kind = Value::Kind::String;
            v.text = decodeStepString(s);
            return v;
        }
        if (c == QLatin1Char('.')) {
            ++m_pos;
            const int start = m_pos;
            while (m_pos < m_text.size() && m_text[m_pos] != QLatin1Char('.')) {
                ++m_pos;
            }
            v.kind = Value::Kind::Enum;
            v.text = m_text.mid(start, m_pos - start).toUpper();
            ++m_pos;
            return v;
        }
        if (c == QLatin1Char('(')) {
            return parseList();
        }
        if (c.isDigit() || c == QLatin1Char('-') || c == QLatin1Char('+')) {
            const int start = m_pos;
            ++m_pos;
            while (m_pos < m_text.size() && (m_text[m_pos].isDigit() || m_text[m_pos] == QLatin1Char('.') || m_text[m_pos] == QLatin1Char('E') || m_text[m_pos] == QLatin1Char('e')
                                             || ((m_text[m_pos] == QLatin1Char('-') || m_text[m_pos] == QLatin1Char('+')) && (m_text[m_pos - 1] == QLatin1Char('E') || m_text[m_pos - 1] == QLatin1Char('e'))))) {
                ++m_pos;
            }
            v.kind = Value::Kind::Number;
            v.number = m_text.mid(start, m_pos - start).toDouble();
            return v;
        }
        if (c.isLetter()) {
            // valor tipado: IFCLABEL('x'), IFCLENGTHMEASURE(1.)
            const int start = m_pos;
            while (m_pos < m_text.size() && (m_text[m_pos].isLetterOrNumber() || m_text[m_pos] == QLatin1Char('_'))) {
                ++m_pos;
            }
            v.kind = Value::Kind::Typed;
            v.text = m_text.mid(start, m_pos - start).toUpper();
            skipSpace();
            if (m_pos < m_text.size() && m_text[m_pos] == QLatin1Char('(')) {
                v.list = parseList().list;
            }
            return v;
        }
        ++m_pos;   // caractere inesperado: ignora
        return v;
    }

    static QString decodeStepString(const QString &s)
    {
        // \X2\hhhh...\X0\ (UTF-16) e \X\hh (ISO 8859-1)
        QString out;
        int i = 0;
        while (i < s.size()) {
            if (s.mid(i, 4) == QLatin1String("\\X2\\")) {
                const int end = s.indexOf(QLatin1String("\\X0\\"), i + 4);
                const QString hex = s.mid(i + 4, (end < 0 ? s.size() : end) - (i + 4));
                for (int k = 0; k + 3 < hex.size(); k += 4) {
                    out += QChar(static_cast<ushort>(hex.mid(k, 4).toUInt(nullptr, 16)));
                }
                i = end < 0 ? s.size() : end + 4;
            } else if (s.mid(i, 3) == QLatin1String("\\X\\") && i + 5 <= s.size()) {
                out += QChar(static_cast<ushort>(s.mid(i + 3, 2).toUInt(nullptr, 16)));
                i += 5;
            } else if (s.mid(i, 2) == QLatin1String("\\\\")) {
                out += QLatin1Char('\\');
                i += 2;
            } else {
                out += s[i++];
            }
        }
        return out;
    }

    const QString &m_text;
    int m_pos {0};
};

/// Transformacao afim: p' = R p + t.
struct Affine {
    double r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    V3 t {0.0, 0.0, 0.0};
    [[nodiscard]] V3 apply(const V3 &p) const
    {
        return {r[0][0] * p[0] + r[0][1] * p[1] + r[0][2] * p[2] + t[0], r[1][0] * p[0] + r[1][1] * p[1] + r[1][2] * p[2] + t[1], r[2][0] * p[0] + r[2][1] * p[1] + r[2][2] * p[2] + t[2]};
    }
    [[nodiscard]] V3 rotate(const V3 &d) const
    {
        return {r[0][0] * d[0] + r[0][1] * d[1] + r[0][2] * d[2], r[1][0] * d[0] + r[1][1] * d[1] + r[1][2] * d[2], r[2][0] * d[0] + r[2][1] * d[1] + r[2][2] * d[2]};
    }
    [[nodiscard]] Affine then(const Affine &parent) const   // parent * this
    {
        Affine out;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                out.r[i][j] = parent.r[i][0] * r[0][j] + parent.r[i][1] * r[1][j] + parent.r[i][2] * r[2][j];
            }
        }
        out.t = parent.apply(t);
        return out;
    }
};

V3 normalized(V3 v, const V3 &fallback)
{
    const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n < 1e-12) {
        return fallback;
    }
    return {v[0] / n, v[1] / n, v[2] / n};
}

V3 cross3(const V3 &a, const V3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double dot3(const V3 &a, const V3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

class IfcModel
{
public:
    explicit IfcModel(QHash<int, Entity> entities) : m_entities(std::move(entities)) {}

    const Entity *get(const Value &v) const
    {
        if (!v.isRef()) {
            return nullptr;
        }
        const auto it = m_entities.constFind(v.ref);
        return it == m_entities.constEnd() ? nullptr : &it.value();
    }

    static double number(const QVector<Value> &args, int index, double fallback = 0.0)
    {
        if (index < 0 || index >= args.size()) {
            return fallback;
        }
        const Value &v = args[index];
        if (v.kind == Value::Kind::Number) {
            return v.number;
        }
        if (v.kind == Value::Kind::Typed && !v.list.isEmpty() && v.list.front().kind == Value::Kind::Number) {
            return v.list.front().number;
        }
        return fallback;
    }

    std::optional<V3> pointOf(const Value &ref) const
    {
        const Entity *e = get(ref);
        if (!e || e->type != QLatin1String("IFCCARTESIANPOINT") || e->args.isEmpty() || !e->args[0].isList()) {
            return std::nullopt;
        }
        const auto &c = e->args[0].list;
        V3 p {0.0, 0.0, 0.0};
        for (int i = 0; i < std::min<int>(3, c.size()); ++i) {
            p[i] = c[i].number;
        }
        return p;
    }

    std::optional<V3> directionOf(const Value &ref) const
    {
        const Entity *e = get(ref);
        if (!e || e->type != QLatin1String("IFCDIRECTION") || e->args.isEmpty() || !e->args[0].isList()) {
            return std::nullopt;
        }
        const auto &c = e->args[0].list;
        V3 d {0.0, 0.0, 0.0};
        for (int i = 0; i < std::min<int>(3, c.size()); ++i) {
            d[i] = c[i].number;
        }
        return d;
    }

    /// IfcAxis2Placement3D / 2D -> afim (escala de comprimento ainda nao aplicada).
    Affine axisPlacement(const Value &ref) const
    {
        Affine a;
        const Entity *e = get(ref);
        if (!e) {
            return a;
        }
        if (e->type == QLatin1String("IFCAXIS2PLACEMENT3D") && e->args.size() >= 1) {
            if (const auto p = pointOf(e->args[0])) {
                a.t = *p;
            }
            V3 z {0.0, 0.0, 1.0}, x {1.0, 0.0, 0.0};
            if (e->args.size() > 1) {
                if (const auto d = directionOf(e->args[1])) {
                    z = normalized(*d, z);
                }
            }
            if (e->args.size() > 2) {
                if (const auto d = directionOf(e->args[2])) {
                    x = *d;
                }
            }
            // x ortogonalizado em relacao a z
            const double projection = dot3(x, z);
            x = normalized({x[0] - projection * z[0], x[1] - projection * z[1], x[2] - projection * z[2]}, std::abs(z[0]) < 0.9 ? V3 {1.0, 0.0, 0.0} : V3 {0.0, 1.0, 0.0});
            const V3 y = cross3(z, x);
            for (int i = 0; i < 3; ++i) {
                a.r[i][0] = x[i];
                a.r[i][1] = y[i];
                a.r[i][2] = z[i];
            }
        } else if (e->type == QLatin1String("IFCAXIS2PLACEMENT2D") && e->args.size() >= 1) {
            if (const auto p = pointOf(e->args[0])) {
                a.t = *p;
            }
        }
        return a;
    }

    Affine placement(const Value &ref, int depth = 0) const
    {
        Affine identity;
        const Entity *e = get(ref);
        if (!e || e->type != QLatin1String("IFCLOCALPLACEMENT") || e->args.size() < 2 || depth > 64) {
            return identity;
        }
        const Affine local = axisPlacement(e->args[1]);
        if (!e->args[0].isRef()) {
            return local;
        }
        return local.then(placement(e->args[0], depth + 1));
    }

    double lengthScale() const
    {
        // IFCSIUNIT(*, .LENGTHUNIT., prefixo, .METRE.) e IFCCONVERSIONBASEDUNIT(dim, .LENGTHUNIT., 'INCH', medida)
        for (auto it = m_entities.constBegin(); it != m_entities.constEnd(); ++it) {
            const Entity &e = it.value();
            if (e.type == QLatin1String("IFCSIUNIT") && e.args.size() >= 4 && e.args[1].text == QLatin1String("LENGTHUNIT")) {
                const QString prefix = e.args[2].text;
                if (prefix == QLatin1String("MILLI")) return 0.001;
                if (prefix == QLatin1String("CENTI")) return 0.01;
                if (prefix == QLatin1String("DECI")) return 0.1;
                if (prefix == QLatin1String("KILO")) return 1000.0;
                return 1.0;
            }
            if (e.type == QLatin1String("IFCCONVERSIONBASEDUNIT") && e.args.size() >= 3 && e.args[1].text == QLatin1String("LENGTHUNIT")) {
                const QString name = e.args[2].text.toUpper();
                if (name == QLatin1String("INCH")) return 0.0254;
                if (name == QLatin1String("FOOT")) return 0.3048;
            }
        }
        return 1.0;
    }

    /// Itens de uma representacao: lista de (identificador, itens).
    void representations(const Value &productShape, QVector<std::pair<QString, QVector<Value>>> &out) const
    {
        const Entity *shape = get(productShape);
        if (!shape || shape->type != QLatin1String("IFCPRODUCTDEFINITIONSHAPE") || shape->args.size() < 3 || !shape->args[2].isList()) {
            return;
        }
        for (const Value &r : shape->args[2].list) {
            const Entity *rep = get(r);
            if (!rep || rep->args.size() < 4 || !rep->args[3].isList()) {
                continue;
            }
            out.push_back({rep->args[1].text, rep->args[3].list});
        }
    }

    QString profileNameOf(const Entity &solid) const
    {
        if (solid.args.isEmpty()) {
            return {};
        }
        const Entity *profile = get(solid.args[0]);
        if (!profile || profile->args.size() < 2) {
            return {};
        }
        return profile->args[1].kind == Value::Kind::String ? profile->args[1].text : QString();
    }

    const QHash<int, Entity> &entities() const { return m_entities; }

private:
    QHash<int, Entity> m_entities;
};

bool isLinearMember(const QString &type)
{
    for (const char *prefix : {"IFCBEAM", "IFCCOLUMN", "IFCMEMBER", "IFCSTRUCTURALCURVEMEMBER"}) {
        if (type.startsWith(QLatin1String(prefix))) {
            return true;
        }
    }
    return false;
}

} // namespace

ImportResult IfcImporter::parse(const QString &text)
{
    ImportResult result;
    QHash<int, Entity> entities;
    StepParser parser(text);
    if (!parser.parse(entities)) {
        result.warnings << QStringLiteral("IFC: arquivo sem seção DATA ou fora do formato STEP.");
        return result;
    }
    const IfcModel model(std::move(entities));
    const double scale = model.lengthScale();
    int skipped = 0, noGeometry = 0;
    QList<int> ids = model.entities().keys();
    std::sort(ids.begin(), ids.end());
    for (const int id : ids) {
        const Entity &e = model.entities().value(id);
        if (!isLinearMember(e.type)) {
            continue;
        }
        // IfcProduct: GlobalId, OwnerHistory, Name, Description, ObjectType, ObjectPlacement, Representation
        if (e.args.size() < 7) {
            ++skipped;
            continue;
        }
        const Affine placement = model.placement(e.args[5]);
        QVector<std::pair<QString, QVector<Value>>> reps;
        model.representations(e.args[6], reps);
        std::optional<std::pair<V3, V3>> axis;   // locais ao elemento
        QString profile;
        // 1) representacao "Axis" (polilinha)
        for (const auto &rep : reps) {
            if (rep.first.compare(QLatin1String("Axis"), Qt::CaseInsensitive) != 0) {
                continue;
            }
            for (const Value &item : rep.second) {
                const Entity *curve = model.get(item);
                if (curve && curve->type == QLatin1String("IFCPOLYLINE") && !curve->args.isEmpty() && curve->args[0].isList() && curve->args[0].list.size() >= 2) {
                    const auto a = model.pointOf(curve->args[0].list.front());
                    const auto b = model.pointOf(curve->args[0].list.back());
                    if (a && b) {
                        axis = std::make_pair(*a, *b);
                    }
                }
            }
        }
        // 2) solido extrudado do corpo
        if (!axis) {
            for (const auto &rep : reps) {
                for (const Value &item : rep.second) {
                    const Entity *solid = model.get(item);
                    if (!solid || (solid->type != QLatin1String("IFCEXTRUDEDAREASOLID") && solid->type != QLatin1String("IFCEXTRUDEDAREASOLIDTAPERED")) || solid->args.size() < 4) {
                        continue;
                    }
                    const Affine position = solid->args[1].isRef() ? model.axisPlacement(solid->args[1]) : Affine();
                    V3 direction {0.0, 0.0, 1.0};
                    if (const auto d = model.directionOf(solid->args[2])) {
                        direction = normalized(*d, direction);
                    }
                    const double depth = IfcModel::number(solid->args, 3);
                    const V3 start = position.t;
                    const V3 rotated = position.rotate(direction);
                    axis = std::make_pair(start, V3 {start[0] + rotated[0] * depth, start[1] + rotated[1] * depth, start[2] + rotated[2] * depth});
                    profile = model.profileNameOf(*solid);
                    break;
                }
                if (axis) {
                    break;
                }
            }
        } else {
            // perfil, quando houver corpo extrudado
            for (const auto &rep : reps) {
                for (const Value &item : rep.second) {
                    const Entity *solid = model.get(item);
                    if (solid && solid->type == QLatin1String("IFCEXTRUDEDAREASOLID")) {
                        profile = model.profileNameOf(*solid);
                    }
                }
            }
        }
        if (!axis) {
            ++noGeometry;
            continue;
        }
        ImportedSegment segment;
        const V3 a = placement.apply(axis->first), b = placement.apply(axis->second);
        for (int k = 0; k < 3; ++k) {
            segment.a[k] = a[k] * scale;
            segment.b[k] = b[k] * scale;
        }
        if (lengthOf(segment.a, segment.b) < 1e-9) {
            ++skipped;
            continue;
        }
        segment.layer = e.type;
        segment.profile = profile;
        segment.name = e.args[2].kind == Value::Kind::String ? e.args[2].text : QString();
        result.segments.push_back(segment);
    }
    if (noGeometry > 0) {
        result.warnings << QStringLiteral("IFC: %1 peça(s) linear(es) sem eixo nem sólido extrudado reconhecível (mapeamentos e geometrias complexas não são lidos).").arg(noGeometry);
    }
    if (skipped > 0) {
        result.warnings << QStringLiteral("IFC: %1 peça(s) ignorada(s) por dados incompletos ou comprimento nulo.").arg(skipped);
    }
    if (result.segments.isEmpty()) {
        result.warnings << QStringLiteral("IFC: nenhuma viga, pilar ou barra com geometria linear encontrada.");
    }
    return result;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------
// Topologia
// ------------------------------------------------------------------------------------------------------------------------------------------------

ImportedTopology buildTopology(const QVector<ImportedSegment> &segments, double tolerance)
{
    ImportedTopology topology;
    const double tol = std::max(tolerance, 1e-9);
    // grade espacial: celula de lado `tol`; busca nas 27 celulas vizinhas
    QHash<qint64, QVector<int>> grid;
    auto cellKey = [&](qint64 i, qint64 j, qint64 k) { return (i * 73856093LL) ^ (j * 19349663LL) ^ (k * 83492791LL); };
    auto cellOf = [&](double v) { return static_cast<qint64>(std::floor(v / tol)); };
    auto nodeFor = [&](const V3 &p) {
        const qint64 ci = cellOf(p[0]), cj = cellOf(p[1]), ck = cellOf(p[2]);
        for (qint64 di = -1; di <= 1; ++di) {
            for (qint64 dj = -1; dj <= 1; ++dj) {
                for (qint64 dk = -1; dk <= 1; ++dk) {
                    const auto it = grid.constFind(cellKey(ci + di, cj + dj, ck + dk));
                    if (it == grid.constEnd()) {
                        continue;
                    }
                    for (const int index : it.value()) {
                        if (lengthOf(topology.nodes[index], p) <= tol) {
                            return index;
                        }
                    }
                }
            }
        }
        topology.nodes.push_back(p);
        const int index = static_cast<int>(topology.nodes.size()) - 1;
        grid[cellKey(ci, cj, ck)].push_back(index);
        return index;
    };
    QSet<QPair<int, int>> seen;
    for (const ImportedSegment &s : segments) {
        const int a = nodeFor(s.a), b = nodeFor(s.b);
        if (a == b) {
            ++topology.discardedZeroLength;
            continue;
        }
        const QPair<int, int> key(std::min(a, b), std::max(a, b));
        if (seen.contains(key)) {
            ++topology.mergedDuplicates;
            continue;
        }
        seen.insert(key);
        topology.bars.push_back({a, b, s.layer, s.profile, s.name});
    }
    return topology;
}

} // namespace Structura::App::Services
