#pragma once

#include "app/services/TrussGenerator.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

namespace Structura::UI {

class TrussPreview;

/// Parâmetros de uma treliça a gerar: forma, ponto inicial, perfis e opções do modelo.
struct TrussRequest {
    Structura::App::Services::TrussGenerator::Params shape;
    bool planeYZ {false};         ///< false: plano XZ (vão ao longo de X); true: plano YZ
    double x0 {0.0}, y0 {0.0}, z0 {0.0};   ///< ponto inicial (apoio esquerdo), em m
    int chordSection {0}, verticalSection {0}, diagonalSection {0}, material {0};   ///< índices nas listas informadas
    bool trussBars {true};        ///< barras biarticuladas (só esforço axial)
    bool supports {true};         ///< apoio fixo à esquerda e móvel à direita
    bool lockOutOfPlane {true};   ///< trava o deslocamento fora do plano em todos os nós
};

class TrussDialog final : public QDialog
{
    Q_OBJECT

public:
    TrussDialog(const QStringList &sections, const QStringList &materials, QWidget *parent = nullptr);
    [[nodiscard]] TrussRequest request() const;

private:
    void updatePreview();

    QComboBox *m_profile {nullptr}, *m_web {nullptr}, *m_plane {nullptr};
    QDoubleSpinBox *m_span {nullptr}, *m_height {nullptr}, *m_endHeight {nullptr}, *m_x0 {nullptr}, *m_y0 {nullptr}, *m_z0 {nullptr};
    QSpinBox *m_panels {nullptr};
    QComboBox *m_chord {nullptr}, *m_vertical {nullptr}, *m_diagonal {nullptr}, *m_material {nullptr};
    QCheckBox *m_truss {nullptr}, *m_supports {nullptr}, *m_lock {nullptr};
    TrussPreview *m_preview {nullptr};
};

} // namespace Structura::UI
