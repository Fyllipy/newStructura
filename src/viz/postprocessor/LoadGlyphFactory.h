#pragma once

#include <array>
#include <vector>

#include <vtkSmartPointer.h>

class vtkPolyData;

namespace Structura::Visualization {

/**
 * @brief Gera glifos vetoriais para representação de carregamentos.
 *
 * Filosofia: representação 2D de alta legibilidade.
 * Apenas linhas, polylines e polígonos planos. SEM cones 3D.
 *
 * Tipos suportados:
 *
 * Força concentrada nodal:
 *   Haste (linha) + ponta triangular plana (3 vértices).
 *
 * Carga distribuída uniforme:
 *   Linha de referência paralela à barra + N setas espaçadas uniformemente.
 *   N = max(3, int(length / 0.5))
 *
 * Momento nodal:
 *   Arco poligonal de 16 segmentos (180°) + ponta tangencial plana.
 */
class LoadGlyphFactory {
public:
    /// Força concentrada nodal
    struct NodalLoadData {
        std::array<double, 3> position{};
        std::array<double, 3> direction{}; ///< Vetor força normalizado
        double                magnitude{0.0};
        double                arrowSize{0.5};
    };

    /// Carga distribuída uniforme sobre uma barra
    struct DistributedLoadData {
        std::array<double, 3> node1{};
        std::array<double, 3> node2{};
        std::array<double, 3> loadDir{}; ///< Direção da carga normalizada
        double                magnitude{0.0};
        double                arrowSize{0.3};
    };

    /// Momento nodal
    struct NodalMomentData {
        std::array<double, 3> position{};
        std::array<double, 3> axis{};     ///< Eixo de rotação normalizado
        double                magnitude{0.0};
        double                radius{0.3};
    };

    static vtkSmartPointer<vtkPolyData> buildNodalLoads(
        const std::vector<NodalLoadData>& loads);

    static vtkSmartPointer<vtkPolyData> buildDistributedLoads(
        const std::vector<DistributedLoadData>& loads);

    static vtkSmartPointer<vtkPolyData> buildNodalMoments(
        const std::vector<NodalMomentData>& moments);

private:
    static void addArrow(vtkPolyData* pd,
                         const std::array<double, 3>& tip,
                         const std::array<double, 3>& dir,
                         double shaftLen,
                         double headSize,
                         const std::array<double, 3>& perpRef);

    static void appendLine(vtkPolyData* pd,
                           const std::array<double, 3>& a,
                           const std::array<double, 3>& b);

    static std::array<double, 3> perpToDir(const std::array<double, 3>& dir);
};

} // namespace Structura::Visualization
