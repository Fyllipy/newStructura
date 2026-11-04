#pragma once

#include <QDialog>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QRadioButton;
class QTableWidget;

namespace Structura::UI {

/// Dados por combinacao/caso para comparacao lado a lado.
struct CompareData {
    struct View {
        QString name;
        bool isCombination {true};
        QHash<int, std::array<double, 6>> displacements;   ///< por no (m, rad)
        QHash<int, std::array<double, 6>> reactions;       ///< por no (kN, kN m)
        /// Esforcos de uma barra ao longo de todo o comprimento (convencao dos diagramas): maximo, minimo e valores nas extremidades.
        struct BarForces {
            std::array<double, 6> maxAlong {};   ///< N, Vy, Vz, T, My, Mz
            std::array<double, 6> minAlong {};
            std::array<double, 6> atI {};
            std::array<double, 6> atJ {};
        };
        QHash<int, BarForces> forces;
    };
    QVector<View> views;
    QVector<int> nodeIds;
    QVector<int> barIds;
    QSet<int> selectedNodes;
    QSet<int> selectedBars;
};

/// Compara os resultados (deslocamentos, reacoes ou esforcos) de varias combinacoes numa tabela: uma coluna por grandeza.
/// Tres formas: detalhada (uma linha por elemento e combinacao), envoltoria (uma linha por combinacao) e a linha unica dos valores mais criticos.
class CompareResultsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CompareResultsDialog(CompareData data, QWidget *parent = nullptr);

private:
    void refresh();
    void exportCsv();

    CompareData m_data;
    QRadioButton *m_nodes {nullptr};
    QRadioButton *m_bars {nullptr};
    QRadioButton *m_modeDetail {nullptr};
    QRadioButton *m_modeEnvelope {nullptr};
    QRadioButton *m_modeCritical {nullptr};
    QComboBox *m_endMode {nullptr};
    QCheckBox *m_onlySelected {nullptr};
    QListWidget *m_viewList {nullptr};
    QTableWidget *m_table {nullptr};
    QLabel *m_summary {nullptr};
};

} // namespace Structura::UI
