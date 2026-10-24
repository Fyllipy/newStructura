#include <QElapsedTimer>
#include <QMap>
#include <QString>

#include "design/BarDesign.h"
#include "design/DesignSection.h"
#include "viz/structure/SectionShapes.h"

#include <cmath>
#include <cstdio>

using namespace Structura;
using Structura::Visualization::SectionShapes;

static Design::DesignSection makeSection(const QString &family, const QMap<QString, double> &params)
{
    QString err;
    const auto shape = SectionShapes::build(family, params, false, &err);
    Design::ProfileSpec spec;
    spec.family = family.toStdString();
    for (auto it = params.begin(); it != params.end(); ++it) {
        spec.params[it.key().toStdString()] = it.value();
    }
    std::vector<Design::Loop> loops;
    for (const auto &l : shape->loops) {
        loops.emplace_back(l.begin(), l.end());
    }
    return Design::makeDesignSection(spec, loops, 0.0);
}

int main()
{
    QElapsedTimer timer;
    timer.start();
    const auto section = makeSection("W Laminado", {{"d", 300}, {"bf", 150}, {"tw", 8}, {"tf", 12}});
    std::printf("makeDesignSection: %lld ms\n", timer.elapsed());
    const auto tube = makeSection("Tubular Circular", {{"D", 114.3}, {"t", 4.8}});
    for (const Design::DesignSection *sec : {&section, &tube}) {
        Design::BarDesignInput in;
        in.section = sec;
        in.steel = {"aco", 345, 450};
        in.length = 6000;
        for (int c = 0; c < 10; ++c) {
            Design::ComboEffects combo;
            combo.name = "C" + std::to_string(c);
            for (int i = 0; i < 21; ++i) {
                Design::StationEffects st;
                st.x = in.length * i / 20.0;
                st.e.N = -100e3 - c * 5e3;
                st.e.Mx = (80e6 + c * 3e6) * std::sin(3.14159 * i / 20.0);
                st.e.My = 5e6;
                st.e.Vy = 40e3;
                st.e.Vz = 5e3;
                combo.stations.push_back(st);
            }
            in.combos.push_back(combo);
        }
        timer.restart();
        const int reps = 200;
        double acc = 0.0;
        for (int r = 0; r < reps; ++r) {
            acc += Design::designBar(in).maxRatio;
        }
        std::printf("designBar (10 combos x 21 est.) %s: %.3f ms/chamada (acc %.4f)\n", sec == &section ? "W" : "tubo", timer.elapsed() / double(reps), acc);
    }
    return 0;
}
