#include "CustomInteractorStyle.h"

#include <vtkCamera.h>
#include <vtkCellPicker.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>

#include <algorithm>
#include <cmath>

vtkStandardNewMacro(CustomInteractorStyle);

CustomInteractorStyle::CustomInteractorStyle()
    : m_picker(vtkSmartPointer<vtkCellPicker>::New())
{
    m_picker->SetTolerance(0.0008);
}

void CustomInteractorStyle::OnLeftButtonDown()
{
    // Rotacao no botao esquerdo desativada (o botao esquerdo e da selecao, tratada pela aplicacao).
}

void CustomInteractorStyle::OnLeftButtonUp()
{
}

bool CustomInteractorStyle::pickPoint(int x, int y, double point[3])
{
    vtkRenderer *renderer = this->CurrentRenderer;
    if (!renderer) {
        return false;
    }
    if (m_picker->Pick(x, y, 0.0, renderer) != 0) {
        m_picker->GetPickPosition(point);
        return true;
    }
    return false;
}

bool CustomInteractorStyle::pointOnPlane(int x, int y, const double planePoint[3], const double normal[3], double out[3])
{
    vtkRenderer *renderer = this->CurrentRenderer;
    if (!renderer) {
        return false;
    }
    double near4[4], far4[4];
    renderer->SetDisplayPoint(x, y, 0.0);
    renderer->DisplayToWorld();
    renderer->GetWorldPoint(near4);
    renderer->SetDisplayPoint(x, y, 1.0);
    renderer->DisplayToWorld();
    renderer->GetWorldPoint(far4);
    for (int i = 0; i < 3; ++i) {
        if (std::abs(near4[3]) > 1e-14) near4[i] /= near4[3];
        if (std::abs(far4[3]) > 1e-14) far4[i] /= far4[3];
    }
    double dir[3] = {far4[0] - near4[0], far4[1] - near4[1], far4[2] - near4[2]};
    const double denom = vtkMath::Dot(dir, normal);
    if (std::abs(denom) < 1e-14) {
        return false;
    }
    const double diff[3] = {planePoint[0] - near4[0], planePoint[1] - near4[1], planePoint[2] - near4[2]};
    const double t = vtkMath::Dot(diff, normal) / denom;
    for (int i = 0; i < 3; ++i) {
        out[i] = near4[i] + t * dir[i];
    }
    return true;
}

void CustomInteractorStyle::OnRightButtonDown()
{
    const int *pos = this->Interactor->GetEventPosition();
    this->FindPokedRenderer(pos[0], pos[1]);
    if (!this->CurrentRenderer) {
        return;
    }
    bool have = false;
    if (m_aroundCursor) {
        have = pickPoint(pos[0], pos[1], m_pivot);
    }
    if (!have && m_fallbackPivot) {
        have = m_fallbackPivot(m_pivot);
    }
    if (!have) {
        this->CurrentRenderer->GetActiveCamera()->GetFocalPoint(m_pivot);
    }
    m_rotating = true;
}

void CustomInteractorStyle::OnRightButtonUp()
{
    if (m_rotating) {
        m_rotating = false;
    }
}

void CustomInteractorStyle::orbit(const double pivot[3], double yawDeg, double pitchDeg)
{
    vtkCamera *camera = this->CurrentRenderer->GetActiveCamera();
    double position[3], focal[3], up[3], dop[3];
    camera->GetPosition(position);
    camera->GetFocalPoint(focal);
    camera->GetViewUp(up);
    camera->GetDirectionOfProjection(dop);

    auto rotate = [&](double angle, const double axis[3]) {
        vtkNew<vtkTransform> transform;
        transform->Identity();
        transform->Translate(pivot[0], pivot[1], pivot[2]);
        transform->RotateWXYZ(angle, axis[0], axis[1], axis[2]);
        transform->Translate(-pivot[0], -pivot[1], -pivot[2]);
        transform->TransformPoint(position, position);
        transform->TransformPoint(focal, focal);
        double newUp[3];
        transform->TransformVector(up, newUp);
        std::copy(newUp, newUp + 3, up);
    };

    // mesa giratoria: guinada em torno do eixo Z global
    const double zAxis[3] = {0.0, 0.0, 1.0};
    rotate(yawDeg, zAxis);

    // arfagem em torno do eixo direito da camera (mesmo sentido de vtkCamera::Elevation), sem passar pelos polos
    camera->SetPosition(position);
    camera->SetFocalPoint(focal);
    camera->SetViewUp(up);
    camera->OrthogonalizeViewUp();
    camera->GetViewUp(up);
    camera->GetDirectionOfProjection(dop);
    double right[3];
    vtkMath::Cross(dop, up, right);
    vtkMath::Normalize(right);
    const double axis[3] = {-right[0], -right[1], -right[2]};
    double trialPosition[3] = {position[0], position[1], position[2]};
    double trialFocal[3] = {focal[0], focal[1], focal[2]};
    {
        vtkNew<vtkTransform> transform;
        transform->Identity();
        transform->Translate(pivot[0], pivot[1], pivot[2]);
        transform->RotateWXYZ(pitchDeg, axis[0], axis[1], axis[2]);
        transform->Translate(-pivot[0], -pivot[1], -pivot[2]);
        transform->TransformPoint(position, trialPosition);
        transform->TransformPoint(focal, trialFocal);
    }
    double newDop[3] = {trialFocal[0] - trialPosition[0], trialFocal[1] - trialPosition[1], trialFocal[2] - trialPosition[2]};
    vtkMath::Normalize(newDop);
    const bool nearPole = std::abs(newDop[2]) > 0.9995 && std::abs(newDop[2]) > std::abs(dop[2]);
    if (!nearPole) {
        std::copy(trialPosition, trialPosition + 3, position);
        std::copy(trialFocal, trialFocal + 3, focal);
        dop[0] = newDop[0];
        dop[1] = newDop[1];
        dop[2] = newDop[2];
    }
    camera->SetPosition(position);
    camera->SetFocalPoint(focal);
    if (std::abs(dop[2]) < 0.999) {
        camera->SetViewUp(0.0, 0.0, 1.0);   // horizonte sempre nivelado
    } else {
        camera->SetViewUp(up);
    }
    camera->OrthogonalizeViewUp();
    this->CurrentRenderer->ResetCameraClippingRange();
}

void CustomInteractorStyle::OnMouseMove()
{
    if (!m_rotating) {
        this->vtkInteractorStyleTrackballCamera::OnMouseMove();
        return;
    }
    const int *pos = this->Interactor->GetEventPosition();
    const int *last = this->Interactor->GetLastEventPosition();
    const int *size = this->CurrentRenderer->GetRenderWindow()->GetSize();
    const double perPixel = 200.0 / std::max(1, std::min(size[0], size[1]) + 200);   // graus por pixel
    const double dx = -(pos[0] - last[0]);
    const double dy = -(pos[1] - last[1]);
    orbit(m_pivot, dx * perPixel * 1.4, dy * perPixel * 1.4);
    this->Interactor->Render();
    this->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
}

void CustomInteractorStyle::zoomAt(double factor, int x, int y)
{
    this->FindPokedRenderer(x, y);
    vtkRenderer *renderer = this->CurrentRenderer;
    if (!renderer) {
        return;
    }
    vtkCamera *camera = renderer->GetActiveCamera();
    double dop[3], focal[3], position[3];
    camera->GetDirectionOfProjection(dop);
    camera->GetFocalPoint(focal);
    camera->GetPosition(position);
    double target[3];
    if (!pickPoint(x, y, target)) {
        // plano do foco, paralelo a vista
        if (!pointOnPlane(x, y, focal, dop, target)) {
            std::copy(focal, focal + 3, target);
        }
    }
    if (camera->GetParallelProjection()) {
        double before[3];
        pointOnPlane(x, y, target, dop, before);
        camera->SetParallelScale(camera->GetParallelScale() / factor);
        double after[3];
        pointOnPlane(x, y, target, dop, after);
        const double shift[3] = {before[0] - after[0], before[1] - after[1], before[2] - after[2]};
        for (int i = 0; i < 3; ++i) {
            position[i] += shift[i];
            focal[i] += shift[i];
        }
    } else {
        const double toTarget[3] = {target[0] - position[0], target[1] - position[1], target[2] - position[2]};
        const double depth = vtkMath::Dot(toTarget, dop);
        const double move = depth * (1.0 - 1.0 / factor);
        for (int i = 0; i < 3; ++i) {
            position[i] += dop[i] * move;
            focal[i] += dop[i] * move;
        }
        camera->SetPosition(position);
        camera->SetFocalPoint(focal);
        // o ponto sob o cursor deve continuar sob o cursor: compensa lateralmente
        double after[3];
        if (pointOnPlane(x, y, target, dop, after)) {
            const double shift[3] = {target[0] - after[0], target[1] - after[1], target[2] - after[2]};
            for (int i = 0; i < 3; ++i) {
                position[i] += shift[i];
                focal[i] += shift[i];
            }
        }
    }
    camera->SetPosition(position);
    camera->SetFocalPoint(focal);
    renderer->ResetCameraClippingRange();
    if (this->Interactor->GetLightFollowCamera()) {
        renderer->UpdateLightsGeometryToFollowCamera();
    }
    this->Interactor->Render();
    this->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
}

void CustomInteractorStyle::OnMouseWheelForward()
{
    const int *pos = this->Interactor->GetEventPosition();
    zoomAt(1.18, pos[0], pos[1]);
}

void CustomInteractorStyle::OnMouseWheelBackward()
{
    const int *pos = this->Interactor->GetEventPosition();
    zoomAt(1.0 / 1.18, pos[0], pos[1]);
}
