#pragma once

#include <array>
#include <vector>

#include <vtkActor.h>
#include <vtkSmartPointer.h>

class vtkRenderer;

namespace Structura::Visualization {

/// Malha triangulada colorida das ligacoes (chapas, parafusos, soldas), em um unico ator.
class ConnectionRenderer
{
public:
    struct Mesh {
        std::vector<std::array<float, 3>> points;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<unsigned char, 3>> colors;
        std::vector<std::array<int, 3>> triangles;
    };

    ConnectionRenderer();
    void attach(vtkRenderer *renderer);
    void setMesh(const Mesh &mesh);
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const noexcept { return m_visible; }

private:
    vtkRenderer *m_renderer {nullptr};
    vtkSmartPointer<vtkActor> m_actor;
    bool m_visible {false};
};

} // namespace Structura::Visualization
