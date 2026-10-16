#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QUuid>
#include <QVector>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace Structura::UI {

class ProfileBrowser;
class ProfileDetailPanel;
class ProfileEditorWidget;

/// Gerenciador de perfis: navega pela biblioteca (por categoria, tipo e busca) e pelas secoes do projeto,
/// mostra contorno e propriedades, atribui as barras selecionadas e cria novos perfis na propria janela.
class ProfileManagerDialog : public QDialog
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit ProfileManagerDialog(QWidget *parent = nullptr);

    void setProjectSections(const QVector<SectionInfo> &sections);
    void setSelectedBarCount(int count);

    /// Abre a janela diretamente no painel de criacao de perfil.
    void showCreatePage();
    void showBrowsePage();
    /// Depois que a janela principal criou o perfil: volta a lista e seleciona o novo perfil.
    void profileCreated(const QUuid &id);

signals:
    void addToProjectRequested(const QUuid &id);
    void assignRequested(const QUuid &id);
    /// O perfil sai sem uuid nem id externo; a janela principal os completa.
    void createProfileRequested(const SectionInfo &info, bool assignToSelection, bool saveToLibrary);

private:
    void updateButtons();

    int m_barCount {0};
    QStackedWidget *m_pages;
    ProfileBrowser *m_browser;
    ProfileDetailPanel *m_detail;
    ProfileEditorWidget *m_editor;
    QLabel *m_header;
    QPushButton *m_newButton;
    QPushButton *m_variationButton;
    QPushButton *m_addButton;
    QPushButton *m_assignButton;
    QVector<SectionInfo> m_project;
};

} // namespace Structura::UI
