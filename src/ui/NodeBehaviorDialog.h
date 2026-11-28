#pragma once

#include "app/services/ConnectionMechanicalResponseService.h"
#include "app/services/TypologyCapabilityService.h"
#include "design/jointengine/RealVsRequired.h"
#include "design/jointengine/RequiredBehavior.h"

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTableWidget;
class QTabWidget;

namespace Structura::UI {

/// "Comportamento requerido do nó" (Incremento 1) + "Definir ligação" (Incremento 2). A primeira aba mostra as barras do nó, a idealização do modelo, as metas de
/// rigidez da NBR 8800:2024 (6.1.2), a compatibilidade com o sistema/contraventamento declarados e as lacunas. A segunda aba lista TODAS as tipologias do configurador
/// existente com quatro capacidades independentes (configurável, dimensionamento normativo, modelo mecânico, curva M–θ) e as ligações já definidas no nó. O
/// comportamento requerido NÃO filtra as tipologias. A janela não cria ligação: devolve um pedido (`request()`) que a janela principal encaminha ao configurador existente.
class NodeBehaviorDialog final : public QDialog
{
    Q_OBJECT
public:
    /// Resposta mecânica de uma ligação definida no nó (Incremento 3).
    struct MechanicalEntry {
        int connectionId {0};
        int memberId {0};
        QString typeName;
        Structura::App::Services::ConnectionMechanicalResult result;
        /// Incremento 4: classificação normativa da ligação real e comparação com o comportamento requerido do par.
        Structura::Design::JointEngine::ConnectionRequirementComparison comparison;
        /// Incremento 5: resposta e comparação do SENTIDO NEGATIVO (mesa inferior tracionada) e o resultado global conservador dos dois sentidos.
        /// `hasNegative = false` mantém a apresentação de uma resposta só (positiva).
        bool hasNegative {false};
        Structura::App::Services::ConnectionMechanicalResult negative;
        Structura::Design::JointEngine::ConnectionRequirementComparison comparisonNegative;
        Structura::Design::JointEngine::DirectionalComparison directional;
        bool symmetryDemonstrated {false};
        QString symmetryNote;
    };
    struct Context {
        std::vector<Structura::App::Services::TypologyRow> typologies;
        std::vector<Structura::App::Services::DefinedConnection> defined;
        std::vector<MechanicalEntry> mechanical;
    };
    /// Pedido de definição de ligação (a viga e o outro elemento do par; `type` = id do configurador).
    struct Request {
        bool valid {false};
        int nodeId {0};
        int beamId {0};
        int otherId {0};
        int type {0};
    };

    explicit NodeBehaviorDialog(const Structura::Design::JointEngine::RequiredBehaviorAssessment &assessment, const Context &context = {}, QWidget *parent = nullptr);

    QString memoryText() const;
    int resultRowCount() const;
    QString resultCell(int row, int column) const;
    QString summaryText() const;
    int typologyRowCount() const;
    QString typologyCell(int row, int column) const;
    int definedRowCount() const;
    QString definedCell(int row, int column) const;
    Request request() const { return m_request; }
    /// O usuário pediu para declarar o sistema estrutural e o contraventamento (abre a configuração existente em Parâmetros).
    bool declareRequested() const { return m_declareRequested; }
    /// Aciona o botao "Declarar sistema estrutural e contraventamento..." como o usuario faria (para testes e verificacao automatizada).
    void requestDeclare();
    int mechanicalCount() const;
    QString mechanicalSummary(int index) const;
    int mechanicalLevelRows() const;
    int mechanicalComponentRows() const;
    QString mechanicalComponentCell(int row, int column) const;
    /// Incremento 4: texto do quadro de classificação/comparação da ligação selecionada, e o aviso no topo da janela (vazio se não há ligação definida).
    QString comparisonText() const;
    QString comparisonNotesText() const;
    QString alertText() const;
    /// Incremento 5: tabela de indicadores por sentido (positivo/negativo) da ligação selecionada e o quadro do resultado global.
    int directionRowCount() const;
    QString directionCell(int row, int column) const;
    void selectDirection(int row);
    QString directionalText() const;
    void selectMechanical(int index);
    /// Seleciona o par e a tipologia e confirma, como o usuário faria (para testes).
    bool requestDefinition(int pairIndex, int typeId);
    /// Grava uma imagem de cada aba (`<prefixo>_aba1.png`, `<prefixo>_aba2.png`): verificação visual automatizada (só desenvolvimento).
    bool saveScreenshots(const QString &prefix);   // (aba 3 gera tambem <prefixo>_aba3b.png rolada ao fim)

private:
    void acceptSelection();
    void showMechanical(int index);
    void fillDirections(int index);
    Structura::Design::JointEngine::RequiredBehaviorAssessment m_assessment;
    Context m_context;
    Request m_request;
    bool m_declareRequested {false};
    QComboBox *m_mechCombo {nullptr};
    QLabel *m_mechSummary {nullptr};
    QLabel *m_mechNumbers {nullptr};
    QLabel *m_cmpLabel {nullptr};
    QLabel *m_alert {nullptr};
    QLabel *m_directional {nullptr};
    QTableWidget *m_directions {nullptr};
    QScrollArea *m_mechScroll {nullptr};
    int m_direction {0};   // 0 = positivo, 1 = negativo
    QWidget *m_curve {nullptr};
    QTableWidget *m_levels {nullptr};
    QTableWidget *m_mechComponents {nullptr};
    QPlainTextEdit *m_mechNotes {nullptr};
    QLabel *m_summary {nullptr};
    QTableWidget *m_members {nullptr};
    QTableWidget *m_results {nullptr};
    QTableWidget *m_typologies {nullptr};
    QTableWidget *m_defined {nullptr};
    QComboBox *m_pair {nullptr};
    QPushButton *m_configure {nullptr};
    QTabWidget *m_tabs {nullptr};
    QPlainTextEdit *m_memory {nullptr};
};

} // namespace Structura::UI
