#include "core/math/Vec3Ops.h"
#include "MainWindow.h"

#include "ConnectionCopyPanel.h"
#include "DesignRunData.h"
#include "PlacementController.h"
#include "SelectionModel.h"
#include "ShellDialog.h"
#include "app/services/ConnectionCatalog.h"
#include "app/services/DesignService.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellFactory.h"
#include "shell/ShellMesh.h"
#include "shell/ShellVisual.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionPlane.h"
#include "viz/construction/ConstructionScene.h"
#include "app/services/SectionPropertiesService.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QTextStream>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <cmath>

using Structura::Shell::Kind;
using Structura::Shell::Object;
using ShellVec = std::array<double, 3>;

void MainWindow::automatedCapture(const QString &directory)
{
    QDir().mkpath(directory);
    auto settle = [](int ms) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    };
    auto shot = [&](const QString &name) {
        settle(1200);
        const QPixmap pixmap = grab();   // renderiza a propria janela (nao depende de outras janelas sobrepostas na tela)
        pixmap.save(QDir(directory).filePath(name));
    };
    settle(500);
    if (const int barId = qEnvironmentVariableIntValue("STRUCTURA_SELECT_BAR"); barId > 0 && m_sceneController && m_selectionModel) {
        for (const auto &bar : m_sceneController->bars()) {
            if (bar.externalId == barId) {
                m_selectionModel->selectBars({bar.id});   // verificacao automatizada: seleciona a barra (painel de propriedades preenchido)
                break;
            }
        }
    }
    if (const QStringList c = qEnvironmentVariable("STRUCTURA_PLACE_BAR").split(QLatin1Char(',')); c.size() == 6 && m_placement && m_placement->onBarPlaced) {
        // verificacao automatizada: lanca uma barra entre dois pontos e grava as barras resultantes (divisao nos nos intermediarios)
        m_placement->onBarPlaced({c[0].toDouble(), c[1].toDouble(), c[2].toDouble()}, {c[3].toDouble(), c[4].toDouble(), c[5].toDouble()});
        QHash<QUuid, int> nodeNumber;
        for (const auto &node : m_sceneController->nodeInfos()) {
            nodeNumber.insert(node.id, node.externalId);
        }
        QFile out(QDir(directory).filePath(QStringLiteral("placebar.txt")));
        if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream text(&out);
            for (const auto &bar : m_sceneController->bars()) {
                text << "bar " << bar.externalId << ": " << nodeNumber.value(bar.startNodeId) << " -> " << nodeNumber.value(bar.endNodeId) << '\n';
            }
        }
    }
    if (qEnvironmentVariableIntValue("STRUCTURA_TEST_COPYCONN") > 0 && m_sceneController && m_selectionModel) {
        // verificacao automatizada da copia de ligacoes (galpao_3_andares.dat): cria uma chapa de cisalhamento na viga 46 (no 16) apoiada no pilar 1
        // e a copia para a viga 47 (no 18) apoiada no pilar 3, conduzindo a janela de copia como o usuario faria
        using Structura::App::ProjectData::NodeConnection;
        auto barOf = [this](int ext) -> std::optional<SceneController::BarInfo> {
            for (const auto &bar : m_sceneController->bars()) {
                if (bar.externalId == ext) {
                    return bar;
                }
            }
            return std::nullopt;
        };
        if (qEnvironmentVariableIntValue("STRUCTURA_TEST_COPYCONN") == 2) {
            // trelica: no de treliça (banzo = viga 46 no no 17) copiado para o no 20 (banzo 48)
            int braceType = 0;
            for (const auto &entry : Structura::App::Services::ConnectionCatalog::entries()) {
                if (braceType == 0 && Structura::App::Services::ConnectionCatalog::usesBraces(entry.id)) {
                    braceType = entry.id;
                }
            }
            if (const auto chord = barOf(46); chord && braceType > 0) {
                NodeConnection c = makeBracedConnection(braceType, *chord, 17, false);
                c.id = 1;
                c.atEnd = true;
                m_extras.connections.append(c);
            }
        } else
        if (const auto beam = barOf(46); beam && barOf(1)) {
            NodeConnection c = Structura::App::Services::ConnectionService::makeDefault(NodeConnection::kShearTab, connectionBeamInfo(connectionSectionInfo(beam->sectionId), 46), 250.0, 400.0);
            c.id = 1;
            c.memberId = 46;
            c.atEnd = false;
            c.nodeId = 16;
            attachConnectionToSupport(c, *beam, 16, 1);
            m_extras.connections.append(c);
        }
        auto select = [this, barOf](int ext) {
            if (const auto bar = barOf(ext)) {
                m_selectionModel->selectBars({bar->id});
            }
        };
        auto press = [](const QString &text, int index) {
            int seen = 0;
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (auto *panel = qobject_cast<Structura::UI::ConnectionCopyPanel *>(w)) {
                    for (auto *button : panel->findChildren<QPushButton *>()) {
                        if (button->text() == text && seen++ == index) {
                            button->click();
                        }
                    }
                }
            }
        };
        QTimer::singleShot(300, this, [=]() { select(46); });   // clicar na viga da ligacao define a origem
        auto selectNode = [this](int ext) {
            for (const auto &node : m_sceneController->nodeInfos()) {
                if (node.externalId == ext) {
                    m_selectionModel->selectNodes({node.id});
                }
            }
        };
        QTimer::singleShot(600, this, [=]() { selectNode(qEnvironmentVariableIntValue("STRUCTURA_TEST_COPYCONN") == 2 ? 20 : 18); });   // clicar no no de destino copia a ligacao
        QTimer::singleShot(1500, this, [=]() { selectNode(21); });
        QTimer::singleShot(2400, this, [=]() { press(QStringLiteral("Concluir"), 0); });
        auto *closer = new QTimer(this);
        connect(closer, &QTimer::timeout, this, []() {
            if (QWidget *modal = QApplication::activeModalWidget()) {
                modal->close();
            }
        });
        closer->start(400);
        onCopyConnections();
        closer->stop();
        QFile out(QDir(directory).filePath(QStringLiteral("copyconn.txt")));
        if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream text(&out);
            for (const auto &c : std::as_const(m_extras.connections)) {
                text << "id " << c.id << " no " << c.nodeId << " viga " << c.memberId << (c.atEnd ? " fim" : " inicio") << " verificada " << c.verified << " eta " << c.eta
                     << " sup " << c.values.value(QStringLiteral("supMember")) << " gov " << c.governing << '\n';
            }
        }
    }
    if (m_sceneController) {
        const double position[3] = {14.0, -9.0, 11.0}, target[3] = {3.0, 3.0, 3.0}, up[3] = {0.0, 0.0, 1.0};
        m_sceneController->setCameraState(position, target, up, 30.0, false);
    }
    shot(QStringLiteral("1_modelo.png"));
    if (const QByteArray options = qgetenv("STRUCTURA_ANALYSIS_OPTIONS"); !options.isEmpty()) {
        // verificacao automatizada: "second,stab,shear,rigid=0.5,steps=6" liga as opcoes de analise antes de rodar
        for (const QString &item : QString::fromLatin1(options).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const QStringList kv = item.split(QLatin1Char('='));
            if (kv.first() == QLatin1String("second")) {
                m_extras.design.secondOrder = true;
            } else if (kv.first() == QLatin1String("stab")) {
                m_extras.design.stabilityFunctions = true;
            } else if (kv.first() == QLatin1String("shear")) {
                m_extras.design.shearDeformation = true;
            } else if (kv.first() == QLatin1String("rigid")) {
                m_extras.design.rigidZoneFactor = kv.value(1).toDouble();
            } else if (kv.first() == QLatin1String("geo")) {
                m_extras.design.geometricImperfection = true;
            } else if (kv.first() == QLatin1String("steps")) {
                m_extras.design.loadSteps = kv.value(1).toInt();
            }
        }
    }
    QString report;
    const bool ok = executeAnalysis(&report);
    QFile log(QDir(directory).filePath(QStringLiteral("analise.txt")));
    if (log.open(QIODevice::WriteOnly | QIODevice::Text)) {
        log.write(QStringLiteral("analise ok = %1\n").arg(ok).toUtf8());
        log.write(report.toUtf8());
    }
    if (ok && qEnvironmentVariableIntValue("STRUCTURA_DESIGN_REPORT") > 0) {
        // verificacao automatizada: dimensionamento com os esforcos da analise; grava avisos e as barras mais solicitadas
        DesignRunData run;
        QFile out(QDir(directory).filePath(QStringLiteral("dimensionamento.txt")));
        if (buildDesignRun(run) && out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream text(&out);
            for (const QString &w : std::as_const(run.warnings)) {
                text << "AVISO: " << w << QLatin1Char(10);
            }
            const auto verdicts = Structura::App::Services::DesignService::verifyAll(run.effects, run.refs, run.settings, run.lengthFactor);
            for (const auto &v : verdicts) {
                if (v.supported && !v.output.checks.empty()) {
                    text << "barra " << v.memberId << " " << v.sectionName << " eta " << QString::number(v.output.maxRatio, 'f', 3) << " " << QString::fromStdString(v.output.governingCheck)
                         << " " << QString::fromStdString(v.output.governingCombo) << QLatin1Char(10);
                }
            }
        }
    }
    if (!ok) {
        return;
    }
    if (m_postProcessor) {
        showResultView(0);
    }
    if (m_showDiagramsAction) {
        m_showDiagramsAction->trigger();
    }
    for (int index = 3; index <= 6 && m_diagramCombo; ++index) {
        m_diagramCombo->setCurrentIndex(index);
        shot(QStringLiteral("2_diagrama_%1.png").arg(m_diagramCombo->itemText(index)));
    }
}
