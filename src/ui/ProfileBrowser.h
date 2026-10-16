#pragma once

#include "ProfileLibrary.h"
#include "app/ProjectDataTypes.h"

#include <QUuid>
#include <QVector>
#include <QWidget>

#include <optional>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace Structura::UI {

class SectionPreviewWidget;

/// Navegador de perfis: biblioteca + secoes do projeto, com filtros por categoria
/// (laminado, moldado a frio, concreto), por tipo e por texto.
class ProfileBrowser : public QWidget
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit ProfileBrowser(QWidget *parent = nullptr);

    void setProjectSections(const QVector<SectionInfo> &sections);
    /// Seleciona o item de `id` (identificador da biblioteca ou uuid de uma secao do projeto).
    void setCurrent(const QUuid &id);
    /// Identificador a ser resolvido pela janela principal (biblioteca ou projeto); nulo se nada selecionado.
    [[nodiscard]] QUuid currentId() const;
    /// Secao do item atual com as propriedades calculadas.
    [[nodiscard]] std::optional<SectionInfo> currentSection() const;
    [[nodiscard]] bool currentInProject() const;
    void focusSearch();

signals:
    void currentChanged();
    void activated(const QUuid &id);

private:
    struct Item {
        QUuid id;
        QUuid projectId;
        QString family;
        QString name;
        Structura::Visualization::SectionShapes::Parameters parameters;
        ProfileLibrary::Category category {ProfileLibrary::Category::SemGeometria};
        bool fromLibrary {false};
        SectionInfo projectSection;
    };

    void buildItems();
    void refreshTypeCombo();
    void rebuildTree();
    [[nodiscard]] const Item *itemOf(const QTreeWidgetItem *treeItem) const;
    void updateCategoryCounts();

    QVector<SectionInfo> m_project;
    QVector<Item> m_items;
    std::optional<ProfileLibrary::Category> m_category;
    QLineEdit *m_search;
    QButtonGroup *m_categoryGroup;
    QComboBox *m_typeCombo;
    QCheckBox *m_onlyProject;
    QTreeWidget *m_tree;
    QLabel *m_count;
};

/// Painel de detalhes (contorno + propriedades) de uma secao.
class ProfileDetailPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileDetailPanel(QWidget *parent = nullptr);

    void setSection(const std::optional<ProfileBrowser::SectionInfo> &section, bool inProject);

private:
    QLabel *m_title;
    QLabel *m_subtitle;
    SectionPreviewWidget *m_preview;
    QLabel *m_properties;
};

} // namespace Structura::UI
