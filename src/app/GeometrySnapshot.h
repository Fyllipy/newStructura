#pragma once

#include <QUuid>

#include <vector>

namespace Structura::App {

/// Copia da topologia (ids internos e externos preservados) para excluir, dividir e desfazer.
struct GeometrySnapshot {
    struct N {
        QUuid id;
        int externalId {0};
        double x {0.0}, y {0.0}, z {0.0};
    };
    struct B {
        QUuid id, start, end, material, section;
        int externalId {0};
    };
    std::vector<N> nodes;
    std::vector<B> bars;
};

} // namespace Structura::App
