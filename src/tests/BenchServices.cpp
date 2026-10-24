#include <QCoreApplication>
#include <QElapsedTimer>

#include "app/services/ModelCheckService.h"

#include <cmath>
#include <cstdio>

using namespace Structura::App::Services;

/// Micro-benchmark dos servicos de aplicacao com um modelo sintetico grande (nao e teste automatico).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const int nx = 40, ny = 40, nz = 6;   // 9600 nos
    ModelCheckService::Input in;
    auto id = [&](int i, int j, int k) { return 1 + i + nx * (j + ny * k); };
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                in.nodes.append({id(i, j, k), i * 3.0, j * 3.0, k * 3.5});
            }
        }
    }
    int barId = 1;
    auto addBar = [&](int a, int b) {
        ModelCheckService::Bar bar;
        bar.id = barId++;
        bar.nodeI = a;
        bar.nodeJ = b;
        bar.hasMaterial = true;
        bar.hasSection = true;
        bar.sectionName = "W 310 x 38,7";
        bar.family = "W Laminado";
        in.bars.append(bar);
    };
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                if (i + 1 < nx) addBar(id(i, j, k), id(i + 1, j, k));
                if (j + 1 < ny) addBar(id(i, j, k), id(i, j + 1, k));
                if (k + 1 < nz) addBar(id(i, j, k), id(i, j, k + 1));
            }
        }
    }
    QElapsedTimer timer;
    timer.start();
    const auto issues = ModelCheckService::run(in);
    std::printf("ModelCheckService::run: %d nos, %d barras -> %lld ms (%d avisos)\n", int(in.nodes.size()), int(in.bars.size()), timer.elapsed(), int(issues.size()));
    return 0;
}
