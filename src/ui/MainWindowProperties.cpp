#include "MainWindow.h"

#include <QVTKOpenGLNativeWidget.h>
#include "app/UndoRedoService.h"
#include "AxesPanel.h"
#include "SectionPlacementPanel.h"
#include "ProfileLibrary.h"
#include "ProfileManagerDialog.h"

#include "MaterialDialog.h"
#include "NewProjectWizard.h"
#include "app/services/DesignService.h"
#include "app/services/MaterialLibrary.h"
#include "app/services/UserLibrary.h"
#include "viz/SceneController.h"

#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QElapsedTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <algorithm>
#include <functional>
#include <QVBoxLayout>

namespace {

template<typename InfoContainer, typename IdFn, typename NameFn>
QVector<QPair<QUuid, QString>> mapToOptions(const InfoContainer &items, IdFn idFn, NameFn nameFn)
{
    QVector<QPair<QUuid, QString>> options;
    options.reserve(static_cast<int>(items.size()));
    for (const auto &item : items) {
        options.append({idFn(item), nameFn(item)});
    }
    return options;
}

template<typename Output, typename InputContainer, typename Mapper>
std::vector<Output> mapToStdVector(const InputContainer &items, Mapper mapper)
{
    std::vector<Output> output;
    output.reserve(items.size());
    for (const auto &item : items) {
        output.push_back(mapper(item));
    }
    return output;
}

} // namespace

void MainWindow::ensurePropertiesPanel()
{
    if (m_propertiesPanel) {
        return;
    }
    if (!m_contentLayout) {
        return;
    }

    // flutuante: filho da viewport, no lado direito, sem empurrar a visualizacao (ver positionPlaneLabel)
    m_propertiesContainer = new QWidget(static_cast<QWidget *>(m_vtkWidget));
    m_propertiesContainer->setObjectName(QStringLiteral("PropertiesContainer"));
    m_propertiesContainer->setFixedWidth(320);
    m_propertiesContainer->setStyleSheet(QStringLiteral(
        "#PropertiesContainer { background: #f5f7fb; border-left: 1px solid #d6dde8; }"));
    m_propertiesContainer->setAttribute(Qt::WA_StyledBackground, true);

    auto *panelLayout = new QVBoxLayout(m_propertiesContainer);
    panelLayout->setContentsMargins(12, 12, 12, 12);
    panelLayout->setSpacing(8);

    m_propertiesPanel = new PropertiesPanel(m_propertiesContainer);
    panelLayout->addWidget(m_propertiesPanel);
    panelLayout->addStretch(1);

    m_propertiesContainer->hide();

    connect(m_propertiesPanel, &PropertiesPanel::nodeCoordinateEdited,
            this, &MainWindow::onNodeCoordinateEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barMaterialEdited,
            this, &MainWindow::onBarMaterialEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barSectionEdited,
            this, &MainWindow::onBarSectionEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barReportRequested, this, &MainWindow::onBarReport);
    connect(m_propertiesPanel, &PropertiesPanel::placementEditorRequested, this, [this]() {
        if (m_placementPanel && !m_placementPanel->isVisible()) {
            onTogglePlacementPanel();
        } else if (m_placementPanel) {
            m_placementPanel->raise();
        }
    });
    connect(m_propertiesPanel, &PropertiesPanel::manageProfilesRequested,
            this, &MainWindow::onOpenProfileManager);

    m_propertiesPanel->setMaterialOptions(buildMaterialOptions());
    m_propertiesPanel->setSections(m_sections);
}

namespace {

/// Desfaz/refaz uma troca do conjunto de posicionamentos. Edicoes consecutivas sobre as mesmas barras
/// (arrastar a secao, mexer no controle de giro) sao fundidas num unico passo de desfazer.
class SetPlacementsCommand : public QUndoCommand
{
public:
    using Placements = QVector<Structura::App::ProjectData::MemberPlacement>;
    static constexpr int kId = 0x5043;   // "PC"
    static constexpr qint64 kMergeWindowMs = 2500;

    SetPlacementsCommand(std::function<void(const Placements &)> apply, Placements before, Placements after, QString key)
        : QUndoCommand(QObject::tr("Posicionar seção"))
        , m_apply(std::move(apply))
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_key(std::move(key))
    {
        m_clock.start();
    }

    int id() const override { return kId; }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *next = dynamic_cast<const SetPlacementsCommand *>(other);
        if (!next || next->m_key != m_key || m_clock.elapsed() > kMergeWindowMs) {
            return false;
        }
        m_after = next->m_after;
        m_clock.restart();
        return true;
    }

    void redo() override { m_apply(m_after); }
    void undo() override { m_apply(m_before); }

private:
    std::function<void(const Placements &)> m_apply;
    Placements m_before;
    Placements m_after;
    QString m_key;
    QElapsedTimer m_clock;
};

} // namespace

void MainWindow::onBarPlacementEdited(const QVector<QUuid> &requestedIds,
                                      const Structura::App::ProjectData::MemberPlacementPatch &patch)
{
    if (!m_sceneController || !m_undoService || requestedIds.isEmpty()) {
        return;
    }
    const QVector<QUuid> ids = expandWithGroups(requestedIds);

    using Placement = Structura::App::ProjectData::MemberPlacement;
    QVector<Placement> updated = m_extras.placements;
    QStringList keyParts;
    bool changed = false;
    for (const QUuid &id : ids) {
        const auto *bar = m_sceneController->findBar(id);
        if (!bar || bar->externalId() <= 0) {
            continue;
        }
        keyParts << QString::number(bar->externalId());
        int index = -1;
        for (int i = 0; i < updated.size(); ++i) {
            if (updated[i].memberId == bar->externalId()) {
                index = i;
                break;
            }
        }
        Placement placement;
        placement.memberId = bar->externalId();
        if (index >= 0) {
            placement = updated[index];
        }
        const Placement before = placement;
        patch.applyTo(placement);
        if (placement == before) {
            continue;
        }
        changed = true;
        if (index >= 0) {
            if (placement.isDefault()) {
                updated.removeAt(index);
            } else {
                updated[index] = placement;
            }
        } else if (!placement.isDefault()) {
            updated.append(placement);
        }
    }
    if (!changed) {
        return;
    }

    keyParts.sort();
    m_undoService->stack()->push(new SetPlacementsCommand(
        [this](const QVector<Placement> &placements) {
            m_extras.placements = placements;
            syncPlacementVisuals();
        },
        m_extras.placements,
        updated,
        keyParts.join(QLatin1Char(','))));
}

void MainWindow::onTogglePlacementPanel()
{
    if (!m_placementPanel) {
        return;
    }
    if (m_placementPanel->isVisible()) {
        m_placementPanel->hide();
        return;
    }
    if (m_axesPanel) {
        m_axesPanel->hide();
    }
    m_placementPanel->show();
    refreshPlacementPanel();
    m_placementPanel->reposition();
    m_placementPanel->raise();
}

void MainWindow::refreshPlacementPanel()
{
    if (!m_placementPanel || !m_placementPanel->isVisible() || !m_sceneController) {
        return;
    }
    QHash<QUuid, const SectionInfo *> sectionsById;
    for (const auto &section : m_sections) {
        sectionsById.insert(section.uuid, &section);
    }
    QVector<Structura::UI::SectionPlacementPanel::Target> targets;
    const QSet<QUuid> barIds = m_selectionModel ? m_selectionModel->selectedBars() : QSet<QUuid>();
    // Ordem estavel: ID externo crescente (a primeira barra define os valores exibidos).
    QVector<const SceneController::Bar *> bars;
    for (const QUuid &id : barIds) {
        if (const auto *bar = m_sceneController->findBar(id)) {
            bars.append(bar);
        }
    }
    std::sort(bars.begin(), bars.end(), [](const auto *a, const auto *b) { return a->externalId() < b->externalId(); });
    for (const auto *bar : bars) {
        Structura::UI::SectionPlacementPanel::Target target;
        target.barId = bar->id();
        target.externalId = bar->externalId();
        target.placement.memberId = bar->externalId();
        for (const auto &placement : m_extras.placements) {
            if (placement.memberId == bar->externalId()) {
                target.placement = placement;
                break;
            }
        }
        const auto sectionIt = sectionsById.constFind(bar->sectionId());
        if (sectionIt != sectionsById.constEnd()) {
            target.section = **sectionIt;
            target.hasSection = true;
        }
        targets.append(target);
    }
    m_placementPanel->setTargets(targets);
}

void MainWindow::syncBarTypeMarkers()
{
    if (!m_sceneController) {
        return;
    }
    using Structura::App::ProjectData::MemberRelease::kEndRy;
    using Structura::App::ProjectData::MemberRelease::kEndRz;
    using Structura::App::ProjectData::MemberRelease::kStartRy;
    using Structura::App::ProjectData::MemberRelease::kStartRz;
    QHash<int, Structura::App::ProjectData::MemberPlacement> byMember;
    for (const auto &placement : m_extras.placements) {
        if (placement.releaseMask != 0 || placement.memberType != 0) {
            byMember.insert(placement.memberId, placement);
        }
    }
    std::vector<Structura::Visualization::BarTypeRenderer::Marker> markers;
    if (!byMember.isEmpty()) {
        QHash<QUuid, std::array<double, 3>> position;
        for (const auto &node : m_sceneController->nodeInfos()) {
            position.insert(node.id, {node.x, node.y, node.z});
        }
        for (const auto &bar : m_sceneController->bars()) {
            const auto it = byMember.constFind(bar.externalId);
            if (it == byMember.constEnd() || m_hiddenBars.contains(bar.id) || !position.contains(bar.startNodeId) || !position.contains(bar.endNodeId)) {
                continue;
            }
            Structura::Visualization::BarTypeRenderer::Marker marker;
            marker.start = position.value(bar.startNodeId);
            marker.end = position.value(bar.endNodeId);
            marker.hingeStart = (it->releaseMask & (kStartRy | kStartRz)) != 0;
            marker.hingeEnd = (it->releaseMask & (kEndRy | kEndRz)) != 0;
            marker.kind = it->memberType;
            markers.push_back(marker);
        }
    }
    m_sceneController->setBarTypeMarkers(markers);
}

void MainWindow::syncPlacementVisuals()
{
    if (!m_sceneController) {
        return;
    }
    QHash<int, double> rotations;
    for (const auto &placement : m_extras.placements) {
        if (placement.rotationDeg != 0.0) {
            rotations.insert(placement.memberId, placement.rotationDeg);
        }
    }
    m_sceneController->setBarSectionRotations(rotations);
    syncSolidVisuals();
    syncLoadVisuals();
    syncBarTypeMarkers();
    refreshPlacementPanel();
}

void MainWindow::refreshPropertiesPanel()
{
    ensurePropertiesPanel();
    if (!m_propertiesPanel) {
        return;
    }

    m_propertiesPanel->setMaterialOptions(buildMaterialOptions());
    m_propertiesPanel->setSections(m_sections);

    const QSet<QUuid> nodeIds = m_selectionModel ? m_selectionModel->selectedNodes() : QSet<QUuid>();
    const QSet<QUuid> barIds = m_selectionModel ? m_selectionModel->selectedBars() : QSet<QUuid>();

    m_propertiesPanel->setNodeEntries(buildNodeEntries(nodeIds));
    m_propertiesPanel->setBarEntries(buildBarEntries(barIds));
    updateLoadActionsEnabled();
    refreshLoadsPanel();
    refreshPlacementPanel();
}

QVector<QPair<QUuid, QString>> MainWindow::buildMaterialOptions() const
{
    return mapToOptions(
        m_materials,
        [](const MaterialInfo &mat) { return mat.uuid; },
        [](const MaterialInfo &mat) { return mat.name; });
}

std::vector<AssignBarPropertiesDialog::BarInfo> MainWindow::buildAssignDialogBarInfos() const
{
    const auto sceneBars = m_sceneController->bars();
    return mapToStdVector<AssignBarPropertiesDialog::BarInfo>(
        sceneBars,
        [](const auto &bar) {
            return AssignBarPropertiesDialog::BarInfo{bar.id,
                                                      bar.startNodeId,
                                                      bar.endNodeId,
                                                      bar.materialId,
                                                      bar.sectionId,
                                                      bar.externalId};
        });
}

std::vector<AssignBarPropertiesDialog::NodeInfo> MainWindow::buildAssignDialogNodeInfos() const
{
    const auto sceneNodes = m_sceneController->nodeInfos();
    return mapToStdVector<AssignBarPropertiesDialog::NodeInfo>(
        sceneNodes,
        [](const auto &node) {
            return AssignBarPropertiesDialog::NodeInfo{node.id, node.externalId, node.x, node.y, node.z};
        });
}

std::vector<QUuid> MainWindow::collectAssignDialogSelectionIds(const QList<int> &selection,
                                                               const std::vector<AssignBarPropertiesDialog::BarInfo> &bars) const
{
    std::vector<QUuid> barIds;
    barIds.reserve(selection.size());
    for (int idx : selection) {
        if (idx >= 0 && idx < static_cast<int>(bars.size())) {
            barIds.push_back(bars[static_cast<std::size_t>(idx)].id);
        }
    }
    return barIds;
}

void MainWindow::onCreateMaterial()
{
    StateGuard guard(this, tr("Novo material"));
    bool pascal = false;
    for (const auto &material : m_materials) {
        pascal = pascal || material.youngModulus > 1e10;
    }
    MaterialDialog dialog(pascal, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    MaterialInfo info;
    info.uuid = QUuid::createUuid();
    info.externalId = nextMaterialExternalId();
    info.name = dialog.name();
    info.youngModulus = dialog.youngModulus();
    info.shearModulus = dialog.shearModulus();
    info.unitWeight = dialog.unitWeight();
    info.library = dialog.libraryName();
    info.fy = dialog.fy();
    info.fu = dialog.fu();
    if (info.library.isEmpty() && info.fy > 0.0 && info.fu > 0.0) {
        Structura::App::Services::MaterialLibrary::registerCustomSteel(dialog.name(), info.fy, info.fu, false);
    }
    if (info.name.trimmed().isEmpty()) {
        info.name = tr("Material %1").arg(info.externalId);
    }
    if (dialog.saveToLibrary() && !info.name.isEmpty()) {
        Structura::App::Services::MaterialLibrary::Entry entry;
        entry.name = info.name;
        entry.category = info.fy > 0.0 && info.fu > 0.0 ? Structura::App::Services::MaterialLibrary::Category::StructuralSteel
                                                           : Structura::App::Services::MaterialLibrary::Category::Custom;
        entry.fy = info.fy;
        entry.fu = info.fu;
        entry.eGPa = dialog.youngModulusGPa();
        entry.gGPa = dialog.shearModulusGPa();
        entry.unitWeight = info.unitWeight;
        entry.user = true;
        if (Structura::App::Services::UserLibrary::saveMaterial(entry)) {
            Structura::App::Services::MaterialLibrary::setUserEntries(Structura::App::Services::UserLibrary::loadMaterials());
            showStatusMessage(tr("Material \"%1\" salvo no banco de materiais do programa.").arg(info.name), 5000);
        } else {
            QMessageBox::warning(this, tr("Banco de materiais"), tr("Não foi possível gravar o banco de materiais em %1.").arg(Structura::App::Services::UserLibrary::directory()));
        }
    }
    m_materials.append(info);
    m_lastMaterialId = info.uuid;
    refreshPropertiesPanel();
}

void MainWindow::onCreateSection()
{
    // A criacao de perfis acontece no gerenciador de perfis (painel "Novo perfil").
    ensureProfileManager();
    syncProfileManager();
    m_profileManager->showCreatePage();
}

void MainWindow::onAssignProperties()
{
    const auto bars = buildAssignDialogBarInfos();
    if (bars.empty()) {
        QMessageBox::information(this, tr("Atribuir a barras"), tr("Nao existem barras cadastradas."));
        return;
    }

    const auto nodes = buildAssignDialogNodeInfos();

    AssignBarPropertiesDialog dialog(buildMaterialOptions(), m_sections, bars, nodes, this);
    dialog.setCurrentMaterial(m_lastMaterialId);
    dialog.setCurrentSection(m_lastSectionId);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QList<int> selection = dialog.selectedBarIndices();
    if (selection.isEmpty()) {
        return;
    }

    const QUuid materialId = dialog.selectedMaterial();
    const QUuid sectionId = resolveSectionId(dialog.selectedSection());

    std::vector<QUuid> barIds = collectAssignDialogSelectionIds(selection, bars);
    {
        QVector<QUuid> requested;
        for (const QUuid &id : barIds) {
            requested.append(id);
        }
        barIds.clear();
        for (const QUuid &id : expandWithGroups(requested)) {
            barIds.push_back(id);
        }
    }

    if (!barIds.empty()) {
        m_sceneController->assignBarProperties(barIds,
                                               std::optional<QUuid>(materialId),
                                               std::optional<QUuid>(sectionId));
    }

    if (!materialId.isNull()) {
        m_lastMaterialId = materialId;
    }
    if (!sectionId.isNull()) {
        m_lastSectionId = sectionId;
    }

    refreshPropertiesPanel();
}

void MainWindow::onOpenModel()
{
    if (!maybeSave()) {
        return;
    }
    const QString initialDir = datDialogInitialDirectory();
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          tr("Abrir modelo"),
                                                          initialDir,
                                                          tr("Arquivos DAT (*.dat);;Todos os arquivos (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (loadFromDat(filePath)) {
        m_lastDatDirectory = QFileInfo(filePath).absolutePath();
        showDatStatusMessage(tr("Modelo carregado"), filePath);
    }
}

void MainWindow::onSaveModel()
{
    if (m_currentFilePath.isEmpty()) {
        onSaveModelAs();
        return;
    }
    if (saveToDat(m_currentFilePath)) {
        showDatStatusMessage(tr("Modelo salvo"), m_currentFilePath);
    }
}

void MainWindow::onSaveModelAs()
{
    const QString initialDir = datDialogInitialDirectory();
    const QString suggestion = m_currentFilePath.isEmpty()
        ? initialDir + QLatin1String("/") + (m_projectName.isEmpty() ? QStringLiteral("modelo") : m_projectName) + QLatin1String(".dat")
        : m_currentFilePath;
    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          tr("Salvar modelo como"),
                                                          suggestion,
                                                          tr("Arquivos DAT (*.dat);;Todos os arquivos (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    QString finalPath = filePath;
    if (!finalPath.endsWith(QLatin1String(".dat"), Qt::CaseInsensitive)) {
        finalPath.append(QLatin1String(".dat"));
    }
    if (saveToDat(finalPath)) {
        m_lastDatDirectory = QFileInfo(finalPath).absolutePath();
        showDatStatusMessage(tr("Modelo salvo"), finalPath);
    }
}

void MainWindow::onNewModel()
{
    if (!m_sceneController) {
        return;
    }
    if (!maybeSave()) {
        return;
    }
    QStringList steels;
    for (const auto &steel : Structura::App::Services::DesignService::steelGrades()) {
        steels << steel.name;
    }
    Structura::UI::NewProjectWizard wizard(steels, m_extras.design.defaultSteel, this);
    if (wizard.exec() != QDialog::Accepted) {
        return;
    }
    const auto settings = wizard.settings();
    auto applyIdentification = [&]() {
        if (!settings.name.isEmpty()) {
            m_projectName = settings.name;
        }
        auto &title = m_extras.titleBlock;
        title.project = settings.name;
        title.company = settings.company;
        title.client = settings.client;
        title.site = settings.site;
        title.author = settings.author;
        title.registration = settings.registration;
    };
    bool fromTemplate = false;
    if (settings.start == Structura::UI::NewProjectSettings::Start::ReadyModel) {
        fromTemplate = newFromTemplateDialog();
        if (!fromTemplate) {
            return;   // cancelou o modelo pronto: nada e alterado
        }
        applyIdentification();
    } else {
        resetModel();
        m_projectName.clear();
        applyIdentification();
        addSteelMaterial(settings.steel);
        ensureDefaultMaterial();
        m_extras.loadCases.clear();
        auto addCase = [this](const QString &name, const QString &abbreviation, bool selfWeight) {
            Structura::App::ProjectData::LoadCase c;
            c.name = name;
            c.abbreviation = abbreviation;
            c.selfWeight = selfWeight;
            m_extras.loadCases.append(c);
        };
        if (settings.selfWeightCase) {
            addCase(tr("Permanente"), QStringLiteral("PER"), true);
        }
        if (settings.liveCase) {
            addCase(tr("Sobrecarga"), QStringLiteral("SC"), false);
        }
        if (settings.windX) {
            addCase(tr("Vento X"), QStringLiteral("VX"), false);
        }
        if (settings.windY) {
            addCase(tr("Vento Y"), QStringLiteral("VY"), false);
        }
        rebuildCaseSelector();
    }
    if (m_undoService) {
        m_undoService->stack()->clear();
    }
    m_extraDirty = true;
    m_resultsFingerprint = 0;
    updateWindowTitle();
    updateStaleIndicator();
    updateStatus();
    showStatusMessage(fromTemplate ? tr("Novo projeto criado a partir de um modelo pronto.") : tr("Novo projeto criado. Defina perfis, desenhe o modelo e aplique as cargas."), 6000);
    if (!fromTemplate && settings.openCombinationGenerator && !m_extras.loadCases.isEmpty()) {
        onGenerateCombinations();
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this,
                       tr("Sobre o Structura"),
                       tr("<b>Structura</b><br>Modelagem, análise e dimensionamento de estruturas metálicas em 3D.<br><br>"
                          "Análise: solver de elementos de barra (1ª e 2ª ordem).<br>"
                          "Dimensionamento: NBR 8800:2024 e NBR 14762:2010.<br>"
                          "Ligações: NBR 8800:2024, NBR 6118:2023 e CIDECT Design Guide 1."));
}

QString MainWindow::datDialogInitialDirectory() const
{
    return m_lastDatDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : m_lastDatDirectory;
}

void MainWindow::showDatStatusMessage(const QString &prefix, const QString &filePath)
{
    statusBar()->showMessage(tr("%1: %2").arg(prefix, QFileInfo(filePath).fileName()), 5000);
}

bool MainWindow::addSteelMaterial(const QString &name)
{
    const auto steel = Structura::App::Services::DesignService::findSteel(name);
    if (!steel) {
        return false;
    }
    const auto *entry = Structura::App::Services::MaterialLibrary::find(steel->name);
    MaterialInfo info;
    info.uuid = QUuid::createUuid();
    info.externalId = nextMaterialExternalId();
    info.name = steel->name;
    info.youngModulus = (entry ? entry->eGPa : 200.0) * 1e6;   // kN/m2
    info.shearModulus = (entry ? entry->gGPa : 77.0) * 1e6;
    info.unitWeight = entry ? entry->unitWeight : 78.5;
    info.library = entry ? entry->name : QString();
    info.fy = steel->fy;
    info.fu = steel->fu;
    m_materials.append(info);
    m_lastMaterialId = info.uuid;
    m_extras.design.defaultSteel = steel->name;
    return true;
}

/// Todo projeto tem ao menos um material: sem ele nao ha como atribuir propriedades as barras.
void MainWindow::ensureDefaultMaterial()
{
    if (!m_materials.isEmpty()) {
        return;
    }
    if (!addSteelMaterial(m_extras.design.defaultSteel)) {
        const auto grades = Structura::App::Services::DesignService::steelGrades();
        if (!grades.isEmpty()) {
            addSteelMaterial(grades.first().name);
        }
    }
}
