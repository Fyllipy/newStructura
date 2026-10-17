#pragma once

#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkSmartPointer.h>

#include <functional>

class vtkCellPicker;

/**
 * @brief Estilo de camera inspirado no Tekla Structures (os botoes do mouse nao mudam):
 *  - botao direito: gira o modelo em torno do ponto sob o cursor (se houver geometria), senao em torno do centro do modelo;
 *    o giro e do tipo mesa giratoria (eixo Z sempre na vertical, com o polo travado);
 *  - roda do mouse: zoom em direcao ao cursor (o ponto sob o cursor fica parado);
 *  - botao esquerdo desativado (selecao fica por conta da aplicacao); botao do meio faz pan (comportamento padrao).
 */
class CustomInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    static CustomInteractorStyle *New();
    vtkTypeMacro(CustomInteractorStyle, vtkInteractorStyleTrackballCamera);

    void OnLeftButtonDown() override;
    void OnLeftButtonUp() override;
    void OnRightButtonDown() override;
    void OnRightButtonUp() override;
    void OnMouseMove() override;
    void OnMouseWheelForward() override;
    void OnMouseWheelBackward() override;

    /// Pivo usado quando nao ha geometria sob o cursor (preenche 3 coordenadas e devolve true se existir).
    void setFallbackPivot(std::function<bool(double *)> provider) { m_fallbackPivot = std::move(provider); }
    /// false: sempre gira em torno do pivo de reserva (centro do modelo).
    void setRotateAroundCursor(bool enabled) { m_aroundCursor = enabled; }

protected:
    CustomInteractorStyle();

private:
    bool pickPoint(int x, int y, double point[3]);
    bool pointOnPlane(int x, int y, const double planePoint[3], const double normal[3], double out[3]);
    void orbit(const double pivot[3], double yawDeg, double pitchDeg);
    void zoomAt(double factor, int x, int y);

    vtkSmartPointer<vtkCellPicker> m_picker;
    std::function<bool(double *)> m_fallbackPivot;
    bool m_aroundCursor {true};
    bool m_rotating {false};
    double m_pivot[3] {0.0, 0.0, 0.0};
};
