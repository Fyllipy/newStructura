#include "MainWindow.h"

#include "ModelCheckDialog.h"
#include "SelectionModel.h"
#include "app/services/ModelCheckService.h"
#include "viz/SceneController.h"

#include <QFile>
#include <QMessageBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QUndoStack>
#include "TemplateDialog.h"
#include "app/UndoRedoService.h"

using Structura::App::Services::ModelCheckService;

QVector<ModelCheckService::Issue> MainWindow::checkModel()
{
    ModelCheckService::Input input;
    input.materials = m_materials;
    input.sections = m_sections;
    input.supports = m_supports;
    input.nodalLoads = m_nodalLoads;
    input.memberLoads = m_memberLoads;
    input.extras = m_extras;
    QHash<QUuid, int> nodeExternal;
    for (const auto &node : m_sceneController->nodeInfos()) {
        input.nodes.append({node.externalId, node.x, node.y, node.z});
        nodeExternal.insert(node.id, node.externalId);
    }
    QHash<int, int> typeOf;
    for (const auto &p : m_extras.placements) {
        typeOf.insert(p.memberId, p.memberType);
    }
    for (const auto &bar : m_sceneController->bars()) {
        ModelCheckService::Bar b;
        b.id = bar.externalId;
        b.nodeI = nodeExternal.value(bar.startNodeId, -1);
        b.nodeJ = nodeExternal.value(bar.endNodeId, -1);
        for (const auto &m : m_materials) {
            if (m.uuid == bar.materialId) {
                b.hasMaterial = true;
            }
        }
        for (const auto &s : m_sections) {
            if (s.uuid == bar.sectionId) {
                b.hasSection = true;
                b.sectionName = s.name;
                b.family = s.family;
            }
        }
        b.memberType = typeOf.value(bar.externalId, 0);
        input.bars.append(b);
    }
    return ModelCheckService::run(input);
}

void MainWindow::focusCheckTarget(int kind, int id)
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    if (kind == static_cast<int>(ModelCheckService::TargetKind::Bar)) {
        focusMember(id);
    } else if (kind == static_cast<int>(ModelCheckService::TargetKind::Node)) {
        for (const auto &node : m_sceneController->nodeInfos()) {
            if (node.externalId == id) {
                m_selectionModel->selectNode(node.id);
                break;
            }
        }
    }
}

void MainWindow::onCheckModel()
{
    const auto issues = checkModel();
    Structura::UI::ModelCheckDialog dialog(issues, false, this);
    connect(&dialog, &Structura::UI::ModelCheckDialog::focusRequested, this,
            [this](ModelCheckService::TargetKind kind, int id) { focusCheckTarget(static_cast<int>(kind), id); });
    dialog.exec();
}

void MainWindow::onNewFromTemplate()
{
    if (!m_sceneController) {
        return;
    }
    if (!maybeSave()) {
        return;
    }
    newFromTemplateDialog();
}

bool MainWindow::newFromTemplateDialog()
{
    Structura::UI::TemplateDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    QString error;
    const QString text = dialog.generate(&error);
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Modelo pronto"), error.isEmpty() ? tr("Não foi possível gerar o modelo.") : error);
        return false;
    }
    QTemporaryDir dir;
    if (!dir.isValid()) {
        return false;
    }
    const QString path = dir.filePath(QStringLiteral("modelo.dat"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Modelo pronto"), tr("Não foi possível gravar o arquivo temporário."));
        return false;
    }
    file.write(text.toUtf8());
    file.close();
    if (loadFromDat(path)) {
        m_currentFilePath.clear();
        m_projectName = dialog.suggestedName();
        if (m_undoService) {
            m_undoService->stack()->clear();
        }
        m_sceneController->zoomExtents();
        showStatusMessage(tr("Modelo gerado. Use Checar modelo, rode a análise (F5) e depois o dimensionamento."), 8000);
        return true;
    }
    return false;
}
