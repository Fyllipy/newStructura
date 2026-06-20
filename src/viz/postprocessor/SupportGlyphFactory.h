#pragma once

#include <array>
#include <vector>

#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkActor;

namespace Structura::Visualization {

/**
 * @brief Gera glifos vetoriais para representação de apoios estruturais.
 *
 * Filosofia: aparência de desenho técnico profissional.
 * Apenas linhas, polylines e polígonos planos. SEM sólidos 3D.
 *
 * Tipos suportados (determinados pelos DOFs restringidos):
 *
 * Articulado (pino):
 *   Triângulo de contorno — 3 linhas, sem preenchimento.
 *
 * Móvel (rolete):
 *   Triângulo + 3 círculos planos (polígonos de 8 vértices) na base.
 *
 * Engaste:
 *   Linha de ancoragem + 5 hachuras diagonais espaçadas.
 *
 * Fixo total (6 DOF):
 *   Engaste bilateral — hachuras em ambos os lados.
 */
class SupportGlyphFactory {
public:
    /// Dados de entrada para um apoio
    struct SupportData {
        std::array<double, 3> position{};
        std::array<bool, 6>  restraints{}; // UX, UY, UZ, RX, RY, RZ
        double                glyphSize{0.5}; ///< Tamanho em metros do mundo
    };

    /**
     * @brief Constrói um vtkPolyData com todos os glifos de apoio.
     * Um único ator para todos os apoios da cena.
     */
    static vtkSmartPointer<vtkPolyData> build(
        const std::vector<SupportData>& supports);

private:
    /// Determina o "nível" do apoio: 0=livre, 1=rolete, 2=pino, 3=engaste, 4=fixo total
    static int restraintLevel(const std::array<bool, 6>& r);

    /// Glifos individuais — acrescentam pontos/células ao polydata passado.
    static void addPinGlyph(vtkPolyData* pd,
                            const std::array<double, 3>& pos,
                            double size,
                            const std::array<double, 3>& up);

    static void addRollerGlyph(vtkPolyData* pd,
                               const std::array<double, 3>& pos,
                               double size,
                               const std::array<double, 3>& up,
                               const std::array<double, 3>& right);

    static void addFixedGlyph(vtkPolyData* pd,
                              const std::array<double, 3>& pos,
                              double size,
                              const std::array<double, 3>& up,
                              const std::array<double, 3>& right,
                              bool bilateral);

    static void appendLine(vtkPolyData* pd,
                           const std::array<double, 3>& a,
                           const std::array<double, 3>& b);

    static void appendPolygon(vtkPolyData* pd,
                              const std::vector<std::array<double, 3>>& verts);
};

} // namespace Structura::Visualization
