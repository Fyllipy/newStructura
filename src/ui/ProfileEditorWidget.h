#pragma once

#include "ProfileLibrary.h"
#include "app/ProjectDataTypes.h"

#include <QMap>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace Structura::UI {

class SectionPreviewWidget;

/// Criacao de perfil em tres passos (categoria, tipo, dimensoes), com contorno e propriedades em tempo real.
/// Substitui a antiga janela "Nova secao".
class ProfileEditorWidget : public QWidget
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit ProfileEditorWidget(QWidget *parent = nullptr);

    void setExistingNames(const QStringList &names);
    void setSelectedBarCount(int count);
    /// Volta ao estado inicial (aco laminado, primeiro tipo, valores padrao).
    void reset();
    /// Pre-seleciona categoria/tipo/dimensoes a partir de uma secao existente (para criar variacoes).
    void startFrom(const SectionInfo &section);

signals:
    /// `info` sai sem uuid nem id externo; a janela principal completa e adiciona ao projeto.
    void createRequested(const SectionInfo &info, bool assignToSelection, bool saveToLibrary);
    void cancelled();

private:
    void setCategory(ProfileLibrary::Category category);
    void onTypeChanged();
    void rebuildParameterFields();
    void onParameterEdited();
    void onCatalogChosen(int index);
    void recalculate();
    void updateAutoName();
    void updateState();
    [[nodiscard]] QString currentFamily() const;
    [[nodiscard]] QMap<QString, double> currentParameters() const;
    [[nodiscard]] bool directMode() const;
    [[nodiscard]] SectionInfo buildInfo() const;

    ProfileLibrary::Category m_category {ProfileLibrary::Category::Laminado};
    QStringList m_existingNames;
    int m_barCount {0};
    bool m_autoName {true};
    bool m_valid {false};
    bool m_updating {false};
    double m_area {0.0};
    double m_iz {0.0};
    double m_iy {0.0};
    double m_j {0.0};

    QButtonGroup *m_categoryGroup;
    QListWidget *m_typeList;
    QStackedWidget *m_inputStack;
    QComboBox *m_catalogCombo;
    QWidget *m_parameterHost;
    QFormLayout *m_parameterLayout;
    QMap<QString, QDoubleSpinBox *> m_parameterSpins;
    QCheckBox *m_mirror;
    QCheckBox *m_saveToLibrary {nullptr};
    QDoubleSpinBox *m_areaSpin;
    QDoubleSpinBox *m_izSpin;
    QDoubleSpinBox *m_iySpin;
    QDoubleSpinBox *m_jSpin;
    QLineEdit *m_nameEdit;
    SectionPreviewWidget *m_preview;
    QLabel *m_properties;
    QLabel *m_message;
    QPushButton *m_createButton;
    QPushButton *m_createAssignButton;
};

} // namespace Structura::UI
