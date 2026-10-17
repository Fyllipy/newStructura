#include "View3DDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::ProjectData::SavedView3D;

View3DDialog::View3DDialog(const SavedView3D &view, Capture capture, QWidget *parent)
    : QDialog(parent)
    , m_view(view)
    , m_capture(std::move(capture))
{
    setWindowTitle(tr("Vista 3D da prancha"));
    setMinimumWidth(460);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("A vista é desenhada só com linhas (arestas visíveis dos perfis, com as linhas de dobra), no estilo de desenho CAD. "
                                "Defina por ângulos ou capture a câmera atual do modelo (a posição em que você deixou a visualização)."));
    intro->setWordWrap(true);
    root->addWidget(intro);
    auto *form = new QFormLayout;
    m_name = new QLineEdit(view.name);
    m_name->setPlaceholderText(tr("ex.: Isométrica sudeste"));
    m_projection = new QComboBox;
    m_projection->addItem(tr("Ortogonal (axonométrica, com escala)"), false);
    m_projection->addItem(tr("Perspectiva"), true);
    m_projection->setCurrentIndex(view.perspective ? 1 : 0);
    m_azimuth = new QDoubleSpinBox;
    m_azimuth->setRange(-360, 360);
    m_azimuth->setDecimals(1);
    m_azimuth->setSuffix(QStringLiteral(" °"));
    m_azimuth->setValue(view.azimuthDeg);
    m_azimuth->setToolTip(tr("0° = vista frontal (olho em −Y, X para a direita); 90° = olho em +X; 180° = fundos; 270° = olho em −X"));
    m_elevation = new QDoubleSpinBox;
    m_elevation->setRange(-89, 89);
    m_elevation->setDecimals(1);
    m_elevation->setSuffix(QStringLiteral(" °"));
    m_elevation->setValue(view.elevationDeg);
    m_fov = new QDoubleSpinBox;
    m_fov->setRange(10, 100);
    m_fov->setSuffix(QStringLiteral(" °"));
    m_fov->setValue(view.fovDeg);
    form->addRow(tr("Nome"), m_name);
    form->addRow(tr("Projeção"), m_projection);
    form->addRow(tr("Azimute"), m_azimuth);
    form->addRow(tr("Elevação"), m_elevation);
    form->addRow(tr("Abertura (perspectiva)"), m_fov);
    root->addLayout(form);

    m_useCaptured = new QCheckBox(tr("Usar a câmera capturada do modelo (ignora azimute/elevação)"));
    m_useCaptured->setChecked(view.explicitCamera);
    root->addWidget(m_useCaptured);
    auto *capturing = new QPushButton(tr("Capturar a câmera atual do modelo"));
    capturing->setEnabled(static_cast<bool>(m_capture));
    root->addWidget(capturing);
    m_cameraInfo = new QLabel;
    m_cameraInfo->setStyleSheet(QStringLiteral("color:#5a6472;"));
    m_cameraInfo->setWordWrap(true);
    root->addWidget(m_cameraInfo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
    connect(capturing, &QPushButton::clicked, this, [this]() { captureCamera(); });
    connect(m_useCaptured, &QCheckBox::toggled, this, [this]() { updateState(); });
    connect(m_projection, &QComboBox::currentIndexChanged, this, [this]() { updateState(); });
    updateState();
}

void View3DDialog::captureCamera()
{
    SavedView3D captured = m_view;
    if (!m_capture || !m_capture(captured)) {
        QMessageBox::information(this, tr("Vista 3D"), tr("Não foi possível ler a câmera do modelo."));
        return;
    }
    m_view.position = captured.position;
    m_view.target = captured.target;
    m_view.up = captured.up;
    m_view.fovDeg = captured.fovDeg;
    m_view.explicitCamera = true;
    m_fov->setValue(captured.fovDeg);
    m_projection->setCurrentIndex(captured.perspective ? 1 : 0);
    m_useCaptured->setChecked(true);
    updateState();
}

void View3DDialog::updateState()
{
    const bool captured = m_useCaptured->isChecked();
    m_azimuth->setEnabled(!captured);
    m_elevation->setEnabled(!captured);
    m_fov->setEnabled(m_projection->currentData().toBool());
    if (captured) {
        m_cameraInfo->setText(tr("Câmera capturada: olho (%1, %2, %3) m, alvo (%4, %5, %6) m.")
                                  .arg(m_view.position[0], 0, 'f', 2).arg(m_view.position[1], 0, 'f', 2).arg(m_view.position[2], 0, 'f', 2)
                                  .arg(m_view.target[0], 0, 'f', 2).arg(m_view.target[1], 0, 'f', 2).arg(m_view.target[2], 0, 'f', 2));
    } else {
        m_cameraInfo->setText(tr("Câmera definida por azimute e elevação em torno do centro do modelo."));
    }
}

SavedView3D View3DDialog::view() const
{
    SavedView3D v = m_view;
    v.name = m_name->text().trimmed();
    v.perspective = m_projection->currentData().toBool();
    v.azimuthDeg = m_azimuth->value();
    v.elevationDeg = m_elevation->value();
    v.fovDeg = m_fov->value();
    v.explicitCamera = m_useCaptured->isChecked() && (m_view.explicitCamera || (m_view.position != std::array<double, 3> {0, 0, 0}));
    return v;
}

} // namespace Structura::UI
