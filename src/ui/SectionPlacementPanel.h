#pragma once

#include "app/ProjectDataTypes.h"

#include <QFrame>
#include <QUuid>
#include <QVector>

#include <array>

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSlider;
class QToolButton;

namespace Structura::UI {

class SectionPlacementWidget;

/**
 * @brief Janela flutuante de posicionamento da secao e vinculos de extremidade das barras selecionadas.
 *
 * Mesmo estilo do painel de eixos de referencia: as alteracoes sao aplicadas ao vivo (o MainWindow agrupa
 * edicoes consecutivas num unico passo de desfazer). Contem:
 *  - desenho interativo da secao (clique no ponto de referencia, arraste para deslocar);
 *  - giro da secao (graus) com atalhos de 90 graus;
 *  - ponto de referencia (3x3) e deslocamento adicional (mm), por extremidade se desejado;
 *  - liberacoes (articulacoes) de torcao e flexao nas extremidades I e J;
 *  - resultado: posicao do centroide em relacao ao no e inercias nos eixos de referencia.
 */
class SectionPlacementPanel : public QFrame
{
    Q_OBJECT

public:
    using MemberPlacement = Structura::App::ProjectData::MemberPlacement;
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    struct Target {
        QUuid barId;
        int externalId {0};
        MemberPlacement placement;
        SectionInfo section;
        bool hasSection {false};
        double length {0.0};
    };

    explicit SectionPlacementPanel(QWidget *parent);

    /// Barras editadas (a primeira define os valores exibidos). Vazio mostra a orientacao de uso.
    void setTargets(const QVector<Target> &targets);
    void reposition();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void placementEdited(const QVector<QUuid> &ids, const Structura::App::ProjectData::MemberPlacementPatch &patch);

private:
    void buildUi();
    void refreshAll();
    void refreshResult();
    void emitPatch(const Structura::App::ProjectData::MemberPlacementPatch &patch);
    void applyReleasePreset(int mask);
    [[nodiscard]] int currentReleaseMask() const;
    [[nodiscard]] MemberPlacement current() const;
    [[nodiscard]] double currentOffsetA() const;
    [[nodiscard]] double currentOffsetB() const;
    void setEnd(int end);
    void applyHeight();

    QVector<Target> m_targets;
    int m_end {0};              ///< 0 = extremidade I, 1 = J (para o deslocamento adicional)
    bool m_block {false};

    QLabel *m_targetLabel {nullptr};
    QLabel *m_mixedLabel {nullptr};
    SectionPlacementWidget *m_preview {nullptr};
    QSlider *m_rotationSlider {nullptr};
    QDoubleSpinBox *m_rotationSpin {nullptr};
    std::array<QToolButton *, 9> m_referenceButtons {};
    QButtonGroup *m_referenceGroup {nullptr};
    QDoubleSpinBox *m_offsetASpin {nullptr};
    QDoubleSpinBox *m_offsetBSpin {nullptr};
    QCheckBox *m_endDiffers {nullptr};
    QToolButton *m_endButtonI {nullptr};
    QToolButton *m_endButtonJ {nullptr};
    QWidget *m_endTabs {nullptr};
    std::array<QCheckBox *, 6> m_releaseBoxes {};   ///< rx I, ry I, rz I, rx J, ry J, rz J
    QLabel *m_resultCentroid {nullptr};
    QLabel *m_resultInertia {nullptr};
    QLabel *m_resultNote {nullptr};
    QWidget *m_body {nullptr};
    QWidget *m_content {nullptr};
    QScrollArea *m_scroll {nullptr};
    QLabel *m_emptyLabel {nullptr};
};

} // namespace Structura::UI
