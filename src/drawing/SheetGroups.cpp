#include "drawing/SheetGroups.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Structura::Drawing {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

int groupOf(const Entity &entity)
{
    return std::visit([](const auto &e) { return e.group; }, entity);
}

void setGroup(Entity &entity, int group)
{
    std::visit([&](auto &e) { e.group = group; }, entity);
}

void grow(Rect &r, double x, double y)
{
    r.x0 = std::min(r.x0, x);
    r.y0 = std::min(r.y0, y);
    r.x1 = std::max(r.x1, x);
    r.y1 = std::max(r.y1, y);
}

double distanceToSegment(Point p, Point a, Point b)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 1e-12 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

double distanceToEntity(const Entity &entity, Point p)
{
    if (const auto *line = std::get_if<Line>(&entity)) {
        return distanceToSegment(p, line->a, line->b);
    }
    if (const auto *poly = std::get_if<Polyline>(&entity)) {
        double best = kInf;
        const std::size_t n = poly->points.size();
        for (std::size_t i = 0; i + 1 < n; ++i) {
            best = std::min(best, distanceToSegment(p, poly->points[i], poly->points[i + 1]));
        }
        if (poly->closed && n > 2) {
            best = std::min(best, distanceToSegment(p, poly->points.back(), poly->points.front()));
        }
        return best;
    }
    if (const auto *circle = std::get_if<Circle>(&entity)) {
        return std::abs(std::hypot(p.x - circle->center.x, p.y - circle->center.y) - circle->radius);
    }
    const Rect r = entityBounds(entity);
    const double dx = std::max({r.x0 - p.x, 0.0, p.x - r.x1}), dy = std::max({r.y0 - p.y, 0.0, p.y - r.y1});
    return std::hypot(dx, dy);
}

} // namespace

Rect entityBounds(const Entity &entity)
{
    Rect r {kInf, kInf, -kInf, -kInf};
    if (const auto *line = std::get_if<Line>(&entity)) {
        grow(r, line->a.x, line->a.y);
        grow(r, line->b.x, line->b.y);
    } else if (const auto *poly = std::get_if<Polyline>(&entity)) {
        for (const auto &p : poly->points) {
            grow(r, p.x, p.y);
        }
    } else if (const auto *circle = std::get_if<Circle>(&entity)) {
        grow(r, circle->center.x - circle->radius, circle->center.y - circle->radius);
        grow(r, circle->center.x + circle->radius, circle->center.y + circle->radius);
    } else if (const auto *text = std::get_if<Text>(&entity)) {
        // largura estimada: ~0,6 da altura por caractere visivel (bytes UTF-8 de continuacao nao contam)
        int chars = 0;
        for (const unsigned char c : text->text) {
            chars += (c & 0xC0) != 0x80 ? 1 : 0;
        }
        const double w = chars * text->height * 0.6, h = text->height;
        const double x0 = text->align == TextAlign::Left ? 0.0 : (text->align == TextAlign::Center ? -w / 2.0 : -w);
        const bool vertical = std::abs(std::fmod(std::abs(text->rotationDeg), 180.0) - 90.0) < 1.0;
        if (vertical) {
            grow(r, text->position.x - h, text->position.y + x0);
            grow(r, text->position.x + h * 0.3, text->position.y + x0 + w);
        } else {
            grow(r, text->position.x + x0, text->position.y - h * 0.25);
            grow(r, text->position.x + x0 + w, text->position.y + h);
        }
    }
    return r;
}

Rect sheetBounds(const Sheet &sheet)
{
    Rect r {kInf, kInf, -kInf, -kInf};
    for (const auto &entity : sheet.entities) {
        const Rect e = entityBounds(entity);
        if (e.x0 <= e.x1) {
            grow(r, e.x0, e.y0);
            grow(r, e.x1, e.y1);
        }
    }
    return r;
}

Rect groupBounds(const Sheet &sheet, int group, bool *valid)
{
    Rect r {kInf, kInf, -kInf, -kInf};
    bool any = false;
    for (const auto &entity : sheet.entities) {
        if (groupOf(entity) != group) {
            continue;
        }
        const Rect e = entityBounds(entity);
        if (e.x0 > e.x1) {
            continue;
        }
        grow(r, e.x0, e.y0);
        grow(r, e.x1, e.y1);
        any = true;
    }
    if (valid) {
        *valid = any;
    }
    return r;
}

int hitGroup(const Sheet &sheet, Point p, double tol)
{
    int best = 0;
    double bestDistance = tol;
    for (const auto &entity : sheet.entities) {
        const int group = groupOf(entity);
        if (group <= 0) {
            continue;
        }
        const double d = distanceToEntity(entity, p);
        if (d < bestDistance) {
            bestDistance = d;
            best = group;
        }
    }
    return best;
}

int findGroup(const Sheet &sheet, const std::string &key)
{
    for (const auto &g : sheet.groups) {
        if (g.key == key) {
            return g.id;
        }
    }
    return 0;
}

void translateGroup(Sheet &sheet, int group, double dx, double dy)
{
    for (auto &entity : sheet.entities) {
        if (groupOf(entity) != group) {
            continue;
        }
        std::visit(
            [&](auto &e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, Line>) {
                    e.a.x += dx;
                    e.a.y += dy;
                    e.b.x += dx;
                    e.b.y += dy;
                } else if constexpr (std::is_same_v<T, Polyline>) {
                    for (auto &p : e.points) {
                        p.x += dx;
                        p.y += dy;
                    }
                } else if constexpr (std::is_same_v<T, Circle>) {
                    e.center.x += dx;
                    e.center.y += dy;
                } else {
                    e.position.x += dx;
                    e.position.y += dy;
                }
            },
            entity);
    }
}

void eraseGroup(Sheet &sheet, int group)
{
    sheet.entities.erase(std::remove_if(sheet.entities.begin(), sheet.entities.end(), [group](const Entity &e) { return groupOf(e) == group; }), sheet.entities.end());
}

double overlapWithOccupied(const Sheet &sheet, const Rect &bounds, double margin)
{
    double total = 0.0;
    for (const auto &o : sheet.occupied) {
        if (!bounds.intersects(o, margin)) {
            continue;
        }
        const double w = std::min(bounds.x1 + margin, o.x1) - std::max(bounds.x0 - margin, o.x0);
        const double h = std::min(bounds.y1 + margin, o.y1) - std::max(bounds.y0 - margin, o.y0);
        total += std::max(0.0, w) * std::max(0.0, h) + 0.01;
    }
    return total;
}

void commitGroup(Sheet &target, const Sheet &scratch, const std::string &key)
{
    if (scratch.entities.empty()) {
        return;
    }
    const int id = target.beginGroup(key);
    Rect bounds {kInf, kInf, -kInf, -kInf};
    for (Entity entity : scratch.entities) {
        const Rect e = entityBounds(entity);
        if (e.x0 <= e.x1) {
            grow(bounds, e.x0, e.y0);
            grow(bounds, e.x1, e.y1);
        }
        setGroup(entity, id);
        target.entities.push_back(std::move(entity));
    }
    target.endGroup();
    if (bounds.x0 <= bounds.x1) {
        target.occupied.push_back(bounds);
    }
}

} // namespace Structura::Drawing
