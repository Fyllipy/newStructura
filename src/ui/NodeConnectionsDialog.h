#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/ConnectionService.h"

#include <QDialog>
#include <QMap>

#include <functional>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;
class QFormLayout;

namespace Structura::UI {

/// Ligacoes viga-apoio do modelo: lista, edicao dos parametros, verificacao com os esforcos da analise e proposta automatica.
class NodeConnectionsDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Callbacks {
        /// Cria uma ligacao (escolha de viga/extremidade/tipo pelo usuario); id == 0 = cancelado.
        std::function<Structura::App::ProjectData::NodeConnection(int type)> create;
        /// Esforcos de calculo (envoltoria das combinacoes ultimas) na extremidade da viga.
        std::function<std::optional<Structura::App::Services::ConnectionService::Forces>(const Structura::App::ProjectData::NodeConnection &)> forces;
        /// Seleciona a viga/no no modelo.
        std::function<void(const Structura::App::ProjectData::NodeConnection &)> focus;
        /// Abre a calculadora avulsa.
        std::function<void()> calculator;
    };

    NodeConnectionsDialog(const QVector<Structura::App::ProjectData::NodeConnection> &connections, Callbacks callbacks, QWidget *parent = nullptr);

    [[nodiscard]] QVector<Structura::App::ProjectData::NodeConnection> connections() const { return m_connections; }

private:
    void rebuildList();
    void selectRow(int row);
    void buildForm();
    void readForm();
    void verifyCurrent();
    void verifyAll();
    void chooseType();
    void addConnection(int type);
    void removeCurrent();
    void proposeCurrent(bool announce = true);
    void refreshForces();
    void showEvaluation(const Structura::App::Services::ConnectionService::Evaluation &evaluation);
    [[nodiscard]] QString memorialText() const;

    QVector<Structura::App::ProjectData::NodeConnection> m_connections;
    Callbacks m_callbacks;
    int m_current {-1};
    bool m_building {false};
    std::optional<Structura::App::Services::ConnectionService::Forces> m_forces;
    Structura::App::Services::ConnectionService::Evaluation m_evaluation;

    class ConnectionPreview *m_preview {nullptr};
    QTableWidget *m_list {nullptr};
    QLabel *m_header {nullptr};
    QLabel *m_forcesLabel {nullptr};
    QFormLayout *m_form {nullptr};
    QWidget *m_formHost {nullptr};
    QComboBox *m_grade {nullptr};
    QCheckBox *m_thread {nullptr};
    QMap<QString, QDoubleSpinBox *> m_fields;
    QMap<QString, QComboBox *> m_choices;
    QComboBox *m_steelPreset {nullptr};
    QTableWidget *m_table {nullptr};
    QLabel *m_verdict {nullptr};
    QLabel *m_parts {nullptr};
};

} // namespace Structura::UI
