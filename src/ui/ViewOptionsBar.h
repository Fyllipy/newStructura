#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QMenu;
class QToolButton;

namespace Structura::UI {

/**
 * @brief Barra de opcoes de exibicao (porta da paleta de icones do opensa).
 *
 * Conjunto de botoes alternaveis (grid, eixos de referencia, identificadores,
 * nos, apoios, cargas, snap), controles do plano de construcao (anterior /
 * alternar plano / proximo) e o tamanho da malha do grid.
 */
class ViewOptionsBar : public QWidget
{
    Q_OBJECT

public:
    /// Chaves dos botoes alternaveis de exibicao.
    static const QString kGrid;
    static const QString kReferenceAxes;
    static const QString kNodeLabels;
    static const QString kBarLabels;
    static const QString kLocalAxes;
    static const QString kNodes;
    static const QString kSupports;
    static const QString kConstraints;
    static const QString kSolids;
    static const QString kConnections;
    static const QString kAllLevels;
    static const QString kPlaneOnly;
    static const QString kNodeForces;
    static const QString kNodeMoments;
    static const QString kBarForces;
    static const QString kBarMoments;
    static const QString kHideLoadLabels;
    static const QString kAreaPanels;
    static const QString kSnap;

    /// Chaves dos tipos de snap (menu do botao de imã).
    static const QString kSnapGrid;
    static const QString kSnapEndpoint;
    static const QString kSnapCenter;
    static const QString kSnapPerpendicular;
    static const QString kSnapOrthogonal;

    explicit ViewOptionsBar(QWidget *parent = nullptr);

    [[nodiscard]] bool isChecked(const QString &key) const;
    /// Altera o estado sem emitir `toggled` (usado para restaurar do arquivo).
    void setChecked(const QString &key, bool checked);
    [[nodiscard]] bool isSnapTypeChecked(const QString &key) const;
    void setSnapTypeChecked(const QString &key, bool checked);

    [[nodiscard]] double gridSpacing() const;
    void setGridSpacing(double spacing);
    /// Atualiza o rotulo/icone do plano ativo (ex.: "XY").
    void setPlaneName(const QString &name);

    /// Casos de carga exibidos no viewport; `active` vazio significa "todos".
    void setCases(const QStringList &names, const QString &active);
    [[nodiscard]] QString activeCase() const;
    /// Resultados disponiveis (combinacoes e casos) para os diagramas.
    void setResultViews(const QStringList &names, int current);
    [[nodiscard]] int currentResultView() const;

    /// Lista de niveis/posicoes do plano ativo (texto + valor) e o valor atual.
    void setLevels(const QStringList &texts, const QVector<double> &values, double current);

    /// Destaca o botao de selecao quando ha filtros ativos.
    void setSelectionFilterActive(bool active);
    /// 0 = sem cores, 1 = por secao, 2 = por material.
    [[nodiscard]] int colorMode() const { return m_colorMode; }
    /// 3 = por aproveitamento do dimensionamento
    void setColorMode(int mode);

signals:
    void toggled(const QString &key, bool checked);
    void snapTypeToggled(const QString &key, bool checked);
    void gridSpacingChanged(double spacing);
    void previousPlaneRequested();
    void nextPlaneRequested();
    void cyclePlaneRequested();
    void activeCaseChanged(const QString &name);
    void resultViewChanged(int index);
    void levelSelected(double value);
    void selectionFilterRequested();
    void selectMatchingRequested();
    void invertSelectionRequested();
    void clearSelectionRequested();
    void loadsPanelRequested();
    void planViewRequested();
    void colorModeChanged(int mode);

private:
    QToolButton *addToggle(const QString &key, const QString &icon, const QString &tooltip, bool checked = true);
    QToolButton *makeButton(const QString &icon, const QString &tooltip);
    void addSeparator();

    QHash<QString, QToolButton *> m_toggles;
    QHash<QString, QAction *> m_snapActions;
    QToolButton *m_planeButton {nullptr};
    QDoubleSpinBox *m_spacingBox {nullptr};
    QComboBox *m_caseCombo {nullptr};
    QComboBox *m_resultCombo {nullptr};
    QComboBox *m_levelCombo {nullptr};
    QToolButton *m_selectionButton {nullptr};
    int m_colorMode {0};
    QList<QAction *> m_colorActions;
};

} // namespace Structura::UI
