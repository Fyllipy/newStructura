#include "MainWindow.h"

#include "TemporaryAnalysisResultsDialog.h"
#include "app/SolverBridge.h"
#include "app/services/AnalysisService.h"
#include "app/services/DatModelIoService.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QStatusBar>

void MainWindow::onRunStructuralAnalysis()
{
    if (!m_sceneController) {
        return;
    }

    auto bars = m_sceneController->bars();
    auto nodes = m_sceneController->nodeInfos();

    if (nodes.empty()) {
        QMessageBox::information(this, tr("Analise estrutural"), tr("Nao ha nos no modelo para analisar."));
        return;
    }
    if (bars.empty()) {
        QMessageBox::information(this, tr("Analise estrutural"), tr("Nao ha barras no modelo para analisar."));
        return;
    }
    if (m_materials.isEmpty()) {
        QMessageBox::warning(this, tr("Analise estrutural"), tr("Cadastre ao menos um material antes de rodar a analise."));
        return;
    }
    if (m_sections.isEmpty()) {
        QMessageBox::warning(this, tr("Analise estrutural"), tr("Cadastre ao menos uma secao antes de rodar a analise."));
        return;
    }

    Structura::App::Services::AnalysisService analysisService;
    auto buildResult = analysisService.buildModel(m_sceneController,
                                                  m_materials,
                                                  m_sections,
                                                  m_supports,
                                                  m_nodalLoads,
                                                  m_memberLoads);

    if (!buildResult.success) {
        QMessageBox::warning(this,
                             tr("Analise estrutural"),
                             tr("Nao foi possivel montar o modelo para o solver:\n%1").arg(buildResult.errors.join(QLatin1Char('\n'))));
        return;
    }

    try {
        Structura::App::SolverBridge solver;
        const auto result = solver.solve(buildResult.model);
        statusBar()->showMessage(tr("Analise concluida com sucesso."), 5000);

        const QString report = analysisService.formatReport(result);

        Structura::UI::TemporaryAnalysisResultsDialog dialog(
            report,
            this);
        dialog.exec();
    } catch (const Structura::App::SolverBridgeError &error) {
        QMessageBox::critical(this,
                              tr("Falha na analise estrutural"),
                              tr("A DLL retornou erro:\n%1").arg(QString::fromStdString(error.what())));
    } catch (const std::exception &error) {
        QMessageBox::critical(this,
                              tr("Falha na analise estrutural"),
                              tr("Erro inesperado:\n%1").arg(QString::fromStdString(error.what())));
    }
}

void MainWindow::resetModel()
{
    setCommand(Command::None);
    m_sceneController->clearAll();
    m_firstBarNodeId = QUuid();
    if (m_selectionModel) {
        m_selectionModel->clear();
    }
    m_materials.clear();
    m_sections.clear();
    m_supports.clear();
    m_nodalLoads.clear();
    m_memberLoads.clear();
    m_lastMaterialId = QUuid();
    m_lastSectionId = QUuid();
    m_lastNodalPreset = {};
    m_lastDistributedPreset = {};
    syncLoadVisuals();
    refreshPropertiesPanel();
}

bool MainWindow::loadFromDat(const QString &filePath)
{
    resetModel();

    Structura::App::Services::DatModelIoService datModelIoService;
    QString errorMessage;
    QStringList warnings;
    if (!datModelIoService.load(filePath,
                                m_sceneController,
                                m_materials,
                                m_sections,
                                m_supports,
                                m_nodalLoads,
                                m_memberLoads,
                                m_lastMaterialId,
                                m_lastSectionId,
                                &errorMessage,
                                &warnings)) {
        QMessageBox::critical(this, tr("Erro"), errorMessage);
        return false;
    }

    if (!warnings.isEmpty()) {
        QMessageBox::warning(this, tr("Erro"), warnings.join(QLatin1Char('\n')));
    }

    syncLoadVisuals();
    syncSupportVisuals();
    refreshPropertiesPanel();
    updateStatus();
    return true;
}

bool MainWindow::saveToDat(const QString &filePath)
{
    Structura::App::Services::DatModelIoService datModelIoService;
    QString errorMessage;
    if (!datModelIoService.save(filePath,
                                m_sceneController,
                                m_materials,
                                m_sections,
                                m_supports,
                                m_nodalLoads,
                                m_memberLoads,
                                &errorMessage)) {
        QMessageBox::critical(this, tr("Erro"), errorMessage);
        return false;
    }

    return true;
}
