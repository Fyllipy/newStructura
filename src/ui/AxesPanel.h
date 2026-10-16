#pragma once

#include "app/ProjectDataTypes.h"

#include <QFrame>

#include <array>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QVBoxLayout;
class QWidget;

namespace Structura::UI {

/**
 * @brief Janela flutuante de eixos de referencia organizados por niveis (pavimentos).
 *
 * Cada nivel tem nome, elevacao e seus proprios eixos X/Y nomeados. E possivel criar niveis
 * (vazios ou copiando os eixos do nivel atual) e copiar os eixos para outros niveis ja
 * existentes ou para novas alturas. As alteracoes sao aplicadas ao vivo; linhas invalidas ou
 * duplicadas ficam em vermelho e sao ignoradas.
 */
class AxesPanel : public QFrame
{
    Q_OBJECT

public:
    using ReferenceAxes = Structura::App::ProjectData::ReferenceAxes;

    explicit AxesPanel(QWidget *parent);

    /// Define os eixos exibidos e o nivel inicialmente selecionado (o mais proximo de `elevation`).
    void setAxes(const ReferenceAxes &axes, double elevation = 0.0);
    /// Sugestao de passo entre eixos novos (normalmente o tamanho da malha).
    void setSuggestedStep(double step) { m_suggestedStep = step; }
    /// Posiciona o painel no canto direito do pai.
    void reposition();
    /// Oculta sem aplicar edicoes pendentes (troca/limpeza de modelo).
    void discard();

signals:
    void axesCommitted(const Structura::App::ProjectData::ReferenceAxes &axes);
    /// O usuario escolheu (ou alterou a elevacao de) um nivel: o plano de construcao deve ir para ele.
    void levelActivated(double elevation);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    struct Row {
        QWidget *widget {nullptr};
        QLineEdit *label {nullptr};
        QLineEdit *value {nullptr};
    };
    struct Card {
        QVBoxLayout *rows {nullptr};
        QLabel *count {nullptr};
        std::vector<Row> items;
    };

    QWidget *makeCard(int axis);
    QWidget *makeLevelBar();
    void addRow(int axis, const QString &label, const QString &value, bool focus);
    void removeRow(int axis, QWidget *widget);
    void addSuggestedRow(int axis);
    void clearRows();
    void loadCurrentLevel();
    void rebuildLevelCombo();
    void commit();
    void updateStatus();
    void scheduleResize();
    void addLevel(bool copyCurrentAxes);
    void removeCurrentLevel();
    void openCopyDialog();
    void sortLevels(const QString &keepName);
    [[nodiscard]] Structura::App::ProjectData::AxisLevel *currentLevel();
    [[nodiscard]] QString suggestedLabel(int axis) const;
    [[nodiscard]] QString suggestedLevelName() const;
    [[nodiscard]] static QString levelText(const Structura::App::ProjectData::AxisLevel &level);

    std::array<Card, 2> m_cards {};
    ReferenceAxes m_axes;
    ReferenceAxes m_lastEmitted;
    int m_levelIndex {0};
    QComboBox *m_levelCombo {nullptr};
    QLineEdit *m_levelName {nullptr};
    QLineEdit *m_levelElevation {nullptr};
    QLabel *m_status {nullptr};
    double m_suggestedStep {1.0};
    bool m_building {false};
};

} // namespace Structura::UI
