#include "MainWindow.h"

#include "BusyProgress.h"
#include "app/UndoRedoService.h"
#include "viz/SceneController.h"
#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QUndoStack>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkRenderWindow.h>

namespace {

constexpr int kAutosaveIntervalMs = 90 * 1000;

QString recoveryDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/recovery");
}

QString recoveryModelPath()
{
    return recoveryDirectory() + QStringLiteral("/recuperacao.dat");
}

QString recoveryInfoPath()
{
    return recoveryDirectory() + QStringLiteral("/recuperacao.txt");
}

} // namespace

// ── Estado do modelo: alteracoes nao salvas e resultados desatualizados ──────

bool MainWindow::isModelDirty() const
{
    return m_extraDirty || (m_undoService && !m_undoService->stack()->isClean());
}

void MainWindow::markModified()
{
    m_extraDirty = true;
    updateWindowTitle();
    updateStaleIndicator();
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_projectName.isEmpty() ? tr("Sem título") : m_projectName;
    setWindowTitle(QStringLiteral("%1%2 — Structura 3D").arg(isModelDirty() ? QStringLiteral("● ") : QString(), name));
}

quint64 MainWindow::quickFingerprint() const
{
    QByteArray bytes = QByteArray::number(stateFingerprint());
    if (m_sceneController) {
        const auto geometry = m_sceneController->captureGeometry();
        for (const auto &n : geometry.nodes) {
            bytes += QByteArray::number(n.externalId) + ':' + QByteArray::number(n.x, 'g', 12) + ',' + QByteArray::number(n.y, 'g', 12) + ','
                + QByteArray::number(n.z, 'g', 12) + ';';
        }
        for (const auto &b : geometry.bars) {
            bytes += QByteArray::number(b.externalId) + ':' + b.start.toByteArray() + b.end.toByteArray() + b.material.toByteArray() + b.section.toByteArray() + ';';
        }
    }
    return qHash(QCryptographicHash::hash(bytes, QCryptographicHash::Md5), 0);
}

void MainWindow::markResultsCurrent()
{
    m_resultsFingerprint = quickFingerprint();
    updateStaleIndicator();
}

void MainWindow::updateStaleIndicator()
{
    const bool hasResults = !m_caseRuns.isEmpty();
    const bool stale = hasResults && quickFingerprint() != m_resultsFingerprint;
    if (m_staleLabel) {
        m_staleLabel->setVisible(stale);
    }
    if (m_postProcessor && m_postProcessor->isStale() != stale) {
        m_postProcessor->setStale(stale);
        if (m_vtkWidget && m_vtkWidget->renderWindow()) {
            m_vtkWidget->renderWindow()->Render();
        }
    }
}

void MainWindow::setupSessionServices()
{
    connect(m_undoService->stack(), &QUndoStack::indexChanged, this, [this](int) {
        updateWindowTitle();
        updateStaleIndicator();
    });
    connect(m_undoService->stack(), &QUndoStack::cleanChanged, this, [this](bool) { updateWindowTitle(); });
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(kAutosaveIntervalMs);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() { autosaveNow(); });
    m_autosaveTimer->start();
    updateWindowTitle();
    QTimer::singleShot(0, this, [this]() { updateWindowTitle(); });   // o rotulo do titulo e criado depois
}

// ── Salvar antes de sair / abrir / criar ─────────────────────────────────────

bool MainWindow::maybeSave()
{
    if (!isModelDirty()) {
        return true;
    }
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Alterações não salvas"));
    box.setText(tr("O modelo \"%1\" tem alterações não salvas.").arg(m_projectName.isEmpty() ? tr("Sem título") : m_projectName));
    box.setInformativeText(tr("Deseja salvar antes de continuar?"));
    QPushButton *save = box.addButton(tr("Salvar"), QMessageBox::AcceptRole);
    box.addButton(tr("Descartar"), QMessageBox::DestructiveRole);
    QPushButton *cancel = box.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    box.setDefaultButton(save);
    box.exec();
    if (box.clickedButton() == cancel) {
        return false;
    }
    if (box.clickedButton() == save) {
        onSaveModel();
        return !isModelDirty();   // cancelou o "Salvar como": permanece aberto
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    removeRecoveryFiles();
    event->accept();
}

// ── Recuperacao automatica ───────────────────────────────────────────────────

void MainWindow::removeRecoveryFiles()
{
    QFile::remove(recoveryModelPath());
    QFile::remove(recoveryInfoPath());
}

void MainWindow::autosaveNow()
{
    if (Structura::UI::isBusy() || !isModelDirty() || !m_sceneController || m_sceneController->nodeCount() == 0) {
        return;
    }
    QDir().mkpath(recoveryDirectory());
    if (!saveToDat(recoveryModelPath(), false)) {
        return;
    }
    QFile info(recoveryInfoPath());
    if (info.open(QIODevice::WriteOnly | QIODevice::Text)) {
        info.write((m_projectName + QLatin1Char('\n') + m_currentFilePath + QLatin1Char('\n') + QDateTime::currentDateTime().toString(Qt::ISODate)).toUtf8());
    }
}

void MainWindow::offerRecovery()
{
    if (!QFile::exists(recoveryModelPath())) {
        return;
    }
    QString name, original, when;
    QFile info(recoveryInfoPath());
    if (info.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(info.readAll()).split(QLatin1Char('\n'));
        name = lines.value(0);
        original = lines.value(1);
        when = lines.value(2);
    }
    const QDateTime stamp = QDateTime::fromString(when, Qt::ISODate);
    const auto answer = QMessageBox::question(this, tr("Recuperar trabalho"),
                                              tr("Foi encontrada uma cópia de recuperação automática%1%2.\n\nO programa pode ter sido fechado de forma inesperada. Deseja recuperá-la?")
                                                  .arg(name.isEmpty() ? QString() : tr(" do modelo \"%1\"").arg(name),
                                                       stamp.isValid() ? tr(" (salva em %1)").arg(stamp.toString(QStringLiteral("dd/MM/yyyy HH:mm"))) : QString()));
    if (answer == QMessageBox::Yes) {
        if (loadFromDat(recoveryModelPath())) {
            m_projectName = name;
            m_currentFilePath.clear();   // forca "Salvar como": o arquivo original nao e sobrescrito sem querer
            m_extraDirty = true;
            updateWindowTitle();
            showStatusMessage(original.isEmpty() ? tr("Trabalho recuperado. Salve o modelo.") : tr("Trabalho recuperado (original: %1). Use Salvar como.").arg(original), 10000);
        }
    }
    removeRecoveryFiles();
}
