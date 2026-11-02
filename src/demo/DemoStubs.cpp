// Edicao Demonstrativa: definicoes vazias dos pontos de entrada dos recursos exclusivos da edicao Comercial.
// A interface compartilhada declara esses metodos; aqui eles nao fazem nada e nao ha nenhum codigo de calculo
// (a interface da Demonstrativa tambem nao oferece os comandos correspondentes).

#include "ui/MainWindow.h"
#include "ui/ShellDialog.h"
#include "app/services/DesignService.h"
#include "app/services/ReportService.h"
#include "app/services/StructuralChecks.h"
#include "viz/ModeShapeRenderer.h"
#include "viz/structure/ConnectionRenderer.h"

// --- janela principal ---------------------------------------------------------------------------------------------
void MainWindow::onDesignSettings() {}
void MainWindow::onVerifyDesign() {}
void MainWindow::onOptimizeDesign() {}
void MainWindow::showDesignResults(bool) {}
void MainWindow::ensureDesignPanel() {}
void MainWindow::focusMember(int) {}
void MainWindow::onChangeProfileForMember(int) {}
bool MainWindow::buildDesignRun(DesignRunData &) { return false; }
QString MainWindow::designCacheKey() const { return {}; }
QUuid MainWindow::sectionForCandidate(const Structura::App::ProjectData::SectionInfo &) { return {}; }
void MainWindow::fillDesignReportInput(Structura::App::Services::ReportService::Input &) {}
void MainWindow::addConnectionQuantities(Structura::App::Services::QuantityService::Summary &) const {}
void MainWindow::onCheckConnections() {}
void MainWindow::onOpenConnectionCalculator() {}
void MainWindow::onCopyConnections() {}
void MainWindow::refreshConnectionNodeMembers() {}
void MainWindow::storeNodeMembers(Structura::App::ProjectData::NodeConnection &) const {}
QVector<MainWindow::ConnectionPlacementInfo> MainWindow::connectionPlacements() const { return {}; }
void MainWindow::syncConnectionVisuals() {}
void MainWindow::onCompositeFloor() {}
void MainWindow::onGenerateDrawings() {}
Structura::App::Services::SheetBuilder::Input MainWindow::buildSheetInput() { return {}; }
void MainWindow::onExportDstv() {}
void MainWindow::onExportIfc() {}
void MainWindow::onStructuralIntegrity() {}
void MainWindow::onBucklingAnalysis() {}
void MainWindow::onModalAnalysis() {}
void MainWindow::onDynamics() {}
bool MainWindow::computeModal(const QString &, int, Structura::App::SolverBridge::ModelData &, Structura::App::SolverBridge::Result &, bool *, QString *)
{
    return false;
}
void MainWindow::automatedCapture(const QString &) {}

namespace Structura::UI {
void ShellDialog::runDesign() {}
}

// --- servicos -----------------------------------------------------------------------------------------------------
namespace Structura::App::Services {

QVector<DesignService::MemberVerdict> DesignService::verifyAll(const QVector<MemberEffects> &, const std::vector<MemberRef> &, const DesignSettings &, double,
                                                               const std::function<void(int, int)> &)
{
    return {};
}

std::vector<int> StructuralChecks::storyLevelNodes(const Structura::App::SolverBridge::ModelData &)
{
    return {};   // sem andares impedidos: a analise de 1a ordem nao separa as estruturas nt e lt (amplificacao B1/B2 e do dimensionamento)
}

QVector<ReportService::ItemInfo> ReportService::calculationItems() { return {}; }
Structura::Report::ReportDocument ReportService::buildBar(const Input &, int, const QSet<QString> &, bool, bool) { return {}; }
void ReportService::appendDesignChapters(Structura::Report::ReportDocument &, const Input &, const Options &, const std::function<void(const QString &)> &) {}

} // namespace Structura::App::Services

// --- visualizacao -------------------------------------------------------------------------------------------------
namespace Structura::Visualization {
ConnectionRenderer::ConnectionRenderer() = default;
void ConnectionRenderer::attach(vtkRenderer *) {}
void ConnectionRenderer::setMesh(const Mesh &) {}
void ConnectionRenderer::setVisible(bool) {}
ModeShapeRenderer::ModeShapeRenderer() = default;
ModeShapeRenderer::~ModeShapeRenderer() = default;
void ModeShapeRenderer::attach(vtkRenderer *) {}
void ModeShapeRenderer::render(const std::vector<std::vector<Vec3>> &, bool) {}
} // namespace Structura::Visualization
