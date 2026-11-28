#include "LegalNoticesDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <iterator>

namespace Structura::UI {

namespace {

struct Document {
    const char *title;
    const char *resource;
};

constexpr Document kDocuments[] = {
    {QT_TR_NOOP("Avisos de terceiros"), ":/legal/Avisos.txt"},
    {QT_TR_NOOP("Qt: GNU LGPL 3.0"), ":/legal/LGPL-3.0.txt"},
    {QT_TR_NOOP("Qt: GNU GPL 3.0 (incorporada pela LGPL 3.0)"), ":/legal/GPL-3.0.txt"},
    {QT_TR_NOOP("VTK: licença BSD"), ":/legal/VTK-License.txt"},
};

QString readResource(const char *path)
{
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QObject::tr("Não foi possível ler o texto da licença (%1).").arg(QString::fromLatin1(path));
    }
    return QString::fromUtf8(file.readAll()).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
}

} // namespace

LegalNoticesDialog::LegalNoticesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Dizeres legais"));
    resize(980, 640);
    auto *root = new QVBoxLayout(this);
    auto *split = new QSplitter(Qt::Horizontal, this);
    auto *list = new QListWidget(split);
    list->setMinimumWidth(220);
    auto *text = new QPlainTextEdit(split);
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    split->addWidget(list);
    split->addWidget(text);
    split->setStretchFactor(1, 1);
    split->setSizes({260, 720});
    for (const Document &doc : kDocuments) {
        list->addItem(tr(doc.title));
    }
    connect(list, &QListWidget::currentRowChanged, this, [text](int row) {
        if (row >= 0 && row < static_cast<int>(std::size(kDocuments))) {
            text->setPlainText(readResource(kDocuments[row].resource));
            text->moveCursor(QTextCursor::Start);
        }
    });
    list->setCurrentRow(0);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Fechar"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(split, 1);
    root->addWidget(buttons);
}

} // namespace Structura::UI
