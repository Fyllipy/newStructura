#include "MainWindow.h"

#include "ConnectionForces.h"

#include "DesignRunData.h"
#include "ReportDialogs.h"
#include "SelectionModel.h"
#include "app/services/DesignService.h"
#include "app/services/ReportService.h"
#include "viz/SceneController.h"

#include <QDate>
#include <QMessageBox>

#include <algorithm>
#include <cmath>

using Structura::App::Services::DesignService;
using Structura::App::Services::ReportService;

namespace {

QString supportText(const Structura::App::ProjectData::NodeSupport &support)
{
    static const char *names[6] = {"UX", "UY", "UZ", "RX", "RY", "RZ"};
    QStringList parts;
    for (int i = 0; i < 6; ++i) {
        if (support.restraints[i]) {
            parts << QString::fromLatin1(names[i]);
        }
    }
    return parts.join(QLatin1Char(' '));
}

} // namespace

bool MainWindow::buildReportInput(ReportService::Input &input, bool withDesign)
{
    input = ReportService::Input {};
    input.projectName = m_projectName.isEmpty() ? tr("Projeto") : m_projectName;
    input.date = QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"));
    const auto factors = DesignService::unitFactors(m_materials);
    input.forceUnit = factors.first == 1.0 ? QStringLiteral("N") : QStringLiteral("kN");
    input.materials = m_materials;
    input.sections = m_sections;
    input.extras = m_extras;
    input.nodalLoads = m_nodalLoads;
    input.memberLoads = m_memberLoads;
    input.designSettings = m_extras.design;

    QHash<int, Structura::App::ProjectData::NodeSupport> supportOf;
    for (const auto &s : m_supports) {
        supportOf.insert(s.nodeId, s);
    }
    QHash<QUuid, Structura::App::SceneNodeInfo> nodeByUuid;
    for (const auto &node : m_sceneController->nodeInfos()) {
        nodeByUuid.insert(node.id, node);
        ReportService::NodeRow row;
        row.id = node.externalId;
        row.x = node.x;
        row.y = node.y;
        row.z = node.z;
        const auto it = supportOf.constFind(node.externalId);
        if (it != supportOf.constEnd()) {
            row.support = supportText(*it);
        }
        input.nodes.append(row);
    }
    std::sort(input.nodes.begin(), input.nodes.end(), [](const auto &a, const auto &b) { return a.id < b.id; });

    QHash<int, QString> groupOf;
    for (const auto &g : m_extras.groups) {
        for (const int member : g.members) {
            groupOf.insert(member, g.name.isEmpty() ? QString::number(g.id) : g.name);
        }
    }
    QHash<int, Structura::App::ProjectData::MemberPlacement> placementOf;
    for (const auto &p : m_extras.placements) {
        placementOf.insert(p.memberId, p);
    }
    auto sectionById = [&](const QUuid &id) -> const Structura::App::ProjectData::SectionInfo * {
        for (const auto &s : m_sections) {
            if (s.uuid == id) {
                return &s;
            }
        }
        return nullptr;
    };
    auto materialName = [&](const QUuid &id) {
        for (const auto &m : m_materials) {
            if (m.uuid == id) {
                return m.name;
            }
        }
        return QString();
    };
    std::vector<DesignService::MemberRef> refs;
    for (const auto &bar : m_sceneController->bars()) {
        ReportService::BarRow row;
        row.id = bar.externalId;
        const auto a = nodeByUuid.constFind(bar.startNodeId);
        const auto b = nodeByUuid.constFind(bar.endNodeId);
        if (a != nodeByUuid.constEnd() && b != nodeByUuid.constEnd()) {
            row.nodeI = a->externalId;
            row.nodeJ = b->externalId;
            row.length = std::sqrt((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
        }
        const auto *section = sectionById(bar.sectionId);
        row.section = section ? section->name : QString();
        row.material = materialName(bar.materialId);
        row.group = groupOf.value(bar.externalId);
        const auto pit = placementOf.constFind(bar.externalId);
        if (pit != placementOf.constEnd()) {
            QStringList parts;
            if (pit->rotationDeg != 0.0) {
                parts << tr("giro %1°").arg(pit->rotationDeg, 0, 'f', 1);
            }
            if (pit->releaseMask != 0) {
                parts << tr("articulada");
            }
            if (pit->offsetA != 0.0 || pit->offsetB != 0.0 || pit->reference != 0) {
                parts << tr("excentricidade");
            }
            row.placement = parts.join(QStringLiteral(", "));
        }
        input.bars.append(row);
        if (section) {
            input.sectionOfMember.insert(bar.externalId, *section);
            input.lengthMOfMember.insert(bar.externalId, row.length);
        }
    }
    std::sort(input.bars.begin(), input.bars.end(), [](const auto &x, const auto &y) { return x.id < y.id; });

    // Resultados da analise
    QHash<int, int> externalNodeOf;   // id do solver (= id externo)
    for (const auto &view : m_resultViews) {
        ReportService::ResultSummary summary;
        summary.name = view.name;
        summary.isCombination = view.isCombination;
        double maxDisp = 0.0;
        for (const auto &node : view.result.nodeResults) {
            const double d = std::sqrt(node.displacements[0] * node.displacements[0] + node.displacements[1] * node.displacements[1] + node.displacements[2] * node.displacements[2]);
            if (d > maxDisp) {
                maxDisp = d;
                summary.maxDisplacementNode = node.nodeId;
            }
            bool any = false;
            for (const double r : node.reactions) {
                any = any || std::abs(r) > 1e-9;
            }
            if (any) {
                ReportService::ReactionRow row;
                row.node = node.nodeId;
                for (int i = 0; i < 6; ++i) {
                    row.values[i] = node.reactions[static_cast<std::size_t>(i)];
                }
                summary.reactions.append(row);
                summary.sumRx += row.values[0];
                summary.sumRy += row.values[1];
                summary.sumRz += row.values[2];
            }
        }
        summary.maxDisplacementMm = maxDisp * factors.second;
        std::sort(summary.reactions.begin(), summary.reactions.end(), [](const auto &x, const auto &y) { return x.node < y.node; });
        input.results.append(summary);
    }

    if (withDesign && !m_resultViews.isEmpty()) {
        DesignRunData run;
        if (buildDesignRun(run)) {
            input.hasDesign = true;
            input.designSettings = run.settings;
            input.verdicts = DesignService::verifyAll(run.effects, run.refs, run.settings, run.lengthFactor);
            input.deflections = run.deflections;
            input.stories = run.elsStories.stories;
            input.storyResults = run.elsStories.storyResults;
            input.designWarnings = run.warnings;
            for (const auto &connection : m_extras.connections) {
                ReportService::Input::ConnectionEntry entry;
                entry.connection = connection;
                const auto forces = Structura::UI::connectionForces(run, connection, m_extras.connections);
                entry.hasForces = forces.has_value();
                entry.forces = forces.value_or(Structura::App::Services::ConnectionService::Forces {});
                entry.evaluation = Structura::App::Services::ConnectionService::evaluate(connection, entry.forces);
                input.connections.append(entry);
            }
        }
    }
    return true;
}

void MainWindow::onGenerateReport()
{
    if (!m_sceneController) {
        return;
    }
    if (m_resultViews.isEmpty()) {
        const auto answer = QMessageBox::question(this, tr("Memorial de cálculo"),
                                                  tr("A análise ainda não foi executada, então o memorial não terá resultados nem dimensionamento.\n\n"
                                                     "Executar a análise agora?"),
                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel) {
            return;
        }
        if (answer == QMessageBox::Yes) {
            executeAnalysis(nullptr);
        }
    }
    ReportService::Input input;
    buildReportInput(input, true);
    QVector<int> selected;
    const auto selectedBars = m_selectionModel->selectedBars();
    for (const auto &bar : m_sceneController->bars()) {
        if (selectedBars.contains(bar.id)) {
            selected.append(bar.externalId);
        }
    }
    Structura::UI::ReportDialog dialog(input, selected, this);
    dialog.exec();
}

void MainWindow::onBarReport()
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    int member = 0;
    const auto selectedBars = m_selectionModel->selectedBars();
    for (const auto &bar : m_sceneController->bars()) {
        if (selectedBars.contains(bar.id)) {
            member = member == 0 ? bar.externalId : std::min(member, bar.externalId);
        }
    }
    if (member == 0) {
        QMessageBox::information(this, tr("Memorial da barra"), tr("Selecione uma barra."));
        return;
    }
    if (m_resultViews.isEmpty() && !executeAnalysis(nullptr)) {
        return;
    }
    ReportService::Input input;
    buildReportInput(input, true);
    if (!input.hasDesign) {
        QMessageBox::information(this, tr("Memorial da barra"),
                                 tr("Não há dimensionamento disponível. Execute a análise e crie combinações últimas (ELU)."));
        return;
    }
    Structura::UI::BarReportDialog dialog(input, member, this);
    dialog.exec();
}
