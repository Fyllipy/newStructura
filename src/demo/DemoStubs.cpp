// Edicao Demonstrativa: definicoes vazias dos pontos de entrada dos recursos exclusivos da edicao Comercial.
// A interface compartilhada declara esses metodos; aqui eles nao fazem nada e nao ha nenhum codigo de calculo
// (a interface da Demonstrativa tambem nao oferece os comandos correspondentes).

#include "ui/MainWindow.h"
#include "ui/SvgIcons.h"

#include <QAction>
#include "ui/ShellDialog.h"
#include "app/services/DesignService.h"
#include "app/services/ReportService.h"
#include "app/services/StructuralChecks.h"
#include "viz/ModeShapeRenderer.h"
#include "viz/structure/ConnectionRenderer.h"

// --- janela principal ---------------------------------------------------------------------------------------------
// Na Demonstrativa os recursos comerciais aparecem como botoes desabilitados, sem nenhuma logica: so icone, texto e dica.
void MainWindow::createCommercialActions()
{
    auto make = [this](const QString &label, const char *icon, const QString &what) {
        auto *action = new QAction(label, this);
        action->setIcon(Structura::UI::actionIcon(QString::fromLatin1(icon)));
        action->setToolTip(what + QLatin1Char(' ') + tr("Disponível na versão paga (edição comercial)."));
        action->setEnabled(false);
        return action;
    };
    m_bucklingAction = make(tr("Flambagem\nelastica"), "buckling.svg", tr("Fatores críticos de carga e modos de flambagem elástica linear de uma combinação."));
    m_dynamicsAction = make(tr("Dinamica"), "dynamics.svg", tr("Espectro de resposta, vibração de pisos e resposta no tempo."));
    m_modalAction = make(tr("Analise\nmodal"), "modal.svg", tr("Frequências naturais, períodos, participação de massa e formas modais."));
    m_connectionsAction = make(tr("Ligacoes"), "connection.svg", tr("Verificação de ligações parafusadas e soldadas, com os esforços da barra selecionada."));
    m_copyConnectionsAction = make(tr("Copiar\nligações"), "connection-copy.svg", tr("Copia uma ligação para outros nós."));
    m_exportDstvAction = make(tr("Exportar NC\n(DSTV)"), "dxf.svg", tr("Grava arquivos .nc1 (DSTV) das chapas das ligações para máquinas CNC."));
    m_exportIfcAction = make(tr("Exportar\nIFC"), "dxf.svg", tr("Exporta as peças de perfil em IFC."));
    m_sheetsAction = make(tr("Pranchas"), "sheets.svg", tr("Gera plantas, elevações, lista de barras e detalhes de ligações; exporta PDF e DXF."));
    m_integrityAction = make(tr("Integridade") + QLatin1Char(10) + tr("estrutural"), "model-check.svg", tr("Integridade estrutural: travamento dos pilares, contenções e colapso progressivo."));
    m_compositeAction = make(tr("Vigas e lajes\nmistas"), "composite.svg", tr("Viga mista de aço e concreto e laje mista com forma de aço incorporada."));
    m_designSettingsAction = make(tr("Parâmetros"), "design-params.svg", tr("Aço, comprimentos efetivos, travamentos e combinações do dimensionamento."));
    m_verifyDesignAction = make(tr("Verificar\nperfis"), "check-design.svg", tr("Verifica os perfis adotados com os esforços da última análise."));
    m_optimizeDesignAction = make(tr("Dimensionar\nperfis"), "design-optimize.svg", tr("Adota o perfil mais leve da série que atende, reanalisando até convergir."));
}
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
QWidget *ShellDialog::buildDesignTab() { return nullptr; }
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
