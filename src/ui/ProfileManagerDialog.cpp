#include "ProfileManagerDialog.h"

#include "ProfileBrowser.h"
#include "ProfileEditorWidget.h"
#include "ProfilePropertiesDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Structura::UI {

ProfileManagerDialog::ProfileManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_pages(new QStackedWidget(this))
    , m_browser(new ProfileBrowser(this))
    , m_detail(new ProfileDetailPanel(this))
    , m_editor(new ProfileEditorWidget(this))
    , m_header(new QLabel(this))
    , m_newButton(new QPushButton(tr("Novo perfil..."), this))
    , m_variationButton(new QPushButton(tr("Criar variação deste perfil..."), this))
    , m_propertiesButton(new QPushButton(tr("Propriedades completas do perfil..."), this))
    , m_addButton(new QPushButton(tr("Adicionar ao projeto"), this))
    , m_assignButton(new QPushButton(this))
{
    setWindowTitle(tr("Gerenciador de perfis"));
    resize(960, 640);

    m_header->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 16px; color: #0d7fb9;"));

    // ---- pagina 1: lista + detalhes
    auto *closeButton = new QPushButton(tr("Fechar"), this);
    m_newButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0d7fb9; color: white; border: none; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background: #0b6a9b; }"));

    auto *side = new QVBoxLayout();
    side->setSpacing(6);
    side->addWidget(m_detail, 1);
    side->addWidget(m_propertiesButton);
    side->addWidget(m_addButton);
    side->addWidget(m_assignButton);
    side->addWidget(m_variationButton);
    side->addWidget(m_newButton);
    side->addWidget(closeButton);

    auto *browsePage = new QWidget(this);
    auto *browseLayout = new QHBoxLayout(browsePage);
    browseLayout->setContentsMargins(0, 0, 0, 0);
    browseLayout->setSpacing(14);
    browseLayout->addWidget(m_browser, 6);
    browseLayout->addLayout(side, 4);

    // ---- pagina 2: criacao
    auto *back = new QPushButton(tr("← Voltar à lista"), this);
    back->setFlat(true);
    auto *editorPage = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(editorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    auto *editorHeader = new QHBoxLayout();
    editorHeader->addWidget(new QLabel(tr("<b style='font-size:15px;color:#0d7fb9;'>Novo perfil</b>"), editorPage));
    editorHeader->addStretch(1);
    editorHeader->addWidget(back);
    editorLayout->addLayout(editorHeader);
    editorLayout->addWidget(m_editor, 1);

    m_pages->addWidget(browsePage);
    m_pages->addWidget(editorPage);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_header);
    layout->addWidget(m_pages, 1);

    connect(m_browser, &ProfileBrowser::currentChanged, this, [this]() {
        m_detail->setSection(m_browser->currentSection(), m_browser->currentInProject());
        updateButtons();
    });
    connect(m_browser, &ProfileBrowser::activated, this, [this](const QUuid &id) {
        if (m_barCount > 0) {
            emit assignRequested(id);
        }
    });
    connect(m_addButton, &QPushButton::clicked, this, [this]() { emit addToProjectRequested(m_browser->currentId()); });
    connect(m_assignButton, &QPushButton::clicked, this, [this]() { emit assignRequested(m_browser->currentId()); });
    connect(m_newButton, &QPushButton::clicked, this, &ProfileManagerDialog::showCreatePage);
    connect(m_propertiesButton, &QPushButton::clicked, this, [this]() {
        if (const auto section = m_browser->currentSection()) {
            ProfilePropertiesDialog dialog(*section, this);
            dialog.exec();
        }
    });
    connect(m_variationButton, &QPushButton::clicked, this, [this]() {
        if (const auto section = m_browser->currentSection()) {
            showCreatePage();
            m_editor->startFrom(*section);
        }
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(back, &QPushButton::clicked, this, &ProfileManagerDialog::showBrowsePage);
    connect(m_editor, &ProfileEditorWidget::cancelled, this, &ProfileManagerDialog::showBrowsePage);
    connect(m_editor, &ProfileEditorWidget::createRequested, this,
            [this](const SectionInfo &info, bool assign, bool save) { emit createProfileRequested(info, assign, save); });

    setSelectedBarCount(0);
    showBrowsePage();
}

void ProfileManagerDialog::setProjectSections(const QVector<SectionInfo> &sections)
{
    m_project = sections;
    m_browser->setProjectSections(sections);
    QStringList names;
    for (const auto &section : sections) {
        names << section.name;
    }
    m_editor->setExistingNames(names);
    m_header->setText(tr("Biblioteca de perfis  ·  %1 no projeto").arg(sections.size()));
    updateButtons();
}

void ProfileManagerDialog::setSelectedBarCount(int count)
{
    m_barCount = count;
    m_editor->setSelectedBarCount(count);
    updateButtons();
}

void ProfileManagerDialog::updateButtons()
{
    const bool selected = !m_browser->currentId().isNull();
    m_addButton->setEnabled(selected && !m_browser->currentInProject());
    m_assignButton->setEnabled(selected && m_barCount > 0);
    m_assignButton->setText(m_barCount > 0 ? tr("Atribuir a %1 barra(s) selecionada(s)").arg(m_barCount)
                                           : tr("Atribuir às barras selecionadas (nenhuma)"));
    m_variationButton->setEnabled(selected);
    m_propertiesButton->setEnabled(selected);
}

void ProfileManagerDialog::showCreatePage()
{
    m_editor->reset();
    m_pages->setCurrentIndex(1);
    setWindowTitle(tr("Gerenciador de perfis - novo perfil"));
    show();
    raise();
    activateWindow();
}

void ProfileManagerDialog::showBrowsePage()
{
    m_pages->setCurrentIndex(0);
    setWindowTitle(tr("Gerenciador de perfis"));
    m_browser->focusSearch();
}

void ProfileManagerDialog::profileCreated(const QUuid &id)
{
    showBrowsePage();
    m_browser->setCurrent(id);
}

} // namespace Structura::UI
