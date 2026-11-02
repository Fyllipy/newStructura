#include "MainWindow.h"
#include "SvgIcons.h"

#include <QAction>
#include <QIcon>

/// Acoes exclusivas da edicao Comercial: flambagem, analise modal e dinamica, ligacoes, DSTV/IFC, pranchas, dimensionamento e vigas/lajes mistas.
/// A edicao Demonstrativa nao compila este arquivo (ver src/demo/DemoStubs.cpp): suas acoes nem existem.
void MainWindow::createCommercialActions()
{
    m_bucklingAction = new QAction(tr("Flambagem\nelastica"), this);
    m_bucklingAction->setIcon(Structura::UI::actionIcon(QStringLiteral("buckling.svg")));
    m_bucklingAction->setToolTip(tr("Fatores críticos de carga e modos de flambagem elástica linear de uma combinação"));
    connect(m_bucklingAction, &QAction::triggered, this, &MainWindow::onBucklingAnalysis);
    m_dynamicsAction = new QAction(tr("Dinamica"), this);
    m_dynamicsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("dynamics.svg")));
    m_dynamicsAction->setToolTip(tr("Espectro de resposta (SRSS/CQC), vibração de pisos (NBR 8800, Anexo I) e resposta no tempo"));
    connect(m_dynamicsAction, &QAction::triggered, this, &MainWindow::onDynamics);
    m_modalAction = new QAction(tr("Analise\nmodal"), this);
    m_modalAction->setIcon(Structura::UI::actionIcon(QStringLiteral("modal.svg")));
    m_modalAction->setToolTip(tr("Frequências naturais, períodos, participação de massa e formas modais"));
    connect(m_modalAction, &QAction::triggered, this, &MainWindow::onModalAnalysis);
    m_connectionsAction = new QAction(tr("Ligacoes"), this);
    m_connectionsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("connection.svg")));
    m_connectionsAction->setToolTip(tr("Verifica grupos de parafusos e filetes de solda (NBR 8800:2024, 6.2 e 6.3), com esforços da barra selecionada"));
    connect(m_connectionsAction, &QAction::triggered, this, &MainWindow::onCheckConnections);
    m_copyConnectionsAction = new QAction(tr("Copiar\nligações"), this);
    m_copyConnectionsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("connection-copy.svg")));
    m_copyConnectionsAction->setToolTip(tr("Copia uma ligação para outros nós: a cada cópia selecione a barra que se apoia e a barra de apoio; as cópias que não atendem aos esforços do nó de destino ficam em vermelho"));
    connect(m_copyConnectionsAction, &QAction::triggered, this, &MainWindow::onCopyConnections);
    m_exportDstvAction = new QAction(tr("Exportar NC\n(DSTV)"), this);
    m_exportDstvAction->setIcon(Structura::UI::actionIcon(QStringLiteral("dxf.svg")));
    m_exportDstvAction->setToolTip(tr("Grava um arquivo .nc1 (DSTV) por chapa das ligações, com contorno e furação, para máquinas de corte e furação CNC"));
    connect(m_exportDstvAction, &QAction::triggered, this, &MainWindow::onExportDstv);
    m_exportIfcAction = new QAction(tr("Exportar\nIFC"), this);
    m_exportIfcAction->setIcon(Structura::UI::actionIcon(QStringLiteral("dxf.svg")));
    m_exportIfcAction->setToolTip(tr("Exporta as peças de perfil (pilares, vigas e diagonais, com o comprimento de corte e a seção) em IFC4 para BIM e outros programas"));
    connect(m_exportIfcAction, &QAction::triggered, this, &MainWindow::onExportIfc);
    m_sheetsAction = new QAction(tr("Pranchas"), this);
    m_sheetsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("sheets.svg")));
    m_sheetsAction->setToolTip(tr("Gera plantas, elevações com cotas, lista de barras e detalhes de ligações; exporta PDF e DXF"));
    connect(m_sheetsAction, &QAction::triggered, this, &MainWindow::onGenerateDrawings);

    m_integrityAction = new QAction(tr("Integridade") + QLatin1Char(10) + tr("estrutural"), this);
    m_integrityAction->setIcon(Structura::UI::actionIcon(QStringLiteral("model-check.svg")));
    m_integrityAction->setToolTip(tr("NBR 8800:2024, 4.13: travamento dos pilares, força de tração das contenções e ligações, e colapso progressivo por falha acidental de um pilar"));
    connect(m_integrityAction, &QAction::triggered, this, &MainWindow::onStructuralIntegrity);
    m_compositeAction = new QAction(tr("Vigas e lajes\nmistas"), this);
    m_compositeAction->setIcon(Structura::UI::actionIcon(QStringLiteral("composite.svg")));
    m_compositeAction->setToolTip(tr("Viga mista de aço e concreto (NBR 8800, Anexo L) e laje mista com forma de aço incorporada — steel deck (Anexo N)"));
    connect(m_compositeAction, &QAction::triggered, this, &MainWindow::onCompositeFloor);
    m_designSettingsAction = new QAction(tr("Parâmetros"), this);
    m_designSettingsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("design-params.svg")));
    m_designSettingsAction->setToolTip(tr("Aço, comprimentos efetivos, travamentos e combinações do dimensionamento"));
    connect(m_designSettingsAction, &QAction::triggered, this, &MainWindow::onDesignSettings);
    m_verifyDesignAction = new QAction(tr("Verificar\nperfis"), this);
    m_verifyDesignAction->setIcon(Structura::UI::actionIcon(QStringLiteral("check-design.svg")));
    m_verifyDesignAction->setToolTip(tr("Verifica os perfis adotados (NBR 8800 / NBR 14762) com os esforços da última análise"));
    connect(m_verifyDesignAction, &QAction::triggered, this, &MainWindow::onVerifyDesign);
    m_optimizeDesignAction = new QAction(tr("Dimensionar\nperfis"), this);
    m_optimizeDesignAction->setIcon(Structura::UI::actionIcon(QStringLiteral("design-optimize.svg")));
    m_optimizeDesignAction->setToolTip(tr("Adota o perfil mais leve da série que atende, reanalisando até convergir"));
    connect(m_optimizeDesignAction, &QAction::triggered, this, &MainWindow::onOptimizeDesign);
}
