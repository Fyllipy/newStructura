#include "DiagramLabelManager.h"

#include <vtkBillboardTextActor3D.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace Structura::Visualization {

void DiagramLabelManager::initialize(vtkRenderer* renderer)
{
    m_renderer = renderer;
}

// ── Criação de label ────────────────────────────────────────────────────────

vtkSmartPointer<vtkBillboardTextActor3D> DiagramLabelManager::createLabel(
    const std::array<double, 3>& position,
    const std::string& text,
    bool isMax) const
{
    vtkNew<vtkBillboardTextActor3D> actor;
    actor->SetInput(text.c_str());
    actor->SetPosition(position[0], position[1], position[2]);

    auto* prop = actor->GetTextProperty();
    prop->SetFontFamilyToArial();
    prop->SetFontSize(12);
    prop->BoldOn();

    // Texto branco sobre fundo escuro semi-transparente
    prop->SetColor(1.0, 1.0, 1.0);
    prop->SetBackgroundColor(0.10, 0.10, 0.12);
    prop->SetBackgroundOpacity(0.82);
    prop->FrameOn();
    prop->SetFrameWidth(1);
    prop->SetFrameColor(isMax ? 0.18 : 0.86, isMax ? 0.43 : 0.20, isMax ? 1.0 : 0.20);

    return vtkSmartPointer<vtkBillboardTextActor3D>(actor);
}

// ── update ──────────────────────────────────────────────────────────────────

void DiagramLabelManager::update(const ExtremaResult& extrema, const QString& unit)
{
    clearAll();

    if (!m_renderer) {
        return;
    }

    const std::string unitStr = unit.isEmpty() ? "" : " " + unit.toStdString();

    auto formatValue = [&](double v) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        if (v >= 0.0) ss << "+";
        ss << v << unitStr;
        return ss.str();
    };

    // Label MAX (azul-branco)
    for (const auto& pt : extrema.maxima) {
        const std::string text = "MAX: " + formatValue(pt.value);
        auto label = createLabel(pt.worldPosition, text, true);
        addToRenderer(label);
        m_labels.push_back(label);
    }

    // Label MIN (vermelho-branco)
    for (const auto& pt : extrema.minima) {
        const std::string text = "MIN: " + formatValue(pt.value);
        auto label = createLabel(pt.worldPosition, text, false);
        addToRenderer(label);
        m_labels.push_back(label);
    }
}

// ── clearAll ────────────────────────────────────────────────────────────────

void DiagramLabelManager::clearAll()
{
    for (auto& label : m_labels) {
        removeFromRenderer(label);
    }
    m_labels.clear();
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void DiagramLabelManager::addToRenderer(vtkSmartPointer<vtkBillboardTextActor3D> actor)
{
    if (m_renderer && actor) {
        m_renderer->AddActor(actor);
    }
}

void DiagramLabelManager::removeFromRenderer(vtkSmartPointer<vtkBillboardTextActor3D> actor)
{
    if (m_renderer && actor) {
        m_renderer->RemoveActor(actor);
    }
}

} // namespace Structura::Visualization
