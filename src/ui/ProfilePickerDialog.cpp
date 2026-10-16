#include "ProfilePickerDialog.h"

#include "ProfileBrowser.h"
#include "ProfileLibrary.h"
#include "SectionPreviewWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Structura::UI {

ProfilePickerDialog::ProfilePickerDialog(const QVector<SectionInfo> &sections, const QUuid &current,
                                         bool allowManage, QWidget *parent)
    : QDialog(parent)
    , m_browser(new ProfileBrowser(this))
    , m_detail(new ProfileDetailPanel(this))
    , m_ok(new QPushButton(tr("Selecionar"), this))
{
    setWindowTitle(tr("Escolher perfil"));
    setModal(true);
    resize(820, 560);

    auto *none = new QPushButton(tr("Sem seção"), this);
    none->setToolTip(tr("Remove a seção da barra."));
    auto *manage = new QPushButton(tr("Gerenciar / criar perfis..."), this);
    manage->setVisible(allowManage);
    auto *cancel = new QPushButton(tr("Cancelar"), this);
    m_ok->setDefault(true);
    m_ok->setEnabled(false);

    auto *buttons = new QHBoxLayout();
    buttons->addWidget(manage);
    buttons->addStretch(1);
    buttons->addWidget(none);
    buttons->addWidget(cancel);
    buttons->addWidget(m_ok);

    auto *columns = new QHBoxLayout();
    columns->setSpacing(14);
    columns->addWidget(m_browser, 6);
    columns->addWidget(m_detail, 4);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(columns, 1);
    layout->addLayout(buttons);

    m_browser->setProjectSections(sections);
    if (!current.isNull()) {
        m_browser->setCurrent(current);
    }
    m_detail->setSection(m_browser->currentSection(), m_browser->currentInProject());
    m_ok->setEnabled(!m_browser->currentId().isNull());

    connect(m_browser, &ProfileBrowser::currentChanged, this, [this]() {
        m_detail->setSection(m_browser->currentSection(), m_browser->currentInProject());
        m_ok->setEnabled(!m_browser->currentId().isNull());
    });
    connect(m_browser, &ProfileBrowser::activated, this, [this](const QUuid &id) {
        m_selected = id;
        accept();
    });
    connect(m_ok, &QPushButton::clicked, this, [this]() {
        m_selected = m_browser->currentId();
        accept();
    });
    connect(none, &QPushButton::clicked, this, [this]() {
        m_selected = QUuid();
        accept();
    });
    connect(manage, &QPushButton::clicked, this, [this]() {
        m_manage = true;
        reject();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

// ------------------------------------------------------------------ botao

SectionPickerButton::SectionPickerButton(QWidget *parent)
    : QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setIconSize(QSize(20, 20));
    setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; padding: 4px 8px; background: #ffffff; border: 1px solid #cbd4e2;"
        "  border-radius: 4px; color: #1e232b; }"
        "QPushButton:hover { border-color: #0d7fb9; }"));
    connect(this, &QPushButton::clicked, this, &SectionPickerButton::openPicker);
    updateText();
}

void SectionPickerButton::setSections(const QVector<SectionInfo> &sections)
{
    m_sections = sections;
    updateText();
}

void SectionPickerButton::setCurrent(const QUuid &id)
{
    m_current = id;
    m_mixed = false;
    updateText();
}

void SectionPickerButton::setMixed()
{
    m_current = QUuid();
    m_mixed = true;
    updateText();
}

void SectionPickerButton::updateText()
{
    QString name;
    QIcon icon;
    if (m_mixed) {
        name = tr("Seções variadas");
    } else if (m_current.isNull()) {
        name = tr("Sem seção");
    } else {
        QString family;
        Structura::Visualization::SectionShapes::Parameters parameters;
        bool found = false;
        for (const auto &section : m_sections) {
            if (section.uuid == m_current) {
                name = section.name;
                family = section.family;
                parameters = section.geometry;
                found = true;
                break;
            }
        }
        if (!found) {
            if (const auto *entry = ProfileLibrary::find(m_current)) {
                name = entry->name;
                family = entry->family;
                parameters = entry->parameters;
            } else {
                name = tr("Seção desconhecida");
            }
        }
        if (!family.isEmpty()) {
            icon = QIcon(SectionPreviewWidget::thumbnail(family, parameters, QSize(20, 20)));
        }
    }
    setIcon(icon);
    setText(name + QStringLiteral("  ▾"));
    setToolTip(tr("Clique para escolher o perfil (filtros por categoria, tipo e busca)"));
}

void SectionPickerButton::openPicker()
{
    ProfilePickerDialog dialog(m_sections, m_current, m_allowManage, window());
    const int result = dialog.exec();
    if (dialog.manageRequested()) {
        emit manageRequested();
        return;
    }
    if (result != QDialog::Accepted) {
        return;
    }
    m_current = dialog.selectedId();
    m_mixed = false;
    updateText();
    emit picked(m_current);
}

} // namespace Structura::UI
