#include "TitleBlockDialog.h"

#include "DrawingDialog.h"
#include "app/services/SheetBuilder.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::ProjectData::TitleBlockSettings;
using Structura::App::Services::SheetBuilder;

TitleBlockDialog::TitleBlockDialog(const TitleBlockSettings &settings, const QVector<ViewTitle> &views, QWidget *parent)
    : QDialog(parent)
    , m_views(views)
{
    setWindowTitle(tr("Editar carimbo"));
    resize(1100, 700);
    auto *root = new QHBoxLayout(this);

    auto *tabs = new QTabWidget;
    auto *fields = new QWidget;
    auto *form = new QFormLayout(fields);
    m_company = new QLineEdit(settings.company);
    m_client = new QLineEdit(settings.client);
    m_site = new QLineEdit(settings.site);
    m_project = new QLineEdit(settings.project);
    m_author = new QLineEdit(settings.author);
    m_registration = new QLineEdit(settings.registration);
    m_drafter = new QLineEdit(settings.drafter);
    m_checker = new QLineEdit(settings.checker);
    m_date = new QLineEdit(settings.date.isEmpty() ? QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy")) : settings.date);
    m_revision = new QLineEdit(settings.revision);
    m_prefix = new QLineEdit(settings.drawingPrefix);
    m_prefix->setPlaceholderText(tr("ex.: EST (gera EST-01, EST-02…)"));
    form->addRow(tr("Empresa"), m_company);
    form->addRow(tr("Cliente"), m_client);
    form->addRow(tr("Obra / local"), m_site);
    form->addRow(tr("Projeto"), m_project);
    form->addRow(tr("Responsável técnico"), m_author);
    form->addRow(tr("CREA / CAU"), m_registration);
    form->addRow(tr("Desenho"), m_drafter);
    form->addRow(tr("Verificação"), m_checker);
    form->addRow(tr("Data"), m_date);
    form->addRow(tr("Revisão atual"), m_revision);
    form->addRow(tr("Prefixo do nº do desenho"), m_prefix);
    form->addRow(new QLabel(tr("Notas gerais (rodapé de todas as pranchas, uma por linha):")));
    m_notes = new QPlainTextEdit(settings.notes);
    m_notes->setMaximumHeight(110);
    form->addRow(m_notes);
    tabs->addTab(fields, tr("Campos"));

    auto *revisionPage = new QWidget;
    auto *revisionLayout = new QVBoxLayout(revisionPage);
    revisionLayout->addWidget(new QLabel(tr("Tabela de revisões (mostra as 6 últimas acima do carimbo):")));
    m_revisions = new QTableWidget(0, 4);
    m_revisions->setHorizontalHeaderLabels({tr("Rev."), tr("Data"), tr("Descrição"), tr("Autor")});
    m_revisions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_revisions->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_revisions->verticalHeader()->hide();
    for (const auto &r : settings.revisions) {
        const int row = m_revisions->rowCount();
        m_revisions->insertRow(row);
        const QStringList cells {r.rev, r.date, r.description, r.author};
        for (int c = 0; c < 4; ++c) {
            m_revisions->setItem(row, c, new QTableWidgetItem(cells[c]));
        }
    }
    revisionLayout->addWidget(m_revisions, 1);
    auto *revButtons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Adicionar revisão"));
    auto *remove = new QPushButton(tr("Remover selecionada"));
    revButtons->addWidget(add);
    revButtons->addWidget(remove);
    revButtons->addStretch(1);
    revisionLayout->addLayout(revButtons);
    tabs->addTab(revisionPage, tr("Revisões"));

    auto *titlesPage = new QWidget;
    auto *titlesLayout = new QVBoxLayout(titlesPage);
    titlesLayout->addWidget(new QLabel(tr("Títulos das pranchas (deixe em branco para o título padrão):")));
    m_titles = new QTableWidget(views.size(), 2);
    m_titles->setHorizontalHeaderLabels({tr("Padrão"), tr("Título personalizado")});
    m_titles->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_titles->verticalHeader()->hide();
    for (int i = 0; i < views.size(); ++i) {
        auto *def = new QTableWidgetItem(views[i].defaultTitle);
        def->setFlags(def->flags() & ~Qt::ItemIsEditable);
        m_titles->setItem(i, 0, def);
        m_titles->setItem(i, 1, new QTableWidgetItem(settings.sheetTitles.value(views[i].id)));
    }
    titlesLayout->addWidget(m_titles, 1);
    tabs->addTab(titlesPage, tr("Títulos"));
    root->addWidget(tabs, 1);

    auto *right = new QVBoxLayout;
    right->addWidget(new QLabel(tr("<b>Pré-visualização do carimbo</b>")));
    m_preview = new SheetPreviewWidget;
    m_preview->setMinimumSize(430, 260);
    right->addWidget(m_preview, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Aplicar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    right->addWidget(buttons);
    root->addLayout(right, 1);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, [this]() {
        const int row = m_revisions->rowCount();
        m_revisions->insertRow(row);
        m_revisions->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        m_revisions->setItem(row, 1, new QTableWidgetItem(QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"))));
        m_revisions->setItem(row, 2, new QTableWidgetItem());
        m_revisions->setItem(row, 3, new QTableWidgetItem(m_author->text()));
        m_revisions->editItem(m_revisions->item(row, 2));
        refreshPreview();
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int row = m_revisions->currentRow();
        if (row >= 0) {
            m_revisions->removeRow(row);
            refreshPreview();
        }
    });
    for (auto *edit : {m_company, m_client, m_site, m_project, m_author, m_registration, m_drafter, m_checker, m_date, m_revision, m_prefix}) {
        connect(edit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    }
    connect(m_revisions, &QTableWidget::itemChanged, this, [this]() { refreshPreview(); });
    refreshPreview();
}

TitleBlockSettings TitleBlockDialog::settings() const
{
    TitleBlockSettings s;
    s.company = m_company->text().trimmed();
    s.client = m_client->text().trimmed();
    s.site = m_site->text().trimmed();
    s.project = m_project->text().trimmed();
    s.author = m_author->text().trimmed();
    s.registration = m_registration->text().trimmed();
    s.drafter = m_drafter->text().trimmed();
    s.checker = m_checker->text().trimmed();
    s.date = m_date->text().trimmed();
    s.revision = m_revision->text().trimmed().isEmpty() ? QStringLiteral("0") : m_revision->text().trimmed();
    s.drawingPrefix = m_prefix->text().trimmed();
    s.notes = m_notes->toPlainText().trimmed();
    for (int row = 0; row < m_revisions->rowCount(); ++row) {
        auto text = [&](int c) { return m_revisions->item(row, c) ? m_revisions->item(row, c)->text().trimmed() : QString(); };
        if (!text(0).isEmpty() || !text(2).isEmpty()) {
            s.revisions.append({text(0), text(1), text(2), text(3)});
        }
    }
    for (int i = 0; i < m_views.size(); ++i) {
        const QString custom = m_titles->item(i, 1) ? m_titles->item(i, 1)->text().trimmed() : QString();
        if (!custom.isEmpty()) {
            s.sheetTitles.insert(m_views[i].id, custom);
        }
    }
    return s;
}

void TitleBlockDialog::refreshPreview()
{
    m_previewSheet.clear();
    m_previewSheet.push_back(SheetBuilder::previewTitleBlock(settings()));
    m_preview->setSheet(&m_previewSheet.front());
}

} // namespace Structura::UI
