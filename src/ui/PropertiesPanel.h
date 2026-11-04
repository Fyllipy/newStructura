#pragma once

#include "app/ProjectDataTypes.h"
#include "SectionPlacementPanel.h"

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QUuid>
#include <QPair>
#include <optional>
#include <array>

class QLabel;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;

namespace Structura::UI {
class SectionPickerButton;
class SectionPlacementWidget;
}

class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    struct NodeEntry {
        QUuid id;
        int externalId {0};
        double x {0.0};
        double y {0.0};
        double z {0.0};
        std::array<bool, 6> restraints;
        int loadCount {0};
    };

    struct BarEntry {
        QUuid id;
        int externalId {0};
        int nodeI {0};
        int nodeJ {0};
        double length {0.0};
        QUuid materialId;
        QString materialName;
        QUuid sectionId;
        QString sectionName;
        int distributedLoadCount {0};
        // Posicionamento da secao (giro, ponto de referencia, deslocamento adicional) e inercias principais.
        double rotationDeg {0.0};
        int reference {0};
        double offsetA {0.0};
        double offsetB {0.0};
        double sectionIy {0.0};
        double sectionIz {0.0};
        Structura::App::ProjectData::SectionInfo section;
        bool hasSection {false};
        int releaseMask {0};
        int memberType {0};
        QString loadDetails;   ///< cargas atuantes (texto formatado, uma linha por carga)
        QString bucklingDetails;   ///< comprimentos de flambagem atuais (K e comprimento efetivo); vazio = nao exibe
        bool compositeSection {false};     ///< secao de barra composta (5.3.6): dois perfis ligados a intervalos
        double compositeGapMm {0.0};       ///< afastamento e entre os dois perfis (mm), parametro "e" da secao
        double compositeSpacingMm {0.0};   ///< espacamento l entre as ligacoes dos perfis (mm); 0 = L/3
        bool compositeWelded {false};      ///< ligacoes soldadas ou com parafusos protendidos
    };

    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setNodeEntries(const QVector<NodeEntry> &entries);
    void setBarEntries(const QVector<BarEntry> &entries);
    void setMaterialOptions(const QVector<QPair<QUuid, QString>> &options);
    void setSections(const QVector<Structura::App::ProjectData::SectionInfo> &sections);
    /// Barras selecionadas com os dados completos de posicionamento (o editor mostra a primeira).
    void setPlacementTargets(const QVector<Structura::UI::SectionPlacementPanel::Target> &targets);

signals:
    void nodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value);
    void barMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId);
    void barSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId);
    void manageProfilesRequested();
    /// Posicionamento da secao (giro, ponto de referencia, deslocamento e articulacoes) editado dentro das Propriedades.
    void placementEdited(const QVector<QUuid> &ids, const Structura::App::ProjectData::MemberPlacementPatch &patch);
    /// Abre o memorial de calculo da barra selecionada.
    void barReportRequested();
    /// Espacamento entre as ligacoes dos perfis de uma barra composta (mm) e tipo da ligacao; entra no dimensionamento (NBR 8800, 5.3.6).
    /// Afastamento e entre os dois perfis (mm): altera a geometria da secao e, por consequencia, area, inercias e J enviados ao calculo.
    void barCompositeGapEdited(const QVector<QUuid> &ids, double gapMm);
    void barCompositeEdited(const QVector<QUuid> &ids, double spacingMm, bool welded);

private slots:
    void handleNodeXEditing();
    void handleNodeYEditing();
    void handleNodeZEditing();
    void handleMaterialComboChanged(int index);
    void handleSectionPicked(const QUuid &id);

private:
    void buildNodeGroup();
    void buildBarGroup();
    QString formatDouble(double value) const;
    QString restraintsSummary(const NodeEntry &entry) const;
    void updateMaterialComboSelection(const QVector<BarEntry> &entries);
    void updateSectionComboSelection(const QVector<BarEntry> &entries);

    QGroupBox *m_nodeGroup {nullptr};
    QLabel *m_nodeIdLabel {nullptr};
    QLineEdit *m_nodeXEdit {nullptr};
    QLineEdit *m_nodeYEdit {nullptr};
    QLineEdit *m_nodeZEdit {nullptr};
    QLabel *m_nodeRestraintsLabel {nullptr};
    QLabel *m_nodeLoadsLabel {nullptr};

    QGroupBox *m_barGroup {nullptr};
    QLabel *m_barIdLabel {nullptr};
    QLabel *m_barNodesLabel {nullptr};
    QLabel *m_barLengthLabel {nullptr};
    QLabel *m_barDistributedLoadsLabel {nullptr};
    QLabel *m_barLoadDetailsLabel {nullptr};
    QLabel *m_barBucklingLabel {nullptr};
    QComboBox *m_materialCombo {nullptr};
    Structura::UI::SectionPickerButton *m_sectionPicker {nullptr};
    Structura::UI::SectionPlacementPanel *m_placementEditor {nullptr};
    QWidget *m_compositeRow {nullptr};
    QDoubleSpinBox *m_compositeGap {nullptr};
    QDoubleSpinBox *m_compositeSpacing {nullptr};
    QComboBox *m_compositeKind {nullptr};


    QVector<QUuid> m_currentNodeIds;
    QVector<QUuid> m_currentBarIds;
    QVector<QPair<QUuid, QString>> m_materialOptions;

    bool m_blockSignals {false};
};
