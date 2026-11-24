#include "StairGenerator.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <utility>

namespace Structura::App::Services {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1e-6;

using Role = StairGenerator::Role;
using Layout = StairGenerator::Layout;

/// Posições transversais das longarinas e dos nós dos degraus (m, a partir do eixo; no espiral, raios).
struct Offsets {
    std::vector<double> stringers;
    std::vector<bool> lateral;
    std::vector<double> treads;
};

Offsets straightOffsets(Layout layout, double w)
{
    Offsets o;
    switch (layout) {
    case Layout::Two:
        o.stringers = {-0.5 * w, 0.5 * w};
        o.lateral = {true, true};
        o.treads = o.stringers;
        break;
    case Layout::Three:
        o.stringers = {-0.5 * w, 0.0, 0.5 * w};
        o.lateral = {true, false, true};
        o.treads = o.stringers;
        break;
    case Layout::Center:
        o.stringers = {0.0};
        o.lateral = {false};
        o.treads = {-0.5 * w, 0.0, 0.5 * w};
        break;
    }
    return o;
}

Offsets spiralOffsets(Layout layout, double ri, double ro)
{
    const double rm = 0.5 * (ri + ro);
    Offsets o;
    switch (layout) {
    case Layout::Two:
        o.stringers = {ri, ro};
        o.lateral = {true, true};
        o.treads = o.stringers;
        break;
    case Layout::Three:
        o.stringers = {ri, rm, ro};
        o.lateral = {true, false, true};
        o.treads = o.stringers;
        break;
    case Layout::Center:
        o.stringers = {rm};
        o.lateral = {false};
        o.treads = {ri, rm, ro};
        break;
    }
    return o;
}

class Builder
{
public:
    Builder(StairGenerator::Result &result, double ox, double oy, double oz, double azimuthRad)
        : m_result(result)
        , m_ox(ox)
        , m_oy(oy)
        , m_oz(oz)
        , m_cos(std::cos(azimuthRad))
        , m_sin(std::sin(azimuthRad))
    {
    }

    /// Nó em coordenadas locais (u ao longo da direção de subida, v à esquerda, z relativo ao nível de partida).
    int node(double u, double v, double z)
    {
        const double x = m_ox + u * m_cos - v * m_sin;
        const double y = m_oy + u * m_sin + v * m_cos;
        const double zz = m_oz + z;
        const std::array<long long, 3> key {std::llround(x * 1e6), std::llround(y * 1e6), std::llround(zz * 1e6)};
        const auto it = m_index.find(key);
        if (it != m_index.end()) {
            return it->second;
        }
        const int id = static_cast<int>(m_result.nodes.size());
        m_result.nodes.push_back({x, y, zz});
        m_local.push_back({u, v, z});
        m_index.emplace(key, id);
        return id;
    }

    int bar(int a, int b, Role role, bool lateral, double areaWidth)
    {
        if (a == b) {
            return -1;
        }
        const std::pair<int, int> key {std::min(a, b), std::max(a, b)};
        if (!m_bars.insert(key).second) {
            return -1;
        }
        StairGenerator::Bar bar;
        bar.a = a;
        bar.b = b;
        bar.role = role;
        bar.lateral = lateral;
        bar.areaWidth = areaWidth;
        m_result.bars.push_back(bar);
        return static_cast<int>(m_result.bars.size()) - 1;
    }

    struct Ends {
        std::vector<int> start, end;
    };

    /// Lance de degraus: `pos(i, off)` dá (u, v) do ponto `off` na seção transversal do degrau `i`.
    Ends ladder(int steps, double z0, double riser, const Offsets &offsets, double treadArea,
                const std::function<std::pair<double, double>(int, double)> &pos)
    {
        const std::size_t lines = offsets.stringers.size();
        std::vector<int> previous(lines, -1);
        Ends ends;
        for (int i = 0; i <= steps; ++i) {
            const double z = z0 + i * riser;
            std::vector<int> current(lines, -1);
            for (std::size_t j = 0; j < lines; ++j) {
                const auto [u, v] = pos(i, offsets.stringers[j]);
                current[j] = node(u, v, z);
                if (i > 0) {
                    bar(previous[j], current[j], Role::Stringer, offsets.lateral[j], 0.0);
                }
            }
            if (i == 0) {
                ends.start = current;
            }
            if (i == steps) {
                ends.end = current;
            }
            if (i >= 1 && i < steps) {
                int last = -1;
                for (const double off : offsets.treads) {
                    const auto [u, v] = pos(i, off);
                    const int id = node(u, v, z);
                    if (last >= 0) {
                        bar(last, id, Role::Tread, false, treadArea);
                    }
                    last = id;
                }
            }
            previous = current;
        }
        return ends;
    }

    Ends straightFlight(double u0, double v0, double du, double dv, double z0, int steps, double riser, double depth, double width, Layout layout)
    {
        const Offsets offsets = straightOffsets(layout, width);
        const double nu = -dv, nv = du;   // normal à esquerda
        return ladder(steps, z0, riser, offsets, depth, [&](int i, double off) {
            return std::make_pair(u0 + du * i * depth + nu * off, v0 + dv * i * depth + nv * off);
        });
    }

    /// Patamar retangular (u de umin a umax, v de vmin a vmax) no nível z: vigas de borda e vigas secundárias ao longo de v.
    void landing(double umin, double umax, double vmin, double vmax, double z, double depth)
    {
        const double length = umax - umin;
        const int m = std::max(1, static_cast<int>(std::ceil(length / std::max(depth, 0.05) - 1e-9)));
        const double s = length / m;
        for (int k = 1; k < m; ++k) {
            const double u = umin + k * s;
            bar(node(u, vmin, z), node(u, vmax, z), Role::LandingTread, false, s);
        }
        node(umin, vmin, z);
        node(umin, vmax, z);
        node(umax, vmin, z);
        node(umax, vmax, z);
        struct Edge {
            bool alongV;       // true: u constante, percorre v
            double fixed, lo, hi;
            double area;
        };
        const Edge edges[] = {{true, umin, vmin, vmax, 0.5 * s}, {true, umax, vmin, vmax, 0.5 * s},
                              {false, vmin, umin, umax, 0.0}, {false, vmax, umin, umax, 0.0}};
        for (const auto &edge : edges) {
            std::vector<std::pair<double, int>> onEdge;
            for (std::size_t id = 0; id < m_local.size(); ++id) {
                const auto &p = m_local[id];
                if (std::abs(p[2] - z) > kTol) {
                    continue;
                }
                const double fixed = edge.alongV ? p[0] : p[1];
                const double along = edge.alongV ? p[1] : p[0];
                if (std::abs(fixed - edge.fixed) < kTol && along >= edge.lo - kTol && along <= edge.hi + kTol) {
                    onEdge.emplace_back(along, static_cast<int>(id));
                }
            }
            std::sort(onEdge.begin(), onEdge.end());
            for (std::size_t i = 1; i < onEdge.size(); ++i) {
                bar(onEdge[i - 1].second, onEdge[i].second, Role::LandingBeam, false, edge.area);
            }
        }
    }

private:
    StairGenerator::Result &m_result;
    double m_ox, m_oy, m_oz, m_cos, m_sin;
    std::map<std::array<long long, 3>, int> m_index;
    std::vector<std::array<double, 3>> m_local;
    std::set<std::pair<int, int>> m_bars;
};

} // namespace

StairGenerator::Result StairGenerator::generate(const Params &params)
{
    Result result;
    const double height = params.topZ - params.baseZ;
    if (height < 0.05 || params.riser < 0.01 || params.tread < 0.05) {
        result.warnings.push_back("Níveis, altura do degrau ou profundidade do piso inválidos: a escada não foi gerada.");
        return result;
    }
    const int risers = std::max(2, static_cast<int>(std::lround(height / params.riser)));
    const double riser = height / risers;
    double tread = params.tread;
    double azimuth = params.azimuthDeg * kPi / 180.0;
    const double width = std::max(params.width, 0.2);

    if (params.type == Type::Straight && params.endByPoint) {
        const double dx = params.x1 - params.x0, dy = params.y1 - params.y0;
        const double dist = std::hypot(dx, dy);
        if (dist > 0.05) {
            azimuth = std::atan2(dy, dx);
            tread = dist / risers;
        } else {
            result.warnings.push_back("Ponto final coincide com o inicial: usando a direção e a profundidade informadas.");
        }
    }

    Builder builder(result, params.x0, params.y0, params.baseZ, azimuth);
    const double sign = params.turnLeft ? 1.0 : -1.0;
    Builder::Ends first, last;

    switch (params.type) {
    case Type::Straight: {
        first = last = builder.straightFlight(0.0, 0.0, 1.0, 0.0, 0.0, risers, riser, tread, width, params.layout);
        result.firstFlight = risers;
        break;
    }
    case Type::LShaped:
    case Type::UShaped: {
        if (risers < 2) {
            break;
        }
        int n1 = params.firstFlightSteps > 0 ? params.firstFlightSteps : (risers + 1) / 2;
        n1 = std::clamp(n1, 1, risers - 1);
        const int n2 = risers - n1;
        result.firstFlight = n1;
        const double u1 = n1 * tread;
        const double z1 = n1 * riser;
        const double landingLength = std::max(params.landingLength > 0.0 ? params.landingLength : width, width);
        first = builder.straightFlight(0.0, 0.0, 1.0, 0.0, 0.0, n1, riser, tread, width, params.layout);
        if (params.type == Type::LShaped) {
            builder.landing(u1, u1 + landingLength, -0.5 * width, 0.5 * width, z1, tread);
            last = builder.straightFlight(u1 + landingLength - 0.5 * width, sign * 0.5 * width, 0.0, sign, z1, n2, riser, tread, width, params.layout);
        } else {
            const double gap = std::max(params.gap, 0.0);
            const double offset = sign * (width + gap);
            const double vmin = sign > 0 ? -0.5 * width : offset - 0.5 * width;
            const double vmax = sign > 0 ? offset + 0.5 * width : 0.5 * width;
            builder.landing(u1, u1 + landingLength, vmin, vmax, z1, tread);
            last = builder.straightFlight(u1, offset, -1.0, 0.0, z1, n2, riser, tread, width, params.layout);
        }
        break;
    }
    case Type::Spiral: {
        const double ri = std::max(params.innerRadius, 0.05);
        const double ro = std::max(params.outerRadius, ri + 0.3);
        const double rm = 0.5 * (ri + ro);
        const double dTheta = params.totalAngleDeg > 0.0 ? params.totalAngleDeg * kPi / 180.0 / risers : tread / rm;
        tread = rm * dTheta;
        const Offsets offsets = spiralOffsets(params.layout, ri, ro);
        first = last = builder.ladder(risers, 0.0, riser, offsets, tread, [&](int i, double radius) {
            const double phi = sign * i * dTheta;
            return std::make_pair(radius * std::cos(phi), radius * std::sin(phi));
        });
        result.firstFlight = risers;
        break;
    }
    }

    result.risers = risers;
    result.riser = riser;
    result.tread = tread;
    result.baseNodes = first.start;
    result.topNodes = last.end;

    if (params.type == Type::Spiral && params.totalAngleDeg > 0.0 && tread < 0.05) {
        result.warnings.push_back("Ângulo total muito pequeno para o número de degraus.");
    }
    const double blondel = 2.0 * riser + tread;
    if (blondel < 0.60 || blondel > 0.65) {
        result.warnings.push_back("Fórmula de Blondel (2e + p entre 0,60 e 0,65 m) não atendida: 2e + p = " + std::to_string(blondel) + " m.");
    }
    if (riser > 0.18 + 1e-9) {
        result.warnings.push_back("Altura do degrau acima de 0,18 m (NBR 9050).");
    }
    if (tread < 0.28 - 1e-9) {
        result.warnings.push_back("Profundidade do piso abaixo de 0,28 m (NBR 9050).");
    }
    bool hasHandrail = false;
    for (const auto &load : params.loads) {
        hasHandrail = hasHandrail || load.kind == LoadKind::HandrailLine;
    }
    if (hasHandrail && params.layout == Layout::Center) {
        result.warnings.push_back("Carga de guarda-corpo ignorada: com uma só longarina central não há longarina lateral.");
    }
    return result;
}

std::vector<StairGenerator::MemberLoadSpec> StairGenerator::memberLoads(const Params &params, const Result &result)
{
    std::map<std::pair<int, std::string>, double> sum;   // N/m para baixo
    for (const auto &load : params.loads) {
        if (std::abs(load.value) < 1e-12) {
            continue;
        }
        for (std::size_t i = 0; i < result.bars.size(); ++i) {
            const Bar &bar = result.bars[i];
            double q = 0.0;
            switch (load.kind) {
            case LoadKind::TreadArea:
                if (bar.role == Role::Tread) {
                    q = load.value * 1000.0 * bar.areaWidth;
                }
                break;
            case LoadKind::LandingArea:
                if ((bar.role == Role::LandingTread || bar.role == Role::LandingBeam) && bar.areaWidth > 0.0) {
                    q = load.value * 1000.0 * bar.areaWidth;
                }
                break;
            case LoadKind::StringerLine:
                if (bar.role == Role::Stringer) {
                    q = load.value * 1000.0;
                }
                break;
            case LoadKind::HandrailLine:
                if (bar.role == Role::Stringer && bar.lateral) {
                    q = load.value * 1000.0;
                }
                break;
            }
            if (q != 0.0) {
                sum[{static_cast<int>(i), load.loadCase}] += q;
            }
        }
    }
    std::vector<MemberLoadSpec> specs;
    for (const auto &[key, q] : sum) {
        specs.push_back({key.first, key.second, -q});
    }
    return specs;
}

const char *StairGenerator::typeName(Type type)
{
    switch (type) {
    case Type::Straight: return "Escada reta";
    case Type::LShaped: return "Escada em L";
    case Type::UShaped: return "Escada em U";
    case Type::Spiral: return "Escada em espiral";
    }
    return "";
}

const char *StairGenerator::layoutName(Layout layout)
{
    switch (layout) {
    case Layout::Two: return "Duas longarinas laterais";
    case Layout::Three: return "Três longarinas (laterais e central)";
    case Layout::Center: return "Uma longarina central";
    }
    return "";
}

const char *StairGenerator::loadKindName(LoadKind kind)
{
    switch (kind) {
    case LoadKind::TreadArea: return "Sobrecarga nos degraus (kN/m²)";
    case LoadKind::LandingArea: return "Sobrecarga no patamar (kN/m²)";
    case LoadKind::StringerLine: return "Carga linear nas longarinas (kN/m)";
    case LoadKind::HandrailLine: return "Guarda-corpo nas longarinas laterais (kN/m)";
    }
    return "";
}

const char *StairGenerator::supportName(SupportKind kind)
{
    switch (kind) {
    case SupportKind::None: return "Sem apoio";
    case SupportKind::Pinned: return "Articulado";
    case SupportKind::Fixed: return "Engastado";
    }
    return "";
}

} // namespace Structura::App::Services
