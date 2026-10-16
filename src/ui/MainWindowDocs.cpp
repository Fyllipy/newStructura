#include "MainWindow.h"

#include "QuantityDialog.h"
#include "app/services/DxfExporter.h"
#include "app/services/QuantityService.h"
#include "viz/SceneController.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>

#include <cmath>

void MainWindow::onShowQuantities()
{
    using Structura::App::Services::QuantityService;
    if (!m_sceneController) {
        return;
    }
    QHash<QUuid, std::array<double, 3>> position;
    for (const auto &n : m_sceneController->nodeInfos()) {
        position.insert(n.id, {n.x, n.y, n.z});
    }
    QHash<int, int> typeOf;
    for (const auto &p : m_extras.placements) {
        typeOf.insert(p.memberId, p.memberType);
    }
    QVector<QuantityService::Bar> bars;
    for (const auto &bar : m_sceneController->bars()) {
        if (!position.contains(bar.startNodeId) || !position.contains(bar.endNodeId)) {
            continue;
        }
        const auto a = position.value(bar.startNodeId), b = position.value(bar.endNodeId);
        QuantityService::Bar q;
        q.id = bar.externalId;
        q.length = std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
        q.sectionId = bar.sectionId;
        q.materialId = bar.materialId;
        q.tie = typeOf.value(bar.externalId, 0) == Structura::App::ProjectData::MemberType::kTie;
        bars.append(q);
    }
    if (bars.isEmpty()) {
        QMessageBox::information(this, tr("Quantitativos"), tr("Não há barras no modelo."));
        return;
    }
    Structura::UI::QuantityDialog dialog(QuantityService::compute(bars, m_sections, m_materials), this);
    dialog.exec();
}

void MainWindow::onExportDxf()
{
    using Structura::App::Services::DxfExporter;
    if (!m_sceneController || m_sceneController->bars().empty()) {
        QMessageBox::information(this, tr("Exportar DXF"), tr("Não há barras no modelo."));
        return;
    }
    const QStringList views {tr("Modelo 3D"), tr("Planta (X-Y)"), tr("Elevação X-Z"), tr("Elevação Y-Z")};
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, tr("Exportar DXF"), tr("Vista a exportar:"), views, 0, false, &ok);
    if (!ok) {
        return;
    }
    DxfExporter::Options options;
    options.view = static_cast<DxfExporter::View>(views.indexOf(choice));
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar DXF"), QStringLiteral("modelo.dxf"), tr("DXF (*.dxf)"));
    if (path.isEmpty()) {
        return;
    }
    QHash<QUuid, DxfExporter::Node> nodeById;
    QVector<DxfExporter::Node> nodes;
    for (const auto &n : m_sceneController->nodeInfos()) {
        DxfExporter::Node node {n.externalId, n.x, n.y, n.z};
        nodeById.insert(n.id, node);
        nodes.append(node);
    }
    QHash<int, int> typeOf;
    for (const auto &p : m_extras.placements) {
        typeOf.insert(p.memberId, p.memberType);
    }
    QVector<DxfExporter::Bar> bars;
    for (const auto &bar : m_sceneController->bars()) {
        if (!nodeById.contains(bar.startNodeId) || !nodeById.contains(bar.endNodeId)) {
            continue;
        }
        const auto a = nodeById.value(bar.startNodeId), b = nodeById.value(bar.endNodeId);
        DxfExporter::Bar d;
        d.id = bar.externalId;
        d.x1 = a.x; d.y1 = a.y; d.z1 = a.z;
        d.x2 = b.x; d.y2 = b.y; d.z2 = b.z;
        for (const auto &s : m_sections) {
            if (s.uuid == bar.sectionId) {
                d.section = s.name;
            }
        }
        d.tie = typeOf.value(bar.externalId, 0) == Structura::App::ProjectData::MemberType::kTie;
        bars.append(d);
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Exportar DXF"), tr("Não foi possível gravar o arquivo."));
        return;
    }
    file.write(DxfExporter::build(nodes, bars, options).toLatin1());
    statusBar()->showMessage(tr("DXF exportado: %1 barras, %2 nós.").arg(bars.size()).arg(nodes.size()), 6000);
}
