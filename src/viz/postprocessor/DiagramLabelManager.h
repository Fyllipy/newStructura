#pragma once

#include "DiagramExtremaFinder.h"

#include <QString>
#include <vtkSmartPointer.h>
#include <array>
#include <string>
#include <vector>

class vtkRenderer;
class vtkBillboardTextActor3D;

namespace Structura::Visualization {

/**
 * @brief Gerencia labels de anotação para máximos e mínimos globais.
 *
 * Usa vtkBillboardTextActor3D — labels que sempre ficam de frente
 * para a câmera, com fundo escuro e texto branco (estilo SAP2000).
 *
 * Por padrão exibe apenas MAX e MIN globais.
 * Limpar e recriar ao trocar de diagrama ou análise.
 */
class DiagramLabelManager {
public:
    DiagramLabelManager() = default;
    ~DiagramLabelManager() = default;

    void initialize(vtkRenderer* renderer);

    /**
     * @brief Recria labels a partir dos extremos fornecidos.
     * Remove labels antigos antes de criar os novos.
     */
    void update(const ExtremaResult& extrema, const QString& unit = QString());

    struct Label {
        std::array<double, 3> position {};
        std::string text;
        int kind {2};   ///< 0 = maximo (azul), 1 = minimo (vermelho), 2 = neutro
    };
    /// Substitui os labels pelos fornecidos.
    void setLabels(const std::vector<Label> &labels);

    /// Remove todos os labels da cena.
    void clearAll();

private:
    vtkSmartPointer<vtkBillboardTextActor3D> createLabel(
        const std::array<double, 3>& position,
        const std::string& text,
        int kind) const;

    void addToRenderer(vtkSmartPointer<vtkBillboardTextActor3D> actor);
    void removeFromRenderer(vtkSmartPointer<vtkBillboardTextActor3D> actor);

    vtkRenderer* m_renderer{nullptr};
    std::vector<vtkSmartPointer<vtkBillboardTextActor3D>> m_labels;
};

} // namespace Structura::Visualization
