#include "MainWindow.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QVTKOpenGLNativeWidget.h>

bool MainWindow::handleWindowDragEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMousePress(me->button(), me->globalPos());
    }
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMouseMove(me->globalPos());
    }
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMouseRelease(me->button());
    }
    case QEvent::MouseButtonDblClick: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMouseDoubleClick(me->button());
    }
    default:
        return false;
    }
}

bool MainWindow::handleInsertNodeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleInsertNodeMousePress(me->button(), me->pos());
    }
    case QEvent::MouseMove: {
        auto *mm = static_cast<QMouseEvent *>(event);
        return handleInsertNodeMouseMove(mm->pos());
    }
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        return handleInsertNodeKeyPress(ke->key());
    }
    default:
        return false;
    }
}

bool MainWindow::handleInsertBarEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *mm = static_cast<QMouseEvent *>(event);
        return handleInsertBarMouseMove(mm->pos());
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleInsertBarMousePress(me->button(), me->pos());
    }
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        return handleInsertBarKeyPress(ke->key());
    }
    default:
        return false;
    }
}

bool MainWindow::handleGridInsertEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *mm = static_cast<QMouseEvent *>(event);
        return handleGridInsertMouseMove(mm->pos());
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleGridInsertMousePress(me->button());
    }
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        return handleGridInsertKeyPress(ke->key(), ke->modifiers(), ke->text());
    }
    default:
        return false;
    }
}

bool MainWindow::handleGridDeleteEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *mm = static_cast<QMouseEvent *>(event);
        return handleGridDeleteMouseMove(mm->pos());
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleGridDeleteMousePress(me->button(), me->pos());
    }
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        return handleGridDeleteKeyPress(ke->key());
    }
    default:
        return false;
    }
}

bool MainWindow::handleDefaultViewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *mm = static_cast<QMouseEvent *>(event);
        return handleDefaultViewportMouseMove(mm->pos());
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleDefaultViewportMousePress(me->button(), me->modifiers(), me->pos());
    }
    case QEvent::Leave:
        return handleDefaultViewportLeave();
    default:
        return false;
    }
}

bool MainWindow::handleViewportCommandEvent(QEvent *event)
{
    if (m_command == Command::InsertNode) {
        return handleInsertNodeEvent(event);
    }
    if (m_command == Command::InsertBarFirst || m_command == Command::InsertBarSecond) {
        return handleInsertBarEvent(event);
    }
    if (isGridInsertCommand(m_command)) {
        return handleGridInsertEvent(event);
    }
    if (m_command == Command::DeleteGridLine) {
        return handleGridDeleteEvent(event);
    }
    return handleDefaultViewportEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_quickBar || obj == m_titleLabel || (obj && obj->property("dragRegion").toBool())) {
        const bool handled = handleWindowDragEvent(event);
        if (handled) {
            return true;
        }
    }

    if (obj == m_vtkWidget && m_sceneController) {
        const bool handled = handleViewportCommandEvent(event);
        if (handled) {
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
