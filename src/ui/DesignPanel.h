#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/DesignService.h"
#include "app/services/StructuralChecks.h"

#include <QDialog>
#include <QHash>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QCheckBox;
class QDoubleSpinBox;

namespace Structura::UI {

/// Dados exibidos no painel de dimensionamento.
struct DesignPanelData {
    QString title;
    QVector<Structura::App::Services::DesignService::MemberVerdict> verdicts;
    QHash<int, double> memberWeightKg;   ///< peso de cada barra (kg), para o resumo
    QVector<Structura::App::Services::StructuralChecks::DeflectionResult> deflections;
    QVector<Structura::App::Services::StructuralChecks::Story> stories;
    QVector<Structura::App::Services::StructuralChecks::StoryResult> storyResults;
    QStringList warnings;
    Structura::App::ProjectData::DesignSettings settings;
};

/// Painel acoplado de resultados do dimensionamento: resumo, tabela de barras por aproveitamento,
/// verificacoes de servico (flechas e deslocamentos por andar) e memoria de calculo.
class DesignPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit DesignPanel(QWidget *parent = nullptr);

    void setData(const DesignPanelData &data);
    void selectMember(int memberId);

signals:
    void changeProfileRequested(int memberId);
    void verifyRequested();
    void optimizeRequested();
    void settingsRequested();
    void memberActivated(int memberId);

private:
    void rebuildTable();
    void showDetail(int memberId);
    void exportReport();
    QWidget *makeCard(const QString &caption, QLabel **value);

    DesignPanelData m_data;
    QLabel *m_title {nullptr};
    QLabel *m_cardOk {nullptr};
    QLabel *m_cardFail {nullptr};
    QLabel *m_cardEta {nullptr};
    QLabel *m_cardWeight {nullptr};
    QLabel *m_banner {nullptr};
    QComboBox *m_filter {nullptr};
    QLineEdit *m_search {nullptr};
    QTabWidget *m_tabs {nullptr};
    QTableWidget *m_table {nullptr};
    QTableWidget *m_deflections {nullptr};
    QTableWidget *m_stories {nullptr};
    QPlainTextEdit *m_detail {nullptr};
    QPushButton *m_changeProfile {nullptr};
    bool m_updating {false};
};

/// Previa do dimensionamento automatico: perfil atual -> proposto por grupo, com opcao de excluir grupos.
class DesignPreviewDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Row {
        QString key;             ///< identificador do grupo (uuid da secao ou familia)
        QString label;           ///< descricao do grupo
        int bars {0};
        QString current;
        QString proposed;
        double currentEta {0.0};
        double proposedEta {0.0};
        double currentWeightKg {0.0};
        double proposedWeightKg {0.0};
        bool found {false};
        QString note;
    };

    DesignPreviewDialog(const QVector<Row> &rows, double targetEta, QWidget *parent = nullptr);

    [[nodiscard]] QStringList excludedKeys() const;

private:
    QVector<Row> m_rows;
    QTableWidget *m_table {nullptr};
    QLabel *m_total {nullptr};
    void updateTotals();
};

/// Escolha de perfil para uma barra: lista da serie com o aproveitamento de cada candidato.
class ProfileChoiceDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Row {
        QString name;
        double areaCm2 {0.0};
        double kgPerM {0.0};
        double eta {0.0};
        bool current {false};
        bool supported {true};
    };

    ProfileChoiceDialog(const QString &title, const QString &subtitle, const QVector<Row> &rows, QWidget *parent = nullptr);

    [[nodiscard]] QString selectedName() const { return m_selected; }

private:
    void rebuild();

    QVector<Row> m_rows;
    QTableWidget *m_table {nullptr};
    QCheckBox *m_onlyPassing {nullptr};
    QPushButton *m_ok {nullptr};
    QString m_selected;
};

} // namespace Structura::UI
