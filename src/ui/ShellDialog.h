#pragma once

#include "app/ProjectDataTypes.h"
#include "shell/ShellDesign.h"
#include "shell/ShellResults.h"
#include "shell/ShellVisual.h"

#include <QDialog>
#include <QMap>
#include <QStringList>

#include <functional>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QWidget;

namespace Structura::UI {

/// Cascas e chapas: placas/lajes, reservatorios, tanques e vasos de pressao. Edicao do modelo, malha, cargas, apoios, analise por MEF
/// (mefs), resultados coloridos na vista 3D e verificacao (NBR 8800, NBR 6118, criterio de vasos).
class ShellDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Callbacks {
        std::function<QStringList()> caseNames;                                              ///< casos de carga do projeto
        std::function<QVector<Structura::App::ProjectData::LoadCombination>()> combinations;  ///< combinacoes do projeto
        std::function<void(const Structura::Shell::RenderMesh &)> render;                    ///< envia a malha colorida a vista 3D
        std::function<void(bool)> setVisible;                                                ///< mostra/oculta a malha na vista 3D
        std::function<double()> unitsPerMeter;                                               ///< unidades do cenario por metro
        std::function<void(const Structura::Shell::Model &)> modelChanged;                   ///< grava o modelo no projeto
        /// Projeto acoplado as barras: resultados das cascas vindos da analise do projeto (uma analise unica de barras e cascas).
        /// runAnalysis = true executa a analise do projeto antes; false apenas le os resultados disponiveis.
        std::function<bool(const QStringList &cases, bool runAnalysis, Structura::Shell::Results &out, std::string &error)> coupledResults;
        std::function<void(int)> selected;                                                   ///< objeto selecionado na lista (id; -1 nenhum)
    };

    ShellDialog(Structura::Shell::Model model, Callbacks callbacks, QString femLibrary, QWidget *parent = nullptr);

    [[nodiscard]] Structura::Shell::Model model() const { return m_model; }
    /// Substitui o modelo (edicao feita fora da janela, p.ex. desenho na vista 3D ou desfazer).
    void setModel(Structura::Shell::Model model);
    void selectObjectId(int id);
    /// Mostra os resultados da analise acoplada do projeto (sem executa-la de novo).
    void showProjectResults();

private:
    void buildUi();
    QWidget *buildGeometryTab();
    QWidget *buildMaterialsTab();
    QWidget *buildLoadsTab();
    QWidget *buildSupportsTab();
    QWidget *buildResultsTab();
    QWidget *buildDesignTab();

    void ensurePresets();
    void refreshObjectList();
    void selectObject(int row);
    void rebuildGeometryForm();
    void addObject(Structura::Shell::Kind kind);
    void duplicateObject();
    void removeObject();
    void refreshMaterialTable();
    void refreshLoadTable();
    void refreshSupportTable();
    void notifyChanged();

    void generateMesh();
    void analyze();
    void refreshResultSelector();
    void refreshView();
    void updateLegend(const Structura::Shell::RenderMesh &mesh, const QString &title, const QString &unit);
    void runDesign();
    void exportCsv();
    [[nodiscard]] const Structura::Shell::CaseResult *currentResult() const;
    [[nodiscard]] QStringList shellCaseNames() const;

    Structura::Shell::Model m_model;
    Callbacks m_callbacks;
    QString m_femLibrary;
    int m_current {-1};
    bool m_building {false};

    Structura::Shell::Mesh m_mesh;
    Structura::Shell::Results m_results;
    QVector<Structura::Shell::CaseResult> m_combined;   ///< combinacoes calculadas a partir dos casos
    Structura::Shell::DesignOutput m_design;
    bool m_hasMesh {false};
    bool m_runProjectAnalysis {true};
    bool m_analyzing {false};

    QTabWidget *m_tabs {nullptr};
    QListWidget *m_objects {nullptr};
    QWidget *m_geometryHost {nullptr};
    QFormLayout *m_geometryForm {nullptr};
    QTableWidget *m_materials {nullptr};
    QTableWidget *m_loads {nullptr};
    QLabel *m_loadHelp {nullptr};
    QTableWidget *m_supports {nullptr};
    QLabel *m_meshInfo {nullptr};
    QLabel *m_status {nullptr};

    QComboBox *m_resultCase {nullptr};
    QComboBox *m_quantity {nullptr};
    QDoubleSpinBox *m_deformScale {nullptr};
    QCheckBox *m_smooth {nullptr};
    QCheckBox *m_edges {nullptr};
    QCheckBox *m_fixedRange {nullptr};
    QDoubleSpinBox *m_rangeMin {nullptr};
    QDoubleSpinBox *m_rangeMax {nullptr};
    QLabel *m_legend {nullptr};
    QLabel *m_legendText {nullptr};
    QPlainTextEdit *m_summary {nullptr};

    QListWidget *m_ultimate {nullptr};
    QComboBox *m_service {nullptr};
    QComboBox *m_designView {nullptr};
    QTableWidget *m_designTable {nullptr};
    QPlainTextEdit *m_designNotes {nullptr};
};

} // namespace Structura::UI
