#include "app/services/CutListService.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::App::Services {

namespace {

struct Piece {
    QString mark;
    double length {0.0};
};

struct Item {
    double w {0.0}, h {0.0};
    double area {0.0};
};

struct Shelf {
    double y {0.0}, height {0.0}, used {0.0};
};

} // namespace

CutListService::Plan CutListService::compute(const QVector<FabMember> &members, const FabBom &bom, const Options &options)
{
    Plan plan;

    // ---- perfis: um grupo por secao
    std::map<QString, std::vector<Piece>> bySection;
    std::map<QString, double> massPerMeter;
    for (const auto &m : members) {
        if (m.cutLength <= 0.0) {
            continue;
        }
        massPerMeter[m.section] = m.massPerMeter;
        for (int i = 0; i < m.quantity; ++i) {
            bySection[m.section].push_back({m.mark, m.cutLength});
        }
    }
    for (auto &[section, pieces] : bySection) {
        std::sort(pieces.begin(), pieces.end(), [](const Piece &a, const Piece &b) { return a.length > b.length; });
        BarPlan bar;
        bar.section = section;
        bar.massPerMeter = massPerMeter[section];
        std::vector<double> remaining;   // comprimento livre de cada barra
        std::vector<QVector<QPair<QString, double>>> patterns;
        for (const Piece &p : pieces) {
            if (p.length > options.stockLength) {
                plan.warnings << QStringLiteral("Peça %1 (%2 mm) maior que a barra de estoque (%3 mm): emenda necessária.").arg(p.mark).arg(p.length, 0, 'f', 0).arg(options.stockLength, 0, 'f', 0);
                continue;
            }
            bool placed = false;
            for (std::size_t i = 0; i < remaining.size() && !placed; ++i) {
                const double need = p.length + (patterns[i].isEmpty() ? 0.0 : options.kerf);
                if (remaining[i] + 1e-9 >= need) {
                    remaining[i] -= need;
                    patterns[i].push_back({p.mark, p.length});
                    placed = true;
                }
            }
            if (!placed) {
                remaining.push_back(options.stockLength - p.length);
                patterns.push_back({{p.mark, p.length}});
            }
            bar.usedLength += p.length;
        }
        bar.barCount = static_cast<int>(patterns.size());
        bar.stockLengthTotal = bar.barCount * options.stockLength;
        bar.wasteLength = bar.stockLengthTotal - bar.usedLength;   // sobras e serra contam como perda (compra da barra inteira)
        for (auto &p : patterns) {
            bar.patterns.push_back(p);
        }
        plan.barMassKg += bar.stockLengthTotal / 1000.0 * bar.massPerMeter;
        plan.usedBarMassKg += bar.usedLength / 1000.0 * bar.massPerMeter;
        plan.bars.push_back(bar);
    }

    // ---- chapas: prateleiras por espessura
    std::map<long, std::vector<Item>> byThickness;
    for (const auto &row : bom.plates) {
        for (int i = 0; i < row.quantity; ++i) {
            const double w = std::max(row.width, row.height), h = std::min(row.width, row.height);
            byThickness[std::lround(row.thickness * 100.0)].push_back({w, h, row.width * row.height});
        }
    }
    const double sheetL = std::max(options.sheetLength, options.sheetWidth), sheetW = std::min(options.sheetLength, options.sheetWidth);
    for (auto &[key, items] : byThickness) {
        std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.h > b.h; });
        SheetPlan sp;
        sp.thickness = key / 100.0;
        std::vector<std::vector<Shelf>> sheets;
        for (const Item &it : items) {
            double w = it.w, h = it.h;
            if (w > sheetL || h > sheetW) {
                if (h > sheetL || w > sheetW) {
                    ++sp.oversize;
                    continue;
                }
                std::swap(w, h);   // gira 90 graus
            }
            bool placed = false;
            for (auto &sheet : sheets) {
                for (auto &shelf : sheet) {
                    if (h <= shelf.height + 1e-9 && shelf.used + w + options.plateKerf <= sheetL + 1e-9) {
                        shelf.used += w + options.plateKerf;
                        placed = true;
                        break;
                    }
                }
                if (placed) {
                    break;
                }
                double top = 0.0;
                for (const auto &shelf : sheet) {
                    top = std::max(top, shelf.y + shelf.height + options.plateKerf);
                }
                if (top + h <= sheetW + 1e-9) {
                    sheet.push_back({top, h, w + options.plateKerf});
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                sheets.push_back({{0.0, h, w + options.plateKerf}});
            }
            sp.usedArea += it.area;
            ++sp.pieceCount;
        }
        sp.sheetCount = static_cast<int>(sheets.size());
        sp.sheetArea = sp.sheetCount * sheetL * sheetW;
        plan.sheets.push_back(sp);
    }
    if (plan.sheets.size() > 0) {
        for (const auto &s : plan.sheets) {
            if (s.oversize > 0) {
                plan.warnings << QStringLiteral("%1 chapa(s) de %2 mm maiores que a chapa padrão (%3 x %4 mm).").arg(s.oversize).arg(s.thickness, 0, 'f', 1).arg(options.sheetLength, 0, 'f', 0).arg(options.sheetWidth, 0, 'f', 0);
            }
        }
    }
    return plan;
}

} // namespace Structura::App::Services
