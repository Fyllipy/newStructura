#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QPushButton>
#include <QUuid>
#include <QVector>

namespace Structura::UI {

class ProfileBrowser;
class ProfileDetailPanel;

/// Janela de escolha de perfil para uma barra: filtros por categoria/tipo/busca e contorno do perfil.
class ProfilePickerDialog : public QDialog
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit ProfilePickerDialog(const QVector<SectionInfo> &sections, const QUuid &current,
                                 bool allowManage, QWidget *parent = nullptr);

    /// Identificador escolhido (biblioteca ou projeto); nulo quando "sem secao".
    [[nodiscard]] QUuid selectedId() const { return m_selected; }
    [[nodiscard]] bool manageRequested() const { return m_manage; }

private:
    ProfileBrowser *m_browser;
    ProfileDetailPanel *m_detail;
    QPushButton *m_ok;
    QUuid m_selected;
    bool m_manage {false};
};

/// Botao que mostra a secao atual e abre a janela de escolha (substitui a lista suspensa de secoes).
class SectionPickerButton : public QPushButton
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit SectionPickerButton(QWidget *parent = nullptr);

    void setSections(const QVector<SectionInfo> &sections);
    void setCurrent(const QUuid &id);
    void setMixed();
    void setManageAllowed(bool allowed) { m_allowManage = allowed; }
    [[nodiscard]] QUuid current() const { return m_current; }

signals:
    /// O usuario escolheu um perfil (nulo = sem secao). Atualiza o botao antes de emitir.
    void picked(const QUuid &id);
    void manageRequested();

private:
    void openPicker();
    void updateText();

    QVector<SectionInfo> m_sections;
    QUuid m_current;
    bool m_mixed {false};
    bool m_allowManage {true};
};

} // namespace Structura::UI
