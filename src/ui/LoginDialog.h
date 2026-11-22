#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>

#include "license_config.h"

namespace Structura::UI {

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    // Método estático para garantir autenticação antes de abrir a janela principal.
    // Retorna true se houver sessão válida (existente, restaurada ou logada agora).
    // Retorna false se o usuário cancelou ou se a autenticação falhou.
    static bool ensureAuthenticated(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onConfirm2faClicked();
    void onBackToLoginClicked();
    void onChangePasswordClicked();
    void onChangePasswordCancel();

protected:
    void reject() override;

private:
    void setWorking(bool working);
    void showError(const QString &message);
    void showSetupError(const QString &message);
    QString translateError(const std::string &errorCode);
    void switchToSetupPage(const std::string &secret, const std::string &otpauthUri);
    // Conclui o login; se a senha for temporaria, abre antes a pagina de troca obrigatoria.
    void finishLogin(const structura_license::ClientSession &session);
    void abandonPendingSession();

    QStackedWidget *m_stack {nullptr};

    // Página 0: Login Normal
    QWidget *m_loginPage {nullptr};
    QLineEdit *m_emailEdit {nullptr};
    QLineEdit *m_passwordEdit {nullptr};
    QLineEdit *m_totpEdit {nullptr};
    QLabel *m_errorLabel {nullptr};
    QLabel *m_statusLabel {nullptr};
    QPushButton *m_loginButton {nullptr};
    QPushButton *m_cancelButton {nullptr};

    // Página 1: Auto-Enroll 2FA
    QWidget *m_setupPage {nullptr};
    QLabel *m_qrLabel {nullptr};
    QLabel *m_secretLabel {nullptr};
    QLineEdit *m_setupTotpEdit {nullptr};
    QLabel *m_setupErrorLabel {nullptr};
    QLabel *m_setupStatusLabel {nullptr};
    QPushButton *m_confirm2faButton {nullptr};
    QPushButton *m_backToLoginButton {nullptr};

    // Página 2: troca obrigatória da senha temporária
    QWidget *m_changePage {nullptr};
    QLineEdit *m_newPasswordEdit {nullptr};
    QLineEdit *m_newPasswordConfirmEdit {nullptr};
    QLabel *m_changeErrorLabel {nullptr};
    QPushButton *m_changeButton {nullptr};
    QPushButton *m_changeCancelButton {nullptr};
    structura_license::ClientSession m_pendingSession;
    bool m_hasPendingSession {false};
    std::string m_pendingPassword;

    std::string m_setupSecret;
    std::string m_setupOtpAuthUri;
};

} // namespace Structura::UI
