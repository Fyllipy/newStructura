#pragma once

#include <QObject>
#include <QSet>
#include <QUuid>
#include <QVector>

namespace Structura {

class SelectionModel : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        Replace,
        Add,
        Toggle
    };

    explicit SelectionModel(QObject *parent = nullptr);

    void selectNode(const QUuid &id, Mode mode = Mode::Replace);
    void selectNodes(const QVector<QUuid> &ids, Mode mode = Mode::Replace);

    void selectBar(const QUuid &id, Mode mode = Mode::Replace);
    void selectBars(const QVector<QUuid> &ids, Mode mode = Mode::Replace);

    void clear();

    const QSet<QUuid> &selectedNodes() const noexcept;
    const QSet<QUuid> &selectedBars() const noexcept;

    bool isNodeSelected(const QUuid &id) const noexcept;
    bool isBarSelected(const QUuid &id) const noexcept;

signals:
    void selectionChanged(const QSet<QUuid> &nodes, const QSet<QUuid> &bars);

private:
    bool applySelection(QSet<QUuid> &set, const QVector<QUuid> &ids, Mode mode);

    QSet<QUuid> m_selectedNodes;
    QSet<QUuid> m_selectedBars;
};

} // namespace Structura
