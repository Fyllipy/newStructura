#pragma once

#include <QString>
#include <QStringList>
#include <QWizard>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;

namespace Structura::UI {

/// Escolhas do assistente de novo projeto.
struct NewProjectSettings {
    enum class Start { Blank, ReadyModel };

    QString name;
    QString company, client, site, author, registration;
    QString steel;               ///< aço padrão (nome na biblioteca)
    bool selfWeightCase {true};  ///< caso "Permanente" com peso próprio
    bool liveCase {true};
    bool windX {false};
    bool windY {false};
    bool openCombinationGenerator {true};
    Start start {Start::Blank};
};

/// Assistente exibido ao criar um novo projeto: identificação, aço padrão, casos de carga e ponto de partida.
class NewProjectWizard final : public QWizard
{
    Q_OBJECT

public:
    explicit NewProjectWizard(const QStringList &steels, const QString &defaultSteel, QWidget *parent = nullptr);
    [[nodiscard]] NewProjectSettings settings() const;

private:
    QLineEdit *m_name {nullptr}, *m_company {nullptr}, *m_client {nullptr}, *m_site {nullptr}, *m_author {nullptr}, *m_registration {nullptr};
    QComboBox *m_steel {nullptr};
    QCheckBox *m_dead {nullptr}, *m_live {nullptr}, *m_windX {nullptr}, *m_windY {nullptr}, *m_combinations {nullptr};
    QRadioButton *m_blank {nullptr}, *m_ready {nullptr};
};

} // namespace Structura::UI
