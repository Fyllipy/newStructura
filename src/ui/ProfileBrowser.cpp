#include "ProfileBrowser.h"

#include "SectionPreviewWidget.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Category = ProfileLibrary::Category;
using Structura::Visualization::SectionShapes;

namespace {

constexpr int kItemRole = Qt::UserRole;      // indice em m_items (-1 = grupo)
constexpr int kProjectRole = Qt::UserRole + 1;

QString keyOf(const QString &family, const QString &name)
{
    return family + QLatin1Char('|') + name;
}

QIcon familyIcon(const QString &family)
{
    static QHash<QString, QIcon> cache;
    auto it = cache.find(family);
    if (it == cache.end()) {
        SectionShapes::Parameters defaults;
        if (const auto *info = SectionShapes::findFamily(family)) {
            for (const auto &parameter : info->parameters) {
                defaults.insert(parameter.key, parameter.defaultValue);
            }
        }
        it = cache.insert(family, QIcon(SectionPreviewWidget::thumbnail(family, defaults, QSize(22, 22))));
    }
    return it.value();
}

} // namespace

ProfileBrowser::ProfileBrowser(QWidget *parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_categoryGroup(new QButtonGroup(this))
    , m_typeCombo(new QComboBox(this))
    , m_onlyProject(new QCheckBox(tr("Somente perfis do projeto"), this))
    , m_tree(new QTreeWidget(this))
    , m_count(new QLabel(this))
{
    m_search->setPlaceholderText(tr("Buscar perfil (ex.: W 310, HEA, 20x40)..."));
    m_search->setClearButtonEnabled(true);

    setStyleSheet(QStringLiteral(
        "QPushButton[chip=\"true\"] { background: #ffffff; border: 1px solid #cbd4e2; border-radius: 12px;"
        "  padding: 3px 12px; color: #1e232b; }"
        "QPushButton[chip=\"true\"]:hover { border-color: #0d7fb9; }"
        "QPushButton[chip=\"true\"]:checked { background: #0d7fb9; border-color: #0d7fb9; color: #ffffff; }"));

    auto *chips = new QHBoxLayout();
    chips->setSpacing(6);
    auto addChip = [&](int id, const QString &text) {
        auto *button = new QPushButton(text, this);
        button->setCheckable(true);
        button->setProperty("chip", true);
        button->setCursor(Qt::PointingHandCursor);
        m_categoryGroup->addButton(button, id);
        chips->addWidget(button);
        return button;
    };
    addChip(0, tr("Todos"))->setChecked(true);
    for (Category category : ProfileLibrary::categories()) {
        addChip(static_cast<int>(category) + 1, ProfileLibrary::categoryName(category));
    }
    chips->addStretch(1);
    m_categoryGroup->setExclusive(true);

    auto *typeRow = new QHBoxLayout();
    typeRow->addWidget(new QLabel(tr("Tipo:"), this));
    typeRow->addWidget(m_typeCombo, 1);
    typeRow->addWidget(m_onlyProject);

    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setIconSize(QSize(22, 22));
    m_tree->setRootIsDecorated(true);
    m_tree->setColumnWidth(0, 230);

    m_count->setStyleSheet(QStringLiteral("color: #6e7781; font-size: 11px;"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(m_search);
    layout->addLayout(chips);
    layout->addLayout(typeRow);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_count);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { rebuildTree(); });
    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this]() { rebuildTree(); });
    connect(m_onlyProject, &QCheckBox::toggled, this, [this]() { rebuildTree(); });
    connect(m_categoryGroup, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == 0) {
            m_category.reset();
        } else {
            m_category = static_cast<Category>(id - 1);
        }
        refreshTypeCombo();
        rebuildTree();
    });
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this]() { emit currentChanged(); });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *treeItem) {
        if (const Item *item = itemOf(treeItem)) {
            emit activated(item->id);
        }
    });

    buildItems();
    refreshTypeCombo();
    rebuildTree();
}

void ProfileBrowser::setProjectSections(const QVector<SectionInfo> &sections)
{
    m_project = sections;
    const QUuid previous = currentId();
    buildItems();
    refreshTypeCombo();
    rebuildTree();
    if (!previous.isNull()) {
        setCurrent(previous);
    }
}

void ProfileBrowser::focusSearch()
{
    m_search->setFocus();
}

void ProfileBrowser::buildItems()
{
    m_items.clear();
    QHash<QString, int> projectByKey;
    for (int i = 0; i < m_project.size(); ++i) {
        projectByKey.insert(keyOf(m_project[i].family, m_project[i].name), i);
    }
    QSet<int> used;
    for (const auto &entry : ProfileLibrary::entries()) {
        Item item;
        item.id = entry.id;
        item.family = entry.family;
        item.name = entry.name;
        item.parameters = entry.parameters;
        item.category = ProfileLibrary::categoryOf(entry.family);
        item.fromLibrary = true;
        const auto it = projectByKey.constFind(keyOf(entry.family, entry.name));
        if (it != projectByKey.constEnd()) {
            item.projectId = m_project[it.value()].uuid;
            item.projectSection = m_project[it.value()];
            used.insert(it.value());
        }
        m_items.append(item);
    }
    for (int i = 0; i < m_project.size(); ++i) {
        if (used.contains(i)) {
            continue;
        }
        Item item;
        item.id = m_project[i].uuid;
        item.projectId = m_project[i].uuid;
        item.family = m_project[i].family;
        item.name = m_project[i].name;
        item.parameters = m_project[i].geometry;
        item.category = ProfileLibrary::categoryOf(m_project[i].family);
        item.projectSection = m_project[i];
        m_items.append(item);
    }
    updateCategoryCounts();
}

void ProfileBrowser::updateCategoryCounts()
{
    QHash<int, int> counts;
    for (const Item &item : m_items) {
        ++counts[static_cast<int>(item.category) + 1];
    }
    for (Category category : ProfileLibrary::categories()) {
        const int id = static_cast<int>(category) + 1;
        if (auto *button = m_categoryGroup->button(id)) {
            button->setText(QStringLiteral("%1 (%2)").arg(ProfileLibrary::categoryName(category)).arg(counts.value(id)));
        }
    }
    if (auto *button = m_categoryGroup->button(0)) {
        button->setText(tr("Todos (%1)").arg(m_items.size()));
    }
}

void ProfileBrowser::refreshTypeCombo()
{
    const QSignalBlocker blocker(m_typeCombo);
    const QString previous = m_typeCombo->currentData().toString();
    m_typeCombo->clear();
    m_typeCombo->addItem(tr("Todos os tipos"), QString());

    QStringList families;
    if (m_category) {
        families = ProfileLibrary::familiesIn(*m_category);
    } else {
        for (const auto &family : SectionShapes::families()) {
            families.append(family.name);
        }
    }
    for (const QString &family : families) {
        const QString label = m_category ? ProfileLibrary::typeLabel(family)
                                         : tr("%1 (%2)").arg(ProfileLibrary::typeLabel(family),
                                                             ProfileLibrary::categoryName(ProfileLibrary::categoryOf(family)));
        m_typeCombo->addItem(familyIcon(family), label, family);
    }
    const int index = m_typeCombo->findData(previous);
    m_typeCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void ProfileBrowser::rebuildTree()
{
    const QUuid previous = currentId();
    const QStringList words = m_search->text().simplified().toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString typeFilter = m_typeCombo->currentData().toString();
    const bool narrowed = !words.isEmpty() || !typeFilter.isEmpty();

    QFont bold = m_tree->font();
    bold.setBold(true);

    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    QHash<QString, QTreeWidgetItem *> groups;
    QHash<QString, int> groupCounts;
    QTreeWidgetItem *toSelect = nullptr;
    int shown = 0;

    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items[i];
        if (m_category && item.category != *m_category) {
            continue;
        }
        if (!typeFilter.isEmpty() && item.family != typeFilter) {
            continue;
        }
        if (m_onlyProject->isChecked() && item.projectId.isNull()) {
            continue;
        }
        if (!words.isEmpty()) {
            const QString haystack = (item.name + QLatin1Char(' ') + ProfileLibrary::typeLabel(item.family)).toLower();
            bool match = true;
            for (const QString &word : words) {
                if (!haystack.contains(word)) {
                    match = false;
                    break;
                }
            }
            if (!match) {
                continue;
            }
        }

        const QString groupKey = item.family.isEmpty() ? tr("Só propriedades") : item.family;
        QTreeWidgetItem *&group = groups[groupKey];
        if (!group) {
            const QString label = item.family.isEmpty()
                                      ? groupKey
                                      : (m_category ? ProfileLibrary::typeLabel(item.family)
                                                    : tr("%1 - %2").arg(ProfileLibrary::typeLabel(item.family),
                                                                         ProfileLibrary::categoryName(item.category)));
            group = new QTreeWidgetItem(m_tree, QStringList(label));
            group->setData(0, kItemRole, -1);
            group->setFont(0, bold);
            if (!item.family.isEmpty()) {
                group->setIcon(0, familyIcon(item.family));
            }
        }
        ++groupCounts[groupKey];
        auto *treeItem = new QTreeWidgetItem(group, QStringList({item.name, item.projectId.isNull() ? QString() : tr("no projeto")}));
        treeItem->setData(0, kItemRole, i);
        treeItem->setForeground(1, QColor(QStringLiteral("#0d7fb9")));
        if (!previous.isNull() && (item.id == previous || item.projectId == previous)) {
            toSelect = treeItem;
        }
        ++shown;
    }

    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        it.value()->setText(0, tr("%1  (%2)").arg(it.value()->text(0)).arg(groupCounts.value(it.key())));
        it.value()->setExpanded(narrowed || groups.size() == 1);
    }
    if (toSelect) {
        toSelect->parent()->setExpanded(true);
        m_tree->setCurrentItem(toSelect);
        m_tree->scrollToItem(toSelect);
    }
    m_tree->resizeColumnToContents(1);
    QString countText = tr("%1 perfil(is) exibido(s)").arg(shown);
    if (shown == 0) {
        bool categoryHasItems = false;
        for (const Item &item : m_items) {
            categoryHasItems = categoryHasItems || (m_category && item.category == *m_category);
        }
        countText = (m_category && !categoryHasItems)
                        ? tr("Ainda não há perfis de biblioteca nesta categoria. Use \"Novo perfil\" para criá-los.")
                        : tr("Nenhum perfil encontrado.");
    }
    m_count->setText(countText);
    emit currentChanged();
}

const ProfileBrowser::Item *ProfileBrowser::itemOf(const QTreeWidgetItem *treeItem) const
{
    if (!treeItem) {
        return nullptr;
    }
    const int index = treeItem->data(0, kItemRole).toInt();
    return index >= 0 && index < m_items.size() ? &m_items[index] : nullptr;
}

void ProfileBrowser::setCurrent(const QUuid &id)
{
    if (id.isNull()) {
        return;
    }
    // Se o item esta oculto pelos filtros, volta para "Todos".
    auto find = [&]() -> QTreeWidgetItem * {
        QTreeWidgetItemIterator it(m_tree);
        while (*it) {
            if (const Item *item = itemOf(*it)) {
                if (item->id == id || item->projectId == id) {
                    return *it;
                }
            }
            ++it;
        }
        return nullptr;
    };
    QTreeWidgetItem *found = find();
    if (!found) {
        m_search->clear();
        m_onlyProject->setChecked(false);
        m_category.reset();
        m_categoryGroup->button(0)->setChecked(true);
        refreshTypeCombo();
        rebuildTree();
        found = find();
    }
    if (found) {
        found->parent()->setExpanded(true);
        m_tree->setCurrentItem(found);
        m_tree->scrollToItem(found);
    }
}

QUuid ProfileBrowser::currentId() const
{
    const Item *item = itemOf(m_tree->currentItem());
    return item ? item->id : QUuid();
}

bool ProfileBrowser::currentInProject() const
{
    const Item *item = itemOf(m_tree->currentItem());
    return item && !item->projectId.isNull();
}

std::optional<ProfileBrowser::SectionInfo> ProfileBrowser::currentSection() const
{
    const Item *item = itemOf(m_tree->currentItem());
    if (!item) {
        return std::nullopt;
    }
    if (!item->projectId.isNull()) {
        return item->projectSection;
    }
    if (const auto *entry = ProfileLibrary::find(item->id)) {
        return ProfileLibrary::makeSection(*entry);
    }
    return std::nullopt;
}

// ------------------------------------------------------------------ detalhes

ProfileDetailPanel::ProfileDetailPanel(QWidget *parent)
    : QWidget(parent)
    , m_title(new QLabel(this))
    , m_subtitle(new QLabel(this))
    , m_preview(new SectionPreviewWidget(this))
    , m_properties(new QLabel(this))
{
    m_title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 15px; color: #0d7fb9;"));
    m_title->setWordWrap(true);
    m_subtitle->setStyleSheet(QStringLiteral("color: #6e7781;"));
    m_properties->setTextFormat(Qt::RichText);
    m_properties->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(m_title);
    layout->addWidget(m_subtitle);
    layout->addWidget(m_preview, 1);
    layout->addWidget(m_properties);
    setSection(std::nullopt, false);
}

void ProfileDetailPanel::setSection(const std::optional<ProfileBrowser::SectionInfo> &section, bool inProject)
{
    if (!section) {
        m_title->setText(tr("Selecione um perfil"));
        m_subtitle->clear();
        m_preview->clear();
        m_preview->setPlaceholder(tr("Escolha um perfil na lista para ver o contorno e as propriedades."));
        m_properties->clear();
        return;
    }
    m_title->setText(section->name);
    const QString category = ProfileLibrary::categoryName(ProfileLibrary::categoryOf(section->family));
    m_subtitle->setText(section->family.isEmpty()
                            ? category
                            : tr("%1 · %2%3").arg(category, ProfileLibrary::typeLabel(section->family),
                                                  inProject ? tr(" · no projeto") : QString()));
    m_preview->setPlaceholder(tr("Sem contorno para exibir"));
    m_preview->setSection(section->family, section->geometry, section->mirrored);
    m_properties->setText(
        tr("<b>A</b> = %1 cm&sup2;<br><b>Iz</b> = %2 cm&#8308;<br><b>Iy</b> = %3 cm&#8308;<br><b>J</b> = %4 cm&#8308;")
            .arg(section->area * 1e4, 0, 'f', 2)
            .arg(section->iz * 1e8, 0, 'f', 1)
            .arg(section->iy * 1e8, 0, 'f', 1)
            .arg(section->j * 1e8, 0, 'f', 2));
}

} // namespace Structura::UI
