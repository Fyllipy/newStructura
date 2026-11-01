#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QSet>
#include <QVector>

class QListWidget;
class QTabWidget;

namespace Structura::UI {

/// Vinculos entre nos e ligacoes: diafragmas rigidos, ligacoes rigidas, apoios inclinados, ligacoes (molas e elementos de interface com leis
/// nao lineares) e vinculos lineares gerais. Os nos de cada novo item vem da selecao atual da viewport.
class ConnectivityDialog final : public QDialog
{
    Q_OBJECT

public:
    ConnectivityDialog(const Structura::App::ProjectData::ProjectExtras &extras, const QVector<int> &selectedNodes, const QSet<int> &existingNodes,
                       QWidget *parent = nullptr);

    /// Copia dos extras com as listas de vinculos editadas.
    [[nodiscard]] Structura::App::ProjectData::ProjectExtras result() const { return m_extras; }

private:
    void refresh();
    void addDiaphragm();
    void addRigidLink();
    void addInclinedSupport();
    void addLink();
    void addConstraint();
    void removeSelected();
    [[nodiscard]] bool requireSelection(int minimum, const QString &what);

    Structura::App::ProjectData::ProjectExtras m_extras;
    QVector<int> m_selected;
    QSet<int> m_existing;
    QTabWidget *m_tabs {nullptr};
    QListWidget *m_diaphragms {nullptr};
    QListWidget *m_rigidLinks {nullptr};
    QListWidget *m_inclined {nullptr};
    QListWidget *m_links {nullptr};
    QListWidget *m_constraints {nullptr};
};

} // namespace Structura::UI
