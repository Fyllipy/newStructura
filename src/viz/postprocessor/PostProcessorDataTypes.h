#pragma once

#include <array>
#include <vector>

namespace Structura::Visualization {

/// Esforços internos nos extremos do elemento (convenção do solver).
/// Índices [0..5]  = nó i: N1, Vy1, Vz1, Mt1, My1, Mz1
/// Índices [6..11] = nó j: N2, Vy2, Vz2, Mt2, My2, Mz2
struct EndForces {
    double N1{0}, Vy1{0}, Vz1{0}, Mt1{0}, My1{0}, Mz1{0};
    double N2{0}, Vy2{0}, Vz2{0}, Mt2{0}, My2{0}, Mz2{0};
};

/// Carga distribuída uniforme sobre o elemento.
/// Único tipo de carga suportado pelo modelo atual.
struct UniformDistributedLoad {
    double qx{0}, qy{0}, qz{0};
    bool isLocalSystem{true}; ///< true = sistema local, false = sistema global
};

/// Dados completos de um elemento de barra para pós-processamento.
struct BeamElementResults {
    int externalId{0};

    std::array<double, 3> node1{};   ///< Coordenadas mundo do nó i
    std::array<double, 3> node2{};   ///< Coordenadas mundo do nó j

    /// Eixos locais normalizados (calculados via DefaultLocalAxisProvider + auxK).
    /// Idênticos ao sistema local utilizado pelo solver.
    std::array<double, 3> xLocal{}; ///< Direção da barra (i → j)
    std::array<double, 3> yLocal{}; ///< Eixo local Y
    std::array<double, 3> zLocal{}; ///< Eixo local Z

    EndForces endForces;

    /// Cargas distribuídas sobre o elemento. Pode estar vazio.
    std::vector<UniformDistributedLoad> distributedLoads;
};

/// Tipo de diagrama de esforços internos.
enum class DiagramType {
    None,
    N,   ///< Esforço Normal
    Vy,  ///< Cortante Y local
    Vz,  ///< Cortante Z local
    Mt,  ///< Torção
    My,  ///< Momento fletor Y
    Mz   ///< Momento fletor Z
};

/// Modo de normalização da escala visual.
enum class DiagramScaleMode {
    Global,          ///< Usa máximo absoluto de todos os elementos
    CurrentLoadCase, ///< Usa máximo dos elementos visíveis no caso atual
    SelectedElements ///< Usa máximo dos elementos atualmente selecionados
};

} // namespace Structura::Visualization
