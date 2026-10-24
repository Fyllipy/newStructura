#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QVector>

#include <functional>
#include <optional>

class QLabel;
class QListWidget;

namespace Structura::UI {

/// Janela flutuante (nao modal) para copiar uma ligacao para outros nos: o usuario clica, no modelo, na ligacao de origem (na viga dela) e depois
/// nos nos de destino; a viga que se apoia e a barra de apoio (ou o banzo e as diagonais, nas trelicas) sao identificadas automaticamente.
/// O modelo permanece clicavel enquanto a janela esta aberta; `run()` so retorna ao fechar.
class ConnectionCopyPanel final : public QDialog
{
    Q_OBJECT

public:
    struct Callbacks {
        /// Numeros (ids externos) das barras e dos nos selecionados no modelo.
        std::function<QVector<int>()> selectedMembers;
        std::function<QVector<int>()> selectedNodes;
        /// Define a ligacao de origem a partir da barra clicada (pergunta qual, se houver mais de uma na barra); devolve a descricao, ou nullopt e `error`.
        std::function<std::optional<QString>(int memberId, QString &error)> chooseSource;
        /// Copia a ligacao de origem para o no (identifica viga e apoio); devolve as ligacoes criadas (vazio e `error` se nao foi possivel).
        std::function<QVector<Structura::App::ProjectData::NodeConnection>(int nodeId, QString &error)> copyToNode;
    };

    explicit ConnectionCopyPanel(Callbacks callbacks, QWidget *parent = nullptr);

    /// Mostra a janela e aguarda o fechamento; devolve as copias confirmadas (vazio se cancelado).
    QVector<Structura::App::ProjectData::NodeConnection> run();

    /// A selecao do modelo mudou: sem origem, uma barra clicada a define; com origem, um no clicado recebe a copia.
    void onSelectionChanged(bool silent = false);
    [[nodiscard]] bool hasSource() const { return m_hasSource; }

private:
    void refresh();

    Callbacks m_callbacks;
    bool m_hasSource {false};
    bool m_busy {false};
    QString m_sourceText;
    QVector<Structura::App::ProjectData::NodeConnection> m_copies;

    QLabel *m_sourceLabel {nullptr};
    QLabel *m_hint {nullptr};
    QLabel *m_message {nullptr};
    QListWidget *m_done {nullptr};
};

} // namespace Structura::UI
