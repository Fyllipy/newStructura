#pragma once

#include <vtkInteractorStyleTrackballCamera.h>

class CustomInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    static CustomInteractorStyle* New();
    vtkTypeMacro(CustomInteractorStyle, vtkInteractorStyleTrackballCamera);

    void OnLeftButtonDown() override;   // disable camera rotation on left
    void OnLeftButtonUp() override;
    void OnRightButtonDown() override;  // use right button to rotate (maps to left)
    void OnRightButtonUp() override;
};

