#include "MainWindow.h"

#include "ShellDialog.h"

#include "TemporaryAnalysisResultsDialog.h"
#include "ModelCheckDialog.h"
#include "app/SolverBridge.h"
#include "app/services/AnalysisService.h"
#include "app/services/CombinationService.h"
#include "ViewOptionsBar.h"
#include "app/services/DatModelIoService.h"
#include "app/services/DesignService.h"
#include "app/services/ResultsPersistence.h"
#include "DesignRunData.h"
#include "viz/SceneController.h"
#include "viz/LocalCoordinateSystem.h"
#include "viz/postprocessor/PostProcessorRenderer.h"
#include "viz/construction/ConstructionScene.h"
#include "AxesPanel.h"

#include <QComboBox>
#include "BusyProgress.h"
#include "app/services/NotionalLoadService.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QSlider>
#include <QUndoStack>
#include "app/UndoRedoService.h"
#include <QStatusBar>

#include <unordered_map>

// ── feedPostProcessor ────────────────────────────────────────────────────────
// Converte resultados do solver em BeamElementResults para o pós-processador.
// `terms` lista (modelo do caso, fator): as cargas de cada caso entram no
// reconstrutor de esforços internos ponderadas pelo fator da combinação.

static void feedPostProcessor(
    MainWindow* window,
    Structura::Visualization::PostProcessorRenderer* pp,
    const Structura::App::SolverBridge::ModelData& geometry,
    const Structura::App::SolverBridge::Result& result,
    const std::vector<std::pair<const Structura::App::SolverBridge::ModelData*, double>>& terms,
    int analysisRevision,
    QComboBox* diagramCombo,
    QSlider* diagramScaleSlider,
    QComboBox* scaleModeCombo,
    QAction* showDiagramsAction,
    QAction* hideDiagramsAction)
{
    // Cargas distribuídas e momentos por barra (todos os casos do termo, ponderados)
    std::unordered_multimap<int, Structura::Visualization::UniformDistributedLoad> loadMap;
    std::unordered_multimap<int, Structura::Visualization::UniformDistributedMoment> momentMap;
    for (const auto& term : terms) {
        if (!term.first) {
            continue;
        }
        const double factor = term.second;
        for (const auto& ml : term.first->memberLoads) {
            Structura::Visualization::UniformDistributedLoad udl;
            udl.qx = ml.qx * factor;
            udl.qy = ml.qy * factor;
            udl.qz = ml.qz * factor;
            udl.isLocalSystem =
                (ml.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            loadMap.emplace(ml.memberId, udl);
        }
        for (const auto& mm : term.first->memberMoments) {
            Structura::Visualization::UniformDistributedMoment udm;
            udm.mx = mm.mx * factor;
            udm.my = mm.my * factor;
            udm.mz = mm.mz * factor;
            udm.isLocalSystem =
                (mm.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            momentMap.emplace(mm.memberId, udm);
        }
    }

    // Mapa rápido: nodeId → coordenadas
    std::unordered_map<int, std::array<double, 3>> nodeMap;
    for (const auto& nd : geometry.nodes) {
        nodeMap[nd.id] = {nd.x, nd.y, nd.z};
    }

    // Mapa rápido: memberId → MemberData (para nodeI/nodeJ)
    std::unordered_map<int, Structura::App::SolverBridge::MemberData> memberMap;
    for (const auto& mb : geometry.members) {
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

        // Excentricidade da secao: os esforcos atuam no eixo da barra (centroide), deslocado do no.
        if (mb.offsetY1 != 0.0 || mb.offsetZ1 != 0.0 || mb.offsetY2 != 0.0 || mb.offsetZ2 != 0.0) {
            for (std::size_t k = 0; k < 3; ++k) {
                ber.node1[k] += mb.offsetY1 * ber.yLocal[k] + mb.offsetZ1 * ber.zLocal[k];
                ber.node2[k] += mb.offsetY2 * ber.yLocal[k] + mb.offsetZ2 * ber.zLocal[k];
            }
        }

        // Esforços internos nodais mapeados para a convenção estrutural PURA:
        // Nó I (face negativa) = -lf, Nó J (face positiva) = +lf para TODOS os eixos.
        // A inversão visual de Ftool (tração na fibra) é feita exclusivamente no DiagramMeshBuilder.
        const auto& lf = mr.localForces;
        ber.endForces = {
            -lf[0], -lf[1], -lf[2], -lf[3], -lf[4], -lf[5],   // nó i (N, Vy, Vz, Mt, My, Mz)
             lf[6],  lf[7],  lf[8],  lf[9],  lf[10], lf[11]   // nó j (N, Vy, Vz, Mt, My, Mz)
        };

        const auto loadRange = loadMap.equal_range(mr.memberId);
        for (auto it = loadRange.first; it != loadRange.second; ++it) {
            ber.distributedLoads.push_back(it->second);
        }
        const auto momentRange = momentMap.equal_range(mr.memberId);
        for (auto it = momentRange.first; it != momentRange.second; ++it) {
            ber.distributedMoments.push_back(it->second);
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

// ── showResultView ───────────────────────────────────────────────────────────
// Alimenta o pós-processador com uma combinação (ou caso) já calculada.

void MainWindow::showResultView(int index)
{
    if (index < 0 || index >= m_resultViews.size() || !m_postProcessor || m_caseRuns.isEmpty()) {
        return;
    }
    const ResultView &view = m_resultViews.at(index);

    std::vector<std::pair<const Structura::App::SolverBridge::ModelData*, double>> terms;
    for (const auto& term : view.terms) {
        const auto runIt = m_caseRuns.constFind(term.first);
        if (runIt != m_caseRuns.constEnd()) {
            terms.emplace_back(&runIt->model, term.second);
        }
    }
    if (terms.empty()) {
        return;
    }

    ++m_analysisRevision;
    feedPostProcessor(this,
                      m_postProcessor.get(),
                      *terms.front().first,
                      view.result,
                      terms,
                      m_analysisRevision,
                      m_diagramCombo,
                      m_diagramScaleSlider,
                      m_scaleModeCombo,
                      m_showDiagramsAction,
                      m_hideDiagramsAction);
    if (m_postProcessor->isActive() && m_diagramCombo) {
        onDiagramTypeChanged(m_diagramCombo->currentIndex());
    }
}

// ── onRunStructuralAnalysis ───────────────────────────────────────────────────

void MainWindow::onRunStructuralAnalysis()
{
    if (m_sceneController) {
        const auto issues = checkModel();
        const int errors = Structura::App::Services::ModelCheckService::count(issues, Structura::App::Services::ModelCheckService::Severity::Error);
        if (errors > 0) {
            Structura::UI::ModelCheckDialog dialog(issues, true, this);
            connect(&dialog, &Structura::UI::ModelCheckDialog::focusRequested, this,
                    [this](Structura::App::Services::ModelCheckService::TargetKind kind, int id) { focusCheckTarget(static_cast<int>(kind), id); });
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
        } else if (!issues.isEmpty()) {
            showStatusMessage(tr("Checagem do modelo: %1 aviso(s)/informação(ões) (Ferramentas > Checar modelo).").arg(issues.size()), 6000);
        }
    }
    QString report;
    if (!executeAnalysis(&report)) {
        return;
    }
    Structura::UI::TemporaryAnalysisResultsDialog dialog(report, this);
    dialog.exec();

    // ── Integração com o pós-processador ──────────────────────────────────
    if (m_postProcessor) {
        showResultView(m_viewOptionsBar ? std::max(0, m_viewOptionsBar->currentResultView()) : 0);
    }
}

namespace {

/// Modelo de segunda ordem de uma combinacao (cargas dos casos ponderadas; rigidez reduzida nas combinacoes ultimas).
bool buildSecondOrderModel(const Structura::App::ProjectData::LoadCombination &combination,
                           const QMap<QString, Structura::App::SolverBridge::ModelData> &caseModels, double stiffnessReduction,
                           Structura::App::SolverBridge::ModelData &out, bool secondOrder = true)
{
    using Structura::App::Services::CombinationService;
    std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> terms;
    for (const auto &term : combination.factors) {
        const auto it = caseModels.constFind(term.first);
        if (it != caseModels.constEnd()) {
            terms.emplace_back(&it.value(), term.second);
        }
    }
    if (terms.empty()) {
        return false;
    }
    out = CombinationService::mergeModels(terms);
    out.secondOrder = secondOrder;
    if (secondOrder && !combination.name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive)) {
        for (auto &material : out.materials) {
            material.e *= stiffnessReduction;
        }
    }
    return true;
}

} // namespace

void MainWindow::rebuildResultViews()
{
    using Structura::App::Services::CombinationService;
    const auto cases = CombinationService::effectiveCases(m_extras);
    auto combinations = CombinationService::effectiveCombinations(m_extras);
    const bool withNotional = m_extras.design.notionalLoads;
    if (withNotional) {
        combinations = Structura::App::Services::NotionalLoadService::withVariants(combinations);
    }
    m_resultViews.clear();
    for (const auto& combination : combinations) {
        ResultView view;
        view.name = combination.name;
        view.isCombination = true;
        std::vector<CombinationService::Term> terms;
        bool missing = false;
        for (const auto& term : combination.factors) {
            const auto runIt = m_caseRuns.constFind(term.first);
            if (runIt == m_caseRuns.constEnd()) {
                missing = missing || Structura::App::Services::NotionalLoadService::isNotionalKey(term.first);
                continue;
            }
            terms.push_back({&runIt->result, term.second});
            view.terms.emplace_back(term.first, term.second);
        }
        if (terms.empty() || missing) {
            continue;
        }
        view.result = CombinationService::combine(terms);
        const auto secondOrderRun = m_caseRuns.constFind(QStringLiteral("COMB:") + combination.name);
        if (secondOrderRun != m_caseRuns.constEnd()) {
            view.result = secondOrderRun->result;
            view.terms.clear();
            view.terms.emplace_back(QStringLiteral("COMB:") + combination.name, 1.0);
        }
        m_resultViews.append(std::move(view));
    }
    for (const auto& loadCase : cases) {
        ResultView view;
        view.name = loadCase.name;
        view.isCombination = false;
        view.result = m_caseRuns.value(loadCase.name).result;
        view.terms.emplace_back(loadCase.name, 1.0);
        m_resultViews.append(std::move(view));
    }
}

bool MainWindow::executeAnalysis(QString *reportOut)
{
    if (!m_sceneController) {
        return false;
    }
    // cascas acopladas: as barras precisam estar divididas nos nos das cascas para a continuidade ao longo da linha
    if (m_extras.shell.coupled && !m_extras.shell.objects.empty()) {
        const int pending = splitBarsAtShellNodes(false);
        if (pending > 0) {
            const auto answer = QMessageBox::question(this, tr("Análise estrutural"),
                                                       tr("%1 nó(s) da malha das cascas caem sobre barras sem dividi-las: nesses pontos a barra e a casca não estão ligadas.\n\n"
                                                          "Dividir as barras nesses nós antes de analisar? (Ctrl+Z desfaz)\n"
                                                          "Não = analisar assim mesmo.").arg(pending),
                                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
            if (answer == QMessageBox::Cancel) {
                return false;
            }
            if (answer == QMessageBox::Yes) {
                splitBarsAtShellNodes(true);
            }
        }
    }

    auto bars = m_sceneController->bars();
    auto nodes = m_sceneController->nodeInfos();

    // modelo so de cascas acopladas (sem barras): os nos vem da malha das cascas
    const bool shellsOnly = m_extras.shell.coupled && !m_extras.shell.objects.empty() && bars.empty();
    if (!shellsOnly) {
        if (nodes.empty()) {
            QMessageBox::information(this, tr("Analise estrutural"), tr("Nao ha nos no modelo para analisar."));
            return false;
        }
        if (bars.empty()) {
            QMessageBox::information(this, tr("Analise estrutural"), tr("Nao ha barras no modelo para analisar."));
            return false;
        }
        if (m_materials.isEmpty()) {
            QMessageBox::warning(this, tr("Analise estrutural"), tr("Cadastre ao menos um material antes de rodar a analise."));
            return false;
        }
        if (m_sections.isEmpty()) {
            QMessageBox::warning(this, tr("Analise estrutural"), tr("Cadastre ao menos uma secao antes de rodar a analise."));
            return false;
        }
    }

    using Structura::App::Services::CombinationService;
    Structura::App::Services::AnalysisService analysisService;

    // Cada caso de carga e resolvido separadamente (o solver e linear); as
    // combinacoes sao somas ponderadas dos resultados dos casos.
    const auto cases = CombinationService::effectiveCases(m_extras);
    auto combinations = CombinationService::effectiveCombinations(m_extras);
    if (m_extras.design.notionalLoads) {
        combinations = Structura::App::Services::NotionalLoadService::withVariants(combinations);
    }

    QMap<QString, CaseRun> runs;
    try {
        // 1) montagem dos modelos (le a cena: na thread da interface)
        QVector<QPair<QString, Structura::App::SolverBridge::ModelData>> built;
        for (const auto& loadCase : cases) {
            auto buildResult = analysisService.buildModel(m_sceneController,
                                                          m_materials,
                                                          m_sections,
                                                          m_supports,
                                                          m_nodalLoads,
                                                          m_memberLoads,
                                                          m_extras,
                                                          loadCase.name);
            if (!buildResult.success) {
                QMessageBox::warning(this,
                                     tr("Analise estrutural"),
                                     tr("Nao foi possivel montar o modelo para o solver:\n%1")
                                         .arg(buildResult.errors.join(QLatin1Char('\n'))));
                return false;
            }
            built.append({loadCase.name, std::move(buildResult.model)});
        }
        // forcas nocionais: casos auxiliares (X e Y) proporcionais as cargas gravitacionais de cada caso
        if (m_extras.design.notionalLoads) {
            const int original = static_cast<int>(built.size());
            for (int i = 0; i < original; ++i) {
                for (int axis = 0; axis < 2; ++axis) {
                    built.append({Structura::App::Services::NotionalLoadService::key(axis, built[i].first),
                                  Structura::App::Services::NotionalLoadService::notionalModel(built[i].second, axis, m_extras.design.notionalFactor)});
                }
            }
        }
        // 2) solucao em segundo plano, com janela de progresso
        const bool secondOrder = m_extras.design.secondOrder;
        const double reduction = m_extras.design.stiffnessReduction;
        // Tirantes (so tracao) tornam a resposta nao linear: a superposicao de casos poderia deixar um tirante comprimido
        // numa combinacao; entao cada combinacao e resolvida com suas cargas ponderadas, como na segunda ordem.
        bool hasTie = false;
        for (const auto &item : built) {
            hasTie = hasTie || std::any_of(item.second.members.begin(), item.second.members.end(), [](const auto &m) { return m.tensionOnly; });
        }
        const bool nonlinearCombos = secondOrder || hasTie;
        const int totalSteps = static_cast<int>(built.size()) + (nonlinearCombos ? static_cast<int>(combinations.size()) : 0);
        runs = Structura::UI::runBusy(this, tr("Análise estrutural"), tr("Preparando o solver…"),
                                      [&](Structura::UI::BusyProgress &progress) {
            QMap<QString, CaseRun> out;
            Structura::App::SolverBridge solver;
            int step = 0;
            for (auto &item : built) {
                progress.report(step, totalSteps, tr("Caso de carga %1 de %2 — %3").arg(step + 1).arg(built.size()).arg(item.first));
                CaseRun run;
                run.model = item.second;
                run.result = solver.solve(run.model);
                out.insert(item.first, std::move(run));
                ++step;
            }
            // Segunda ordem: nao ha superposicao, entao cada combinacao e resolvida com suas cargas ponderadas.
            if (nonlinearCombos) {
                QMap<QString, Structura::App::SolverBridge::ModelData> caseModels;
                for (auto it = out.constBegin(); it != out.constEnd(); ++it) {
                    caseModels.insert(it.key(), it->model);
                }
                for (const auto& combination : combinations) {
                    progress.report(step, totalSteps, tr("Combinação %1 (resolvida com tirantes/segunda ordem)").arg(combination.name));
                    ++step;
                    CaseRun run;
                    if (!buildSecondOrderModel(combination, caseModels, reduction, run.model)) {
                        continue;
                    }
                    run.result = solver.solve(run.model);
                    out.insert(QStringLiteral("COMB:") + combination.name, std::move(run));
                }
            }
            progress.report(totalSteps, totalSteps, tr("Concluindo…"));
            return out;
        });
    } catch (const Structura::App::SolverBridgeError &error) {
        reportActionableError(tr("Falha na análise estrutural"), QString::fromStdString(error.what()));
        return false;
    } catch (const std::exception &error) {
        reportActionableError(tr("Falha na análise estrutural"), QString::fromStdString(error.what()));
        return false;
    }

    m_caseRuns = runs;
    markResultsCurrent();
    {
        std::vector<const Structura::App::SolverBridge::ModelData *> models;
        for (auto it = m_caseRuns.constBegin(); it != m_caseRuns.constEnd(); ++it) {
            if (!it.key().startsWith(QLatin1String("COMB:")) && !Structura::App::Services::NotionalLoadService::isNotionalKey(it.key())) {
                models.push_back(&it->model);
            }
        }
        m_analysisFingerprint = Structura::App::Services::ResultsPersistence::fingerprint(models, m_extras.design.secondOrder, m_extras.design.stiffnessReduction);
    }
    rebuildResultViews();

    if (!m_extras.shell.coupled && !m_extras.shell.objects.empty()) {
        statusBar()->showMessage(tr("Analise concluida. Atenção: as cascas NÃO estão acopladas às barras (suas cargas não chegam às vigas): ative \"Acoplar às barras\"."), 12000);
    } else {
        statusBar()->showMessage(tr("Analise concluida com sucesso."), 5000);
    }

    QString report;
    for (const auto& view : m_resultViews) {
        report += QStringLiteral("\n########## %1: %2 ##########\n")
                      .arg(view.isCombination ? tr("COMBINACAO") : tr("CASO"), view.name);
        report += analysisService.formatReport(view.result);
    }

    rebuildResultViewSelector();
    if (m_shellDialog && m_extras.shell.coupled) {
        m_shellDialog->showProjectResults();
    }
    if (reportOut) {
        *reportOut = report;
    }
    return true;
}

bool MainWindow::restoreStoredResults()
{
    using Structura::App::Services::AnalysisService;
    using Structura::App::Services::CombinationService;
    using Structura::App::Services::ResultsPersistence;
    const auto &stored = m_extras.results;
    if (stored.runs.isEmpty() || !m_sceneController) {
        return false;
    }
    if (m_extras.design.notionalLoads) {
        // os casos nocionais nao sao gravados: refaz a analise para gerar as variantes das combinacoes
        m_extras.results = {};
        showStatusMessage(tr("Forças nocionais ativas: rode a análise novamente para gerar as combinações com forças nocionais."), 9000);
        return false;
    }
    AnalysisService analysis;
    QMap<QString, CaseRun> runs;
    QMap<QString, Structura::App::SolverBridge::ModelData> caseModels;
    for (const auto &loadCase : CombinationService::effectiveCases(m_extras)) {
        auto build = analysis.buildModel(m_sceneController, m_materials, m_sections, m_supports, m_nodalLoads, m_memberLoads, m_extras, loadCase.name);
        if (!build.success) {
            return false;
        }
        CaseRun run;
        run.model = build.model;
        runs.insert(loadCase.name, run);
        caseModels.insert(loadCase.name, build.model);
    }
    std::vector<const Structura::App::SolverBridge::ModelData *> models;
    for (auto it = runs.constBegin(); it != runs.constEnd(); ++it) {
        models.push_back(&it->model);
    }
    const QString current = ResultsPersistence::fingerprint(models, m_extras.design.secondOrder, m_extras.design.stiffnessReduction);
    if (current != stored.fingerprint) {
        m_extras.results = {};
        showStatusMessage(tr("Os resultados gravados no arquivo não correspondem mais ao modelo e foram descartados: rode a análise novamente."), 9000);
        return false;
    }
    auto find = [&stored](const QString &name) -> const Structura::App::ProjectData::StoredRun * {
        for (const auto &run : stored.runs) {
            if (run.name == name) {
                return &run;
            }
        }
        return nullptr;
    };
    for (auto it = runs.begin(); it != runs.end(); ++it) {
        const auto *storedRun = find(it.key());
        bool ok = false;
        if (!storedRun) {
            return false;
        }
        it->result = ResultsPersistence::fromStored(*storedRun, it->model, &ok);
        if (!ok) {
            return false;
        }
    }
    bool restoreHasTie = false;
    for (auto it = caseModels.constBegin(); it != caseModels.constEnd(); ++it) {
        restoreHasTie = restoreHasTie || std::any_of(it->members.begin(), it->members.end(), [](const auto &m) { return m.tensionOnly; });
    }
    if (m_extras.design.secondOrder || restoreHasTie) {
        for (const auto &combination : CombinationService::effectiveCombinations(m_extras)) {
            CaseRun run;
            if (!buildSecondOrderModel(combination, caseModels, m_extras.design.stiffnessReduction, run.model, m_extras.design.secondOrder)) {
                continue;
            }
            const QString key = QStringLiteral("COMB:") + combination.name;
            const auto *storedRun = find(key);
            bool ok = false;
            if (!storedRun) {
                return false;
            }
            run.result = ResultsPersistence::fromStored(*storedRun, run.model, &ok);
            if (!ok) {
                return false;
            }
            runs.insert(key, std::move(run));
        }
    }
    m_caseRuns = runs;
    m_analysisFingerprint = stored.fingerprint;
    rebuildResultViews();
    rebuildResultViewSelector();
    m_designEta.clear();
    for (const auto &d : stored.design) {
        m_designEta.insert(d.memberId, d.eta);
    }
    showStatusMessage(tr("Resultados da análise restaurados do arquivo (%1 caso(s)/combinação(ões)).").arg(stored.runs.size()), 7000);
    return true;
}

void MainWindow::collectResultsForSave()
{
    using Structura::App::Services::DesignService;
    using Structura::App::Services::ResultsPersistence;
    auto &results = m_extras.results;
    results.runs.clear();
    results.design.clear();
    results.fingerprint.clear();
    if (m_caseRuns.isEmpty() || m_analysisFingerprint.isEmpty()) {
        return;
    }
    results.fingerprint = m_analysisFingerprint;
    for (auto it = m_caseRuns.constBegin(); it != m_caseRuns.constEnd(); ++it) {
        results.runs.append(ResultsPersistence::toStored(it.key(), it->result));
    }
    // verificacao dos perfis adotados (aproveitamento e verificacao governante de cada barra)
    DesignRunData run;
    if (!buildDesignRun(run)) {
        return;
    }
    const auto verdicts = DesignService::verifyAll(run.effects, run.refs, run.settings, run.lengthFactor);
    for (const auto &v : verdicts) {
        if (!v.supported || v.output.checks.empty()) {
            continue;
        }
        Structura::App::ProjectData::StoredDesignResult d;
        d.memberId = v.memberId;
        d.section = v.sectionName;
        d.eta = v.output.maxRatio;
        d.check = QString::fromStdString(v.output.governingCheck);
        d.combination = QString::fromStdString(v.output.governingCombo);
        d.lengthM = v.lengthM;
        results.design.append(d);
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
    m_extras = {};
    m_currentFilePath.clear();
    m_caseRuns.clear();
    m_analysisFingerprint.clear();
    m_designEta.clear();
    m_resultViews.clear();
    m_extras.loadCases.append({Structura::App::ProjectData::defaultLoadCaseName(),
                               Structura::App::ProjectData::defaultLoadCaseName(), false});
    m_selectedShellObject = -1;
    m_shellResultsShown = false;
    refreshShellVisuals(true);
    if (m_shellDialog) {
        m_shellDialog->close();
    }
    syncPlacementVisuals();
    rebuildCaseSelector();
    rebuildResultViewSelector();
    if (m_axesPanel) {
        m_axesPanel->discard();
    }
    if (m_sceneController && m_sceneController->constructionScene()) {
        m_sceneController->constructionScene()->setAxes({});
    }
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
    m_projectName = QFileInfo(filePath).completeBaseName();

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
                                m_extras,
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

    if (m_sceneController && m_sceneController->constructionScene()) {
        m_sceneController->constructionScene()->setAxes(m_extras.axes);
    }
    m_currentFilePath = filePath;
    applyViewOptionsFromExtras();
    rebuildCaseSelector();
    refreshGroupsPanel();

    syncLoadVisuals();
    syncSupportVisuals();
    ensureDefaultMaterial();
    syncPlacementVisuals();
    m_selectedShellObject = -1;
    m_shellResultsShown = false;
    refreshShellVisuals(true);
    if (m_shellDialog) {
        m_shellDialog->setModel(m_extras.shell);
    }
    refreshPropertiesPanel();
    restoreStoredResults();
    if (m_undoService) {
        m_undoService->stack()->clear();
    }
    m_extraDirty = false;
    if (!m_caseRuns.isEmpty()) {
        markResultsCurrent();
    }
    updateWindowTitle();
    updateStaleIndicator();
    updateStatus();
    return true;
}

bool MainWindow::saveToDat(const QString &filePath, bool remember)
{
    if (remember) {
        m_projectName = QFileInfo(filePath).completeBaseName();
    }
    storeViewOptionsInExtras();
    collectResultsForSave();

    Structura::App::Services::DatModelIoService datModelIoService;
    QString errorMessage;
    if (!datModelIoService.save(filePath,
                                m_sceneController,
                                m_materials,
                                m_sections,
                                m_supports,
                                m_nodalLoads,
                                m_memberLoads,
                                m_extras,
                                &errorMessage)) {
        QMessageBox::critical(this, tr("Erro"), errorMessage);
        return false;
    }

    if (remember) {
        m_currentFilePath = filePath;
        if (m_undoService) {
            m_undoService->stack()->setClean();
        }
        m_extraDirty = false;
        removeRecoveryFiles();
        updateWindowTitle();
    }
    return true;
}
