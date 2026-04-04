#include "MainWindow.h"

#include "MaterialDialog.h"
#include "SectionDialog.h"
#include "viz/SceneController.h"

#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

void MainWindow::ensurePropertiesPanel()
{
    if (m_propertiesPanel) {
        return;
    }
    if (!m_contentLayout) {
        return;
    }

    QWidget *parentWidget = m_contentLayout->parentWidget();
    m_propertiesContainer = new QWidget(parentWidget);
    m_propertiesContainer->setObjectName(QStringLiteral("PropertiesContainer"));
    m_propertiesContainer->setFixedWidth(320);
    m_propertiesContainer->setStyleSheet(QStringLiteral(
        "#PropertiesContainer { background: #f5f7fb; border-left: 1px solid #d6dde8; }"));

    auto *panelLayout = new QVBoxLayout(m_propertiesContainer);
    panelLayout->setContentsMargins(12, 12, 12, 12);
    panelLayout->setSpacing(8);

    m_propertiesPanel = new PropertiesPanel(m_propertiesContainer);
    panelLayout->addWidget(m_propertiesPanel);
    panelLayout->addStretch(1);

    m_propertiesContainer->hide();
    m_contentLayout->addWidget(m_propertiesContainer, 0);

    connect(m_propertiesPanel, &PropertiesPanel::nodeCoordinateEdited,
            this, &MainWindow::onNodeCoordinateEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barMaterialEdited,
            this, &MainWindow::onBarMaterialEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barSectionEdited,
            this, &MainWindow::onBarSectionEdited);

    m_propertiesPanel->setMaterialOptions(buildMaterialOptions());
    m_propertiesPanel->setSectionOptions(buildSectionOptions());
}

void MainWindow::refreshPropertiesPanel()
{
    ensurePropertiesPanel();
    if (!m_propertiesPanel) {
        return;
    }

    m_propertiesPanel->setMaterialOptions(buildMaterialOptions());
    m_propertiesPanel->setSectionOptions(buildSectionOptions());

    const QSet<QUuid> nodeIds = m_selectionModel ? m_selectionModel->selectedNodes() : QSet<QUuid>();
    const QSet<QUuid> barIds = m_selectionModel ? m_selectionModel->selectedBars() : QSet<QUuid>();

    m_propertiesPanel->setNodeEntries(buildNodeEntries(nodeIds));
    m_propertiesPanel->setBarEntries(buildBarEntries(barIds));
    updateGridInfoOnPanel();
    updateLoadActionsEnabled();
}

QVector<QPair<QUuid, QString>> MainWindow::buildMaterialOptions() const
{
    QVector<QPair<QUuid, QString>> materialOptions;
    materialOptions.reserve(m_materials.size());
    for (const auto &mat : m_materials) {
        materialOptions.append({mat.uuid, mat.name});
    }
    return materialOptions;
}

QVector<QPair<QUuid, QString>> MainWindow::buildSectionOptions() const
{
    QVector<QPair<QUuid, QString>> sectionOptions;
    sectionOptions.reserve(m_sections.size());
    for (const auto &sec : m_sections) {
        sectionOptions.append({sec.uuid, sec.name});
    }
    return sectionOptions;
}

std::vector<AssignBarPropertiesDialog::BarInfo> MainWindow::buildAssignDialogBarInfos() const
{
    const auto sceneBars = m_sceneController->bars();
    std::vector<AssignBarPropertiesDialog::BarInfo> bars;
    bars.reserve(sceneBars.size());
    for (const auto &bar : sceneBars) {
        bars.push_back({bar.id,
                        bar.startNodeId,
                        bar.endNodeId,
                        bar.materialId,
                        bar.sectionId,
                        bar.externalId});
    }
    return bars;
}

std::vector<AssignBarPropertiesDialog::NodeInfo> MainWindow::buildAssignDialogNodeInfos() const
{
    const auto sceneNodes = m_sceneController->nodeInfos();
    std::vector<AssignBarPropertiesDialog::NodeInfo> nodes;
    nodes.reserve(sceneNodes.size());
    for (const auto &node : sceneNodes) {
        nodes.push_back({node.id, node.externalId, node.x, node.y, node.z});
    }
    return nodes;
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

void MainWindow::updateGridInfoOnPanel()
{
    if (!m_propertiesPanel || !m_sceneController) {
        return;
    }
    const bool hasGrid = m_sceneController->hasGrid();
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    int nx = 0;
    int ny = 0;
    int nz = 0;
    if (hasGrid) {
        m_sceneController->gridSpacing(dx, dy, dz);
        m_sceneController->gridCounts(nx, ny, nz);
    }
    m_propertiesPanel->setGridInfo(hasGrid, dx, dy, dz, nx, ny, nz);
    updateGridActionsEnabled();
}

void MainWindow::onCreateMaterial()
{
    MaterialDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    MaterialInfo info;
    info.uuid = QUuid::createUuid();
    info.externalId = nextMaterialExternalId();
    info.name = dialog.name();
    info.youngModulus = dialog.youngModulus();
    info.shearModulus = dialog.shearModulus();
    if (info.name.trimmed().isEmpty()) {
        info.name = tr("Material %1").arg(info.externalId);
    }
    m_materials.append(info);
    m_lastMaterialId = info.uuid;
    refreshPropertiesPanel();
}

void MainWindow::onCreateSection()
{
    SectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    SectionInfo info;
    info.uuid = QUuid::createUuid();
    info.externalId = nextSectionExternalId();
    info.name = dialog.name();
    info.area = dialog.area();
    info.iz = dialog.iz();
    info.iy = dialog.iy();
    info.j = dialog.j();
    if (info.name.trimmed().isEmpty()) {
        info.name = tr("Secao %1").arg(info.externalId);
    }
    m_sections.append(info);
    m_lastSectionId = info.uuid;
    refreshPropertiesPanel();
}

void MainWindow::onAssignProperties()
{
    const auto bars = buildAssignDialogBarInfos();
    if (bars.empty()) {
        QMessageBox::information(this, tr("Atribuir a barras"), tr("Nao existem barras cadastradas."));
        return;
    }

    const auto nodes = buildAssignDialogNodeInfos();

    AssignBarPropertiesDialog dialog(buildMaterialOptions(), buildSectionOptions(), bars, nodes, this);
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
    const QUuid sectionId = dialog.selectedSection();

    std::vector<QUuid> barIds = collectAssignDialogSelectionIds(selection, bars);

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
    const QString initialDir = datDialogInitialDirectory();
    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          tr("Salvar modelo"),
                                                          initialDir + QLatin1String("/modelo.dat"),
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
