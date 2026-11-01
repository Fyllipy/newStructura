#include "LoadHypothesisService.h"

#include <algorithm>

namespace Structura::App::Services {

using Structura::App::ProjectData::LoadCombination;

namespace {

double factorOf(const LoadCombination &c, const QString &name, bool *found)
{
    for (const auto &f : c.factors) {
        if (f.first == name) {
            *found = true;
            return f.second;
        }
    }
    *found = false;
    return 0.0;
}

/// Variantes de uma combinacao para uma hipotese: cada variante e a lista de casos que permanecem (os demais do grupo saem).
QVector<QStringList> variantsOf(const LoadHypothesisService::Hypothesis &h, const QStringList &present, bool *truncated)
{
    QVector<QStringList> out;
    switch (h.kind) {
    case LoadHypothesisService::Kind::Optional:
        out.push_back(present);
        out.push_back({});
        break;
    case LoadHypothesisService::Kind::Exclusive:
        for (const QString &c : present) {
            out.push_back({c});
        }
        break;
    case LoadHypothesisService::Kind::Alternating: {
        const int n = std::min<int>(present.size(), 12);
        if (present.size() > n) {
            *truncated = true;
        }
        for (int mask = 1; mask < (1 << n); ++mask) {
            QStringList subset;
            for (int i = 0; i < n; ++i) {
                if (mask & (1 << i)) {
                    subset.push_back(present[i]);
                }
            }
            out.push_back(subset);
        }
        break;
    }
    }
    return out;
}

} // namespace

LoadHypothesisService::Output LoadHypothesisService::expand(const LoadCombination &base, const QVector<Hypothesis> &hypotheses, int maxCombinations)
{
    Output out;
    QVector<LoadCombination> current {base};
    for (const Hypothesis &h : hypotheses) {
        QVector<LoadCombination> next;
        for (const LoadCombination &c : current) {
            QStringList present;
            for (const QString &name : h.cases) {
                bool found = false;
                factorOf(c, name, &found);
                if (found) {
                    present.push_back(name);
                }
            }
            if (present.isEmpty() || (h.kind == Kind::Exclusive && present.size() < 2)) {
                next.push_back(c);
                continue;
            }
            for (const QStringList &keep : variantsOf(h, present, &out.truncated)) {
                LoadCombination v = c;
                v.factors.clear();
                for (const auto &f : c.factors) {
                    if (present.contains(f.first) && !keep.contains(f.first)) {
                        continue;
                    }
                    v.factors.push_back(f);
                }
                if (v.factors.isEmpty()) {
                    continue;
                }
                const QString tag = keep.isEmpty() ? QStringLiteral("sem %1").arg(h.name) : keep.join(QLatin1Char('+'));
                v.name = c.name + QStringLiteral(" | ") + tag;
                next.push_back(v);
                if (next.size() >= maxCombinations) {
                    out.truncated = true;
                    break;
                }
            }
            if (next.size() >= maxCombinations) {
                break;
            }
        }
        current = next;
    }
    out.combinations = current;
    if (out.truncated) {
        out.notes << QStringLiteral("Expansao limitada (maximo de %1 combinacoes ou 12 casos por grupo alternado).").arg(maxCombinations);
    }
    return out;
}

LoadHypothesisService::Output LoadHypothesisService::expandAll(const QVector<LoadCombination> &bases, const QVector<Hypothesis> &hypotheses, int maxCombinations)
{
    Output all;
    for (const LoadCombination &base : bases) {
        Output part = expand(base, hypotheses, maxCombinations);
        all.truncated = all.truncated || part.truncated;
        for (const auto &c : part.combinations) {
            if (all.combinations.size() >= maxCombinations) {
                all.truncated = true;
                break;
            }
            all.combinations.push_back(c);
        }
        for (const QString &n : part.notes) {
            if (!all.notes.contains(n)) {
                all.notes.push_back(n);
            }
        }
    }
    return all;
}

} // namespace Structura::App::Services
