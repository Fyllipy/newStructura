#pragma once

#include "shell/ShellVisual.h"

#include <vtkActor.h>
#include <vtkCellPicker.h>
#include <vtkSmartPointer.h>

class vtkRenderer;

namespace Structura::Visualization {

/// Malha de cascas colorida por vertice (resultados) com as arestas dos elementos em um segundo ator.
class ShellRenderer
{
public:
    ShellRenderer();
    void attach(vtkRenderer *renderer);
    void setMesh(const Structura::Shell::RenderMesh &mesh);
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    /// Indice do quadrilatero sob o pixel (coordenadas de exibicao, origem embaixo); -1 se nenhum.
    [[nodiscard]] int pickQuad(int displayX, int displayY) const;

private:
    vtkRenderer *m_renderer {nullptr};
    vtkSmartPointer<vtkActor> m_faces;
    vtkSmartPointer<vtkActor> m_edges;
    vtkSmartPointer<vtkCellPicker> m_picker;
    bool m_visible {false};
};

} // namespace Structura::Visualization
