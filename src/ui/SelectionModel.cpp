#include "SelectionModel.h"

using namespace Structura;

SelectionModel::SelectionModel(QObject *parent)
    : QObject(parent)
{
}

void SelectionModel::selectNode(const QUuid &id, Mode mode)
{
    QVector<QUuid> ids;
    ids.reserve(1);
    ids.append(id);
    selectNodes(ids, mode);
}

void SelectionModel::selectNodes(const QVector<QUuid> &ids, Mode mode)
{
    if (applySelection(m_selectedNodes, ids, mode)) {
        emit selectionChanged(m_selectedNodes, m_selectedBars);
    } else if (mode == Mode::Replace && ids.isEmpty()) {
        if (!m_selectedNodes.isEmpty()) {
            m_selectedNodes.clear();
            emit selectionChanged(m_selectedNodes, m_selectedBars);
        }
    }
}

void SelectionModel::selectBar(const QUuid &id, Mode mode)
{
    QVector<QUuid> ids;
    ids.reserve(1);
    ids.append(id);
    selectBars(ids, mode);
}

void SelectionModel::selectBars(const QVector<QUuid> &ids, Mode mode)
{
    if (applySelection(m_selectedBars, ids, mode)) {
        emit selectionChanged(m_selectedNodes, m_selectedBars);
    } else if (mode == Mode::Replace && ids.isEmpty()) {
        if (!m_selectedBars.isEmpty()) {
            m_selectedBars.clear();
            emit selectionChanged(m_selectedNodes, m_selectedBars);
        }
    }
}

void SelectionModel::clear()
{
    if (m_selectedNodes.isEmpty() && m_selectedBars.isEmpty()) {
        return;
    }
    m_selectedNodes.clear();
    m_selectedBars.clear();
    emit selectionChanged(m_selectedNodes, m_selectedBars);
}

const QSet<QUuid> &SelectionModel::selectedNodes() const noexcept
{
    return m_selectedNodes;
}

const QSet<QUuid> &SelectionModel::selectedBars() const noexcept
{
    return m_selectedBars;
}

bool SelectionModel::isNodeSelected(const QUuid &id) const noexcept
{
    return m_selectedNodes.contains(id);
}

bool SelectionModel::isBarSelected(const QUuid &id) const noexcept
{
    return m_selectedBars.contains(id);
}

bool SelectionModel::applySelection(QSet<QUuid> &set, const QVector<QUuid> &ids, Mode mode)
{
    if (mode == Mode::Replace) {
        QSet<QUuid> newSet;
        newSet.reserve(ids.size());
        for (const QUuid &id : ids) {
            if (!id.isNull()) {
                newSet.insert(id);
            }
        }
        if (newSet == set) {
            return false;
        }
        set = std::move(newSet);
        return true;
    }

    bool changed = false;
    for (const QUuid &id : ids) {
        if (id.isNull()) {
            continue;
        }
        if (mode == Mode::Add) {
            if (!set.contains(id)) {
                set.insert(id);
                changed = true;
            }
        } else if (mode == Mode::Toggle) {
            if (set.contains(id)) {
                set.remove(id);
                changed = true;
            } else {
                set.insert(id);
                changed = true;
            }
        }
    }
    return changed;
}
