#include "MainWindow.h"

#include "TemporaryAnalysisResultsDialog.h"
#include "app/SolverBridge.h"
#include "app/services/AnalysisService.h"
#include "app/services/DatModelIoService.h"
#include "viz/SceneController.h"
#include "viz/LocalCoordinateSystem.h"
#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QComboBox>
#include <QMessageBox>
#include <QSlider>
#include <QStatusBar>

#include <unordered_map>

// ── feedPostProcessor ────────────────────────────────────────────────────────
// Converte resultados do solver em BeamElementResults para o pós-processador.

static void feedPostProcessor(
    MainWindow* window,
    Structura::Visualization::PostProcessorRenderer* pp,
    const Structura::App::SolverBridge::ModelData& model,
    const Structura::App::SolverBridge::Result& result,
    int analysisRevision,
    QComboBox* diagramCombo,
    QSlider* diagramScaleSlider,
    QComboBox* scaleModeCombo,
    QAction* showDiagramsAction,
    QAction* hideDiagramsAction)
{
    // Mapa rápido: memberId → cargas distribuídas
    std::unordered_map<int, Structura::App::SolverBridge::MemberLoadData> loadMap;
    for (const auto& ml : model.memberLoads) {
        loadMap[ml.memberId] = ml;
    }

    // Mapa rápido: nodeId → coordenadas
    std::unordered_map<int, std::array<double, 3>> nodeMap;
    for (const auto& nd : model.nodes) {
        nodeMap[nd.id] = {nd.x, nd.y, nd.z};
    }

    // Mapa rápido: memberId → MemberData (para nodeI/nodeJ)
    std::unordered_map<int, Structura::App::SolverBridge::MemberData> memberMap;
    for (const auto& mb : model.members) {
        memberMap[mb.id] = mb;
    }

    // Construir BeamElementResults para cada MemberResult
    Structura::Geometry::DefaultLocalAxisProvider lcsProvider;
    std::vector<Structura::Visualization::BeamElementResults> postResults;
    postResults.reserve(result.memberResults.size());

    for (const auto& mr : result.memberResults) {
        const auto mbIt = memberMap.find(mr.memberId);
        if (mbIt == memberMap.end()) continue;
        const auto& mb = mbIt->second;

        const auto n1It = nodeMap.find(mb.nodeI);
        const auto n2It = nodeMap.find(mb.nodeJ);
        if (n1It == nodeMap.end() || n2It == nodeMap.end()) continue;

        Structura::Visualization::BeamElementResults ber;
        ber.externalId = mr.memberId;
        ber.node1 = n1It->second;
        ber.node2 = n2It->second;

        // Eixos locais: se o solver forneceu auxK, ele é o vetor vAux, não um ponto global.
        const auto& ak = mr.auxK;
        if (ak[0] != 0.0 || ak[1] != 0.0 || ak[2] != 0.0) {
            // Calcula xLocal
            double dx = ber.node2[0] - ber.node1[0];
            double dy = ber.node2[1] - ber.node1[1];
            double dz = ber.node2[2] - ber.node1[2];
            double len = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (len > 1e-9) {
                ber.xLocal = {dx/len, dy/len, dz/len};
            } else {
                ber.xLocal = {1.0, 0.0, 0.0};
            }

            // zLocal = xLocal x auxK
            auto cross = [](const std::array<double,3>& a, const std::array<double,3>& b) -> std::array<double,3> {
                return {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]};
            };
            std::array<double, 3> vAux = {ak[0], ak[1], ak[2]};
            auto zL = cross(ber.xLocal, vAux);
            double zLen = std::sqrt(zL[0]*zL[0] + zL[1]*zL[1] + zL[2]*zL[2]);
            if (zLen > 1e-9) {
                ber.zLocal = {zL[0]/zLen, zL[1]/zLen, zL[2]/zLen};
            } else {
                ber.zLocal = {0.0, 0.0, 1.0};
            }

            // yLocal = zLocal x xLocal
            ber.yLocal = cross(ber.zLocal, ber.xLocal);
        } else {
            // Fallback se auxK não estiver presente
            const auto lcs = lcsProvider.computeLCS(ber.node1, ber.node2, std::nullopt);
            ber.xLocal = lcs.xPrime;
            ber.yLocal = lcs.yPrime;
            ber.zLocal = lcs.zPrime;
        }

        // Esforços internos nodais mapeados para a convenção estrutural PURA:
        // Nó I (face negativa) = -lf, Nó J (face positiva) = +lf para TODOS os eixos.
        // A inversão visual de Ftool (tração na fibra) é feita exclusivamente no DiagramMeshBuilder.
        const auto& lf = mr.localForces;
        ber.endForces = {
            -lf[0], -lf[1], -lf[2], -lf[3], -lf[4], -lf[5],   // nó i (N, Vy, Vz, Mt, My, Mz)
             lf[6],  lf[7],  lf[8],  lf[9],  lf[10], lf[11]   // nó j (N, Vy, Vz, Mt, My, Mz)
        };

        // Carga distribuída (se existir para este elemento)
        const auto loadIt = loadMap.find(mr.memberId);
        if (loadIt != loadMap.end()) {
            const auto& ml = loadIt->second;
            Structura::Visualization::UniformDistributedLoad udl;
            udl.qx = ml.qx;
            udl.qy = ml.qy;
            udl.qz = ml.qz;
            udl.isLocalSystem =
                (ml.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            ber.distributedLoads.push_back(udl);
        }

        postResults.push_back(std::move(ber));
    }

    // Alimentar o pós-processador
    pp->setResults(postResults, analysisRevision);

    // Habilitar controles de diagrama na footer bar
    if (diagramCombo)       diagramCombo->setEnabled(true);
    if (diagramScaleSlider) diagramScaleSlider->setEnabled(true);
    if (scaleModeCombo)     scaleModeCombo->setEnabled(true);
    if (showDiagramsAction) showDiagramsAction->setEnabled(true);
    if (hideDiagramsAction) hideDiagramsAction->setEnabled(true);

    (void)window; // futuro uso
}

// ── onRunStructuralAnalysis ───────────────────────────────────────────────────

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

        // ── Integração com o pós-processador ──────────────────────────────────
        if (m_postProcessor) {
            ++m_analysisRevision;
            feedPostProcessor(this,
                              m_postProcessor.get(),
                              buildResult.model,
                              result,
                              m_analysisRevision,
                              m_diagramCombo,
                              m_diagramScaleSlider,
                              m_scaleModeCombo,
                              m_showDiagramsAction,
                              m_hideDiagramsAction);
        }

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
