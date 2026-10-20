#include "MainWindow.h"
#include "RibbonWidgets.h"
#include "ShellDialog.h"
#include "SvgIcons.h"

#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

using Structura::UI::RibbonItem;
using Structura::UI::RibbonVariant;

/// Icones simples (tracos) da barra superior, desenhados na cor pedida.
QIcon quickIcon(const QString &kind, const QColor &color, int size = 20)
{
    const int logical = 20;   // coordenadas dos tracos
    QPixmap pixmap(size * 2, size * 2);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / double(logical), size / double(logical));
    QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    if (kind == QLatin1String("new")) {
        path.moveTo(5, 3);
        path.lineTo(11, 3);
        path.lineTo(15, 7);
        path.lineTo(15, 17);
        path.lineTo(5, 17);
        path.closeSubpath();
        path.moveTo(11, 3);
        path.lineTo(11, 7);
        path.lineTo(15, 7);
    } else if (kind == QLatin1String("open")) {
        path.moveTo(3, 6);
        path.lineTo(8, 6);
        path.lineTo(10, 8);
        path.lineTo(17, 8);
        path.lineTo(17, 16);
        path.lineTo(3, 16);
        path.closeSubpath();
    } else if (kind == QLatin1String("save")) {
        path.addRoundedRect(QRectF(4, 4, 12, 12), 1.5, 1.5);
        path.addRect(QRectF(7, 4, 6, 4));
        path.addRect(QRectF(7, 11, 6, 5));
    } else if (kind == QLatin1String("undo") || kind == QLatin1String("redo")) {
        if (kind == QLatin1String("redo")) {
            p.translate(logical, 0);
            p.scale(-1, 1);
        }
        path.moveTo(8, 5);
        path.lineTo(4.5, 9);
        path.lineTo(8, 13);
        path.moveTo(4.5, 9);
        path.lineTo(11, 9);
        path.cubicTo(16, 9, 16, 16, 11, 16);
    } else if (kind == QLatin1String("help")) {
        path.addEllipse(QPointF(10, 10), 7.5, 7.5);
        p.drawPath(path);
        QFont font = p.font();
        font.setPixelSize(11);
        font.setBold(true);
        p.setFont(font);
        p.drawText(QRectF(0, 0, logical, logical), Qt::AlignCenter, QStringLiteral("?"));
        return QIcon(pixmap);
    } else if (kind == QLatin1String("play")) {
        p.setBrush(color);
        path.moveTo(6, 4);
        path.lineTo(16, 10);
        path.lineTo(6, 16);
        path.closeSubpath();
    }
    p.drawPath(path);
    return QIcon(pixmap);
}

} // namespace

QWidget *MainWindow::createQuickAccessBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("QuickAccessBar"));
    bar->setFixedHeight(36);
    bar->setStyleSheet(R"(
        #QuickAccessBar { background: #0d7fb9; border-bottom: none; }
        #QuickAccessBar QToolButton {background: transparent; border: none; color: #eef5fb; padding: 2px 4px; margin: 0px;}
        #QuickAccessBar QToolButton:hover { background: rgba(255,255,255,0.18); border-radius: 4px; }
        #QuickAccessBar QToolButton[systemButton="true"] { padding: 2px; margin-left: 2px; margin-right: 0px; border-radius: 4px; }
        #QuickAccessBar QToolButton[systemButton="true"]:hover { background: rgba(255,255,255,0.25); }
        #QuickAccessBar QToolButton[systemButton="true"]:pressed { background: rgba(0,0,0,0.30); }
        #QuickAccessBar QToolButton[systemButton="true"]:last-child:hover { background: #d64545; }
        #QuickAccessBar QToolButton[startButton="true"] { font-weight: 600; padding: 2px 10px; margin-left: 6px; border-radius: 4px; }
        #QuickAccessBar QToolButton[startButton="true"]::menu-indicator { image: none; width: 0px; }
        #QuickAccessBar QToolButton[startButton="true"]:hover { background: rgba(255,255,255,0.24); }
        #QuickAccessBar QToolButton[startButton="true"]:pressed { background: rgba(255,255,255,0.34); }
        #QuickAccessBar QFrame[quickSeparator="true"] { background: rgba(255,255,255,0.32); }
        #QuickAccessBar QLabel { color: #eef5fb; }
        )");

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 3, 8, 3);
    layout->setSpacing(4);

    auto *leftWidget = new QWidget(bar);
    leftWidget->setProperty("dragRegion", true);
    leftWidget->setMouseTracking(true);
    leftWidget->installEventFilter(this);
    leftWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    auto *leftLayout = new QHBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    const QColor white(QStringLiteral("#eef5fb"));
    auto quickButton = [bar, &white](QAction *action, const QString &icon, const QString &tip) {
        auto *button = new QToolButton(bar);
        button->setIcon(quickIcon(icon, white));
        button->setToolTip(tip);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(18, 18));
        button->setAutoRaise(false);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(28, 28);
        button->setFocusPolicy(Qt::NoFocus);
        if (action) {
            connect(button, &QToolButton::clicked, action, &QAction::trigger);
            button->setEnabled(action->isEnabled());
            connect(action, &QAction::changed, button, [button, action]() { button->setEnabled(action->isEnabled()); });
        }
        return button;
    };
    auto separator = [bar]() {
        auto *line = new QFrame(bar);
        line->setProperty("quickSeparator", true);
        line->setFixedSize(1, 18);
        return line;
    };

    auto *logo = new QLabel(bar);
    const QIcon appIcon = QApplication::windowIcon();
    if (!appIcon.isNull()) {
        logo->setPixmap(appIcon.pixmap(QSize(22, 22), devicePixelRatioF()));
    }
    logo->setFixedSize(26, 26);
    logo->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(logo);
    leftLayout->addSpacing(6);
    leftLayout->addWidget(quickButton(m_newModelAction, QStringLiteral("new"), tr("Novo modelo (%1)").arg(QKeySequence(QKeySequence::New).toString(QKeySequence::NativeText))));
    leftLayout->addWidget(quickButton(m_openModelAction, QStringLiteral("open"), tr("Abrir modelo (%1)").arg(QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText))));
    leftLayout->addWidget(quickButton(m_saveModelAction, QStringLiteral("save"), tr("Salvar modelo (%1)").arg(QKeySequence(QKeySequence::Save).toString(QKeySequence::NativeText))));
    leftLayout->addSpacing(2);
    leftLayout->addWidget(separator());
    leftLayout->addSpacing(2);
    leftLayout->addWidget(quickButton(m_undoAction, QStringLiteral("undo"), tr("Desfazer (%1)").arg(QKeySequence(QKeySequence::Undo).toString(QKeySequence::NativeText))));
    leftLayout->addWidget(quickButton(m_redoAction, QStringLiteral("redo"), tr("Refazer (%1)").arg(QKeySequence(QKeySequence::Redo).toString(QKeySequence::NativeText))));
    leftLayout->addSpacing(8);
    m_homeTabButton = new QToolButton(bar);
    m_homeTabButton->setText(tr("Arquivo"));
    m_homeTabButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_homeTabButton->setPopupMode(QToolButton::InstantPopup);
    auto *fileMenu = new QMenu(m_homeTabButton);
    fileMenu->addAction(m_newModelAction);
    fileMenu->addAction(m_openModelAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveModelAction);
    fileMenu->addAction(m_saveAsModelAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_aboutAction);
    m_homeTabButton->setMenu(fileMenu);
    m_homeTabButton->setProperty("startButton", QStringLiteral("true"));
    m_homeTabButton->setCursor(Qt::PointingHandCursor);
    m_homeTabButton->setFocusPolicy(Qt::NoFocus);
    m_homeTabButton->setFixedHeight(24);
    leftLayout->addWidget(m_homeTabButton);
    layout->addWidget(leftWidget, 0, Qt::AlignVCenter);

    m_titleLabel = new QLabel(tr("Structura"), bar);
    m_titleLabel->setTextFormat(Qt::RichText);
    m_titleLabel->setAlignment(Qt::AlignCenter);   // centralizado na barra (ver eventFilter: posicao calculada no redimensionamento)
    auto showTitle = [this](const QString &title) {
        const int cut = title.lastIndexOf(QStringLiteral(" — "));
        const QString name = (cut > 0 ? title.left(cut) : title).toHtmlEscaped();
        const QString app = cut > 0 ? title.mid(cut).toHtmlEscaped() : QString();
        m_titleLabel->setText(QStringLiteral("<span style='font-weight:700; color:#ffffff;'>%1</span><span style='color:#d4e8f5;'>%2</span>").arg(name, app));
    };
    connect(this, &QWidget::windowTitleChanged, m_titleLabel, showTitle);
    showTitle(windowTitle());
    layout->addStretch(1);
    m_titleLabel->lower();

    auto createSystemButton = [bar](const QIcon &icon, const QString &name) {
        auto *button = new QToolButton(bar);
        button->setIcon(icon);
        button->setToolTip(name);
        button->setProperty("systemButton", QStringLiteral("true"));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(false);
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(30, 26);
        button->setCursor(Qt::ArrowCursor);
        return button;
    };

    m_helpButton = quickButton(nullptr, QStringLiteral("help"), tr("Ajuda: mostra a descrição do último comando consultado (F1)"));
    m_helpButton->setIconSize(QSize(18, 18));
    connect(m_helpButton, &QToolButton::clicked, this, &MainWindow::showCommandHelp);
    layout->addWidget(m_helpButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(6);

    m_minimizeButton = createSystemButton(style()->standardIcon(QStyle::SP_TitleBarMinButton), tr("Minimizar"));
    m_maximizeButton = createSystemButton(style()->standardIcon(QStyle::SP_TitleBarMaxButton), tr("Maximizar"));
    m_closeButton = createSystemButton(style()->standardIcon(QStyle::SP_TitleBarCloseButton), tr("Fechar"));

    layout->addWidget(m_minimizeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_maximizeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    connect(m_minimizeButton, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(m_maximizeButton, &QToolButton::clicked, this, &MainWindow::toggleMaximized);
    connect(m_closeButton, &QToolButton::clicked, this, &MainWindow::close);

    return bar;
}

void MainWindow::createRibbon()
{
    using Structura::UI::RibbonPage;
    const QColor ink(QStringLiteral("#1e232b"));
    const QColor danger(QStringLiteral("#c0392b"));

    m_ribbon->setObjectName("mainRibbon");
    m_ribbon->setDocumentMode(true);
    m_ribbon->setTabPosition(QTabWidget::North);
    m_ribbon->setMovable(false);
    m_ribbon->tabBar()->setExpanding(false);
    m_ribbon->setStyleSheet(R"(
        QTabWidget#mainRibbon { background: #0d7fb9; }
        QTabWidget::pane { border: 0; background: #f2f5fa; top: 0px; }
    )");
    if (auto *tabBar = m_ribbon->tabBar()) {
        tabBar->setDrawBase(false);
        tabBar->setAutoFillBackground(true);
        tabBar->setCursor(Qt::PointingHandCursor);
        tabBar->setStyleSheet(R"(
            QTabBar { background: #0d7fb9; border: none; padding: 0; margin: 0; }
            QTabBar::tab { background: transparent; color: #eaf3f9; border: none; border-top: 2px solid transparent; border-top-left-radius: 6px; border-top-right-radius: 6px;
                           padding: 5px 15px 6px 15px; margin: 4px 1px 0 1px; font-weight: 600; min-height: 17px; }
            QTabBar::tab:first { margin-left: 10px; }
            QTabBar::tab:hover:!selected { background: rgba(255,255,255,0.16); }
            QTabBar::tab:selected { background: #f2f5fa; color: #0b6da1; border-top: 2px solid #f2992e; }
        )");
    }

    // recolher/expandir a faixa de opcoes
    m_ribbonCollapseButton = new QToolButton(m_ribbon);
    m_ribbonCollapseButton->setCheckable(true);
    m_ribbonCollapseButton->setToolTip(tr("Recolher a faixa de opções"));
    m_ribbonCollapseButton->setCursor(Qt::PointingHandCursor);
    m_ribbonCollapseButton->setFocusPolicy(Qt::NoFocus);
    m_ribbonCollapseButton->setFixedSize(26, 22);
    m_ribbonCollapseButton->setStyleSheet(R"(
        QToolButton { background: transparent; border: none; border-radius: 4px; margin: 4px 8px 0 0; }
        QToolButton:hover { background: rgba(255,255,255,0.2); }
    )");
    {
        QPixmap up(24, 24), down(24, 24);
        for (QPixmap *pm : {&up, &down}) {
            pm->setDevicePixelRatio(2.0);
            pm->fill(Qt::transparent);
            QPainter p(pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor(QStringLiteral("#eaf3f9")), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath path;
            if (pm == &up) {
                path.moveTo(3.5, 8);
                path.lineTo(6, 4.5);
                path.lineTo(8.5, 8);
            } else {
                path.moveTo(3.5, 4.5);
                path.lineTo(6, 8);
                path.lineTo(8.5, 4.5);
            }
            p.drawPath(path);
        }
        QIcon icon;
        icon.addPixmap(up, QIcon::Normal, QIcon::Off);
        icon.addPixmap(down, QIcon::Normal, QIcon::On);
        m_ribbonCollapseButton->setIcon(icon);
        m_ribbonCollapseButton->setIconSize(QSize(12, 12));
    }
    connect(m_ribbonCollapseButton, &QToolButton::toggled, this, &MainWindow::setRibbonCollapsed);
    m_ribbon->setCornerWidget(m_ribbonCollapseButton, Qt::TopRightCorner);

    // ---- acoes criadas aqui -----------------------------------------------------------------------------------------------
    auto icon = [&ink](const QString &name) { return Structura::UI::svgIcon(name, ink, 64); };
    m_createGroupAction = new QAction(tr("Criar\ngrupo"), this);
    m_createGroupAction->setIcon(icon(QStringLiteral("group.svg")));
    m_createGroupAction->setToolTip(tr("Cria um grupo com as barras selecionadas: perfil, posição e rotação são compartilhados"));
    connect(m_createGroupAction, &QAction::triggered, this, &MainWindow::onCreateGroup);
    m_groupsPanelAction = new QAction(tr("Grupos"), this);
    m_groupsPanelAction->setIcon(icon(QStringLiteral("groups.svg")));
    m_groupsPanelAction->setToolTip(tr("Painel de grupos: exibir/ocultar, adicionar e remover barras"));
    connect(m_groupsPanelAction, &QAction::triggered, this, &MainWindow::onToggleGroupsPanel);

    struct TypeButton {
        const char *text;
        const char *icon;
        const char *tip;
        int op;
    };
    const TypeButton typeButtons[] = {
        {"Rotular início", "hinge-start.svg", "Articula (libera as rotações de flexão) a extremidade inicial das barras selecionadas", 0},
        {"Rotular fim", "hinge-end.svg", "Articula a extremidade final das barras selecionadas", 1},
        {"Bi-rotulada", "hinge-both.svg", "Articula as duas extremidades", 2},
        {"Engastar extremidades", "fixed-ends.svg", "Remove as articulações (extremidades rígidas)", 3},
        {"Treliça", "truss.svg", "Barra de treliça: biarticulada, trabalha só a esforço axial (tração e compressão)", 4},
        {"Tirante", "tie.svg", "Tirante: biarticulado e só tração — a barra é desativada quando encurta (análise iterativa)", 5},
    };
    QList<QAction *> typeActions;
    for (const auto &t : typeButtons) {
        auto *action = new QAction(tr(t.text), this);
        action->setIcon(icon(QString::fromLatin1(t.icon)));
        action->setToolTip(tr(t.tip));
        const int op = t.op;
        connect(action, &QAction::triggered, this, [this, op]() { applyBarTypeOperation(op); });
        typeActions.append(action);
    }

    m_checkModelAction = new QAction(tr("Checar\nmodelo"), this);
    m_checkModelAction->setIcon(icon(QStringLiteral("model-check.svg")));
    m_checkModelAction->setToolTip(tr("Procura problemas antes de analisar: nós soltos, barras duplicadas, mecanismos, apoios insuficientes, cargas e unidades"));
    connect(m_checkModelAction, &QAction::triggered, this, &MainWindow::onCheckModel);
    m_templateAction = new QAction(tr("Modelo\npronto"), this);
    m_templateAction->setIcon(icon(QStringLiteral("template.svg")));
    m_templateAction->setToolTip(tr("Gera um galpão de duas águas ou um edifício de pórticos parametrizado (perfis, grupos, tirantes, cargas e combinações)"));
    connect(m_templateAction, &QAction::triggered, this, &MainWindow::onNewFromTemplate);

    m_areaLoadAction = new QAction(tr("Carga de\nárea"), this);
    m_areaLoadAction->setIcon(icon(QStringLiteral("area-load.svg")));
    m_areaLoadAction->setToolTip(tr("Distribui uma pressão (kN/m²) nas barras selecionadas pela largura de influência"));
    connect(m_areaLoadAction, &QAction::triggered, this, &MainWindow::onApplyAreaLoad);
    m_windLoadAction = new QAction(tr("Vento\nNBR 6123"), this);
    m_windLoadAction->setIcon(icon(QStringLiteral("wind.svg")));
    m_windLoadAction->setToolTip(tr("Gera a carga de vento de uma face (q = 0,613 Vk²) nas barras selecionadas"));
    connect(m_windLoadAction, &QAction::triggered, this, &MainWindow::onApplyWindLoad);
    m_deleteLoadsAction = new QAction(tr("Excluir\ncargas"), this);
    m_deleteLoadsAction->setIcon(icon(QStringLiteral("delete-loads.svg")));
    m_deleteLoadsAction->setToolTip(tr("Exclui cargas por tipo, caso de carga e abrangência (seleção ou modelo inteiro)"));
    connect(m_deleteLoadsAction, &QAction::triggered, this, &MainWindow::onDeleteLoads);

    m_showDiagramsAction = new QAction(tr("Mostrar\nDiagramas"), this);
    m_showDiagramsAction->setIcon(icon(QStringLiteral("diagrams-show.svg")));
    m_showDiagramsAction->setEnabled(false);   // habilitado apos a analise
    connect(m_showDiagramsAction, &QAction::triggered, this, &MainWindow::onShowDiagrams);
    m_hideDiagramsAction = new QAction(tr("Ocultar\nDiagramas"), this);
    m_hideDiagramsAction->setIcon(icon(QStringLiteral("diagrams-hide.svg")));
    m_hideDiagramsAction->setEnabled(false);
    connect(m_hideDiagramsAction, &QAction::triggered, this, &MainWindow::onHideDiagrams);
    m_compareAction = new QAction(tr("Comparar\ncombinações"), this);
    m_compareAction->setIcon(icon(QStringLiteral("compare.svg")));
    m_compareAction->setToolTip(tr("Compara deslocamentos, reações e esforços entre combinações, com a envoltória e a combinação que governa"));
    connect(m_compareAction, &QAction::triggered, this, &MainWindow::onCompareResults);

    m_compositeAction = new QAction(tr("Vigas e lajes\nmistas"), this);
    m_compositeAction->setIcon(icon(QStringLiteral("composite.svg")));
    m_compositeAction->setToolTip(tr("Viga mista de aço e concreto (NBR 8800, Anexo L) e laje mista com forma de aço incorporada — steel deck (Anexo N)"));
    connect(m_compositeAction, &QAction::triggered, this, &MainWindow::onCompositeFloor);
    m_designSettingsAction = new QAction(tr("Parâmetros"), this);
    m_designSettingsAction->setIcon(icon(QStringLiteral("design-params.svg")));
    m_designSettingsAction->setToolTip(tr("Aço, comprimentos efetivos, travamentos e combinações do dimensionamento"));
    connect(m_designSettingsAction, &QAction::triggered, this, &MainWindow::onDesignSettings);
    m_verifyDesignAction = new QAction(tr("Verificar\nperfis"), this);
    m_verifyDesignAction->setIcon(icon(QStringLiteral("check-design.svg")));
    m_verifyDesignAction->setToolTip(tr("Verifica os perfis adotados (NBR 8800 / NBR 14762) com os esforços da última análise"));
    connect(m_verifyDesignAction, &QAction::triggered, this, &MainWindow::onVerifyDesign);
    m_optimizeDesignAction = new QAction(tr("Dimensionar\nperfis"), this);
    m_optimizeDesignAction->setIcon(icon(QStringLiteral("design-optimize.svg")));
    m_optimizeDesignAction->setToolTip(tr("Adota o perfil mais leve da série que atende, reanalisando até convergir"));
    connect(m_optimizeDesignAction, &QAction::triggered, this, &MainWindow::onOptimizeDesign);
    m_reportAction = new QAction(tr("Memorial"), this);
    m_reportAction->setIcon(icon(QStringLiteral("report.svg")));
    m_reportAction->setToolTip(tr("Gera o memorial de cálculo e o relatório geral: escolha o conteúdo, visualize e exporte para Word (.docx)"));
    connect(m_reportAction, &QAction::triggered, this, &MainWindow::onGenerateReport);

    // Cascas
    auto shellTool = [this, &icon](const QString &text, const QString &iconName, const QString &tip, Command command) {
        auto *action = new QAction(text, this);
        action->setIcon(icon(iconName));
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, [this, command]() { setCommand(command); });
        return action;
    };
    m_shellPolygonAction = shellTool(tr("Chapa por\ncontorno"), QStringLiteral("shell-polygon.svg"),
                                     tr("Desenha uma chapa/laje/placa plana clicando nos vértices no plano de construção (Enter termina, C fecha)"), Command::ShellPolygon);
    m_shellRectangleAction = shellTool(tr("Chapa\nretangular"), QStringLiteral("shell-rect.svg"), tr("Chapa retangular por dois cantos no plano de construção"), Command::ShellRectangle);
    m_shellDiskAction = shellTool(tr("Disco /\nanel"), QStringLiteral("shell-disk.svg"), tr("Disco ou anel circular: centro e raio"), Command::ShellDisk);
    m_shellExtrudeAction = shellTool(tr("Extrudar\nlinha"), QStringLiteral("shell-extrude.svg"),
                                     tr("Desenha uma linha ou polilinha (aberta ou fechada) e a extruda em um vetor: paredes, cascas cilíndricas e prismáticas"), Command::ShellExtrude);
    m_shellCylinderAction = shellTool(tr("Cilindro /\ncone"), QStringLiteral("shell-cylinder.svg"), tr("Círculo (centro e raio) extrudado em um vetor; a escala da extremidade faz cone ou tronco"), Command::ShellCylinder);
    m_shellRevolveAction = shellTool(tr("Revolução"), QStringLiteral("shell-revolve.svg"),
                                     tr("Desenha um perfil e o gira em torno de um eixo (dois pontos): domos, esferas, tampos de vasos, tanques e toros"), Command::ShellRevolve);
    m_shellDeleteAction = new QAction(tr("Excluir\ncasca"), this);
    m_shellDeleteAction->setIcon(Structura::UI::svgIcon(QStringLiteral("shell-delete.svg"), danger, 64, danger));
    m_shellDeleteAction->setToolTip(tr("Exclui a casca selecionada na vista 3D (clique nela para selecionar)"));
    connect(m_shellDeleteAction, &QAction::triggered, this, [this]() { deleteSelectedShell(); });
    m_shellCoupleAction = new QAction(tr("Acoplar às\nbarras"), this);
    m_shellCoupleAction->setCheckable(true);
    m_shellCoupleAction->setIcon(icon(QStringLiteral("shell-couple.svg")));
    m_shellCoupleAction->setToolTip(tr("As cascas entram na análise das barras (mesma matriz de rigidez): nós coincidentes são os mesmos, nós de barras sobre a casca ficam ligados a ela e as cargas, apoios e resultados das cascas passam a fazer parte da análise do projeto"));
    connect(m_shellCoupleAction, &QAction::toggled, this, &MainWindow::onShellCoupleToggled);
    m_shellSplitAction = new QAction(tr("Dividir barras\nnos nós"), this);
    m_shellSplitAction->setIcon(icon(QStringLiteral("shell-split.svg")));
    m_shellSplitAction->setToolTip(tr("Divide as barras nos nós da malha das cascas que caem sobre o seu eixo (vigas de borda e de apoio), para que barra e casca trabalhem juntas ao longo da linha"));
    connect(m_shellSplitAction, &QAction::triggered, this, [this]() {
        const int done = splitBarsAtShellNodes(true);
        showStatusMessage(done > 0 ? tr("%1 corte(s) nas barras. Ctrl+Z desfaz.").arg(done) : tr("Nenhum nó de casca cai sobre o eixo de barras: nada a dividir."), 6000);
    });
    m_shellMonolithicAction = new QAction(tr("Ligação\nmonolítica"), this);
    m_shellMonolithicAction->setCheckable(true);
    m_shellMonolithicAction->setIcon(icon(QStringLiteral("shell-couple.svg")));
    m_shellMonolithicAction->setToolTip(tr("Desligado (padrão): a casca APOIA nas barras (só translações ligadas; a laje transmite reações e quinhões de carga, sem momentos nas vigas). Ligado: translações e rotações compartilhadas (laje monolítica com a viga, que recebe também torção e momento)"));
    connect(m_shellMonolithicAction, &QAction::toggled, this, [this](bool on) {
        if (m_extras.shell.monolithic == on) {
            return;
        }
        StateGuard guard(this, on ? tr("Ligação monolítica casca-barra") : tr("Casca apoiada nas barras"));
        m_extras.shell.monolithic = on;
        markModified();
        if (m_shellDialog) {
            m_shellDialog->setModel(m_extras.shell);
        }
    });
    m_shellAction = new QAction(tr("Editor de\ncascas"), this);
    m_shellAction->setIcon(icon(QStringLiteral("shell.svg")));
    m_shellAction->setToolTip(tr("Propriedades, modelos paramétricos (tanques, vasos), cargas, apoios, análise por elementos finitos e verificação"));
    connect(m_shellAction, &QAction::triggered, this, &MainWindow::onShellEditor);

    // atalhos dos comandos principais (exibidos nas dicas e nos menus)
    m_insertBarAction->setShortcut(Qt::Key_B);
    m_copyElementsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+C")));
    m_serialCopyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    for (QAction *action : {m_insertBarAction, m_copyElementsAction, m_serialCopyAction}) {
        addAction(action);   // vale mesmo quando o botao esta recolhido
    }
    m_runAnalysisAction->setText(tr("Processar"));

    // ---- descricoes estendidas e variacoes ------------------------------------------------------------------------------------------
    auto item = [](QAction *action, const QString &label, bool large, const QString &description = QString()) {
        RibbonItem it;
        it.action = action;
        it.label = label;
        it.large = large;
        it.description = description;
        return it;
    };
    auto dangerItem = [&](QAction *action, const QString &label, bool large, const QString &iconName, const QString &description = QString()) {
        RibbonItem it = item(action, label, large, description);
        it.danger = true;
        it.icon = Structura::UI::svgIcon(iconName, danger, 64, danger);
        return it;
    };

    // ---- Início ---------------------------------------------------------------------------------------------------------------
    auto *home = new RibbonPage(this);
    {
        RibbonItem insertBar = item(m_insertBarAction, tr("Inserir barra"), true,
                                    tr("Desenha uma barra entre dois pontos. Clique no início e no fim; nós são criados automaticamente onde não existirem."));
        insertBar.variants = {{m_insertBarAction, tr("Inserir barra")}, {nullptr, QString(), true}, {m_templateAction, tr("Modelo pronto…")}};
        home->addGroup(tr("Modelagem"), {insertBar, item(m_insertNodeAction, tr("Inserir nó"), true), item(m_applyRestraintsAction, tr("Restrições nodais"), true),
                                         item(m_splitBarsAction, tr("Dividir barras"), false), item(m_measureAction, tr("Medir"), false)});
        home->addGroup(tr("Propriedades"), {item(m_profileManagerAction, tr("Gerenciador de perfis"), true), item(m_assignPropertiesAction, tr("Atribuir a barras"), true),
                                            item(m_createMaterialAction, tr("Novo material"), false), item(m_createSectionAction, tr("Nova seção"), false),
                                            item(m_placementAction, tr("Posicionar seção"), false)});
        RibbonItem copy = item(m_copyElementsAction, tr("Copiar"), true, tr("Copia os elementos selecionados para outro ponto: clique no ponto base e no destino."));
        copy.variants = {{m_copyElementsAction, tr("Copiar")}, {m_serialCopyAction, tr("Cópia em série…")}, {nullptr, QString(), true}, {m_mirrorElementsAction, tr("Copiar espelhado…")}};
        home->addGroup(tr("Edição"), {item(m_moveElementsAction, tr("Mover"), true), copy, item(m_serialCopyAction, tr("Cópia em série"), false),
                                      item(m_rotateElementsAction, tr("Rotacionar"), false), item(m_mirrorElementsAction, tr("Espelhar"), false),
                                      dangerItem(m_deleteElementsAction, tr("Excluir"), true, QStringLiteral("trash.svg"))});
        RibbonItem manage = item(m_groupsPanelAction, tr("Gerenciar grupos"), true);
        manage.variants = {{m_groupsPanelAction, tr("Painel de grupos")}, {m_createGroupAction, tr("Criar grupo…")}};
        home->addGroup(tr("Grupos"), {item(m_createGroupAction, tr("Criar grupo"), true), manage});
        QList<RibbonItem> types;
        for (QAction *action : typeActions) {
            types.append(item(action, QString(), false));
        }
        home->addGroup(tr("Tipo de barra"), types);
    }

    // ---- Ferramentas ------------------------------------------------------------------------------------------------------------
    auto *tools = new RibbonPage(this);
    tools->addGroup(tr("Geração"), {item(m_generateTrussAction, tr("Gerar treliça"), true)});
    tools->addGroup(tr("Eixos"), {item(m_axesAction, tr("Eixos de referência"), true)});
    tools->addGroup(tr("Modelo"), {item(m_templateAction, tr("Modelo pronto"), true), item(m_checkModelAction, tr("Checar modelo"), true)});
    tools->addGroup(tr("Documentação"), {item(m_quantitiesAction, tr("Quantitativos"), true), item(m_exportDxfAction, tr("Exportar DXF"), true), item(m_sheetsAction, tr("Pranchas"), true)});

    // ---- Carregamentos ----------------------------------------------------------------------------------------------------------
    auto *loads = new RibbonPage(this);
    loads->addGroup(tr("Carregamentos"), {item(m_applyNodalLoadAction, tr("Força concentrada (nós)"), true), item(m_applyDistributedLoadAction, tr("Distribuída (barras)"), true),
                                          item(m_applyMemberMomentAction, tr("Momento distribuído"), true), item(m_areaLoadAction, tr("Carga de área"), true),
                                          item(m_windLoadAction, tr("Vento NBR 6123"), true), dangerItem(m_deleteLoadsAction, tr("Excluir cargas"), true, QStringLiteral("delete-loads.svg"))});
    loads->addGroup(tr("Casos e combinações"), {item(m_loadCasesAction, tr("Casos e combinações"), true), item(m_genCombinationsAction, tr("Gerar combinações"), true)});

    // ---- Visualização -----------------------------------------------------------------------------------------------------------
    auto *view = new RibbonPage(this);
    view->addGroup(tr("Visualização"), {item(m_resetCameraAction, tr("Visão inicial"), true), item(m_zoomExtentsAction, tr("Ajustar à tela"), true)});

    // ---- Cálculo: todas as funções de análise e dimensionamento --------------------------------------------------------------
    auto *calc = new RibbonPage(this);
    RibbonItem process = item(m_runAnalysisAction, tr("Processar"), true, tr("Roda a análise estrutural linear do modelo com todos os casos e combinações."));
    process.icon = quickIcon(QStringLiteral("play"), QColor(QStringLiteral("#0d7fb9")), 32);
    calc->addGroup(tr("Análise"), {process,
                                   item(m_bucklingAction, tr("Flambagem elástica"), true), item(m_modalAction, tr("Análise modal"), true), item(m_dynamicsAction, tr("Dinâmica"), true)});
    calc->addGroup(tr("Dimensionamento"), {item(m_designSettingsAction, tr("Parâmetros"), true), item(m_verifyDesignAction, tr("Verificar perfis"), true),
                                           item(m_optimizeDesignAction, tr("Dimensionar perfis"), true)});
    calc->addGroup(tr("Ligações e mistas"), {item(m_connectionsAction, tr("Ligações"), true), item(m_compositeAction, tr("Vigas e lajes mistas"), true)});
    calc->addGroup(tr("Resultados"), {item(m_showDiagramsAction, tr("Mostrar diagramas"), true), item(m_hideDiagramsAction, tr("Ocultar diagramas"), true),
                                      item(m_compareAction, tr("Comparar combinações"), true)});
    calc->addGroup(tr("Documentação"), {item(m_reportAction, tr("Memorial"), true)});

    // ---- Cascas -----------------------------------------------------------------------------------------------------------------
    auto *shell = new RibbonPage(this);
    shell->addGroup(tr("Desenhar cascas"), {item(m_shellPolygonAction, tr("Chapa por contorno"), true), item(m_shellRectangleAction, tr("Chapa retangular"), true),
                                            item(m_shellDiskAction, tr("Disco / anel"), true), item(m_shellExtrudeAction, tr("Extrudar linha"), true),
                                            item(m_shellCylinderAction, tr("Cilindro / cone"), true), item(m_shellRevolveAction, tr("Revolução"), true)});
    shell->addGroup(tr("Casca selecionada"), {dangerItem(m_shellDeleteAction, tr("Excluir casca"), true, QStringLiteral("shell-delete.svg"))});
    shell->addGroup(tr("Acoplamento às barras"), {item(m_shellCoupleAction, tr("Acoplar às barras"), true), item(m_shellMonolithicAction, tr("Ligação monolítica"), true),
                                                  item(m_shellSplitAction, tr("Dividir barras nos nós"), true)});
    shell->addGroup(tr("Análise"), {item(m_shellAction, tr("Editor de cascas"), true)});

    m_ribbonPages = {home, tools, loads, view, calc, shell};
    m_ribbon->addTab(home, tr("Início"));
    m_ribbon->addTab(tools, tr("Ferramentas"));
    m_ribbon->addTab(loads, tr("Carregamentos"));
    m_ribbon->addTab(view, tr("Visualização"));
    m_ribbon->addTab(calc, tr("Cálculo"));
    m_ribbon->addTab(shell, tr("Cascas"));

    // Inicializar o pos-processador
    m_postProcessor = std::make_unique<Structura::Visualization::PostProcessorRenderer>();

    connect(m_ribbon, &QTabWidget::currentChanged, this, &MainWindow::onRibbonTabChanged);
    auto *help = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(help, &QShortcut::activated, this, &MainWindow::showCommandHelp);
    applyRibbonLevel();
}

void MainWindow::applyRibbonLevel()
{
    // Reducao por largura da janela: >= 1440 px completo, >= 1200 px medio (botoes pequenos), abaixo recolhe cada grupo em um botao
    const int w = width();
    const int level = w >= 1440 ? 0 : (w >= 1200 ? 1 : 2);
    if (level == m_ribbonLevel) {
        return;
    }
    m_ribbonLevel = level;
    for (auto *page : m_ribbonPages) {
        page->setLevel(level);
    }
}

void MainWindow::setRibbonCollapsed(bool collapsed)
{
    m_ribbonCollapsed = collapsed;
    if (auto *stack = m_ribbon->findChild<QStackedWidget *>()) {
        stack->setVisible(!collapsed);
    }
    if (collapsed) {
        m_ribbon->setMaximumHeight(m_ribbon->tabBar()->sizeHint().height() + 4);
    } else {
        m_ribbon->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    if (m_ribbonCollapseButton) {
        m_ribbonCollapseButton->setToolTip(collapsed ? tr("Expandir a faixa de opções") : tr("Recolher a faixa de opções"));
        QSignalBlocker block(m_ribbonCollapseButton);
        m_ribbonCollapseButton->setChecked(collapsed);
    }
}

void MainWindow::showCommandHelp()
{
    const auto &last = Structura::UI::RibbonToolTip::last();
    if (last.title.isEmpty()) {
        QMessageBox::information(this, tr("Ajuda"),
                                 tr("Passe o mouse sobre um comando da faixa de opções para ver a descrição e o atalho; depois pressione F1 para reler a descrição aqui."));
        return;
    }
    QString text = last.description.isEmpty() ? tr("Sem descrição adicional.") : last.description;
    if (!last.shortcut.isEmpty()) {
        text += tr("\n\nAtalho: %1").arg(last.shortcut);
    }
    QMessageBox::information(this, last.title, text);
}

void MainWindow::onRibbonTabChanged(int index)
{
    Q_UNUSED(index);
    if (m_ribbonCollapsed) {
        setRibbonCollapsed(false);
    }
}
