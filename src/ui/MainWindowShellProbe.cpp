// Legenda de cores e probe dos resultados de cascas na vista principal (Fase 1 do plano de integracao das cascas).
// O ShellDialog continua escolhendo a grandeza; a malha colorida chega aqui pelo mesmo callback `render` e alimenta a cena,
// a legenda e o probe (um unico fluxo de resultados).
#include "MainWindow.h"

#include "ColorLegendOverlay.h"
#include "shell/ShellProbe.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QLabel>
#include <QPoint>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>

#include <cmath>

void MainWindow::applyShellRenderMesh(const Structura::Shell::RenderMesh &mesh)
{
    if (!m_sceneController) {
        return;
    }
    m_shellResultsShown = mesh.hasValues;
    m_sceneController->setShellMesh(mesh);
    // a malha e guardada mesmo sem resultados: a selecao de nos de cascas trabalha sobre ela; legenda e probe exigem `hasValues`
    if (mesh.points.empty()) {
        m_shellRender.reset();
    } else {
        m_shellRender = std::make_shared<const Structura::Shell::RenderMesh>(mesh);
    }
    updateShellLegend();
    refreshShellProbeCard();
    if (m_shellNodeSelectActive || !m_shellNodeSel.empty()) {
        reresolveShellNodeSelection();
        if (m_shellNodeSelectAction) {
            m_shellNodeSelectAction->setEnabled(m_shellRender && m_shellMeshVisible);
        }
    }
}

void MainWindow::setShellViewVisible(bool visible)
{
    m_shellMeshVisible = visible;
    if (m_sceneController) {
        m_sceneController->setShellVisible(visible);
    }
    updateShellLegend();
    refreshShellSupports();
    refreshShellLoads();
    if (!visible) {
        hideShellProbeCard();
    }
}

void MainWindow::updateShellLegend()
{
    const bool show = m_shellRender && m_shellRender->hasValues && m_shellMeshVisible;
    if (show) {
        if (!m_shellLegend && m_vtkWidget) {
            m_shellLegend = new Structura::UI::ColorLegendOverlay(m_vtkWidget);
        }
        if (m_shellLegend) {
            m_shellLegend->setLegend(QString::fromStdString(m_shellRender->title), QString::fromStdString(m_shellRender->unit), m_shellRender->minValue, m_shellRender->maxValue,
                                     m_shellRender->fixedRange, m_shellRender->smoothed);
        }
    } else if (m_shellLegend) {
        m_shellLegend->clearLegend();
    }
    // o probe so existe com resultados visiveis; a selecao de nos, com a malha visivel
    if (m_shellProbeAction) {
        m_shellProbeAction->setEnabled(show);
    }
    const bool meshShown = m_shellRender && !m_shellRender->points.empty() && m_shellMeshVisible;
    if (m_shellNodeSelectAction) {
        m_shellNodeSelectAction->setEnabled(meshShown);
    }
    if (!meshShown && m_shellNodeSelectActive) {
        setShellNodeSelectActive(false);
    }
    if (!show && m_shellProbeActive) {
        setShellProbeActive(false);
    }
}

void MainWindow::positionShellLegend()
{
    if (m_shellLegend && m_shellLegend->hasLegend()) {
        m_shellLegend->placeInParent();
    }
}

void MainWindow::setShellProbeActive(bool active)
{
    if (active && !(m_shellRender && m_shellRender->hasValues && m_shellMeshVisible)) {
        active = false;
        showStatusMessage(tr("Não há resultados de cascas na vista: analise as cascas pelo Editor de cascas."), 4000);
    }
    if (active && m_shellNodeSelectActive) {
        setShellNodeSelectActive(false);   // as duas ferramentas disputam o clique
    }
    m_shellProbeActive = active;
    if (m_shellProbeAction && m_shellProbeAction->isChecked() != active) {
        const QSignalBlocker blocker(m_shellProbeAction);
        m_shellProbeAction->setChecked(active);
    }
    if (m_vtkWidget) {
        m_vtkWidget->setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    if (active) {
        showStatusMessage(tr("Consulta de resultados ativa: clique sobre uma casca colorida."), 5000);
    } else {
        hideShellProbeCard();
    }
}

bool MainWindow::shellProbeClick(const QPoint &widgetPos)
{
    if (!m_shellProbeActive || !m_shellRender || !m_shellRender->hasValues || !m_sceneController) {
        return false;
    }
    const QPoint disp = toDisplayPoint(widgetPos);
    const auto pick = m_sceneController->pickShellPoint(disp.x(), disp.y());
    if (pick.quad < 0) {
        hideShellProbeCard();
        return false;   // fora das cascas: o clique segue o comportamento normal da vista
    }
    m_shellProbeQuad = pick.quad;
    m_shellProbePoint = pick.point;
    m_shellProbeRevision = m_shellRender->geometryRevision;
    showShellProbeCard(widgetPos);
    return true;
}

void MainWindow::hideShellProbeCard()
{
    m_shellProbeQuad = -1;
    if (m_shellProbeCard) {
        m_shellProbeCard->hide();
    }
    if (m_shellProbeDot) {
        m_shellProbeDot->hide();
    }
}

void MainWindow::refreshShellProbeCard()
{
    if (m_shellProbeQuad < 0) {
        return;
    }
    // outra geometria (malha refeita, deformada): o ponto tocado nao vale mais; so cores/grandeza diferentes: reavalia
    if (!m_shellRender || !m_shellRender->hasValues || m_shellRender->geometryRevision != m_shellProbeRevision || !m_shellMeshVisible) {
        hideShellProbeCard();
        return;
    }
    const QPoint where = m_shellProbeCard ? m_shellProbeCard->property("anchor").toPoint() : QPoint();
    showShellProbeCard(where, false);
}

void MainWindow::showShellProbeCard(const QPoint &widgetPos, bool announce)
{
    if (!m_vtkWidget || !m_shellRender) {
        return;
    }
    const auto hit = Structura::Shell::probeQuad(*m_shellRender, m_shellProbeQuad, m_shellProbePoint);
    if (!hit.valid) {
        hideShellProbeCard();
        return;
    }
    if (!m_shellProbeCard) {
        m_shellProbeCard = new QLabel(m_vtkWidget);
        m_shellProbeCard->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_shellProbeCard->setTextFormat(Qt::RichText);
        m_shellProbeCard->setObjectName(QStringLiteral("shellProbeCard"));
        m_shellProbeCard->setStyleSheet(QStringLiteral("QLabel { background: rgba(255,255,255,245); border: 1px solid #8fa0b8; border-radius: 6px; padding: 7px 10px; color: #1e232b; font-size: 11px; }"));
        m_shellProbeDot = new QLabel(m_vtkWidget);
        m_shellProbeDot->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_shellProbeDot->setFixedSize(11, 11);
        m_shellProbeDot->setStyleSheet(QStringLiteral("QLabel { background: #ffb400; border: 2px solid #1e232b; border-radius: 6px; }"));
    }
    auto number = [](double v) { return QString::number(v, 'g', 6); };
    const QString name = hit.objectName.empty() ? tr("(sem nome)") : QString::fromStdString(hit.objectName).toHtmlEscaped();
    const QString unit = QString::fromStdString(hit.unit).toHtmlEscaped();
    QString text = tr("<div><b>Elemento %1</b> · %2</div>").arg(hit.element).arg(name);
    text += QStringLiteral("<div style='color:#5a6472;'>%1</div>").arg(QString::fromStdString(m_shellRender->title).toHtmlEscaped());
    if (hit.hasValue) {
        text += QStringLiteral("<div style='font-size:13px; color:#0d7fb9;'><b>%1 %2</b></div>").arg(number(hit.value), unit);
        // valor exibido (alisado) x valor do proprio elemento: a distincao fica explicita quando diferem
        if (hit.smoothed && hit.hasSolverValue && std::abs(hit.solverValue - hit.value) > 1e-9 * std::max(1.0, std::abs(hit.value))) {
            text += tr("<div style='color:#5a6472;'>valor do elemento (sem média nos nós): %1 %2</div>").arg(number(hit.solverValue), unit);
        } else if (hit.smoothed) {
            text += tr("<div style='color:#5a6472;'>alisado nos nós (igual ao do elemento neste ponto)</div>");
        }
    } else {
        text += tr("<div style='color:#b3261e;'>sem valor válido neste ponto</div>");
    }
    text += tr("<div style='color:#5a6472;'>x = %1 m, y = %2 m, z = %3 m</div>").arg(hit.position[0], 0, 'f', 3).arg(hit.position[1], 0, 'f', 3).arg(hit.position[2], 0, 'f', 3);
    if (hit.deformed) {
        text += tr("<div style='color:#5a6472;'>(coordenadas na geometria sem deformada)</div>");
    }
    text += tr("<div style='color:#5a6472;'>ξ = %1, η = %2%3</div>").arg(hit.xi, 0, 'f', 3).arg(hit.eta, 0, 'f', 3).arg(hit.outside ? tr(" (borda)") : QString());
    m_shellProbeCard->setText(text);
    m_shellProbeCard->adjustSize();
    m_shellProbeCard->setProperty("anchor", widgetPos);

    // marcador no ponto consultado
    const QMatrix4x4 composite = m_sceneController->compositeMatrix();
    const QVector4D clip = composite * QVector4D(static_cast<float>(m_shellProbePoint[0]), static_cast<float>(m_shellProbePoint[1]), static_cast<float>(m_shellProbePoint[2]), 1.0f);
    QPoint anchor = widgetPos;
    if (clip.w() > 1e-9f) {
        const QPointF screen((clip.x() / clip.w() + 1.0) * m_vtkWidget->width() / 2.0, (1.0 - clip.y() / clip.w()) * m_vtkWidget->height() / 2.0);
        m_shellProbeDot->move(qRound(screen.x()) - 5, qRound(screen.y()) - 5);
        m_shellProbeDot->show();
        m_shellProbeDot->raise();
        anchor = QPoint(qRound(screen.x()), qRound(screen.y()));
    }
    QPoint pos = anchor + QPoint(16, 16);
    if (pos.x() + m_shellProbeCard->width() > m_vtkWidget->width() - 6) {
        pos.setX(anchor.x() - m_shellProbeCard->width() - 16);
    }
    if (pos.y() + m_shellProbeCard->height() > m_vtkWidget->height() - 6) {
        pos.setY(anchor.y() - m_shellProbeCard->height() - 16);
    }
    pos.setX(std::max(4, pos.x()));
    pos.setY(std::max(4, pos.y()));
    m_shellProbeCard->move(pos);
    m_shellProbeCard->show();
    m_shellProbeCard->raise();
    if (announce) {
        showStatusMessage(QString::fromStdString(Structura::Shell::describeProbe(hit)), 6000);
    }
}
