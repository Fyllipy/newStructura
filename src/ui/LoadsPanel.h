#pragma once

#include <QFrame>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace Structura::UI {

/// Painel flutuante com as cargas dos elementos selecionados (listar, editar e excluir).
class LoadsPanel : public QFrame
{
    Q_OBJECT

public:
    enum class Kind { Nodal = 0, Distributed = 1, Moment = 2 };

    struct Row {
        Kind kind {Kind::Nodal};
        int index {0};      ///< Indice no vetor de cargas correspondente
        QString element;    ///< Ex.: "No 3", "Barra 7"
        QString loadCase;
        QString values;
    };

    explicit LoadsPanel(QWidget *parent);

    void setRows(const QVector<Row> &rows);
    void reposition();

signals:
    void editRequested(int kind, int index);
    void deleteRequested(const QVector<QPair<int, int>> &items);

private:
    QTreeWidget *m_tree;
    QLabel *m_summary;
    QPushButton *m_edit;
    QPushButton *m_delete;
};

} // namespace Structura::UI
