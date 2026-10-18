#pragma once

#include "shell/ShellMesh.h"

#include <string>
#include <vector>

namespace Structura::Shell {

/// Dados prontos para o solver MEF: malha, materiais, apoios e carregamentos por caso. Sem dependencia de Qt nem da DLL.
struct Job {
    struct Node {
        Vec3 x {};
        std::array<int, 6> fixed {{0, 0, 0, 0, 0, 0}};
    };
    struct SpringDef {
        int node {0};
        std::array<double, 6> k {{0, 0, 0, 0, 0, 0}};
        bool compressionOnly {false};
        Vec3 normal {{0.0, 0.0, 1.0}};
    };
    struct NodalForce {
        int node {0};
        std::array<double, 6> f {{0, 0, 0, 0, 0, 0}};
    };
    struct Pressure {
        int shell {0};
        std::array<double, 4> p {{0, 0, 0, 0}};
    };
    struct Body {
        int shell {0};
        Vec3 b {};
    };
    struct Thermal {
        int shell {0};
        double uniform {0.0}, gradient {0.0};
    };
    struct Case {
        std::string name;
        std::vector<NodalForce> nodal;
        std::vector<Pressure> pressure;
        std::vector<Body> body;
        std::vector<Thermal> thermal;
    };
    std::vector<Node> nodes;
    std::vector<SpringDef> springs;
    std::vector<Tie> ties;   ///< nos pendentes de malhas nao conformes (Mesh::ties)
    std::vector<Case> cases;
    std::vector<std::string> warnings;
};

/// Monta o trabalho de analise: um caso por nome em `caseNames` (cargas com `caseName` igual), apoios e molas.
Job buildJob(const Model &model, const Mesh &mesh, const std::vector<std::string> &caseNames);

/// Areas tributarias dos nos (soma de 1/4 da area de cada elemento do objeto `object`; -1 = todos).
std::vector<double> tributaryAreas(const Mesh &mesh, int object = -1);

} // namespace Structura::Shell
