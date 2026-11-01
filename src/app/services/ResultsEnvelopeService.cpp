#include "ResultsEnvelopeService.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

ResultsEnvelopeService::MemberEnvelope ResultsEnvelopeService::compute(int member, double length, int comboCount, const Provider &provider, int samples)
{
    MemberEnvelope env;
    env.memberId = member;
    env.length = length;
    samples = std::max(samples, 2);
    for (int s = 0; s < samples; ++s) {
        Point p;
        p.x = length * s / (samples - 1);
        for (int c = 0; c < 6; ++c) {
            p.max[c] = -1e300;
            p.min[c] = 1e300;
        }
        for (int combo = 0; combo < comboCount; ++combo) {
            const Forces f = provider(combo, member, p.x);
            for (int c = 0; c < 6; ++c) {
                if (f[c] > p.max[c]) {
                    p.max[c] = f[c];
                    p.maxCombo[c] = combo;
                }
                if (f[c] < p.min[c]) {
                    p.min[c] = f[c];
                    p.minCombo[c] = combo;
                }
            }
        }
        if (comboCount == 0) {
            p.max.fill(0.0);
            p.min.fill(0.0);
        }
        env.points.push_back(p);
    }
    for (int c = 0; c < 6; ++c) {
        env.peak[c] = 0.0;
        for (const Point &p : env.points) {
            if (std::abs(p.max[c]) > std::abs(env.peak[c])) {
                env.peak[c] = p.max[c];
                env.peakX[c] = p.x;
                env.peakCombo[c] = p.maxCombo[c];
            }
            if (std::abs(p.min[c]) > std::abs(env.peak[c])) {
                env.peak[c] = p.min[c];
                env.peakX[c] = p.x;
                env.peakCombo[c] = p.minCombo[c];
            }
        }
    }
    return env;
}

const char *ResultsEnvelopeService::componentName(int component)
{
    static const char *names[] = {"N", "Vy", "Vz", "T", "My", "Mz"};
    return component >= 0 && component < 6 ? names[component] : "";
}

} // namespace Structura::App::Services
