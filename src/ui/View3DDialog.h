#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

namespace Structura::UI {

/// Define uma vista 3D das pranchas: por angulos (azimute / elevacao) ou pela camera do modelo no momento da captura.
class View3DDialog final : public QDialog
{
    Q_OBJECT

public:
    using Capture = std::function<bool(Structura::App::ProjectData::SavedView3D &)>;
    View3DDialog(const Structura::App::ProjectData::SavedView3D &view, Capture capture, QWidget *parent = nullptr);

    [[nodiscard]] Structura::App::ProjectData::SavedView3D view() const;

private:
    void captureCamera();
    void updateState();

    Structura::App::ProjectData::SavedView3D m_view;
    Capture m_capture;
    QLineEdit *m_name {nullptr};
    QComboBox *m_projection {nullptr};
    QDoubleSpinBox *m_azimuth {nullptr};
    QDoubleSpinBox *m_elevation {nullptr};
    QDoubleSpinBox *m_fov {nullptr};
    QLabel *m_cameraInfo {nullptr};
    QCheckBox *m_useCaptured {nullptr};
};

} // namespace Structura::UI
