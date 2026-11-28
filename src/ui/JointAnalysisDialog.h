#pragma once

#include "app/services/JointAnalysisService.h"
#include "app/services/JointComponentService.h"
#include "design/JointClassification.h"
#include "ui/JointCharts.h"

#include <QDialog>

#include <functional>
#include <QString>

class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;
class QTableWidget;

namespace Structura::UI {

/// Janela "Analise de Ligacao": grafico momento-rotacao, grafico de classificacao, painel de resultados, dados da ligacao, ligacoes do no,
/// propriedades, memoria de calculo e referencias. Todas as vistas sao recalculadas a partir de um unico `JointBehavior` (sem dados duplicados).
class JointAnalysisDialog final : public QDialog
{
    Q_OBJECT
public:
    JointAnalysisDialog(const Structura::App::Services::JointAnalysisService::Context &context,
                        const std::optional<Structura::Design::JointClass::JointBehavior> &initial, bool hasModelBehavior,
                        const Structura::App::Services::JointComponentService::Result &component = {}, QWidget *parent = nullptr);

    using ComponentResult = Structura::App::Services::JointComponentService::Result;
    /// Ligacao com o modulo de ligacoes do modelo (o cadastro e o do proprio modulo; a janela nao guarda ligacoes) e idealizacao na analise global.
    struct Hooks {
        std::function<QStringList()> candidates;                 ///< ligacoes do no (descricoes)
        std::function<ComponentResult(int)> useCandidate;        ///< avalia a ligacao de indice dado
        std::function<ComponentResult(const Structura::App::Services::JointComponentService::PreliminarySettings &)> preliminary;   ///< modelo preliminar (nao exige ligacao dimensionada)
        std::function<ComponentResult()> edit;                   ///< abre o editor de ligacoes do modelo e reavalia
        std::function<QString(int, double, double, double)> applyIdealization;   ///< (tipo, Si kN.m/rad, My kN.m, razao de endurecimento): 0 rotulada, 1 semirrigida linear, 2 rigida, 3 rotula bilinear; devolve o estado
        std::function<ComponentResult()> reevaluate;             ///< reavalia a ligacao do modelo (para detectar que o usuario a alterou)
        std::function<QString()> currentIdealization;            ///< como a viga esta modelada agora
    };
    void setHooks(const Hooks &hooks);
    void setComponentResult(const ComponentResult &result);
    /// Calcula o modelo preliminar com os campos atuais (configuracao de referencia). Devolve false se o hook falhar.
    bool calculatePreliminary();
    /// Reavalia a ligacao do modelo; se ela mudou, curva, rigidez, classificacao e memoria sao recalculadas. Chamada ao voltar do editor de ligacoes. Devolve true se mudou.
    bool refreshFromModel();
    /// Aviso quando a idealizacao escolhida difere do comportamento calculado (vazio se coincide ou se nao ha classificacao).
    [[nodiscard]] QString idealizationWarning(int kind) const;
    /// Aplica a idealizacao ao modelo (decisao posterior a analise da ligacao); os resultados da analise mecanica nao mudam.
    bool applyIdealization(int kind, bool confirm = true);
    [[nodiscard]] const Structura::Design::JointClass::JointAnalysis &analysis() const { return m_analysis; }
    [[nodiscard]] Structura::Design::JointClass::JointBehavior behavior() const { return m_analysis.behavior; }
    [[nodiscard]] QString memoryText() const;
    [[nodiscard]] MomentRotationChart *momentRotationChart() const { return m_mrChart; }
    [[nodiscard]] ClassificationChart *classificationChart() const { return m_classChart; }
    [[nodiscard]] QString finalClassificationText() const;
    [[nodiscard]] QString connectionStatusText() const;
    [[nodiscard]] QString idealizationStatusText() const;
    [[nodiscard]] QStringList candidateItems() const;
    [[nodiscard]] QString preliminaryBandText() const;

    // entrada de dados (usada pela aba e pelos testes)
    void setOrigin(Structura::Design::JointClass::DataOrigin origin);
    void setCurveText(const QString &text);
    void setFields(const QString &mrdPos, const QString &theta23Pos, const QString &siPos, const QString &mrdNeg = {}, const QString &theta23Neg = {}, const QString &siNeg = {});
    void setUnits(const QString &thetaUnit, const QString &momentUnit);   ///< "rad"/"mrad"; "kN·m"/"N·m"/"kN·cm" (exibicao dos graficos)
    /// Le os campos, recalcula e atualiza todas as vistas. Devolve false (e mostra o erro na aba) se a entrada e invalida.
    bool applyInputs();
    bool exportMemory(const QString &path) const;

signals:
    void behaviorChanged(const Structura::Design::JointClass::JointBehavior &behavior);

protected:
    void changeEvent(QEvent *event) override;

private:
    void buildUi();
    void refresh();
    void useModelBehavior();
    void reloadCandidates();
    void updateSideUi();
    void computePreliminary();
    void updatePreliminaryBand();
    [[nodiscard]] Structura::App::Services::JointComponentService::PreliminarySettings preliminarySettings(int kind) const;
    void adoptComponent(const ComponentResult &result);
    void loadCurveFile();
    void copyMemory();
    void saveMemory();
    void exportChart(bool svg);
    [[nodiscard]] Structura::Design::JointClass::MemoryContext memoryContext() const;
    [[nodiscard]] QString panelHtml() const;

    Structura::App::Services::JointAnalysisService::Context m_context;
    Structura::Design::JointClass::JointAnalysis m_analysis;
    Structura::App::Services::JointComponentService::Result m_component;
    Hooks m_hooks;
    bool m_hasModelBehavior {false};
    bool m_reevaluating {false};
    QLabel *m_connStatus {nullptr}, *m_idealInfo {nullptr}, *m_idealWarn {nullptr}, *m_idealStatus {nullptr};
    QComboBox *m_candidates {nullptr}, *m_idealKind {nullptr};
    QPushButton *m_useCandidate {nullptr}, *m_editConnections {nullptr}, *m_applyIdeal {nullptr}, *m_prelimCalc {nullptr};
    QComboBox *m_prelimKind {nullptr}, *m_prelimBeta {nullptr};
    QLineEdit *m_prelimPlate {nullptr}, *m_prelimBolt {nullptr}, *m_prelimRows {nullptr};
    QLabel *m_prelimBand {nullptr};
    Structura::Design::JointClass::JointBehavior m_modelBehavior;
    QTabWidget *m_tabs {nullptr};
    MomentRotationChart *m_mrChart {nullptr};
    ClassificationChart *m_classChart {nullptr};
    QLabel *m_header {nullptr};
    QLabel *m_panel {nullptr};
    QLabel *m_inputError {nullptr};
    QComboBox *m_origin {nullptr}, *m_thetaUnit {nullptr}, *m_momentUnit {nullptr}, *m_curveThetaUnit {nullptr}, *m_curveMomentUnit {nullptr};
    QLineEdit *m_description {nullptr};
    QPlainTextEdit *m_curveText {nullptr};
    QLineEdit *m_mrdPos {nullptr}, *m_thetaPos {nullptr}, *m_siPos {nullptr}, *m_mrdNeg {nullptr}, *m_thetaNeg {nullptr}, *m_siNeg {nullptr};
    QPlainTextEdit *m_memory {nullptr};
};

} // namespace Structura::UI
