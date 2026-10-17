#include "app/services/TrussGenerator.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Structura::App::Services {

const char *TrussGenerator::profileName(Profile profile)
{
    switch (profile) {
    case Profile::Parallel: return "Banzos paralelos";
    case Profile::Gable: return "Duas águas";
    case Profile::Shed: return "Uma água";
    case Profile::Bowstring: return "Arco (bowstring)";
    case Profile::Scissors: return "Tesoura (scissors)";
    }
    return "";
}

const char *TrussGenerator::webName(Web web)
{
    switch (web) {
    case Web::Pratt: return "Pratt";
    case Web::Howe: return "Howe";
    case Web::Warren: return "Warren";
    case Web::WarrenVerticals: return "Warren com montantes";
    case Web::Fink: return "Fink";
    case Web::K: return "K";
    }
    return "";
}

TrussGenerator::Result TrussGenerator::generate(const Params &in)
{
    Result r;
    const double span = std::max(in.span, 1e-6);
    int n = std::max(2, in.panels);
    if (in.web != Web::Warren && n % 2 != 0) {
        ++n;
    }
    r.panels = n;
    const double h = in.height;
    const bool flatEnds = in.profile == Profile::Parallel;
    double hEnd = flatEnds ? h : std::clamp(in.endHeight, 0.0, h);
    if (in.profile == Profile::Scissors && hEnd < 0.1 * h) {
        hEnd = 0.1 * h;
    }

    auto zTop = [&](double x) {
        const double t = x / span;
        switch (in.profile) {
        case Profile::Parallel: return h;
        case Profile::Gable:
        case Profile::Scissors: return hEnd + (h - hEnd) * (1.0 - std::abs(2.0 * t - 1.0));
        case Profile::Shed: return hEnd + (h - hEnd) * t;
        case Profile::Bowstring: return hEnd + (h - hEnd) * 4.0 * t * (1.0 - t);
        }
        return h;
    };
    auto zBot = [&](double x) {
        if (in.profile != Profile::Scissors) {
            return 0.0;
        }
        return 0.5 * (h - hEnd) * (1.0 - std::abs(2.0 * x / span - 1.0));
    };

    std::set<std::pair<int, int>> seen;
    auto node = [&](double x, double z) {
        for (std::size_t i = 0; i < r.nodes.size(); ++i) {
            if (std::abs(r.nodes[i][0] - x) < 1e-6 && std::abs(r.nodes[i][1] - z) < 1e-6) {
                return static_cast<int>(i);
            }
        }
        r.nodes.push_back({x, z});
        return static_cast<int>(r.nodes.size()) - 1;
    };
    auto bar = [&](int a, int b, Role role) {
        if (a == b) {
            return;
        }
        const auto key = std::make_pair(std::min(a, b), std::max(a, b));
        if (!seen.insert(key).second) {
            return;
        }
        r.bars.push_back({a, b, role});
    };
    auto X = [&](double i) { return span * i / n; };
    auto top = [&](int i) { return node(X(i), zTop(X(i))); };
    auto bottom = [&](int i) { return node(X(i), zBot(X(i))); };

    if (in.web == Web::Warren) {
        for (int i = 0; i < n; ++i) {
            const int m = node(X(i + 0.5), zTop(X(i + 0.5)));
            bar(bottom(i), m, Role::Diagonal);
            bar(m, bottom(i + 1), Role::Diagonal);
            bar(bottom(i), bottom(i + 1), Role::BottomChord);
            if (i + 1 < n) {
                bar(m, node(X(i + 1.5), zTop(X(i + 1.5))), Role::TopChord);
            }
        }
        return r;
    }

    // banzo inferior (Fink: so os pontos pares) e superior
    if (in.web == Web::Fink) {
        for (int i = 0; i + 2 <= n; i += 2) {
            bar(bottom(i), bottom(i + 2), Role::BottomChord);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            bar(bottom(i), bottom(i + 1), Role::BottomChord);
        }
    }
    for (int i = 0; i < n; ++i) {
        bar(top(i), top(i + 1), Role::TopChord);
    }

    switch (in.web) {
    case Web::Pratt:
    case Web::Howe:
        for (int i = 0; i <= n; ++i) {
            bar(bottom(i), top(i), Role::Vertical);
        }
        for (int i = 0; i < n; ++i) {
            const bool left = i < n / 2;
            if (in.web == Web::Pratt) {
                left ? bar(top(i), bottom(i + 1), Role::Diagonal) : bar(top(i + 1), bottom(i), Role::Diagonal);
            } else {
                left ? bar(bottom(i), top(i + 1), Role::Diagonal) : bar(bottom(i + 1), top(i), Role::Diagonal);
            }
        }
        break;
    case Web::WarrenVerticals:
        for (int i = 0; i <= n; ++i) {
            bar(bottom(i), top(i), Role::Vertical);
        }
        for (int i = 0; i < n; ++i) {
            i % 2 == 0 ? bar(top(i), bottom(i + 1), Role::Diagonal) : bar(bottom(i), top(i + 1), Role::Diagonal);
        }
        break;
    case Web::Fink:
        for (int i = 1; i < n; i += 2) {
            bar(top(i), bottom(i - 1), Role::Diagonal);
            bar(top(i), bottom(i + 1), Role::Diagonal);
        }
        for (int i = 0; i <= n; i += 2) {
            bar(top(i), bottom(i), Role::Vertical);
        }
        break;
    case Web::K:
        bar(bottom(0), top(0), Role::Vertical);
        bar(bottom(n), top(n), Role::Vertical);
        for (int i = 1; i < n; ++i) {
            const double zb = zBot(X(i)), zt = zTop(X(i));
            const int k = node(X(i), 0.5 * (zb + zt));
            bar(bottom(i), k, Role::Vertical);
            bar(k, top(i), Role::Vertical);
            std::vector<int> toward;
            if (i <= n / 2) {
                toward.push_back(i - 1);
            }
            if (i >= n / 2) {
                toward.push_back(i + 1);
            }
            for (const int j : toward) {
                bar(k, top(j), Role::Diagonal);
                bar(k, bottom(j), Role::Diagonal);
            }
        }
        break;
    case Web::Warren:
        break;
    }
    return r;
}

} // namespace Structura::App::Services
