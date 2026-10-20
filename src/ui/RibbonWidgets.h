#pragma once

#include <QFrame>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QScrollArea>
#include <QString>
#include <QToolButton>
#include <QVector>

class QAction;
class QHBoxLayout;
class QVBoxLayout;
class QWheelEvent;
class QResizeEvent;
class QTimer;
class QLabel;
class QMenu;

namespace Structura::UI {

/// Paleta da faixa de opcoes (as cores do programa: barra azul #0d7fb9, pagina #f2f5fa, texto #1e232b).
namespace RibbonStyle {
inline constexpr const char *kText = "#1e232b";
inline constexpr const char *kMuted = "#6b7686";
inline constexpr const char *kAccent = "#0d7fb9";
inline constexpr const char *kAccentDark = "#0b6da1";
inline constexpr const char *kPage = "#f2f5fa";
inline constexpr const char *kSeparator = "#d6dde8";
inline constexpr const char *kDanger = "#c0392b";
inline constexpr int kLargeIcon = 32;
inline constexpr int kSmallIcon = 16;
inline constexpr int kLargeHeight = 72;
inline constexpr int kSmallHeight = 24;
} // namespace RibbonStyle

/// Entrada de um menu de variacoes de um botao dividido (texto e atalho exibido; separator = linha divisoria).
struct RibbonVariant {
    QAction *action {nullptr};
    QString text;
    bool separator {false};
};

/// Comando da faixa de opcoes: acao, rotulo, tamanho (grande ou pequeno) e variacoes (botao dividido).
struct RibbonItem {
    QAction *action {nullptr};
    QString label;          ///< vazio = texto da acao
    QString description;    ///< texto da dica estendida; vazio = dica da acao
    QIcon icon;             ///< opcional: substitui o icone da acao
    bool large {false};
    bool danger {false};    ///< acao destrutiva (texto e icone vermelhos)
    QList<RibbonVariant> variants;   ///< nao vazio: botao dividido (icone executa a acao padrao; rotulo + seta abre as variacoes)
};

/// Dica estendida: titulo, atalho, descricao e a linha de ajuda (F1). Janela unica reutilizada.
class RibbonToolTip final : public QFrame
{
    Q_OBJECT

public:
    struct Content {
        QString title;
        QString shortcut;
        QString description;
    };
    static void showFor(QWidget *anchor, const Content &content);
    static void hideTip();
    /// Ultimo comando exibido na dica (para a ajuda contextual por F1).
    static const Content &last();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    RibbonToolTip();
    static RibbonToolTip &instance();
    void fill(const Content &content);

    QLabel *m_title {nullptr};
    QLabel *m_badge {nullptr};
    QLabel *m_description {nullptr};
    QFrame *m_line {nullptr};
    QWidget *m_help {nullptr};
    Content m_content;
    QTimer *m_timeout {nullptr};
    friend class RibbonButton;
};

/// Botao da faixa: grande (icone de 32 px acima do rotulo, ate duas linhas) ou pequeno (icone de 16 px, altura de 24 px), com os estados normal,
/// foco/mouse, pressionado, comando ativo (acao marcada) e desabilitado. Com variacoes vira botao dividido; com `setDropdown`, botao de grupo recolhido.
class RibbonButton final : public QToolButton
{
    Q_OBJECT

public:
    RibbonButton(const RibbonItem &item, bool large, QWidget *parent = nullptr);
    /// Botao de grupo recolhido: abre `menu` ao clicar em qualquer ponto.
    RibbonButton(const QString &label, const QIcon &icon, QMenu *menu, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    enum Zone { None = 0, Main = 1, Arrow = 2 };
    [[nodiscard]] QStringList labelLines() const;
    [[nodiscard]] int zoneAt(const QPoint &p) const;
    [[nodiscard]] QRect mainZone() const;
    [[nodiscard]] QRect arrowZone() const;
    void showMenu();
    void buildMenu(const QList<RibbonVariant> &variants);
    [[nodiscard]] QPixmap iconPixmap(int size, QIcon::Mode mode) const;
    [[nodiscard]] RibbonToolTip::Content tipContent() const;

    RibbonItem m_item;
    QString m_label;
    QIcon m_icon;
    bool m_large {true};
    bool m_split {false};
    bool m_dropdownOnly {false};
    QMenu *m_menu {nullptr};
    int m_hover {None};
    int m_pressed {None};
    bool m_menuOpen {false};
};

/// Grupo da faixa: itens em colunas e o nome embaixo. Tres niveis de reducao por largura da janela: 0 = completo (botoes grandes e pequenos, 3 por
/// coluna), 1 = medio (todos pequenos, 3 por coluna), 2 = recolhido (um botao grande com o nome do grupo que abre os comandos).
class RibbonGroup final : public QWidget
{
    Q_OBJECT

public:
    RibbonGroup(const QString &title, QList<RibbonItem> items, QWidget *parent = nullptr);
    void setLevel(int level);
    [[nodiscard]] int level() const { return m_level; }
    [[nodiscard]] QString title() const { return m_title; }

private:
    void rebuild();
    [[nodiscard]] QWidget *buildColumns(int level, QWidget *parent, QMenu *closeOn) const;

    QString m_title;
    QList<RibbonItem> m_items;
    int m_level {0};
    QWidget *m_content {nullptr};
    QLabel *m_caption {nullptr};
    QVBoxLayout *m_layout {nullptr};
};

/// Pagina da faixa: grupos separados por linhas finas, rolagem horizontal quando nao cabe.
class RibbonPage final : public QScrollArea
{
    Q_OBJECT

public:
    explicit RibbonPage(QWidget *parent = nullptr);
    RibbonGroup *addGroup(const QString &title, const QList<RibbonItem> &items);
    void setLevel(int level);
    static constexpr int kHeight = 118;

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    QWidget *m_inner {nullptr};
    QHBoxLayout *m_layout {nullptr};
    QVector<RibbonGroup *> m_groups;
};

} // namespace Structura::UI
