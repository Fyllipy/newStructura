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
        QHash<int, std::array<double, 12>> forces;         ///< por barra (esforcos locais nas duas extremidades)
    };
    QVector<View> views;
    QVector<int> nodeIds;
    QVector<int> barIds;
    QSet<int> selectedNodes;
    QSet<int> selectedBars;
};

/// Compara uma grandeza (deslocamento, reacao ou esforco) entre varias combinacoes e mostra a envoltoria e a que governa.
class CompareResultsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CompareResultsDialog(CompareData data, QWidget *parent = nullptr);

private:
    void rebuildQuantities();
    void refresh();
    void exportCsv();

    CompareData m_data;
    QRadioButton *m_nodes {nullptr};
    QRadioButton *m_bars {nullptr};
    QComboBox *m_quantity {nullptr};
    QComboBox *m_endMode {nullptr};
    QCheckBox *m_onlySelected {nullptr};
    QListWidget *m_viewList {nullptr};
    QTableWidget *m_table {nullptr};
    QLabel *m_summary {nullptr};
};

} // namespace Structura::UI
