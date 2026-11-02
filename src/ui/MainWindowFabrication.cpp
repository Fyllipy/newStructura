#include "MainWindow.h"

#include "app/services/DstvExporter.h"
#include "app/services/IfcExporter.h"

#include "QuantityDialog.h"
#include "app/services/ConnectionService.h"
#include "app/services/DxfExporter.h"
#include "app/services/QuantityService.h"
#include "viz/SceneController.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>

#include <cmath>

void MainWindow::onExportDstv()
{
    using Structura::App::Services::DstvExporter;
    if (m_extras.connections.isEmpty()) {
        QMessageBox::information(this, tr("Exportar NC (DSTV)"), tr("Não há ligações definidas no modelo."));
        return;
    }
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Pasta para os arquivos .nc1"));
    if (directory.isEmpty()) {
        return;
    }
    DstvExporter::Options options;
    options.order = m_extras.titleBlock.project.isEmpty() ? m_projectName : m_extras.titleBlock.project;
    options.drawing = m_extras.titleBlock.revision;
    QString error;
    const QStringList files = DstvExporter::exportConnections(m_extras.connections, directory, options, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Exportar NC (DSTV)"), error);
        return;
    }
    showStatusMessage(tr("%1 arquivo(s) .nc1 gravado(s) em %2. Valide-os no software da máquina antes de produzir.").arg(files.size()).arg(directory), 9000);
}

void MainWindow::onExportIfc()
{
    using Structura::App::Services::IfcExporter;
    if (!m_sceneController || m_sceneController->bars().empty()) {
        QMessageBox::information(this, tr("Exportar IFC"), tr("Não há barras no modelo."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar IFC"), QStringLiteral("modelo.ifc"), tr("IFC (*.ifc)"));
    if (path.isEmpty()) {
        return;
    }
    IfcExporter::Options options;
    options.projectName = m_extras.titleBlock.project.isEmpty() ? (m_projectName.isEmpty() ? tr("Projeto") : m_projectName) : m_extras.titleBlock.project;
    options.author = m_extras.titleBlock.author;
    const QString text = IfcExporter::build(buildSheetInput(), options);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Exportar IFC"), tr("Não foi possível gravar o arquivo."));
        return;
    }
    file.write(text.toLatin1());
    showStatusMessage(tr("IFC gravado em %1 (perfis I como IfcIShapeProfileDef; os demais perfis como retângulos). Valide o arquivo no programa de destino.").arg(path), 9000);
}

void MainWindow::addConnectionQuantities(Structura::App::Services::QuantityService::Summary &summary) const
{
    using Structura::App::Services::QuantityService;
    for (const auto &connection : m_extras.connections) {
        const auto evaluation = Structura::App::Services::ConnectionService::evaluate(connection, {});
        for (const auto &part : evaluation.parts) {
            QuantityService::addConnectionPart(summary, QString::fromStdString(part.description), part.quantity, part.massEach, part.length);
        }
    }
}
