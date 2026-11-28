// "Grafico Momento-Rotacao": classificacao da rigidez rotacional da ligacao entre duas barras selecionadas (NBR 8800:2024, 6.1.2).
//
// Fluxo: o programa identifica a ligacao ja cadastrada no modulo de ligacoes (nao ha cadastro paralelo), monta o modelo mecanico (metodo dos
// componentes), calcula a curva e CLASSIFICA a ligacao. A idealizacao na analise global (rotulada, semirrigida, rigida) e uma decisao posterior,
// gravada nas ligacoes de extremidade da barra (molas/liberacoes), sem alterar os resultados da analise mecanica.
#include "MainWindow.h"

#include "JointAnalysisDialog.h"
#include "NodeConnectionsDialog.h"
#include "NodeBehaviorDialog.h"
#include "app/services/JointTopologyAdapter.h"
#include "app/services/ConnectionMechanicalResponseService.h"
#include "app/services/ConnectionRequirementService.h"
#include "app/services/NodeBehaviorService.h"
#include "app/services/TypologyCapabilityService.h"
#include "app/services/AnalysisService.h"
#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "app/services/JointAnalysisService.h"
#include "app/services/JointComponentService.h"
#include "app/services/MaterialLibrary.h"
#include "app/services/SectionPropertiesService.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QPointer>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>
#include <QCoreApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

void MainWindow::updateJointAction()
{
    if (m_jointMomentRotationAction && m_selectionModel) {
        m_jointMomentRotationAction->setEnabled(m_selectionModel->selectedBars().size() == 2);
    }
    if (m_nodeBehaviorAction && m_selectionModel) {
        m_nodeBehaviorAction->setEnabled(m_selectionModel->selectedNodes().size() == 1);
    }
}

void MainWindow::onJointMomentRotation()
{
    using Structura::App::ProjectData::NodeConnection;
    using Structura::App::Services::AnalysisService;
    using Structura::App::Services::ConnectionCatalog;
    using Structura::App::Services::ConnectionService;
    using Structura::App::Services::JointAnalysisService;
    using Structura::App::Services::JointComponentService;
    using ComponentResult = JointComponentService::Result;
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    const QSet<QUuid> selected = m_selectionModel->selectedBars();
    if (selected.size() != 2) {
        QMessageBox::information(this, tr("Gráfico Momento-Rotação"), tr("Selecione exatamente duas barras que compartilhem um nó."));
        return;
    }
    std::vector<int> members;
    for (const auto &bar : m_sceneController->bars()) {
        if (selected.contains(bar.id) && bar.externalId > 0) {
            members.push_back(bar.externalId);
        }
    }
    if (members.size() != 2) {
        QMessageBox::information(this, tr("Gráfico Momento-Rotação"), tr("As barras selecionadas não têm identificação válida."));
        return;
    }
    AnalysisService analysis;
    auto build = analysis.buildModel(m_sceneController, m_materials, m_sections, m_supports, m_nodalLoads, m_memberLoads, m_extras);
    if (!build.success) {
        QMessageBox::warning(this, tr("Gráfico Momento-Rotação"), build.errors.join(QLatin1Char('\n')));
        return;
    }
    const std::vector<int> shared = JointAnalysisService::sharedNodes(build.model, members[0], members[1]);
    if (shared.empty()) {
        QMessageBox::warning(this, tr("Gráfico Momento-Rotação"), tr("Seleção inválida: as barras %1 e %2 não compartilham nenhum nó.").arg(members[0]).arg(members[1]));
        return;
    }
    int nodeId = shared.front();
    if (shared.size() > 1) {
        QStringList options;
        for (const int id : shared) {
            options << tr("Nó %1").arg(id);
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, tr("Gráfico Momento-Rotação"), tr("As barras compartilham mais de um nó. Qual nó analisar?"), options, 0, false, &ok);
        if (!ok) {
            return;
        }
        nodeId = shared[static_cast<std::size_t>(std::max(0, static_cast<int>(options.indexOf(choice))))];
    }
    const double forceToKn = Structura::App::Services::SectionPropertiesService::unitFactors(m_materials).first / 1000.0;
    auto context = JointAnalysisService::analyze(build.model, nodeId, members[0], members[1], forceToKn);
    if (!context.valid) {
        QMessageBox::warning(this, tr("Gráfico Momento-Rotação"), QString::fromStdString(context.error));
        return;
    }
    const auto beamBar = context.bars[static_cast<std::size_t>(context.referenceBar)];
    const int beamId = beamBar.memberId;
    const int columnId = members[0] == beamId ? members[1] : members[0];
    const double youngMPa = context.youngModulus / 1000.0;
    const std::vector<int> pair = members;

    auto typeLabel = [](const NodeConnection &c) {
        if (const auto *entry = ConnectionCatalog::find(c.type)) {
            return entry->name;
        }
        return ConnectionService::typeName(c.type);
    };
    // ligacoes cadastradas neste no (qualquer barra): o proprio modulo de ligacoes e a fonte
    auto atNode = [this, nodeId]() {
        std::vector<NodeConnection> list;
        for (const auto &c : m_extras.connections) {
            if (c.nodeId == nodeId) {
                list.push_back(c);
            }
        }
        return list;
    };
    // melhor ligacao do no para as duas barras selecionadas (a da viga tem preferencia) e seu comportamento mecanico
    auto best = [this, nodeId, pair, beamId, youngMPa, typeLabel, atNode]() -> ComponentResult {
        ComponentResult out;
        std::vector<NodeConnection> candidates;
        for (const auto &c : atNode()) {
            if (c.memberId == pair[0] || c.memberId == pair[1]) {
                candidates.push_back(c);
            }
        }
        std::stable_sort(candidates.begin(), candidates.end(), [beamId](const NodeConnection &x, const NodeConnection &y) { return (x.memberId == beamId) > (y.memberId == beamId); });
        QStringList reasons;
        for (const auto &c : candidates) {
            auto r = JointComponentService::evaluate(c, youngMPa);
            if (r.supported) {
                return r;
            }
            reasons << tr("Ligação \"%1\" (barra %2): %3").arg(typeLabel(c)).arg(c.memberId).arg(QString::fromStdString(r.message));
        }
        if (candidates.empty()) {
            out.message = tr("Não há ligação do projeto no nó %1 para as barras %2 e %3 (%4 ligação(ões) cadastrada(s) neste nó para outras barras). Escolha uma ligação do nó, "
                             "crie uma nova configuração construtiva, informe os dados ou importe uma curva.")
                              .arg(nodeId).arg(pair[0]).arg(pair[1]).arg(static_cast<int>(atNode().size())).toStdString();
        } else {
            out.message = reasons.join(QLatin1Char(' ')).toStdString();
        }
        return out;
    };

    const QString key = QStringLiteral("%1:%2:%3").arg(nodeId).arg(std::min(members[0], members[1])).arg(std::max(members[0], members[1]));
    std::optional<Structura::Design::JointClass::JointBehavior> initial;
    const auto stored = m_jointBehaviors.find(key);
    ComponentResult component = best();
    if (component.supported) {
        initial = component.behavior;   // o modelo da ligacao do projeto prevalece sobre dados digitados antes
    } else if (stored != m_jointBehaviors.end()) {
        initial = stored->second;
    } else {
        initial = JointAnalysisService::behaviorFromModel(context);
    }
    auto *dialog = new Structura::UI::JointAnalysisDialog(context, initial, JointAnalysisService::behaviorFromModel(context).has_value(), component, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &Structura::UI::JointAnalysisDialog::behaviorChanged, this,
            [this, key](const Structura::Design::JointClass::JointBehavior &behavior) { m_jointBehaviors[key] = behavior; });

    Structura::UI::JointAnalysisDialog::Hooks hooks;
    hooks.candidates = [atNode, typeLabel]() {
        QStringList items;
        for (const auto &c : atNode()) {
            items << QObject::tr("Barra %1 — %2").arg(c.memberId).arg(typeLabel(c));
        }
        return items;
    };
    hooks.useCandidate = [atNode, youngMPa](int index) -> ComponentResult {
        const auto list = atNode();
        if (index < 0 || index >= static_cast<int>(list.size())) {
            return {};
        }
        auto r = JointComponentService::evaluate(list[static_cast<std::size_t>(index)], youngMPa);
        if (!r.supported && r.message.empty()) {
            r.message = "Esta ligação não tem modelo de componentes.";
        }
        return r;
    };
    // MODELO PRELIMINAR (nivel A): perfis das duas barras + configuracao de referencia; nao cria nem exige ligacao dimensionada
    hooks.preliminary = [this, beamId, columnId, youngMPa](const JointComponentService::PreliminarySettings &settings) -> ComponentResult {
        ComponentResult out;
        JointComponentService::PreliminaryMembers pm;
        pm.youngModulusMPa = youngMPa;
        auto fill = [&](int memberId, bool isBeam) -> bool {
            for (const auto &b : m_sceneController->bars()) {
                if (b.externalId != memberId) {
                    continue;
                }
                const auto section = connectionSectionInfo(b.sectionId);
                if (!section.geometry.contains(QStringLiteral("tf")) || !section.geometry.contains(QStringLiteral("tw"))) {
                    return false;   // nao e perfil I/H
                }
                const auto info = connectionBeamInfo(section, memberId);
                const double radius = section.geometry.value(QStringLiteral("r"), section.geometry.value(QStringLiteral("r1"), 0.0));
                if (isBeam) {
                    pm.beamDepth = info.depth;
                    pm.beamBf = info.bf;
                    pm.beamTf = info.tf;
                    pm.beamTw = info.tw;
                    pm.beamFy = info.fy;
                } else {
                    pm.colDepth = info.depth;
                    pm.colBf = info.bf;
                    pm.colTf = info.tf;
                    pm.colTw = info.tw;
                    pm.colRadius = radius;
                    pm.colFy = info.fy;
                }
                return true;
            }
            return false;
        };
        if (!fill(beamId, true) || !fill(columnId, false)) {
            out.preliminary = true;
            out.message = "Modelo preliminar indisponível: as duas barras precisam ser perfis I/H com dimensões conhecidas (d, bf, tf, tw).";
            return out;
        }
        return JointComponentService::evaluatePreliminary(pm, settings);
    };
    hooks.edit = [this, best]() -> ComponentResult {
        onCheckConnections();   // editor existente do modulo de ligacoes
        return best();
    };
    hooks.reevaluate = best;
    const int dof = beamBar.rotationDof;
    // como a viga esta modelada agora na analise global (e se a modelagem ficou desatualizada em relacao a ligacao do modelo)
    hooks.currentIdealization = [this, beamId, dof, best]() -> QString {
        const auto fresh = best();
        const double siNow = fresh.supported ? fresh.stiffness.initialStiffnessKnm() : 0.0;
        auto stale = [siNow](double k) {
            return siNow > 0.0 && std::abs(k - siNow) / siNow > 0.02 ? QObject::tr(" — DESATUALIZADA: a ligação hoje tem Si = %1 kN·m/rad (reaplique a idealização)").arg(siNow, 0, 'g', 6) : QString();
        };
        for (const auto &spec : m_extras.memberConnections) {
            if (spec.memberId != beamId) {
                continue;
            }
            for (const auto &hinge : spec.hinges) {
                if (hinge.dof == dof) {
                    return tr("semirrígida não linear: rótula bilinear (Si = %1 kN·m/rad, My = %2 kN·m, endurecimento %3) na viga %4")
                               .arg(hinge.kElastic, 0, 'g', 6).arg(hinge.yieldMoment, 0, 'g', 5).arg(hinge.hardeningRatio, 0, 'g', 4).arg(beamId) + stale(hinge.kElastic);
                }
            }
            const double k = spec.springK[static_cast<std::size_t>(dof)];
            if (k == 0.0) {
                return tr("rotulada (articulação) na extremidade da viga %1").arg(beamId);
            }
            if (k > 0.0) {
                return tr("semirrígida linear: mola rotacional de %1 kN·m/rad na viga %2").arg(k, 0, 'g', 6).arg(beamId) + stale(k);
            }
        }
        return tr("rígida (contínua) na viga %1").arg(beamId);
    };
    hooks.applyIdealization = [this, beamId, dof](int kind, double siKnm, double yieldKnm, double hardening) -> QString {
        using Structura::App::ProjectData::MemberConnectionSpec;
        StateGuard guard(this, tr("Idealização da ligação na análise global"));
        MemberConnectionSpec spec;
        spec.memberId = beamId;
        int index = -1;
        for (int i = 0; i < m_extras.memberConnections.size(); ++i) {
            if (m_extras.memberConnections[i].memberId == beamId) {
                spec = m_extras.memberConnections[i];
                index = i;
            }
        }
        // k < 0 = continua (rigida); 0 = articulada; > 0 = mola rotacional (kN.m/rad); kind 3 = rotula plastica bilinear (analise nao linear)
        JointAnalysisService::idealize(spec, dof, kind, siKnm, yieldKnm, hardening);
        if (index >= 0) {
            m_extras.memberConnections.removeAt(index);
        }
        if (!spec.isDefault()) {
            m_extras.memberConnections.append(spec);
        }
        syncBarTypeMarkers();
        showStatusMessage(tr("Idealização da ligação aplicada à viga %1. Rode a análise para ver os efeitos.").arg(beamId), 7000);
        return kind == 0 ? tr("Aplicado: extremidade da viga %1 articulada na análise global.").arg(beamId)
             : kind == 1 ? tr("Aplicado: mola rotacional linear de %1 kN·m/rad na extremidade da viga %2 (Si constante, NBR 8800:2024, 6.1.2.1).").arg(siKnm, 0, 'g', 6).arg(beamId)
             : kind == 3 ? tr("Aplicado: rótula bilinear na extremidade da viga %1 (Si = %2 kN·m/rad, My = %3 kN·m, endurecimento %4). Rótulas plásticas ativam a análise não linear.").arg(beamId).arg(siKnm, 0, 'g', 6).arg(yieldKnm, 0, 'g', 5).arg(hardening, 0, 'g', 4)
                         : tr("Aplicado: extremidade da viga %1 contínua (rígida) na análise global.").arg(beamId);
    };
    dialog->setHooks(hooks);
    dialog->show();
}

// Incremento 1: comportamento estrutural REQUERIDO do no selecionado, anterior a definicao de qualquer ligacao fisica. Usa so o modelo (barras, secoes,
// liberacoes/molas, apoio, andar) e as metas de rigidez da NBR 8800:2024, 6.1.2; nao classifica ligacao real e nao altera o modelo.
// Incremento 1-3: janela do no. Repete (assessa, mostra, encaminha) ate o usuario fechar sem pedir nada: depois de definir uma ligacao ou de declarar o
// sistema estrutural, a janela volta com os resultados atualizados (ligacoes definidas, resposta mecanica, compatibilidade com o sistema).
void MainWindow::onNodeRequiredBehavior()
{
    using Structura::App::Services::AnalysisService;
    using Structura::App::Services::NodeBehaviorService;
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    const QSet<QUuid> selected = m_selectionModel->selectedNodes();
    if (selected.size() != 1) {
        QMessageBox::information(this, tr("Comportamento requerido do nó"), tr("Selecione exatamente um nó."));
        return;
    }
    int nodeId = 0;
    for (const auto &node : m_sceneController->nodeInfos()) {
        if (selected.contains(node.id)) {
            nodeId = node.externalId;
        }
    }
    if (nodeId <= 0) {
        QMessageBox::information(this, tr("Comportamento requerido do nó"), tr("O nó selecionado não tem identificação válida."));
        return;
    }
    for (;;) {
        AnalysisService analysis;
        auto build = analysis.buildModel(m_sceneController, m_materials, m_sections, m_supports, m_nodalLoads, m_memberLoads, m_extras);
        if (!build.success) {
            QMessageBox::warning(this, tr("Comportamento requerido do nó"), build.errors.join(QLatin1Char('\n')));
            return;
        }
        const double forceToKn = Structura::App::Services::SectionPropertiesService::unitFactors(m_materials).first / 1000.0;
        NodeBehaviorService::Options options;
        options.structuralSystem = m_extras.design.structuralSystem;   // declaracao do projeto (0 = nao declarado: nada e presumido)
        options.bracing = m_extras.design.bracingCondition;
        for (const auto &bar : m_sceneController->bars()) {
            if (bar.externalId > 0) {
                options.memberLabels[bar.externalId] = connectionSectionInfo(bar.sectionId).name.toStdString();   // perfil da barra
            }
        }
        const auto assessment = NodeBehaviorService::assess(build.model, nodeId, forceToKn, options);
        Structura::UI::NodeBehaviorDialog::Context context;
        context.typologies = Structura::App::Services::TypologyCapabilityService::list();
        const auto memberIds = Structura::App::Services::JointTopologyAdapter::memberIdsAtNode(build.model, nodeId);
        context.defined = Structura::App::Services::TypologyCapabilityService::describeAtNode(m_extras.connections, nodeId, memberIds, std::numeric_limits<double>::quiet_NaN());
        // resposta mecanica de cada ligacao definida no no, com o E do aco da viga do modelo (sem E nao ha resposta; nada e presumido)
        for (const auto &connection : m_extras.connections) {
            if (connection.nodeId != nodeId) {
                continue;
            }
            double youngMPa = std::numeric_limits<double>::quiet_NaN();
            for (const auto &member : assessment.members) {
                if (member.memberId == connection.memberId && !std::isnan(member.youngKnM2)) {
                    youngMPa = member.youngKnM2 / 1000.0;
                }
            }
            Structura::UI::NodeBehaviorDialog::MechanicalEntry entry;
            entry.connectionId = connection.id;
            entry.memberId = connection.memberId;
            const auto row = Structura::App::Services::TypologyCapabilityService::row(connection.type);
            entry.typeName = row.name.isEmpty() ? Structura::App::Services::ConnectionService::typeName(connection.type) : row.name;
            // Incremento 5: os DOIS sentidos do momento, cada um com a sua resposta mecanica e a sua classificacao (nada e espelhado nem mediado)
            const auto both = Structura::App::Services::ConnectionMechanicalResponseService::evaluateBoth(connection, youngMPa, 40);
            const auto directional = Structura::App::Services::ConnectionRequirementService::evaluateBoth(connection, assessment, both);
            entry.result = both.positive;
            entry.negative = both.negative;
            entry.comparison = directional.positive;
            entry.comparisonNegative = directional.negative;
            entry.directional = directional;
            entry.hasNegative = true;
            entry.symmetryDemonstrated = both.symmetryDemonstrated;
            entry.symmetryNote = QString::fromStdString(both.note);
            context.mechanical.push_back(std::move(entry));
        }
        Structura::UI::NodeBehaviorDialog dialog(assessment, context, this);
        if (!m_nodeBehaviorShotPrefix.isEmpty()) {   // verificacao visual automatizada
            const QString prefix = m_nodeBehaviorShotPrefix;
            m_nodeBehaviorShotPrefix.clear();
            QTimer::singleShot(1500, &dialog, [&dialog, prefix]() {
                dialog.saveScreenshots(prefix);
                dialog.reject();
            });
        }
        dialog.exec();
        const auto request = dialog.request();
        if (request.valid) {
            // encaminha ao configurador de ligacoes EXISTENTE (nao ha configurador paralelo): seleciona a viga e informa no, apoio e tipologia
            for (const auto &bar : m_sceneController->bars()) {
                if (bar.externalId == request.beamId) {
                    m_selectionModel->selectBars({bar.id});
                    break;
                }
            }
            m_pendingConnection = {request.nodeId, request.beamId, request.otherId, request.type};
            onCheckConnections();
            continue;   // volta a janela do no com as ligacoes atualizadas
        }
        if (dialog.declareRequested()) {
            onDesignSettings();   // configuracao existente em Parametros (sem duplicar campos nem persistencia)
            continue;             // depois de salvar, os resultados do no sao atualizados
        }
        break;
    }
}

void MainWindow::openNodeBehaviorForAutomation(int nodeExternalId, const QString &shotPrefix)
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    for (const auto &node : m_sceneController->nodeInfos()) {
        if (node.externalId == nodeExternalId) {
            m_selectionModel->selectNode(node.id);
            break;
        }
    }
    m_nodeBehaviorShotPrefix = shotPrefix;
    onNodeRequiredBehavior();
}

// Verificacao automatizada do fluxo completo no aplicativo real (so desenvolvimento; ativada por STRUCTURA_NODE_FLOW): um temporizador percorre as janelas modais
// que o proprio programa abre (comportamento do no, configurador de ligacoes, perguntas) e executa o que o usuario faria, gravando imagens e um registro.
void MainWindow::runNodeFlowForAutomation(int nodeExternalId, const QString &prefix)
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    for (const auto &node : m_sceneController->nodeInfos()) {
        if (node.externalId == nodeExternalId) {
            m_selectionModel->selectNode(node.id);
            break;
        }
    }
    struct State {
        int behaviorVisits {0};
        int configuratorVisits {0};
        QPointer<QWidget> handled;
        QStringList log;
        QStringList plan;
    };
    auto st = std::make_shared<State>();
    const QString replaceAnswer = qEnvironmentVariable("STRUCTURA_FLOW_REPLACE", QStringLiteral("yes"));
    const int typeId = qEnvironmentVariableIsSet("STRUCTURA_FLOW_TYPE") ? qEnvironmentVariableIntValue("STRUCTURA_FLOW_TYPE") : Structura::App::ProjectData::NodeConnection::kEndPlate;
    const int pairIndex = qEnvironmentVariableIntValue("STRUCTURA_FLOW_PAIR");   // indice do par na lista da janela do no (0 = primeiro)
    // roteiro das visitas a janela do no: declare | create | replace | set | (fim). Padrao: criar e depois tentar substituir (ou alterar valores se STRUCTURA_FLOW_SET)
    st->plan = qEnvironmentVariable("STRUCTURA_FLOW_PLAN").isEmpty()
                   ? (qEnvironmentVariable("STRUCTURA_FLOW_SET").isEmpty() ? QStringList {QStringLiteral("create"), QStringLiteral("replace")} : QStringList {QStringLiteral("create"), QStringLiteral("set")})
                   : qEnvironmentVariable("STRUCTURA_FLOW_PLAN").split(QLatin1Char(','), Qt::SkipEmptyParts);
    auto *timer = new QTimer(this);
    timer->setInterval(900);
    connect(timer, &QTimer::timeout, this, [this, st, prefix, replaceAnswer, typeId, pairIndex]() {
        QWidget *w = QApplication::activeModalWidget();
        if (!w || st->handled == w) {
            return;
        }
        st->handled = w;
        if (auto *behavior = qobject_cast<Structura::UI::NodeBehaviorDialog *>(w)) {
            const int visit = ++st->behaviorVisits;
            st->log << QStringLiteral("[janela do nó, visita %1] ligações definidas listadas: %2; respostas mecânicas: %3").arg(visit).arg(behavior->definedRowCount()).arg(behavior->mechanicalCount());
            for (int i = 0; i < behavior->definedRowCount(); ++i) {
                st->log << QStringLiteral("    ligação %1: %2 | barra %3 | topologia %4 | lacunas %5 | modelo: %6 | M–θ: %7 | fingerprint %8")
                               .arg(i + 1).arg(behavior->definedCell(i, 0), behavior->definedCell(i, 1), behavior->definedCell(i, 2), behavior->definedCell(i, 3), behavior->definedCell(i, 4), behavior->definedCell(i, 5), behavior->definedCell(i, 6));
            }
            if (behavior->mechanicalCount() > 0) {
                st->log << QStringLiteral("    resposta mecânica (1ª): ") + behavior->mechanicalSummary(0).replace(QLatin1Char('\n'), QLatin1Char(' '));
                st->log << QStringLiteral("    níveis de momento: %1; componentes: %2").arg(behavior->mechanicalLevelRows()).arg(behavior->mechanicalComponentRows());
                st->log << QStringLiteral("    aviso do topo: ") + behavior->alertText().remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                st->log << QStringLiteral("    quadro de comparação: ") + behavior->comparisonText().remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                st->log << QStringLiteral("    resultado global: ") + behavior->directionalText().remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                for (int d = 0; d < behavior->directionRowCount(); ++d) {
                    QStringList cells;
                    for (int c = 0; c < 8; ++c) {
                        cells << behavior->directionCell(d, c);
                    }
                    st->log << QStringLiteral("    sentido %1: ").arg(d) + cells.join(QStringLiteral(" | "));
                }
            }
            for (int r = 0; r < behavior->resultRowCount(); ++r) {
                st->log << QStringLiteral("    par %1 | requerido: %2 | sistema/contraventamento: %3 | ligação definida: %4").arg(behavior->resultCell(r, 0), behavior->resultCell(r, 2), behavior->resultCell(r, 7), behavior->resultCell(r, 8));
            }
            behavior->saveScreenshots(QStringLiteral("%1_visita%2").arg(prefix).arg(visit));
            const QString action = st->plan.isEmpty() ? QStringLiteral("end") : st->plan.takeFirst();
            if (action == QLatin1String("declare")) {
                st->log << QStringLiteral("    ação: clicar em \"Declarar sistema estrutural e contraventamento…\"");
                behavior->requestDeclare();
            } else if (action == QLatin1String("create") || action == QLatin1String("replace")) {
                const bool ok = behavior->requestDefinition(pairIndex, typeId);   // create: cria; replace: tenta SUBSTITUIR a ligacao existente
                st->log << QStringLiteral("    ação %1: pedido de definição (par %2, tipologia %3): %4").arg(action).arg(pairIndex).arg(typeId).arg(ok ? QStringLiteral("enviado") : QStringLiteral("RECUSADO"));
            } else if (action == QLatin1String("set")) {
                st->log << QStringLiteral("    (modo STRUCTURA_FLOW_SET: janela fechada; os valores da ligação serão alterados e a janela reaberta)");
                behavior->reject();
            } else {
                behavior->reject();
                st->log << QStringLiteral("    fim do fluxo: janela fechada sem novo pedido");
            }
            return;
        }
        if (auto *configurator = qobject_cast<Structura::UI::NodeConnectionsDialog *>(w)) {
            const int visit = ++st->configuratorVisits;
            const auto list = configurator->connections();
            st->log << QStringLiteral("[configurador, visita %1] ligações na lista: %2").arg(visit).arg(list.size());
            for (const auto &c : list) {
                st->log << QStringLiteral("    id %1 | tipo %2 | nó %3 | viga %4 | extremidade %5 | apoio (barra) %6 | verificada %7")
                               .arg(c.id).arg(c.type).arg(c.nodeId).arg(c.memberId).arg(c.atEnd ? QStringLiteral("fim") : QStringLiteral("início"))
                               .arg(static_cast<int>(c.values.value(QStringLiteral("supMember"), 0.0))).arg(c.verified ? QStringLiteral("sim") : QStringLiteral("não"));
            }
            w->grab().save(QStringLiteral("%1_configurador%2.png").arg(prefix).arg(visit));
            for (auto *button : w->findChildren<QPushButton *>()) {
                if (button->text().contains(QStringLiteral("Aplicar e fechar"))) {
                    st->log << QStringLiteral("    clique em \"Aplicar e fechar\"");
                    button->click();
                    break;
                }
            }
            return;
        }
        if (auto *box = qobject_cast<QMessageBox *>(w)) {
            st->log << QStringLiteral("[pergunta] ") + box->text();
            w->grab().save(QStringLiteral("%1_pergunta%2.png").arg(prefix).arg(st->log.size()));
            const bool replace = box->text().contains(QStringLiteral("Substituir"));
            QAbstractButton *target = nullptr;
            if (replace && replaceAnswer.compare(QStringLiteral("no"), Qt::CaseInsensitive) == 0) {
                target = box->button(QMessageBox::No);
            } else if (replace) {
                target = box->button(QMessageBox::Yes);
            } else {
                target = box->button(QMessageBox::Ok) ? box->button(QMessageBox::Ok) : box->defaultButton();
            }
            st->log << QStringLiteral("    resposta: ") + (target ? target->text() : QStringLiteral("(nenhuma)"));
            if (target) {
                target->click();
            }
            return;
        }
        if (QString::fromLatin1(w->metaObject()->className()).contains(QLatin1String("DesignSettingsDialog"))) {   // Parametros (dimensionamento): declaracao do sistema e do contraventamento
            const int bracing = qEnvironmentVariableIsSet("STRUCTURA_FLOW_BRACING") ? qEnvironmentVariableIntValue("STRUCTURA_FLOW_BRACING") : 1;
            const int system = qEnvironmentVariableIsSet("STRUCTURA_FLOW_SYSTEM") ? qEnvironmentVariableIntValue("STRUCTURA_FLOW_SYSTEM") : 1;
            for (auto *combo : w->findChildren<QComboBox *>()) {
                if (combo->count() == 5 && combo->itemText(1).startsWith(QStringLiteral("Pórtico"))) {
                    combo->setCurrentIndex(system);
                    st->log << QStringLiteral("[Parâmetros] sistema estrutural = ") + combo->currentText();
                } else if (combo->count() == 3 && combo->itemText(1).startsWith(QStringLiteral("Contraventada"))) {
                    combo->setCurrentIndex(bracing);
                    st->log << QStringLiteral("[Parâmetros] contraventamento = ") + combo->currentText();
                }
            }
            w->grab().save(QStringLiteral("%1_parametros.png").arg(prefix));
            if (auto *buttons = w->findChild<QDialogButtonBox *>()) {
                st->log << QStringLiteral("    clique em OK nos Parâmetros");
                buttons->button(QDialogButtonBox::Ok)->click();
            }
            return;
        }
        st->log << QStringLiteral("[janela modal INESPERADA] ") + QString::fromLatin1(w->metaObject()->className());
        w->grab().save(QStringLiteral("%1_inesperada.png").arg(prefix));
        if (auto *dialog = qobject_cast<QDialog *>(w)) {
            dialog->reject();   // evita travar a verificacao
        }
    });
    timer->start();
    onNodeRequiredBehavior();   // bloqueia ate o fluxo terminar
    const QString setSpec = qEnvironmentVariable("STRUCTURA_FLOW_SET");   // ex.: "tRows=1;plateT=12": altera a ligacao criada e reabre a janela do no
    if (!setSpec.isEmpty()) {
        for (auto &connection : m_extras.connections) {
            if (connection.nodeId != nodeExternalId) {
                continue;
            }
            for (const QString &pair : setSpec.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                const QStringList kv = pair.split(QLatin1Char('='));
                if (kv.size() == 2) {
                    connection.values[kv[0].trimmed()] = kv[1].toDouble();
                    st->log << QStringLiteral("[valores alterados] %1 = %2 (ligação %3)").arg(kv[0].trimmed(), kv[1], QString::number(connection.id));
                }
            }
        }
        st->handled = nullptr;
        onNodeRequiredBehavior();
    }
    timer->stop();
    // salvamento: grava o projeto com as ligacoes e confere o arquivo
    const QString savePath = prefix + QStringLiteral("_projeto.dat");
    const bool saved = saveToDat(savePath);
    st->log << QStringLiteral("[salvamento] saveToDat(%1): %2").arg(savePath, saved ? QStringLiteral("ok") : QStringLiteral("FALHOU"));
    QFile file(savePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString text = QString::fromUtf8(file.readAll());
        const int at = text.indexOf(QStringLiteral("[CONNECTIONS]"));
        st->log << QStringLiteral("    seção [CONNECTIONS] no arquivo: %1").arg(at >= 0 ? QStringLiteral("presente") : QStringLiteral("AUSENTE"));
        if (at >= 0) {
            const QStringList lines = text.mid(at).split(QLatin1Char('\n')).mid(0, 6);
            for (const QString &line : lines) {
                st->log << QStringLiteral("      ") + line.left(150);
            }
        }
    }
    st->log << QStringLiteral("ligações no modelo ao final: %1").arg(m_extras.connections.size());
    QFile out(prefix + QStringLiteral("_registro.txt"));
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&out);
        stream.setEncoding(QStringConverter::Utf8);
        stream << st->log.join(QLatin1Char('\n')) << "\n";
    }
}
