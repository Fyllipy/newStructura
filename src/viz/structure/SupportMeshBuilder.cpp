#include "viz/structure/SupportMeshBuilder.h"

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <array>
#include <cmath>
#include <initializer_list>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace Structura::Visualization {

namespace {

constexpr double kSize = SupportMeshBuilder::kSupportSize;
constexpr double kSmall = kSize / 3.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kCylinderResolution = 24;

using Vec = std::array<double, 3>;

struct Node {
    double x {0.0};
    double y {0.0};
    double z {0.0};
    std::array<bool, 6> supports {};
};

/// Acumula pontos e faces; cada parte nova e anexada sem fundir pontos (como `merge_points=False`).
class Mesh
{
public:
    int addPoint(double x, double y, double z)
    {
        m_points.push_back({x, y, z});
        return static_cast<int>(m_points.size()) - 1;
    }

    /// Anexa uma face usando indices locais relativos a `base`.
    void addFace(int base, std::initializer_list<int> local)
    {
        std::vector<int> face;
        face.reserve(local.size());
        for (const int index : local) {
            face.push_back(base + index);
        }
        m_faces.push_back(std::move(face));
    }

    void addFace(std::vector<int> face) { m_faces.push_back(std::move(face)); }

    [[nodiscard]] int pointCount() const { return static_cast<int>(m_points.size()); }
    [[nodiscard]] std::vector<Vec> &points() { return m_points; }

    [[nodiscard]] const std::vector<Vec> &points() const { return m_points; }
    [[nodiscard]] const std::vector<std::vector<int>> &faces() const { return m_faces; }

private:
    std::vector<Vec> m_points;
    std::vector<std::vector<int>> m_faces;
};

void addCube(Mesh &mesh, double cx, double cy, double cz, double sx, double sy, double sz)
{
    const double x0 = cx - sx / 2.0;
    const double x1 = cx + sx / 2.0;
    const double y0 = cy - sy / 2.0;
    const double y1 = cy + sy / 2.0;
    const double z0 = cz - sz / 2.0;
    const double z1 = cz + sz / 2.0;
    const int base = mesh.pointCount();
    mesh.addPoint(x0, y0, z0);
    mesh.addPoint(x1, y0, z0);
    mesh.addPoint(x1, y1, z0);
    mesh.addPoint(x0, y1, z0);
    mesh.addPoint(x0, y0, z1);
    mesh.addPoint(x1, y0, z1);
    mesh.addPoint(x1, y1, z1);
    mesh.addPoint(x0, y1, z1);
    mesh.addFace(base, {0, 1, 2, 3});
    mesh.addFace(base, {4, 7, 6, 5});
    mesh.addFace(base, {0, 4, 5, 1});
    mesh.addFace(base, {1, 5, 6, 2});
    mesh.addFace(base, {2, 6, 7, 3});
    mesh.addFace(base, {3, 7, 4, 0});
}

void addCylinder(Mesh &mesh, double cx, double cy, double cz, Vec direction, double radius, double height)
{
    const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    for (double &component : direction) {
        component /= length;
    }
    // Base ortonormal (u, v) perpendicular ao eixo.
    Vec reference = std::abs(direction[2]) > 0.9 ? Vec {1.0, 0.0, 0.0} : Vec {0.0, 0.0, 1.0};
    Vec u {direction[1] * reference[2] - direction[2] * reference[1],
           direction[2] * reference[0] - direction[0] * reference[2],
           direction[0] * reference[1] - direction[1] * reference[0]};
    const double uLength = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (double &component : u) {
        component /= uLength;
    }
    const Vec v {direction[1] * u[2] - direction[2] * u[1],
                 direction[2] * u[0] - direction[0] * u[2],
                 direction[0] * u[1] - direction[1] * u[0]};

    const int base = mesh.pointCount();
    for (const double sign : {-0.5, 0.5}) {
        for (int i = 0; i < kCylinderResolution; ++i) {
            const double angle = 2.0 * kPi * i / kCylinderResolution;
            const double c = std::cos(angle) * radius;
            const double s = std::sin(angle) * radius;
            mesh.addPoint(cx + direction[0] * height * sign + u[0] * c + v[0] * s,
                          cy + direction[1] * height * sign + u[1] * c + v[1] * s,
                          cz + direction[2] * height * sign + u[2] * c + v[2] * s);
        }
    }
    for (int i = 0; i < kCylinderResolution; ++i) {
        const int next = (i + 1) % kCylinderResolution;
        mesh.addFace({base + i, base + next, base + kCylinderResolution + next, base + kCylinderResolution + i});
    }
    std::vector<int> bottom;
    std::vector<int> top;
    for (int i = 0; i < kCylinderResolution; ++i) {
        bottom.push_back(base + kCylinderResolution - 1 - i);
        top.push_back(base + kCylinderResolution + i);
    }
    mesh.addFace(bottom);
    mesh.addFace(top);
}

void addSphere(Mesh &mesh, double cx, double cy, double cz, double radius, int thetaResolution = 24, int phiResolution = 16)
{
    const int base = mesh.pointCount();
    mesh.addPoint(cx, cy, cz + radius);
    for (int ring = 1; ring < phiResolution; ++ring) {
        const double phi = kPi * ring / phiResolution;
        for (int i = 0; i < thetaResolution; ++i) {
            const double theta = 2.0 * kPi * i / thetaResolution;
            mesh.addPoint(cx + radius * std::sin(phi) * std::cos(theta),
                          cy + radius * std::sin(phi) * std::sin(theta),
                          cz + radius * std::cos(phi));
        }
    }
    const int south = mesh.addPoint(cx, cy, cz - radius);
    auto ringIndex = [&](int ring, int i) { return base + 1 + (ring - 1) * thetaResolution + (i % thetaResolution); };
    for (int i = 0; i < thetaResolution; ++i) {
        mesh.addFace({base, ringIndex(1, i), ringIndex(1, i + 1)});
    }
    for (int ring = 1; ring < phiResolution - 1; ++ring) {
        for (int i = 0; i < thetaResolution; ++i) {
            mesh.addFace({ringIndex(ring, i), ringIndex(ring + 1, i), ringIndex(ring + 1, i + 1), ringIndex(ring, i + 1)});
        }
    }
    for (int i = 0; i < thetaResolution; ++i) {
        mesh.addFace({ringIndex(phiResolution - 1, i), south, ringIndex(phiResolution - 1, i + 1)});
    }
}

void addPyramid(Mesh &mesh, const std::array<Vec, 5> &p)
{
    const int base = mesh.pointCount();
    for (const Vec &point : p) {
        mesh.addPoint(point[0], point[1], point[2]);
    }
    mesh.addFace(base, {0, 1, 2, 3});
    mesh.addFace(base, {0, 1, 4});
    mesh.addFace(base, {1, 2, 4});
    mesh.addFace(base, {2, 3, 4});
    mesh.addFace(base, {3, 0, 4});
}

// ── Celulas e blocos do cubo modular ──────────────────────────────────────

void addCellCube(Mesh &mesh, const Node &n, int xi, int yi, int zi)
{
    addCube(mesh, n.x + (xi - 1) * kSmall, n.y + (yi - 1) * kSmall, n.z - (zi + 0.5) * kSmall,
            kSmall, kSmall, kSmall);
}

void addSphereCell(Mesh &mesh, const Node &n, int xi, int yi, int zi)
{
    addSphere(mesh, n.x + (xi - 1) * kSmall, n.y + (yi - 1) * kSmall, n.z - (zi + 0.5) * kSmall, kSmall / 2.0);
}

void addVerticalCylinder(Mesh &mesh, const Node &n)
{
    addCylinder(mesh, n.x, n.y, n.z - 1.5 * kSmall, {0.0, 0.0, 1.0}, kSmall / 2.0, kSmall);
}

/// Bloco do topo com chanfro em X (celulas 0 e 2) ou cubo (celula 1).
void addChamferedTopBlock(Mesh &mesh, const Node &n, int xi, int yi)
{
    const double x0 = n.x - kSize / 2.0 + xi * kSmall;
    const double x1 = x0 + kSmall;
    const double y0 = n.y - kSize / 2.0 + yi * kSmall;
    const double y1 = y0 + kSmall;
    const double z0 = n.z - kSmall;
    const double z1 = n.z;
    const int base = mesh.pointCount();
    if (xi == 0) {
        mesh.addPoint(x0, y0, z0);
        mesh.addPoint(x1, y0, z0);
        mesh.addPoint(x1, y1, z0);
        mesh.addPoint(x0, y1, z0);
        mesh.addPoint(x1, y0, z1);
        mesh.addPoint(x1, y1, z1);
        mesh.addFace(base, {0, 1, 2, 3});
        mesh.addFace(base, {0, 3, 5, 4});
        mesh.addFace(base, {1, 4, 5, 2});
        mesh.addFace(base, {0, 1, 4});
        mesh.addFace(base, {3, 5, 2});
        return;
    }
    if (xi == 2) {
        mesh.addPoint(x0, y0, z0);
        mesh.addPoint(x1, y0, z0);
        mesh.addPoint(x1, y1, z0);
        mesh.addPoint(x0, y1, z0);
        mesh.addPoint(x0, y0, z1);
        mesh.addPoint(x0, y1, z1);
        mesh.addFace(base, {0, 1, 2, 3});
        mesh.addFace(base, {0, 4, 5, 3});
        mesh.addFace(base, {1, 2, 5, 4});
        mesh.addFace(base, {0, 1, 4});
        mesh.addFace(base, {3, 5, 2});
        return;
    }
    mesh.addPoint(x0, y0, z0);
    mesh.addPoint(x1, y0, z0);
    mesh.addPoint(x1, y1, z0);
    mesh.addPoint(x0, y1, z0);
    mesh.addPoint(x0, y0, z1);
    mesh.addPoint(x1, y0, z1);
    mesh.addPoint(x1, y1, z1);
    mesh.addPoint(x0, y1, z1);
    mesh.addFace(base, {0, 1, 2, 3});
    mesh.addFace(base, {4, 7, 6, 5});
    mesh.addFace(base, {0, 4, 5, 1});
    mesh.addFace(base, {1, 5, 6, 2});
    mesh.addFace(base, {2, 6, 7, 3});
    mesh.addFace(base, {3, 7, 4, 0});
}

/// Mesmo bloco, com o chanfro na direcao Y (troca x/y).
void addChamferedTopBlockY(Mesh &mesh, const Node &n, int xi, int yi)
{
    Node swapped = n;
    swapped.x = n.y;
    swapped.y = n.x;
    const int firstPoint = mesh.pointCount();
    addChamferedTopBlock(mesh, swapped, yi, xi);
    auto &points = mesh.points();
    for (int i = firstPoint; i < mesh.pointCount(); ++i) {
        std::swap(points[static_cast<std::size_t>(i)][0], points[static_cast<std::size_t>(i)][1]);
    }
}

void addDoubleChamferedTopBlock(Mesh &mesh, const Node &n, int xi, int yi)
{
    const bool outerX = xi == 0 || xi == 2;
    const bool outerY = yi == 0 || yi == 2;
    if (outerX && outerY) {
        const double x0 = n.x - kSize / 2.0 + xi * kSmall;
        const double x1 = x0 + kSmall;
        const double y0 = n.y - kSize / 2.0 + yi * kSmall;
        const double y1 = y0 + kSmall;
        const double z0 = n.z - kSmall;
        const double apexX = xi == 0 ? x1 : x0;
        const double apexY = yi == 0 ? y1 : y0;
        addPyramid(mesh, {Vec {x0, y0, z0}, Vec {x1, y0, z0}, Vec {x1, y1, z0}, Vec {x0, y1, z0}, Vec {apexX, apexY, n.z}});
        return;
    }
    if (outerX) {
        addChamferedTopBlock(mesh, n, xi, yi);
        return;
    }
    if (outerY) {
        addChamferedTopBlockY(mesh, n, xi, yi);
        return;
    }
    addCube(mesh, n.x, n.y, n.z - kSmall / 2.0, kSmall, kSmall, kSmall);
}

void addRotationTopBlock(Mesh &mesh, const Node &n, int xi, int yi, bool freeRx, bool freeRy)
{
    if (freeRx && freeRy) {
        addDoubleChamferedTopBlock(mesh, n, xi, yi);
    } else if (freeRx) {
        addChamferedTopBlockY(mesh, n, xi, yi);
    } else if (freeRy) {
        addChamferedTopBlock(mesh, n, xi, yi);
    } else {
        addCellCube(mesh, n, xi, yi, 0);
    }
}

// ── Familias de apoio ─────────────────────────────────────────────────────

void addModularCube(Mesh &mesh, const Node &n, bool includeMiddle, bool centerCylinder)
{
    for (int xi = 0; xi < 3; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            for (int zi = 0; zi < 3; ++zi) {
                if (!includeMiddle && zi == 1) {
                    continue;
                }
                addCellCube(mesh, n, xi, yi, zi);
            }
        }
    }
    if (centerCylinder) {
        addVerticalCylinder(mesh, n);
    }
}

void addMinusXFaceCylinder(Mesh &mesh, const Node &n, bool includeMiddle, bool middleVertical)
{
    for (int xi = 1; xi <= 2; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            for (int zi = 0; zi < 3; ++zi) {
                if (!includeMiddle && zi == 1) {
                    continue;
                }
                addCellCube(mesh, n, xi, yi, zi);
            }
        }
    }
    for (const int zi : {0, 2}) {
        for (int yi = 0; yi < 3; ++yi) {
            addCylinder(mesh, n.x - kSmall, n.y + (yi - 1) * kSmall, n.z - (zi + 0.5) * kSmall,
                        {0.0, 1.0, 0.0}, kSmall / 2.0, kSmall);
        }
    }
    if (middleVertical) {
        addVerticalCylinder(mesh, n);
    }
}

void addRotatedMinusZFaceCylinder(Mesh &mesh, const Node &n, bool freeRx, bool freeRy, bool freeRz, bool rollersAlongX)
{
    const int layerCount = freeRz ? 1 : 2;
    for (int xi = 0; xi < 3; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            for (int zi = 0; zi < layerCount; ++zi) {
                if (zi == 0) {
                    addRotationTopBlock(mesh, n, xi, yi, freeRx, freeRy);
                } else {
                    addCellCube(mesh, n, xi, yi, zi);
                }
            }
        }
    }
    if (rollersAlongX) {
        for (const int yi : {0, 2}) {
            for (int xi = 0; xi < 3; ++xi) {
                addCylinder(mesh, n.x + (xi - 1) * kSmall, n.y + (yi - 1) * kSmall, n.z - 2.5 * kSmall,
                            {1.0, 0.0, 0.0}, kSmall / 2.0, kSmall);
            }
        }
    } else {
        for (const int xi : {0, 2}) {
            for (int yi = 0; yi < 3; ++yi) {
                addCylinder(mesh, n.x + (xi - 1) * kSmall, n.y + (yi - 1) * kSmall, n.z - 2.5 * kSmall,
                            {0.0, 1.0, 0.0}, kSmall / 2.0, kSmall);
            }
        }
    }
    if (freeRz) {
        addVerticalCylinder(mesh, n);
    }
}

void addMinusXSphere(Mesh &mesh, const Node &n, bool freeRx, bool freeRy, bool freeRz)
{
    for (const int yi : {0, 2}) {
        for (const int zi : {0, 2}) {
            addSphereCell(mesh, n, 0, yi, zi);
        }
    }
    for (int xi = 1; xi <= 2; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            for (int zi = 0; zi < 3; ++zi) {
                if (freeRz && zi == 1) {
                    continue;
                }
                if (zi == 0) {
                    addRotationTopBlock(mesh, n, xi, yi, freeRx, freeRy);
                } else {
                    addCellCube(mesh, n, xi, yi, zi);
                }
            }
        }
    }
    if (freeRz) {
        addVerticalCylinder(mesh, n);
    }
}

void addMinusZSphere(Mesh &mesh, const Node &n, bool freeRx, bool freeRy, bool freeRz)
{
    for (const int xi : {0, 2}) {
        for (const int yi : {0, 2}) {
            addSphereCell(mesh, n, xi, yi, 2);
        }
    }
    const int layerCount = freeRz ? 1 : 2;
    for (int xi = 0; xi < 3; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            for (int zi = 0; zi < layerCount; ++zi) {
                if (zi == 0) {
                    addRotationTopBlock(mesh, n, xi, yi, freeRx, freeRy);
                } else {
                    addCellCube(mesh, n, xi, yi, zi);
                }
            }
        }
    }
    if (freeRz) {
        addVerticalCylinder(mesh, n);
    }
}

using Cell = std::tuple<int, int, int>;

void addPlanarCylinder(Mesh &mesh, const Node &n, int xi, int yi, bool alongY)
{
    addCylinder(mesh, n.x + (xi - 1) * kSmall, n.y + (yi - 1) * kSmall, n.z - 1.5 * kSmall,
                alongY ? Vec {0.0, 1.0, 0.0} : Vec {1.0, 0.0, 0.0}, kSmall / 2.0, kSmall);
}

void addGenericSupport(Mesh &mesh, const Node &n)
{
    const bool freeDx = !n.supports[0];
    const bool freeDy = !n.supports[1];
    const bool freeDz = !n.supports[2];
    const bool freeRx = !n.supports[3];
    const bool freeRy = !n.supports[4];
    const bool freeRz = !n.supports[5];

    std::set<Cell> occupied;
    std::set<std::string> blockedFaces;

    // Translacoes liberadas: rolos (uma direcao) ou esferas nos cantos (duas/tres).
    const int freeCount = static_cast<int>(freeDx) + static_cast<int>(freeDy) + static_cast<int>(freeDz);
    if (freeCount == 1 && freeDx) {
        for (int xi = 0; xi < 3; ++xi) {
            occupied.insert({xi, 0, 1});
            addPlanarCylinder(mesh, n, xi, 0, true);
        }
    } else if (freeCount == 1 && freeDy) {
        for (int yi = 0; yi < 3; ++yi) {
            occupied.insert({0, yi, 1});
            addPlanarCylinder(mesh, n, 0, yi, false);
        }
    } else if (freeCount == 2) {
        std::vector<Cell> sphereCells;
        if (freeDx && freeDy) {
            sphereCells = {{0, 0, 0}, {0, 2, 0}, {0, 0, 2}, {0, 2, 2}};
            blockedFaces.insert("-x");
        } else if (freeDx && freeDz) {
            sphereCells = {{0, 0, 0}, {2, 0, 0}, {0, 0, 2}, {2, 0, 2}};
            blockedFaces.insert("-y");
        } else {
            sphereCells = {{0, 0, 2}, {2, 0, 2}, {0, 2, 2}, {2, 2, 2}};
            blockedFaces.insert("-z");
        }
        for (const Cell &cell : sphereCells) {
            occupied.insert(cell);
            addSphereCell(mesh, n, std::get<0>(cell), std::get<1>(cell), std::get<2>(cell));
        }
    } else if (freeCount == 3) {
        blockedFaces.insert("-x");
        blockedFaces.insert("-y");
        blockedFaces.insert("-z");
        const std::set<Cell> sphereCells = {{0, 0, 0}, {0, 2, 0}, {0, 0, 2}, {0, 2, 2}, {2, 0, 0}, {2, 0, 2}, {2, 2, 2}};
        for (const Cell &cell : sphereCells) {
            occupied.insert(cell);
            addSphereCell(mesh, n, std::get<0>(cell), std::get<1>(cell), std::get<2>(cell));
        }
    }

    const bool zFaceFamily = freeDz && !freeDx && !freeDy;
    if (zFaceFamily) {
        for (const int zi : {0, 2}) {
            for (int yi = 0; yi < 3; ++yi) {
                occupied.insert({0, yi, zi});
                addCylinder(mesh, n.x - kSmall, n.y + (yi - 1) * kSmall, n.z - (zi + 0.5) * kSmall,
                            {0.0, 1.0, 0.0}, kSmall / 2.0, kSmall);
            }
        }
    }

    if (freeRz) {
        occupied.insert({1, 1, 1});
        addVerticalCylinder(mesh, n);
    }

    const bool removeMiddleLayer = freeRz;
    for (int xi = 0; xi < 3; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            for (int zi = 0; zi < 3; ++zi) {
                if (occupied.count({xi, yi, zi})) {
                    continue;
                }
                if (blockedFaces.count("-x") && xi == 0) {
                    continue;
                }
                if (blockedFaces.count("-y") && yi == 0) {
                    continue;
                }
                if (blockedFaces.count("-z") && zi == 2) {
                    continue;
                }
                if (zFaceFamily && xi == 0) {
                    continue;
                }
                if (removeMiddleLayer && zi == 1) {
                    continue;
                }
                if (zi == 0) {
                    addRotationTopBlock(mesh, n, xi, yi, freeRx, freeRy);
                } else {
                    addCellCube(mesh, n, xi, yi, zi);
                }
            }
        }
    }
}

void addChamferedModularCube(Mesh &mesh, const Node &n, bool chamferAlongX, bool includeMiddle, bool centerCylinder)
{
    for (int xi = 0; xi < 3; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            if (chamferAlongX) {
                addChamferedTopBlock(mesh, n, xi, yi);
            } else {
                addChamferedTopBlockY(mesh, n, xi, yi);
            }
            if (includeMiddle) {
                addCellCube(mesh, n, xi, yi, 1);
            }
            addCellCube(mesh, n, xi, yi, 2);
        }
    }
    if (centerCylinder) {
        addVerticalCylinder(mesh, n);
    }
}

void addDoubleChamferedModularCube(Mesh &mesh, const Node &n, bool includeMiddle, bool centerCylinder)
{
    for (int xi = 0; xi < 3; ++xi) {
        for (int yi = 0; yi < 3; ++yi) {
            addDoubleChamferedTopBlock(mesh, n, xi, yi);
            if (includeMiddle) {
                addCellCube(mesh, n, xi, yi, 1);
            }
            addCellCube(mesh, n, xi, yi, 2);
        }
    }
    if (centerCylinder) {
        addVerticalCylinder(mesh, n);
    }
}

/// Despacha a geometria do apoio conforme o padrao de restricoes (como `build_support_mesh`).
void addSupportMesh(Mesh &mesh, const Node &n)
{
    const auto &s = n.supports;
    auto matches = [&](bool a, bool b, bool c, bool d, bool e, bool f) {
        return s[0] == a && s[1] == b && s[2] == c && s[3] == d && s[4] == e && s[5] == f;
    };

    if (!(s[0] || s[1] || s[2] || s[3] || s[4] || s[5])) {
        return;
    }
    if (matches(true, true, true, true, true, true)) {
        addModularCube(mesh, n, true, false);
    } else if (matches(true, true, true, true, false, true)) {
        addChamferedModularCube(mesh, n, true, true, false);
    } else if (matches(true, true, true, false, true, true)) {
        addChamferedModularCube(mesh, n, false, true, false);
    } else if (matches(true, true, true, false, false, true)) {
        addDoubleChamferedModularCube(mesh, n, true, false);
    } else if (matches(true, true, true, false, false, false)) {
        addDoubleChamferedModularCube(mesh, n, false, true);
    } else if (matches(true, true, true, true, false, false)) {
        addChamferedModularCube(mesh, n, true, false, true);
    } else if (matches(true, true, true, false, true, false)) {
        addChamferedModularCube(mesh, n, false, false, true);
    } else if (matches(true, true, true, true, true, false)) {
        addModularCube(mesh, n, false, true);
    } else if (matches(true, true, false, true, true, true)) {
        addMinusXFaceCylinder(mesh, n, true, false);
    } else if (matches(true, true, false, true, true, false)) {
        addMinusXFaceCylinder(mesh, n, false, true);
    } else if (s[0] && !s[1] && !s[2]) {
        addMinusXSphere(mesh, n, !s[3], !s[4], !s[5]);
    } else if (s[0] && !s[1] && s[2]) {
        addRotatedMinusZFaceCylinder(mesh, n, !s[3], !s[4], !s[5], true);
    } else if (!s[0] && s[1] && s[2]) {
        addRotatedMinusZFaceCylinder(mesh, n, !s[3], !s[4], !s[5], false);
    } else if (!s[0] && !s[1] && s[2]) {
        addMinusZSphere(mesh, n, !s[3], !s[4], !s[5]);
    } else {
        addGenericSupport(mesh, n);
    }
}

/// Apoio (restringe o deslocamento vertical Z): simbolos classicos abaixo do no, `u` = unidade de tamanho (m).
///   engaste: placa com aletas; articulado: piramide sobre placa; movel: piramide sobre placa e rolos (ou esferas).
void addSupportSymbol(Mesh &mesh, const Vec &p, const std::array<bool, 6> &r, double u)
{
    const bool tx = r[0], ty = r[1];
    const int rotations = (r[3] ? 1 : 0) + (r[4] ? 1 : 0) + (r[5] ? 1 : 0);
    if (tx && ty && rotations == 3) {
        addCube(mesh, p[0], p[1], p[2] - 0.1 * u, 1.6 * u, 1.6 * u, 0.2 * u);
        for (int i = -2; i <= 2; ++i) {
            addCube(mesh, p[0] + i * 0.35 * u, p[1], p[2] - 0.2 * u - 0.18 * u, 0.07 * u, 1.4 * u, 0.36 * u);
        }
        return;
    }
    const double baseZ = p[2] - 0.9 * u;
    const double h = 0.45 * u;
    addPyramid(mesh, {Vec {p[0] - h, p[1] - h, baseZ}, Vec {p[0] + h, p[1] - h, baseZ}, Vec {p[0] + h, p[1] + h, baseZ}, Vec {p[0] - h, p[1] + h, baseZ}, Vec {p[0], p[1], p[2]}});
    addCube(mesh, p[0], p[1], baseZ - 0.05 * u, 1.4 * u, 1.4 * u, 0.1 * u);
    // rotacoes restringidas (semi-engaste): blocos ao lado do apoio
    if (r[3]) {
        addCube(mesh, p[0] + 0.62 * u, p[1], baseZ + 0.12 * u, 0.2 * u, 0.5 * u, 0.24 * u);
    }
    if (r[4]) {
        addCube(mesh, p[0], p[1] + 0.62 * u, baseZ + 0.12 * u, 0.5 * u, 0.2 * u, 0.24 * u);
    }
    if (r[5]) {
        addCube(mesh, p[0] - 0.62 * u, p[1], baseZ + 0.12 * u, 0.2 * u, 0.5 * u, 0.24 * u);
    }
    const bool freeX = !tx, freeY = !ty;
    if (!freeX && !freeY) {
        return;
    }
    const double rollerZ = baseZ - 0.1 * u - 0.14 * u;
    if (freeX && freeY) {
        for (const double ox : {-0.4, 0.4}) {
            for (const double oy : {-0.4, 0.4}) {
                addSphere(mesh, p[0] + ox * u, p[1] + oy * u, rollerZ, 0.14 * u, 14, 10);
            }
        }
    } else {
        const Vec axis = freeX ? Vec {0.0, 1.0, 0.0} : Vec {1.0, 0.0, 0.0};
        for (const double offset : {-0.4, 0.0, 0.4}) {
            addCylinder(mesh, p[0] + (freeX ? offset * u : 0.0), p[1] + (freeX ? 0.0 : offset * u), rollerZ, axis, 0.14 * u, 1.2 * u);
        }
    }
    addCube(mesh, p[0], p[1], rollerZ - 0.14 * u - 0.03 * u, 1.6 * u, 1.6 * u, 0.06 * u);
}

/// Restricao nodal (translacao ou rotacao restringida sem ser apoio): uma barra com batentes por translacao e um anel por rotacao.
void addConstraintSymbol(Mesh &mesh, const Vec &p, const std::array<bool, 6> &r, double u)
{
    const Vec axes[3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    for (int a = 0; a < 3; ++a) {
        if (!r[static_cast<std::size_t>(a)]) {
            continue;
        }
        addCylinder(mesh, p[0], p[1], p[2], axes[a], 0.04 * u, 1.1 * u);
        for (const double side : {-0.55, 0.55}) {
            addCube(mesh, p[0] + axes[a][0] * side * u, p[1] + axes[a][1] * side * u, p[2] + axes[a][2] * side * u,
                    a == 0 ? 0.07 * u : 0.3 * u, a == 1 ? 0.07 * u : 0.3 * u, a == 2 ? 0.07 * u : 0.3 * u);
        }
    }
    for (int a = 0; a < 3; ++a) {
        if (!r[static_cast<std::size_t>(3 + a)]) {
            continue;
        }
        const Vec n = axes[a];
        const Vec e1 = a == 0 ? Vec {0.0, 1.0, 0.0} : (a == 1 ? Vec {0.0, 0.0, 1.0} : Vec {1.0, 0.0, 0.0});
        const Vec e2 {n[1] * e1[2] - n[2] * e1[1], n[2] * e1[0] - n[0] * e1[2], n[0] * e1[1] - n[1] * e1[0]};
        const double radius = 0.4 * u;
        constexpr int kSegments = 16;
        for (int i = 0; i < kSegments; ++i) {
            const double t0 = 2.0 * kPi * i / kSegments, t1 = 2.0 * kPi * (i + 1) / kSegments, tm = 0.5 * (t0 + t1);
            auto at = [&](double t) { return Vec {radius * (std::cos(t) * e1[0] + std::sin(t) * e2[0]), radius * (std::cos(t) * e1[1] + std::sin(t) * e2[1]), radius * (std::cos(t) * e1[2] + std::sin(t) * e2[2])}; };
            const Vec a0 = at(t0), a1 = at(t1), am = at(tm);
            const Vec tangent {a1[0] - a0[0], a1[1] - a0[1], a1[2] - a0[2]};
            const double chord = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1] + tangent[2] * tangent[2]);
            addCylinder(mesh, p[0] + am[0], p[1] + am[1], p[2] + am[2], tangent, 0.03 * u, chord * 1.05);
        }
        const Vec marker {radius * e1[0], radius * e1[1], radius * e1[2]};
        addCube(mesh, p[0] + marker[0], p[1] + marker[1], p[2] + marker[2], 0.14 * u, 0.14 * u, 0.14 * u);
    }
}

vtkSmartPointer<vtkPolyData> toPolyData(const Mesh &mesh)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    points->SetNumberOfPoints(mesh.pointCount());
    for (int i = 0; i < mesh.pointCount(); ++i) {
        const Vec &p = mesh.points()[static_cast<std::size_t>(i)];
        points->SetPoint(i, p[0], p[1], p[2]);
    }
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    for (const auto &face : mesh.faces()) {
        polys->InsertNextCell(static_cast<vtkIdType>(face.size()));
        for (const int index : face) {
            polys->InsertCellPoint(index);
        }
    }
    auto data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(points);
    data->SetPolys(polys);
    return data;
}

} // namespace

bool SupportMeshBuilder::isSupport(const Restraints &r)
{
    return r[2];
}

vtkSmartPointer<vtkPolyData> SupportMeshBuilder::buildSupports(const std::vector<Support> &supports, double unit)
{
    Mesh mesh;
    for (const Support &support : supports) {
        if (isSupport(support.restraints)) {
            addSupportSymbol(mesh, support.position, support.restraints, unit);
        }
    }
    return toPolyData(mesh);
}

vtkSmartPointer<vtkPolyData> SupportMeshBuilder::buildConstraints(const std::vector<Support> &supports, double unit)
{
    Mesh mesh;
    for (const Support &support : supports) {
        const auto &r = support.restraints;
        if (!isSupport(r) && (r[0] || r[1] || r[3] || r[4] || r[5])) {
            addConstraintSymbol(mesh, support.position, r, unit);
        }
    }
    return toPolyData(mesh);
}

vtkSmartPointer<vtkPolyData> SupportMeshBuilder::build(const std::vector<Support> &supports)
{
    Mesh mesh;
    for (const Support &support : supports) {
        Node node;
        node.x = support.position[0];
        node.y = support.position[1];
        node.z = support.position[2];
        node.supports = support.restraints;
        addSupportMesh(mesh, node);
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    points->SetNumberOfPoints(mesh.pointCount());
    for (int i = 0; i < mesh.pointCount(); ++i) {
        const Vec &p = mesh.points()[static_cast<std::size_t>(i)];
        points->SetPoint(i, p[0], p[1], p[2]);
    }
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    for (const auto &face : mesh.faces()) {
        polys->InsertNextCell(static_cast<vtkIdType>(face.size()));
        for (const int index : face) {
            polys->InsertCellPoint(index);
        }
    }
    auto data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(points);
    data->SetPolys(polys);
    return data;
}

} // namespace Structura::Visualization
