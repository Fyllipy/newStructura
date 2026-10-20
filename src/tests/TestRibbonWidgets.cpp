#include <QtTest/QtTest>

#include "ui/RibbonWidgets.h"
#include "ui/SvgIcons.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QVBoxLayout>

using namespace Structura::UI;

namespace {

QAction *makeAction(QObject *parent, const QString &text, const QString &icon, const QString &tip = {})
{
    auto *a = new QAction(text, parent);
    a->setIcon(svgIcon(icon, QColor(QStringLiteral("#1e232b")), 64));
    if (!tip.isEmpty()) {
        a->setToolTip(tip);
    }
    return a;
}

RibbonItem makeItem(QAction *action, bool large)
{
    RibbonItem item;
    item.action = action;
    item.large = large;
    return item;
}

} // namespace

/// Componentes da faixa de opcoes: tamanhos, estados, botao dividido, dica estendida e reducao por largura
class TestRibbonWidgets : public QObject
{
    Q_OBJECT

private slots:
    void buttonSizes()
    {
        QWidget host;
        auto *bar = makeAction(&host, QStringLiteral("Inserir barra"), QStringLiteral("bar-insert.svg"));
        const RibbonItem item = makeItem(bar, true);
        RibbonButton large(item, true, &host);
        RibbonButton small(item, false, &host);
        QCOMPARE(large.height(), RibbonStyle::kLargeHeight);
        QCOMPARE(small.height(), RibbonStyle::kSmallHeight);
        QVERIFY(large.width() >= 56);
        // botao dividido: mesma altura; o rotulo longo vira duas linhas
        RibbonItem split = item;
        split.label = QStringLiteral("Gerenciador de perfis");
        split.variants = {{bar, QStringLiteral("Inserir barra")}};
        RibbonButton splitButton(split, true, &host);
        QCOMPARE(splitButton.height(), RibbonStyle::kLargeHeight);
    }

    void groupLevelsChangeLayout()
    {
        QWidget host;
        QList<RibbonItem> items = {makeItem(makeAction(&host, QStringLiteral("Mover"), QStringLiteral("move.svg")), true),
                                   makeItem(makeAction(&host, QStringLiteral("Copiar"), QStringLiteral("copy.svg")), true),
                                   makeItem(makeAction(&host, QStringLiteral("Cópia em série"), QStringLiteral("serial.svg")), false),
                                   makeItem(makeAction(&host, QStringLiteral("Rotacionar"), QStringLiteral("rotate.svg")), false),
                                   makeItem(makeAction(&host, QStringLiteral("Espelhar"), QStringLiteral("mirror.svg")), false),
                                   makeItem(makeAction(&host, QStringLiteral("Excluir"), QStringLiteral("trash.svg")), true)};
        RibbonGroup group(QStringLiteral("Edição"), items, &host);
        host.show();
        group.show();
        auto buttons = [&group]() { return group.findChildren<RibbonButton *>(); };
        QCOMPARE(buttons().size(), 6);
        int largeCount = 0;
        for (auto *button : buttons()) {
            largeCount += button->height() == RibbonStyle::kLargeHeight ? 1 : 0;
        }
        QCOMPARE(largeCount, 3);   // nivel 0: Mover, Copiar e Excluir grandes
        const int wide = group.sizeHint().width();
        group.setLevel(1);
        QTest::qWait(10);
        for (auto *button : buttons()) {
            if (button->isVisibleTo(&group)) {
                QCOMPARE(button->height(), RibbonStyle::kSmallHeight);   // nivel 1: todos pequenos
            }
        }
        QVERIFY(group.sizeHint().width() < wide);
        group.setLevel(2);
        QTest::qWait(10);
        int visible = 0;
        for (auto *button : buttons()) {
            visible += button->isVisibleTo(&group) ? 1 : 0;
        }
        QCOMPARE(visible, 1);   // nivel 2: so o botao do grupo; os comandos ficam no menu
    }

    void checkedAndDisabledStates()
    {
        QWidget host;
        auto *act = makeAction(&host, QStringLiteral("Inserir barra"), QStringLiteral("bar-insert.svg"));
        act->setCheckable(true);
        RibbonButton button(makeItem(act, true), true, &host);
        QVERIFY(!button.isChecked());
        act->setChecked(true);
        QVERIFY(button.isChecked());   // "comando ativo" acompanha a acao marcada
        act->setEnabled(false);
        QVERIFY(!button.isEnabled());
    }

    /// Imagem de conferencia dos componentes (STRUCTURA_SHOT_DIR)
    void renderSheet()
    {
        QWidget sheet;
        sheet.setStyleSheet(QStringLiteral("background: #f2f5fa;"));
        auto *root = new QVBoxLayout(&sheet);
        root->setContentsMargins(16, 16, 16, 16);
        auto *row = new QHBoxLayout();
        row->setSpacing(10);
        auto mk = [&](const QString &text, bool enabled, bool checked) {
            auto *act = makeAction(&sheet, text, QStringLiteral("bar-insert.svg"));
            act->setCheckable(true);
            act->setChecked(checked);
            act->setEnabled(enabled);
            auto *btn = new RibbonButton(makeItem(act, true), true, &sheet);
            row->addWidget(btn);
            return btn;
        };
        mk(QStringLiteral("Normal"), true, false);
        auto *hover = mk(QStringLiteral("Foco / mouse"), true, false);
        mk(QStringLiteral("Comando ativo"), true, true);
        mk(QStringLiteral("Desabilitado"), false, false);
        auto *copy = makeAction(&sheet, QStringLiteral("Copiar"), QStringLiteral("copy.svg"), QStringLiteral("Copia os elementos selecionados."));
        copy->setShortcut(QKeySequence(QStringLiteral("Ctrl+C")));
        auto *serial = makeAction(&sheet, QStringLiteral("Cópia em série"), QStringLiteral("serial.svg"));
        serial->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
        RibbonItem split = makeItem(copy, true);
        split.variants = {{copy, QStringLiteral("Copiar")}, {serial, QStringLiteral("Cópia em série…")}, {nullptr, QString(), true}, {copy, QStringLiteral("Copiar espelhado…")}};
        row->addWidget(new RibbonButton(split, true, &sheet));
        auto *smalls = new QVBoxLayout();
        smalls->setSpacing(0);
        smalls->addWidget(new RibbonButton(makeItem(serial, false), false, &sheet));
        RibbonItem del = makeItem(makeAction(&sheet, QStringLiteral("Excluir"), QStringLiteral("trash.svg")), false);
        del.danger = true;
        smalls->addWidget(new RibbonButton(del, false, &sheet));
        row->addLayout(smalls);
        row->addStretch(1);
        root->addLayout(row);
        sheet.resize(760, 140);
        sheet.show();
        QTest::qWait(50);
        const QByteArray shots = qgetenv("STRUCTURA_SHOT_DIR");
        if (!shots.isEmpty()) {
            sheet.grab().save(QString::fromLocal8Bit(shots) + QStringLiteral("/ribbon_components.png"));
            RibbonToolTip::showFor(hover, {QStringLiteral("Inserir barra"), QStringLiteral("B"),
                                           QStringLiteral("Desenha uma barra entre dois pontos. Clique no início e no fim; nós são criados automaticamente onde não existirem.")});
            QTest::qWait(50);
            for (QWidget *top : QApplication::topLevelWidgets()) {
                if (top->objectName() == QLatin1String("RibbonToolTip")) {
                    top->grab().save(QString::fromLocal8Bit(shots) + QStringLiteral("/ribbon_tooltip.png"));
                }
            }
            RibbonToolTip::hideTip();
        }
    }
};

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestRibbonWidgets test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestRibbonWidgets.moc"
