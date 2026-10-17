#pragma once

#include <array>
#include <vector>

#include <vtkSmartPointer.h>

class vtkPolyData;

namespace Structura::Visualization {

/**
 * @brief Geometria dos apoios (porta do `support_renderer` do opensa).
 *
 * Cada apoio e um cubo modular de 0,30 m (3x3x3 celulas) pendurado abaixo do
 * no. Rotacoes liberadas viram chanfros/piramides no topo; translacoes
 * liberadas substituem celulas por rolos (cilindros) ou esferas.
 */
class SupportMeshBuilder
{
public:
    static constexpr double kSupportSize = 0.30;

    /// Restricoes na ordem UX, UY, UZ, RX, RY, RZ (true = restringido).
    using Restraints = std::array<bool, 6>;

    /// Une as malhas de varios apoios em um unico vtkPolyData (sem nos livres).
    struct Support {
        std::array<double, 3> position {0.0, 0.0, 0.0};
        Restraints restraints {};
    };

    /// Apoio = restringe o deslocamento vertical (Z); as demais combinacoes de restricoes sao restricoes nodais.
    [[nodiscard]] static bool isSupport(const Restraints &restraints);
    /// Simbolos de apoio (engaste, articulado, movel) na unidade de tamanho `unit` (m).
    static vtkSmartPointer<vtkPolyData> buildSupports(const std::vector<Support> &supports, double unit);
    /// Simbolos de restricao nodal (barras com batentes por translacao e aneis por rotacao).
    static vtkSmartPointer<vtkPolyData> buildConstraints(const std::vector<Support> &supports, double unit);
    /// Constroi a malha de um conjunto de apoios; vazio se nenhum restringe algo.
    static vtkSmartPointer<vtkPolyData> build(const std::vector<Support> &supports);
};

} // namespace Structura::Visualization
