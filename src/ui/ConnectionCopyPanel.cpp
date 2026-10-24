#include "ConnectionCopyPanel.h"

#include <QEventLoop>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::ProjectData::NodeConnection;

ConnectionCopyPanel::ConnectionCopyPanel(Callbacks callbacks, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint)
    , m_callbacks(std::move(callbacks))
{
    setWindowTitle(tr("Copiar ligação para outros nós"));
    setModal(false);
    setMinimumWidth(430);
    auto *root = new QVBoxLayout(this);

    m_sourceLabel = new QLabel;
    m_sourceLabel->setWordWrap(true);
    root->addWidget(m_sourceLabel);
    auto *change = new QPushButton(tr("Trocar a ligação de origem"));
    root->addWidget(change);
    m_hint = new QLabel;
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color:#5a6472;"));
    root->addWidget(m_hint);
    m_message = new QLabel;
    m_message->setWordWrap(true);
    root->addWidget(m_message);

    root->addWidget(new QLabel(tr("Cópias feitas:")));
    m_done = new QListWidget;
    m_done->setMinimumHeight(110);
    m_done->setMaximumHeight(170);
    root->addWidget(m_done);

    auto *bottom = new QHBoxLayout;
    auto *finish = new QPushButton(tr("Concluir"));
    auto *cancel = new QPushButton(tr("Cancelar"));
    bottom->addStretch(1);
    bottom->addWidget(finish);
    bottom->addWidget(cancel);
    root->addLayout(bottom);

    connect(change, &QPushButton::clicked, this, [this]() {
        m_hasSource = false;
        m_message->clear();
        refresh();
    });
    connect(finish, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    refresh();
}

QVector<NodeConnection> ConnectionCopyPanel::run()
{
    QEventLoop loop;
    connect(this, &QDialog::finished, &loop, &QEventLoop::quit);
    show();
    raise();
    onSelectionChanged(true);   // uma barra ja selecionada ao abrir pode ser a origem
    loop.exec();
    return result() == QDialog::Accepted ? m_copies : QVector<NodeConnection> {};
}

void ConnectionCopyPanel::onSelectionChanged(bool silent)
{
    if (m_busy) {
        return;
    }
    m_busy = true;   // as perguntas feitas durante a copia mexem na selecao do modelo
    QString error;
    if (!m_hasSource) {
        const QVector<int> bars = m_callbacks.selectedMembers ? m_callbacks.selectedMembers() : QVector<int> {};
        if (bars.size() == 1 && m_callbacks.chooseSource) {
            if (const auto text = m_callbacks.chooseSource(bars.first(), error)) {
                m_hasSource = true;
                m_sourceText = *text;
                m_message->clear();
                refresh();
            } else if (!silent) {
                m_message->setStyleSheet(QStringLiteral("color:#c0392b;"));
                m_message->setText(error);
            }
        }
    } else {
        const QVector<int> nodes = m_callbacks.selectedNodes ? m_callbacks.selectedNodes() : QVector<int> {};
        if (nodes.size() == 1 && m_callbacks.copyToNode) {
            const QVector<NodeConnection> made = m_callbacks.copyToNode(nodes.first(), error);
            if (made.isEmpty()) {
                m_message->setStyleSheet(QStringLiteral("color:#c0392b;"));
                m_message->setText(error.isEmpty() ? tr("Não foi possível copiar a ligação para o nó %1.").arg(nodes.first()) : error);
            } else {
                for (const auto &c : made) {
                    m_copies.append(c);
                    m_done->addItem(tr("Nó %1 — viga %2 (%3)").arg(c.nodeId).arg(c.memberId).arg(c.atEnd ? tr("fim") : tr("início")));
                }
                m_message->setStyleSheet(QStringLiteral("color:#1e8449;"));
                m_message->setText(tr("%1 ligação(ões) copiada(s) para o nó %2. Clique no próximo nó ou em Concluir.").arg(made.size()).arg(nodes.first()));
            }
        }
    }
    m_busy = false;
}

void ConnectionCopyPanel::refresh()
{
    m_sourceLabel->setText(m_hasSource ? tr("<b>Origem:</b> %1").arg(m_sourceText) : tr("<b>Origem:</b> (nenhuma)"));
    m_hint->setText(!m_hasSource
                        ? tr("Passo 1: clique, no modelo, na ligação que deseja copiar (na viga dela). Se a viga tiver ligação nas duas extremidades, a janela pergunta qual.")
                        : tr("Passo 2: clique no nó de destino. A viga que se apoia e a barra de apoio (ou o banzo e as diagonais, nas treliças) são identificados "
                             "automaticamente pela geometria e pelos perfis; se houver mais de uma possibilidade, a janela pergunta quais copiar."));
}

} // namespace Structura::UI
