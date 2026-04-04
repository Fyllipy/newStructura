#include "CustomInteractorStyle.h"

#include <vtkObjectFactory.h>

vtkStandardNewMacro(CustomInteractorStyle);

void CustomInteractorStyle::OnLeftButtonDown()
{
    // Disable default left-button camera interaction (keep for app-level picking)
}

void CustomInteractorStyle::OnLeftButtonUp()
{
}

void CustomInteractorStyle::OnRightButtonDown()
{
    // Map right button to default left-button camera behavior (rotate)
    this->vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
}

void CustomInteractorStyle::OnRightButtonUp()
{
    this->vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}

