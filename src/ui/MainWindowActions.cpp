#include "MainWindow.h"
#include "SvgIcons.h"

#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QStyle>

/// Cria as acoes da janela principal (modelagem, edicao, cargas, analise, dimensionamento, vista) e as registra para os atalhos.
void MainWindow::createActions()
{
    m_insertBarAction = new QAction(tr("Inserir\nbarra"), this);
    m_insertBarAction->setIcon(Structura::UI::actionIcon(QStringLiteral("bar-insert.svg")));
    connect(m_insertBarAction, &QAction::triggered, this, &MainWindow::onInsertBar);

    m_insertNodeAction = new QAction(tr("Inserir\nnó"), this);
    m_insertNodeAction->setIcon(Structura::UI::actionIcon(QStringLiteral("node-insert.svg")));
    m_insertNodeAction->setToolTip(tr("Insere um nó com snap ao longo das barras: ao colocar o nó sobre uma barra, ela é dividida nesse ponto"));
    connect(m_insertNodeAction, &QAction::triggered, this, &MainWindow::onInsertNode);

    m_measureAction = new QAction(tr("Medir"), this);
    m_measureAction->setIcon(Structura::UI::actionIcon(QStringLiteral("measure.svg")));
    m_measureAction->setToolTip(tr("Mede a distância entre dois pontos (com snap em nós, centros e ao longo das barras): distância, deltas e inclinação"));
    connect(m_measureAction, &QAction::triggered, this, &MainWindow::onMeasure);

    m_splitBarsAction = new QAction(tr("Dividir\nbarras"), this);
    m_splitBarsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("split-bar.svg")));
    m_splitBarsAction->setToolTip(tr("Divide as barras selecionadas em N partes iguais, a cada X metros ou em distâncias informadas"));
    m_splitBarsAction->setEnabled(false);
    connect(m_splitBarsAction, &QAction::triggered, this, &MainWindow::onSplitBars);

    m_generateTrussAction = new QAction(tr("Gerar\ntreliça"), this);
    m_generateTrussAction->setIcon(Structura::UI::actionIcon(QStringLiteral("truss.svg")));
    m_generateTrussAction->setToolTip(tr("Gera uma treliça plana (Pratt, Howe, Warren, Fink, K; banzos paralelos, duas águas, uma água, arco ou tesoura) a partir de um ponto inicial"));
    connect(m_generateTrussAction, &QAction::triggered, this, &MainWindow::onGenerateTruss);

    m_deleteElementsAction = new QAction(tr("Excluir\nelementos"), this);
    m_deleteElementsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("trash.svg")));
    m_deleteElementsAction->setToolTip(tr("Exclui os nós e barras selecionados (Delete). Barras ligadas a um nó excluído também são removidas"));
    m_deleteElementsAction->setShortcut(Qt::Key_Delete);
    m_deleteElementsAction->setEnabled(false);
    connect(m_deleteElementsAction, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    m_openModelAction = new QAction(tr("Abrir .dat"), this);
    m_openModelAction->setIcon(QIcon(QStringLiteral(":/icons/open.png")));
    m_openModelAction->setShortcut(QKeySequence::Open);
    connect(m_openModelAction, &QAction::triggered, this, &MainWindow::onOpenModel);

    m_newModelAction = new QAction(tr("Novo"), this);
    m_newModelAction->setShortcut(QKeySequence::New);
    connect(m_newModelAction, &QAction::triggered, this, &MainWindow::onNewModel);

    m_saveAsModelAction = new QAction(tr("Salvar como…"), this);
    m_saveAsModelAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsModelAction, &QAction::triggered, this, &MainWindow::onSaveModelAs);

    m_aboutAction = new QAction(tr("Sobre"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    m_saveModelAction = new QAction(tr("Salvar .dat"), this);
    m_saveModelAction->setIcon(QIcon(QStringLiteral(":/icons/save.png")));
    m_saveModelAction->setShortcut(QKeySequence::Save);
    connect(m_saveModelAction, &QAction::triggered, this, &MainWindow::onSaveModel);

    m_runAnalysisAction = new QAction(tr("Rodar\nanalise"), this);
    m_runAnalysisAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_runAnalysisAction->setShortcut(Qt::Key_F5);
    connect(m_runAnalysisAction, &QAction::triggered, this, &MainWindow::onRunStructuralAnalysis);

    m_applyNodalLoadAction = new QAction(tr("Forca\nconcentrada (nos)"), this);
    m_applyNodalLoadAction->setIcon(Structura::UI::actionIcon(QStringLiteral("force-point.svg")));
    connect(m_applyNodalLoadAction, &QAction::triggered, this, &MainWindow::onApplyNodalLoad);

    m_applyDistributedLoadAction = new QAction(tr("Distribuida\n(barras)"), this);
    m_applyDistributedLoadAction->setIcon(Structura::UI::actionIcon(QStringLiteral("force-distributed.svg")));
    connect(m_applyDistributedLoadAction, &QAction::triggered, this, &MainWindow::onApplyDistributedLoad);

    m_copyElementsAction = new QAction(tr("Copiar\nelementos"), this);
    m_copyElementsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("copy.svg")));
    m_copyElementsAction->setEnabled(false);
    connect(m_copyElementsAction, &QAction::triggered, this, &MainWindow::onCopyElements);

    m_moveElementsAction = new QAction(tr("Mover\nelementos"), this);
    m_moveElementsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("move.svg")));
    m_moveElementsAction->setEnabled(false);
    connect(m_moveElementsAction, &QAction::triggered, this, &MainWindow::onMoveElements);

    auto makeEditAction = [this](const QString &text, const QString &icon, void (MainWindow::*slot)()) {
        auto *action = new QAction(text, this);
        action->setIcon(Structura::UI::svgIcon(icon, QColor(QStringLiteral("#1e232b")), 64));
        action->setEnabled(false);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    m_serialCopyAction = makeEditAction(tr("Copia em\nserie"), QStringLiteral("serial.svg"), &MainWindow::onSerialCopy);
    m_rotateElementsAction = makeEditAction(tr("Rotacionar\nelementos"), QStringLiteral("rotate.svg"), &MainWindow::onRotateElements);
    m_mirrorElementsAction = makeEditAction(tr("Espelhar\nelementos"), QStringLiteral("mirror.svg"), &MainWindow::onMirrorElements);

    m_applyMemberMomentAction = new QAction(tr("Momento\ndistribuido"), this);
    m_applyMemberMomentAction->setIcon(Structura::UI::actionIcon(QStringLiteral("bar-moment.svg")));
    connect(m_applyMemberMomentAction, &QAction::triggered, this, &MainWindow::onApplyMemberMoment);

    m_loadCaseVisibilityAction = new QAction(tr("Casos\nvisiveis"), this);
    m_loadCaseVisibilityAction->setIcon(Structura::UI::actionIcon(QStringLiteral("cases.svg")));
    m_loadCaseVisibilityAction->setToolTip(tr("Escolhe, por caixas de marcação, quais casos de carregamento aparecem na tela"));
    connect(m_loadCaseVisibilityAction, &QAction::triggered, this, &MainWindow::onLoadCaseVisibility);

    m_loadCasesAction = new QAction(tr("Casos e\ncombinacoes"), this);
    m_loadCasesAction->setIcon(Structura::UI::actionIcon(QStringLiteral("cases.svg")));
    connect(m_loadCasesAction, &QAction::triggered, this, &MainWindow::onEditLoadCases);
    m_genCombinationsAction = new QAction(tr("Gerar\ncombinacoes"), this);
    m_genCombinationsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("combinations.svg")));
    m_genCombinationsAction->setToolTip(tr("Gera automaticamente as combinações ELU e ELS (NBR 8681 / NBR 8800) a partir dos casos de carga"));
    connect(m_genCombinationsAction, &QAction::triggered, this, &MainWindow::onGenerateCombinations);
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
    m_quantitiesAction = new QAction(tr("Quantitativos"), this);
    m_quantitiesAction->setIcon(Structura::UI::actionIcon(QStringLiteral("quantities.svg")));
    m_quantitiesAction->setToolTip(tr("Lista de materiais por perfil: comprimento, massa e área de pintura"));
    connect(m_quantitiesAction, &QAction::triggered, this, &MainWindow::onShowQuantities);
    m_exportDxfAction = new QAction(tr("Exportar\nDXF"), this);
    m_exportDxfAction->setIcon(Structura::UI::actionIcon(QStringLiteral("dxf.svg")));
    m_exportDxfAction->setToolTip(tr("Exporta os eixos das barras em DXF (3D, planta ou elevações), uma camada por perfil"));
    connect(m_exportDxfAction, &QAction::triggered, this, &MainWindow::onExportDxf);
    m_sheetsAction = new QAction(tr("Pranchas"), this);
    m_sheetsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("sheets.svg")));
    m_sheetsAction->setToolTip(tr("Gera plantas, elevações com cotas, lista de barras e detalhes de ligações; exporta PDF e DXF"));
    connect(m_sheetsAction, &QAction::triggered, this, &MainWindow::onGenerateDrawings);

    m_applyRestraintsAction = new QAction(tr("Restricoes\nnodais"), this);
    m_applyRestraintsAction->setIcon(Structura::UI::actionIcon(QStringLiteral("restraint.svg")));
    connect(m_applyRestraintsAction, &QAction::triggered, this, &MainWindow::onApplyRestraints);

    m_createMaterialAction = new QAction(tr("Novo\nmaterial"), this);
    m_createMaterialAction->setIcon(Structura::UI::actionIcon(QStringLiteral("material-new.svg")));
    connect(m_createMaterialAction, &QAction::triggered, this, &MainWindow::onCreateMaterial);

    m_createSectionAction = new QAction(tr("Nova\nsecao"), this);
    m_createSectionAction->setIcon(QIcon(QStringLiteral(":/icons/addSection.png")));
    connect(m_createSectionAction, &QAction::triggered, this, &MainWindow::onCreateSection);

    m_assignPropertiesAction = new QAction(tr("Atribuir\na barras"), this);
    m_assignPropertiesAction->setIcon(QIcon(QStringLiteral(":/icons/setBars.png")));
    connect(m_assignPropertiesAction, &QAction::triggered, this, &MainWindow::onAssignProperties);

    m_resetCameraAction = new QAction(tr("Visao\ninicial"), this);
    m_resetCameraAction->setIcon(QIcon(QStringLiteral(":/icons/initial.png")));
    connect(m_resetCameraAction, &QAction::triggered, this, &MainWindow::onResetCamera);

    m_zoomExtentsAction = new QAction(tr("Zoom\nextents"), this);
    m_zoomExtentsAction->setIcon(QIcon(QStringLiteral(":/icons/zoomExtents.png")));
    connect(m_zoomExtentsAction, &QAction::triggered, this, &MainWindow::onZoomExtents);

    m_profileManagerAction = new QAction(tr("Gerenciador\nde perfis"), this);
    m_profileManagerAction->setIcon(Structura::UI::svgIcon(QStringLiteral("section.svg"),
                                                           QColor(QStringLiteral("#0d7fb9")), 64));
    connect(m_profileManagerAction, &QAction::triggered, this, &MainWindow::onOpenProfileManager);

    m_axesAction = new QAction(tr("Eixos de\nreferencia"), this);
    m_axesAction->setObjectName(QStringLiteral("axesAction"));
    m_axesAction->setIcon(Structura::UI::svgIcon(QStringLiteral("axes-config.svg"),
                                                  QColor(QStringLiteral("#0d7fb9")), 64));
    connect(m_axesAction, &QAction::triggered, this, &MainWindow::onToggleAxesPanel);

    m_placementAction = new QAction(tr("Posicionar\nseção"), this);
    m_placementAction->setIcon(Structura::UI::svgIcon(QStringLiteral("section-place.svg"),
                                                      QColor(QStringLiteral("#0d7fb9")), 64));
    m_placementAction->setToolTip(tr("Posicionar a seção em relação ao nó: giro, ponto de referência, excentricidade e articulações"));
    connect(m_placementAction, &QAction::triggered, this, &MainWindow::onTogglePlacementPanel);

    addAction(m_openModelAction);
    addAction(m_saveModelAction);
    addAction(m_saveAsModelAction);
    addAction(m_deleteElementsAction);
    addAction(m_newModelAction);
    addAction(m_runAnalysisAction);
}
