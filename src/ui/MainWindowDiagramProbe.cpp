#include "MainWindow.h"

#include "BusyProgress.h"
#include "ViewOptionsBar.h"
#include <QTimer>
#include "viz/SceneController.h"
#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPoint>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkRenderWindow.h>
#include <QVector4D>

#include <cmath>

using Structura::Visualization::PostProcessorRenderer;
using Structura::Visualization::DiagramType;

namespace {

QString sign(double v, int decimals = 2)
{
    return (v > 0.0 ? QStringLiteral("+") : QString()) + QString::number(v, 'f', decimals);
}

} // namespace

/// Agrupa os movimentos do mouse: a leitura sob o cursor (que percorre os diagramas/elementos) roda no maximo ~30 vezes por segundo.
void MainWindow::scheduleDiagramProbe(const QPoint &widgetPos)
{
    m_probePos = widgetPos;
    if (!m_probeTimer) {
        m_probeTimer = new QTimer(this);
        m_probeTimer->setSingleShot(true);
        m_probeTimer->setInterval(33);
        connect(m_probeTimer, &QTimer::timeout, this, [this]() {
            if (!Structura::UI::isBusy()) {
                updateDiagramProbe(m_probePos);
            }
        });
    }
    if (!m_probeTimer->isActive()) {
        m_probeTimer->start();
    }
}

void MainWindow::onDiagramLabelModeChanged(int)
{
    if (!m_postProcessor || !m_diagramLabelCombo) {
        return;
    }
    const int index = m_diagramLabelCombo->currentData().toInt();
    m_postProcessor->setLabelMode(index == 0 ? PostProcessorRenderer::LabelMode::None
                                  : index == 1 ? PostProcessorRenderer::LabelMode::Global
                                               : PostProcessorRenderer::LabelMode::PerMember);
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void MainWindow::onDiagramHatchToggled(bool on)
{
    if (!m_postProcessor) {
        return;
    }
    m_postProcessor->setHatchVisible(on);
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

/// Cartao no canto superior esquerdo: tipo do diagrama, unidade, extremos e escala.
void MainWindow::updateDiagramInfo()
{
    if (!m_vtkWidget) {
        return;
    }
    const bool enable = m_postProcessor && m_postProcessor->isActive();
    if (m_diagramLabelCombo) {
        m_diagramLabelCombo->setEnabled(enable && m_diagramCombo && m_diagramCombo->currentIndex() > 0);
    }
    if (m_diagramHatchCheck) {
        m_diagramHatchCheck->setEnabled(enable && m_diagramCombo && m_diagramCombo->currentIndex() > 0);
    }
    const auto summary = m_postProcessor ? m_postProcessor->summary() : PostProcessorRenderer::Summary {};
    if (!enable || !summary.valid) {
        if (m_diagramInfo) {
            m_diagramInfo->hide();
        }
        if (m_probeCard) {
            m_probeCard->hide();
        }
        if (m_probeDot) {
            m_probeDot->hide();
        }
        return;
    }
    if (!m_diagramInfo) {
        m_diagramInfo = new QLabel(m_vtkWidget);
        m_diagramInfo->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_diagramInfo->setTextFormat(Qt::RichText);
        m_diagramInfo->setStyleSheet(QStringLiteral("QLabel { background: rgba(255,255,255,235); border: 1px solid #cbd4e2; border-radius: 6px; padding: 8px 10px; color: #1e232b; font-size: 11px; }"));
    }
    const double perUnit = summary.metersPerUnit;
    QString scale;
    if (perUnit > 1e-12) {
        // escolhe uma ordenada "redonda": quantos kN (ou kN·m) valem 1 m desenhado
        const double unitsPerMeter = 1.0 / perUnit;
        scale = tr("1 m desenhado = %1 %2").arg(unitsPerMeter, 0, 'g', 3).arg(summary.unit);
    }
    m_diagramInfo->setText(tr("<b style='color:#0d7fb9;'>Diagrama %1</b> <span style='color:#5a6472;'>(%2)</span><br>"
                              "<span style='color:#2d6eff;'>■</span> máx: <b>%3</b> (barra %4)<br>"
                              "<span style='color:#dc3232;'>■</span> mín: <b>%5</b> (barra %6)<br>"
                              "<span style='color:#5a6472;'>%7<br>Passe o mouse sobre o diagrama para ler os valores.</span>")
                               .arg(summary.name, summary.unit, sign(summary.maxValue)).arg(summary.maxElement)
                               .arg(sign(summary.minValue)).arg(summary.minElement).arg(scale));
    m_diagramInfo->adjustSize();
    m_diagramInfo->move(12, 12);
    m_diagramInfo->show();
    m_diagramInfo->raise();
}

/// Leitura dos esforcos sob o cursor: marcador na curva e cartao com os seis esforcos da secao.
void MainWindow::updateDiagramProbe(const QPoint &widgetPos)
{
    auto hide = [this]() {
        if (m_probeCard) {
            m_probeCard->hide();
        }
        if (m_probeDot) {
            m_probeDot->hide();
        }
    };
    if (!m_vtkWidget || !m_sceneController) {
        hide();
        return;
    }
    if (!m_postProcessor || !m_postProcessor->isActive() || m_postProcessor->currentDiagram() == DiagramType::None) {
        updateModelHover(widgetPos);
        return;
    }
    const QMatrix4x4 composite = m_sceneController->compositeMatrix();
    const QSizeF size(m_vtkWidget->width(), m_vtkWidget->height());
    const auto hit = m_postProcessor->probe(composite, size, widgetPos, 22.0);
    if (!hit) {
        hide();
        return;
    }
    if (!m_probeCard) {
        m_probeCard = new QLabel(m_vtkWidget);
        m_probeCard->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_probeCard->setTextFormat(Qt::RichText);
        m_probeCard->setStyleSheet(QStringLiteral("QLabel { background: rgba(255,255,255,245); border: 1px solid #8fa0b8; border-radius: 6px; padding: 7px 10px; color: #1e232b; font-size: 11px; }"));
        m_probeDot = new QLabel(m_vtkWidget);
        m_probeDot->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_probeDot->setFixedSize(11, 11);
        m_probeDot->setStyleSheet(QStringLiteral("QLabel { background: #ffb400; border: 2px solid #1e232b; border-radius: 6px; }"));
    }
    const auto type = m_postProcessor->currentDiagram();
    const QString unit = PostProcessorRenderer::unitOf(type);
    auto row = [&](const QString &name, double value, const QString &u, DiagramType own) {
        const bool current = type == own;
        const QString text = QStringLiteral("%1 = %2 %3").arg(name, sign(value), u);
        return current ? QStringLiteral("<tr><td style='color:#0d7fb9;'><b>%1</b></td></tr>").arg(text)
                       : QStringLiteral("<tr><td style='color:#5a6472;'>%1</td></tr>").arg(text);
    };
    const auto &f = hit->forces;
    const double percent = hit->length > 1e-12 ? hit->x / hit->length * 100.0 : 0.0;
    m_probeCard->setText(QStringLiteral("<div><b>Barra %1</b> · x = %2 m (%3% de %4 m)</div><table cellspacing='0' cellpadding='1'>%5%6%7%8%9%10</table>")
                             .arg(hit->elementId)
                             .arg(hit->x, 0, 'f', 3)
                             .arg(percent, 0, 'f', 0)
                             .arg(hit->length, 0, 'f', 2)
                             .arg(row(QStringLiteral("N"), f.N, QStringLiteral("kN"), DiagramType::N),
                                  row(QStringLiteral("Vy"), f.Vy, QStringLiteral("kN"), DiagramType::Vy),
                                  row(QStringLiteral("Vz"), f.Vz, QStringLiteral("kN"), DiagramType::Vz),
                                  row(QStringLiteral("Mt"), f.Mt, QStringLiteral("kN·m"), DiagramType::Mt),
                                  row(QStringLiteral("My"), f.My, QStringLiteral("kN·m"), DiagramType::My),
                                  row(QStringLiteral("Mz"), f.Mz, QStringLiteral("kN·m"), DiagramType::Mz)));
    Q_UNUSED(unit)
    m_probeCard->adjustSize();
    // marcador sobre o ponto da curva
    const QVector4D clip = composite * QVector4D(static_cast<float>(hit->curvePoint[0]), static_cast<float>(hit->curvePoint[1]), static_cast<float>(hit->curvePoint[2]), 1.0f);
    if (clip.w() > 1e-9f) {
        const QPointF screen((clip.x() / clip.w() + 1.0) * size.width() / 2.0, (1.0 - clip.y() / clip.w()) * size.height() / 2.0);
        m_probeDot->move(qRound(screen.x()) - 5, qRound(screen.y()) - 5);
        m_probeDot->show();
        m_probeDot->raise();
    }
    QPoint pos = widgetPos + QPoint(18, 18);
    if (pos.x() + m_probeCard->width() > m_vtkWidget->width() - 6) {
        pos.setX(widgetPos.x() - m_probeCard->width() - 18);
    }
    if (pos.y() + m_probeCard->height() > m_vtkWidget->height() - 6) {
        pos.setY(widgetPos.y() - m_probeCard->height() - 18);
    }
    m_probeCard->move(pos);
    m_probeCard->show();
    m_probeCard->raise();
}

/// Cartao ao passar o mouse sobre um no ou uma barra (sem diagrama ativo): dados do elemento e, havendo
/// resultados, deslocamentos/reacoes do no ou esforcos maximos e aproveitamento da barra.
void MainWindow::updateModelHover(const QPoint &widgetPos)
{
    auto hideCard = [this]() {
        if (m_probeCard) {
            m_probeCard->hide();
        }
        if (m_probeDot) {
            m_probeDot->hide();
        }
    };
    if (m_command != Command::None || widgetPos.x() < 0 || widgetPos.y() < 0) {
        hideCard();
        return;
    }
    const QUuid nodeId = pickNodeAtWidgetPos(widgetPos);
    const QUuid barId = nodeId.isNull() ? pickBarAtWidgetPos(widgetPos) : QUuid();
    if ((nodeId.isNull() && barId.isNull()) || (!nodeId.isNull() && isNodeHidden(nodeId)) || (!barId.isNull() && isBarHidden(barId))) {
        hideCard();
        return;
    }
    // resultado exibido (combinacao ou caso escolhido na barra inferior)
    const Structura::App::SolverBridge::Result *result = nullptr;
    QString viewName;
    if (!m_resultViews.isEmpty()) {
        const int index = std::clamp(m_viewOptionsBar ? m_viewOptionsBar->currentResultView() : 0, 0, static_cast<int>(m_resultViews.size()) - 1);
        result = &m_resultViews.at(index).result;
        viewName = m_resultViews.at(index).name;
    }
    const bool stale = m_staleLabel && m_staleLabel->isVisible();
    auto num = [](double v, int d = 3) { return QString::number(v, 'f', d); };
    QString html;
    if (!nodeId.isNull()) {
        const auto *node = m_sceneController->findNode(nodeId);
        if (!node) {
            hideCard();
            return;
        }
        html = tr("<b>Nó %1</b> · (%2; %3; %4) m").arg(node->externalId()).arg(num(node->x(), 3), num(node->y(), 3), num(node->z(), 3));
        for (const auto &support : m_supports) {
            if (support.nodeId == node->externalId()) {
                QStringList fixed;
                const char *names[6] = {"Ux", "Uy", "Uz", "Rx", "Ry", "Rz"};
                for (int i = 0; i < 6; ++i) {
                    if (support.restraints[i]) {
                        fixed << QLatin1String(names[i]);
                    }
                }
                html += tr("<br>Apoio: %1").arg(fixed.isEmpty() ? tr("livre") : fixed.join(QLatin1Char(' ')));
            }
        }
        if (result) {
            for (const auto &nr : result->nodeResults) {
                if (nr.nodeId != node->externalId()) {
                    continue;
                }
                html += tr("<br><span style='color:#5a6472;'>%1%2</span><table cellspacing='0' cellpadding='1'>").arg(viewName, stale ? tr(" (desatualizado)") : QString());
                html += tr("<tr><td>u = (%1; %2; %3) mm</td></tr>").arg(num(nr.displacements[0] * 1000, 2), num(nr.displacements[1] * 1000, 2), num(nr.displacements[2] * 1000, 2));
                html += tr("<tr><td>θ = (%1; %2; %3) mrad</td></tr>").arg(num(nr.displacements[3] * 1000, 3), num(nr.displacements[4] * 1000, 3), num(nr.displacements[5] * 1000, 3));
                bool reaction = false;
                for (const double r : nr.reactions) {
                    reaction = reaction || std::abs(r) > 1e-9;
                }
                if (reaction) {
                    html += tr("<tr><td>R = (%1; %2; %3) kN</td></tr><tr><td>M = (%4; %5; %6) kN·m</td></tr>")
                                .arg(num(nr.reactions[0], 2), num(nr.reactions[1], 2), num(nr.reactions[2], 2), num(nr.reactions[3], 2), num(nr.reactions[4], 2), num(nr.reactions[5], 2));
                }
                html += QStringLiteral("</table>");
                break;
            }
        }
    } else {
        const auto *bar = m_sceneController->findBar(barId);
        const auto *a = bar ? m_sceneController->findNode(bar->startNodeId()) : nullptr;
        const auto *b = bar ? m_sceneController->findNode(bar->endNodeId()) : nullptr;
        if (!bar || !a || !b) {
            hideCard();
            return;
        }
        QString section = tr("(sem perfil)"), material;
        for (const auto &s : m_sections) {
            if (s.uuid == bar->sectionId()) {
                section = s.name;
            }
        }
        for (const auto &m : m_materials) {
            if (m.uuid == bar->materialId()) {
                material = m.name;
            }
        }
        const double length = std::sqrt((a->x() - b->x()) * (a->x() - b->x()) + (a->y() - b->y()) * (a->y() - b->y()) + (a->z() - b->z()) * (a->z() - b->z()));
        html = tr("<b>Barra %1</b> · nós %2 → %3 · L = %4 m<br>Perfil: %5%6")
                   .arg(bar->externalId()).arg(a->externalId()).arg(b->externalId()).arg(num(length, 3), section, material.isEmpty() ? QString() : tr(" · %1").arg(material));
        const auto eta = m_designEta.constFind(bar->externalId());
        if (eta != m_designEta.constEnd()) {
            const double v = *eta;
            html += tr("<br>Aproveitamento: <b style='color:%1;'>η = %2</b>").arg(v > 1.0 ? QStringLiteral("#c0392b") : (v > 0.9 ? QStringLiteral("#b9770e") : QStringLiteral("#1b7f3b")), num(v, 3));
        }
        if (result) {
            for (const auto &mr : result->memberResults) {
                if (mr.memberId != bar->externalId()) {
                    continue;
                }
                auto big = [&](int c) { return std::max(std::abs(mr.localForces[static_cast<std::size_t>(c)]), std::abs(mr.localForces[static_cast<std::size_t>(c + 6)])); };
                html += tr("<br><span style='color:#5a6472;'>%1%2 — valores máximos nas extremidades</span>").arg(viewName, stale ? tr(" (desatualizado)") : QString());
                html += tr("<br>N = %1 kN · Vy = %2 · Vz = %3 kN<br>T = %4 · My = %5 · Mz = %6 kN·m")
                            .arg(num(big(0), 2), num(big(1), 2), num(big(2), 2), num(big(3), 2), num(big(4), 2), num(big(5), 2));
                break;
            }
        }
    }
    if (!m_probeCard) {
        m_probeCard = new QLabel(m_vtkWidget);
        m_probeCard->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_probeCard->setTextFormat(Qt::RichText);
        m_probeCard->setStyleSheet(QStringLiteral("QLabel { background: rgba(255,255,255,245); border: 1px solid #8fa0b8; border-radius: 6px; padding: 7px 10px; color: #1e232b; font-size: 11px; }"));
        m_probeDot = new QLabel(m_vtkWidget);
        m_probeDot->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_probeDot->setFixedSize(11, 11);
        m_probeDot->setStyleSheet(QStringLiteral("QLabel { background: #ffb400; border: 2px solid #1e232b; border-radius: 6px; }"));
    }
    m_probeDot->hide();
    m_probeCard->setText(html);
    m_probeCard->adjustSize();
    QPoint pos = widgetPos + QPoint(18, 18);
    if (pos.x() + m_probeCard->width() > m_vtkWidget->width() - 6) {
        pos.setX(widgetPos.x() - m_probeCard->width() - 18);
    }
    if (pos.y() + m_probeCard->height() > m_vtkWidget->height() - 6) {
        pos.setY(widgetPos.y() - m_probeCard->height() - 18);
    }
    m_probeCard->move(pos);
    m_probeCard->show();
    m_probeCard->raise();
}
