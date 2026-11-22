#include "LoginDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "license_client.h"
#include "qrcodegen.hpp"

namespace Structura::UI {

namespace {

QPixmap generateQrPixmap(const QString &text, int targetPx = 180)
{
    using namespace qrcodegen;
    try {
        QrCode qr = QrCode::encodeText(text.toUtf8().constData(), QrCode::Ecc::MEDIUM);
        int size = qr.getSize();
        int border = 3;
        int total = size + 2 * border;
        int scale = std::max(1, targetPx / total);
        int imgSize = total * scale;

        QImage img(imgSize, imgSize, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (qr.getModule(x, y)) {
                    p.drawRect((x + border) * scale, (y + border) * scale, scale, scale);
                }
            }
        }
        return QPixmap::fromImage(img);
    } catch (...) {
        return QPixmap();
    }
}

} // namespace

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Structura — Autenticação de Licença"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(480, 490);
    setWindowModality(Qt::ApplicationModal);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // =========================================================================
    // Página 0: Formulário de Login Padrão
    // =========================================================================
    m_loginPage = new QWidget(this);
    auto *loginLayout = new QVBoxLayout(m_loginPage);
    loginLayout->setContentsMargins(12, 8, 12, 8);
    loginLayout->setSpacing(12);

    auto *headerLabel = new QLabel(
        tr("<h2>Acesso ao Sistema</h2><p style='color:#555;'>Informe suas credenciais e o código de autenticação (2FA) para iniciar.</p>"),
        m_loginPage);
    headerLabel->setTextFormat(Qt::RichText);
    headerLabel->setWordWrap(true);
    loginLayout->addWidget(headerLabel);

    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_emailEdit = new QLineEdit(m_loginPage);
    m_emailEdit->setPlaceholderText(tr("exemplo@empresa.com"));
    m_emailEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("<b>E-mail:</b>"), m_emailEdit);

    m_passwordEdit = new QLineEdit(m_loginPage);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Sua senha cadastrada"));
    m_passwordEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("<b>Senha:</b>"), m_passwordEdit);

    m_totpEdit = new QLineEdit(m_loginPage);
    m_totpEdit->setPlaceholderText(tr("000000 (se já configurado)"));
    m_totpEdit->setMaxLength(6);
    m_totpEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("<b>Código 2FA:</b>"), m_totpEdit);

    loginLayout->addLayout(formLayout);

    m_errorLabel = new QLabel(m_loginPage);
    m_errorLabel->setStyleSheet(QStringLiteral("QLabel { color: #d32f2f; font-weight: bold; background: #ffebee; border: 1px solid #ffcdd2; border-radius: 4px; padding: 6px; }"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    loginLayout->addWidget(m_errorLabel);

    m_statusLabel = new QLabel(tr("Conectando ao servidor de autenticação…"), m_loginPage);
    m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #1976d2; font-style: italic; }"));
    m_statusLabel->hide();
    loginLayout->addWidget(m_statusLabel);

    auto *privacyLabel = new QLabel(
        tr("<span style='color:#777;font-size:11px;'>Para prevenção de fraudes, o Structura registra no servidor de licenças "
           "seu e-mail, endereço IP, nome do computador, versão do sistema e do programa a cada acesso. "
           "Nenhum conteúdo de projetos é enviado.</span>"), m_loginPage);
    privacyLabel->setTextFormat(Qt::RichText);
    privacyLabel->setWordWrap(true);
    loginLayout->addWidget(privacyLabel);

    loginLayout->addStretch();

    auto *loginButtonLayout = new QHBoxLayout;
    loginButtonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancelar"), m_loginPage);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    connect(m_cancelButton, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
    loginButtonLayout->addWidget(m_cancelButton);

    m_loginButton = new QPushButton(tr("Entrar"), m_loginPage);
    m_loginButton->setDefault(true);
    m_loginButton->setCursor(Qt::PointingHandCursor);
    m_loginButton->setStyleSheet(QStringLiteral("QPushButton { background-color: #1976d2; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #1565c0; }"));
    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    loginButtonLayout->addWidget(m_loginButton);

    loginLayout->addLayout(loginButtonLayout);
    m_stack->addWidget(m_loginPage);

    // =========================================================================
    // Página 1: Configuração / Auto-Enrollment do 2FA
    // =========================================================================
    m_setupPage = new QWidget(this);
    auto *setupLayout = new QVBoxLayout(m_setupPage);
    setupLayout->setContentsMargins(12, 8, 12, 8);
    setupLayout->setSpacing(10);

    auto *setupHeader = new QLabel(
        tr("<h3>Primeiro acesso: configure a autenticação em duas etapas (2FA)</h3>"
           "<p style='color:#555;'><b>Passo 1.</b> Instale um aplicativo autenticador no celular "
           "(Google Authenticator, Microsoft Authenticator ou Authy).<br>"
           "<b>Passo 2.</b> No aplicativo, escolha <i>Adicionar conta</i> e escaneie o QR Code abaixo "
           "(ou digite a chave manualmente).<br>"
           "<b>Passo 3.</b> Digite o código de 6 dígitos que o aplicativo exibir e confirme.</p>"),
        m_setupPage);
    setupHeader->setTextFormat(Qt::RichText);
    setupHeader->setWordWrap(true);
    setupLayout->addWidget(setupHeader);

    m_qrLabel = new QLabel(m_setupPage);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(180, 180);
    setupLayout->addWidget(m_qrLabel);

    m_secretLabel = new QLabel(m_setupPage);
    m_secretLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_secretLabel->setAlignment(Qt::AlignCenter);
    m_secretLabel->setStyleSheet(QStringLiteral("QLabel { background: #f5f5f5; border: 1px solid #e0e0e0; border-radius: 4px; padding: 4px; font-family: monospace; }"));
    m_secretLabel->setWordWrap(true);
    setupLayout->addWidget(m_secretLabel);

    auto *confirmFormLayout = new QFormLayout;
    confirmFormLayout->setSpacing(8);
    confirmFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_setupTotpEdit = new QLineEdit(m_setupPage);
    m_setupTotpEdit->setPlaceholderText(tr("Código de 6 dígitos"));
    m_setupTotpEdit->setMaxLength(6);
    m_setupTotpEdit->setClearButtonEnabled(true);
    confirmFormLayout->addRow(tr("<b>Código do App:</b>"), m_setupTotpEdit);
    setupLayout->addLayout(confirmFormLayout);

    m_setupErrorLabel = new QLabel(m_setupPage);
    m_setupErrorLabel->setStyleSheet(QStringLiteral("QLabel { color: #d32f2f; font-weight: bold; background: #ffebee; border: 1px solid #ffcdd2; border-radius: 4px; padding: 6px; }"));
    m_setupErrorLabel->setWordWrap(true);
    m_setupErrorLabel->hide();
    setupLayout->addWidget(m_setupErrorLabel);

    m_setupStatusLabel = new QLabel(tr("Confirmando chave 2FA…"), m_setupPage);
    m_setupStatusLabel->setStyleSheet(QStringLiteral("QLabel { color: #1976d2; font-style: italic; }"));
    m_setupStatusLabel->hide();
    setupLayout->addWidget(m_setupStatusLabel);

    setupLayout->addStretch();

    auto *setupButtonLayout = new QHBoxLayout;
    setupButtonLayout->addStretch();

    m_backToLoginButton = new QPushButton(tr("Voltar"), m_setupPage);
    m_backToLoginButton->setCursor(Qt::PointingHandCursor);
    connect(m_backToLoginButton, &QPushButton::clicked, this, &LoginDialog::onBackToLoginClicked);
    setupButtonLayout->addWidget(m_backToLoginButton);

    m_confirm2faButton = new QPushButton(tr("Confirmar e Entrar"), m_setupPage);
    m_confirm2faButton->setDefault(true);
    m_confirm2faButton->setCursor(Qt::PointingHandCursor);
    m_confirm2faButton->setStyleSheet(QStringLiteral("QPushButton { background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #1b5e20; }"));
    connect(m_confirm2faButton, &QPushButton::clicked, this, &LoginDialog::onConfirm2faClicked);
    setupButtonLayout->addWidget(m_confirm2faButton);

    setupLayout->addLayout(setupButtonLayout);
    m_stack->addWidget(m_setupPage);

    // =========================================================================
    // Página 2: Troca obrigatória da senha temporária (criada pelo administrador)
    // =========================================================================
    m_changePage = new QWidget(this);
    auto *changeLayout = new QVBoxLayout(m_changePage);
    changeLayout->setContentsMargins(12, 8, 12, 8);
    changeLayout->setSpacing(12);

    auto *changeHeader = new QLabel(
        tr("<h3>Defina sua senha definitiva</h3>"
           "<p style='color:#555;'>Você entrou com uma senha temporária. Por segurança, escolha uma nova senha "
           "(mínimo de 7 caracteres) para continuar.</p>"), m_changePage);
    changeHeader->setTextFormat(Qt::RichText);
    changeHeader->setWordWrap(true);
    changeLayout->addWidget(changeHeader);

    auto *changeForm = new QFormLayout;
    changeForm->setSpacing(10);
    m_newPasswordEdit = new QLineEdit(m_changePage);
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordEdit->setPlaceholderText(tr("Nova senha"));
    changeForm->addRow(tr("<b>Nova senha:</b>"), m_newPasswordEdit);
    m_newPasswordConfirmEdit = new QLineEdit(m_changePage);
    m_newPasswordConfirmEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordConfirmEdit->setPlaceholderText(tr("Repita a nova senha"));
    changeForm->addRow(tr("<b>Confirmar:</b>"), m_newPasswordConfirmEdit);
    changeLayout->addLayout(changeForm);

    m_changeErrorLabel = new QLabel(m_changePage);
    m_changeErrorLabel->setStyleSheet(QStringLiteral("QLabel { color: #d32f2f; font-weight: bold; background: #ffebee; border: 1px solid #ffcdd2; border-radius: 4px; padding: 6px; }"));
    m_changeErrorLabel->setWordWrap(true);
    m_changeErrorLabel->hide();
    changeLayout->addWidget(m_changeErrorLabel);
    changeLayout->addStretch();

    auto *changeButtons = new QHBoxLayout;
    changeButtons->addStretch();
    m_changeCancelButton = new QPushButton(tr("Cancelar"), m_changePage);
    connect(m_changeCancelButton, &QPushButton::clicked, this, &LoginDialog::onChangePasswordCancel);
    changeButtons->addWidget(m_changeCancelButton);
    m_changeButton = new QPushButton(tr("Alterar senha e entrar"), m_changePage);
    m_changeButton->setDefault(true);
    m_changeButton->setStyleSheet(QStringLiteral("QPushButton { background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }"));
    connect(m_changeButton, &QPushButton::clicked, this, &LoginDialog::onChangePasswordClicked);
    changeButtons->addWidget(m_changeButton);
    changeLayout->addLayout(changeButtons);
    m_stack->addWidget(m_changePage);

    m_stack->setCurrentWidget(m_loginPage);
}

void LoginDialog::finishLogin(const structura_license::ClientSession &session)
{
    if (session.mustChangePassword) {
        // Sessão válida no servidor, mas o app só abre depois da troca da senha temporária.
        m_pendingSession = session;
        m_hasPendingSession = true;
        m_pendingPassword = m_passwordEdit->text().toStdString();
        m_newPasswordEdit->clear();
        m_newPasswordConfirmEdit->clear();
        m_changeErrorLabel->hide();
        setFixedSize(480, 360);
        m_stack->setCurrentWidget(m_changePage);
        m_newPasswordEdit->setFocus();
        return;
    }
    structura_license::SaveSessionToDisk(session);
    accept();
}

void LoginDialog::abandonPendingSession()
{
    if (m_hasPendingSession) {
        // Libera o vínculo do dispositivo no servidor e limpa a sessão em memória.
        structura_license::ClientLogout(structura_license::GetLicenseServerUrl(), m_pendingSession);
        m_hasPendingSession = false;
        m_pendingSession = structura_license::ClientSession{};
        m_pendingPassword.clear();
    }
}

void LoginDialog::reject()
{
    abandonPendingSession();
    QDialog::reject();
}

void LoginDialog::onChangePasswordCancel()
{
    abandonPendingSession();
    setFixedSize(480, 490);
    m_stack->setCurrentWidget(m_loginPage);
}

void LoginDialog::onChangePasswordClicked()
{
    m_changeErrorLabel->hide();
    const QString np = m_newPasswordEdit->text();
    if (np.length() < 7) {
        m_changeErrorLabel->setText(tr("A nova senha deve ter ao menos 7 caracteres."));
        m_changeErrorLabel->show();
        return;
    }
    if (np != m_newPasswordConfirmEdit->text()) {
        m_changeErrorLabel->setText(tr("As senhas não conferem."));
        m_changeErrorLabel->show();
        return;
    }
    if (np.toStdString() == m_pendingPassword) {
        m_changeErrorLabel->setText(tr("A nova senha deve ser diferente da senha temporária."));
        m_changeErrorLabel->show();
        return;
    }
    m_changeButton->setEnabled(false);
    QCoreApplication::processEvents();
    std::string err;
    const bool ok = structura_license::ClientChangePassword(
        structura_license::GetLicenseServerUrl(), m_emailEdit->text().trimmed().toStdString(),
        m_pendingPassword, np.toStdString(), err);
    m_changeButton->setEnabled(true);
    if (!ok) {
        m_changeErrorLabel->setText(translateError(err) + QStringLiteral("\n[código: %1]").arg(QString::fromStdString(err)));
        m_changeErrorLabel->show();
        return;
    }
    structura_license::ClientSession done = m_pendingSession;
    done.mustChangePassword = false;
    m_hasPendingSession = false;
    m_pendingPassword.clear();
    structura_license::SetActiveClientSession(done);
    structura_license::SaveSessionToDisk(done);
    accept();
}

bool LoginDialog::ensureAuthenticated(QWidget *parent)
{
    // 1. Verifica se já existe sessão ativa e válida na memória global
    const auto activeSession = structura_license::GetActiveClientSession();
    if (activeSession.online && !activeSession.licenseToken.empty()) {
        return true;
    }

    const std::string serverUrl = structura_license::GetLicenseServerUrl();
    const std::string installDir = structura_license::GetAppInstallDir();
    const uint8_t *releasePubKey = structura_license::GetReleasePublicKey();

    // 2. Tenta restaurar sessão persistida em disco através de heartbeat
    structura_license::ClientSession savedSession;
    if (structura_license::LoadSessionFromDisk(savedSession)) {
        std::string hbError;
        if (structura_license::ClientHeartbeat(serverUrl, savedSession, installDir, releasePubKey, hbError)) {
            structura_license::SetActiveClientSession(savedSession);
            structura_license::SaveSessionToDisk(savedSession);
            return true;
        }
    }

    // Se a sessão expirou ou não existe, limpa qualquer resquício em disco
    structura_license::ClearSessionOnDisk();

    // 3. Exibe diálogo de login bloqueante
    LoginDialog dlg(parent);
    dlg.raise();
    dlg.activateWindow();
    return (dlg.exec() == QDialog::Accepted);
}

void LoginDialog::onLoginClicked()
{
    m_errorLabel->hide();

    const QString email = m_emailEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    const QString totp = m_totpEdit->text().trimmed();

    if (email.isEmpty() || password.isEmpty()) {
        showError(tr("Preencha os campos obrigatórios: E-mail e Senha."));
        return;
    }

    setWorking(true);
    QCoreApplication::processEvents();

    const std::string serverUrl = structura_license::GetLicenseServerUrl();
    const std::string installDir = structura_license::GetAppInstallDir();
    const uint8_t *releasePubKey = structura_license::GetReleasePublicKey();

    structura_license::ClientSession newSession;
    std::string outError;

    bool ok = structura_license::ClientLogin(
        serverUrl,
        email.toStdString(),
        password.toStdString(),
        totp.toStdString(),
        installDir,
        releasePubKey,
        newSession,
        outError
    );

    setWorking(false);

    if (ok) {
        finishLogin(newSession);
    } else if (outError == "REQUIRES_2FA_SETUP" || outError == "ENROLL_2FA") {
        // Conta válida, porém sem 2FA inscrito: inicia auto-enrollment
        std::string secret, otpauthUri, setupErr;
        setWorking(true);
        bool setupOk = structura_license::ClientTwoFASetup(
            serverUrl,
            email.toStdString(),
            password.toStdString(),
                secret,
            otpauthUri,
            setupErr
        );
        setWorking(false);

        if (setupOk) {
            switchToSetupPage(secret, otpauthUri);
        } else {
            showError(tr("Erro ao inicializar 2FA: %1").arg(translateError(setupErr)));
        }
    } else {
        // Codigo tecnico visivel (diagnostico rapido: HTTP_xxx, DEVICE_IN_USE, SERVER_SIGNATURE_INVALID...)
        showError(translateError(outError) + QStringLiteral("\n[código: %1]").arg(QString::fromStdString(outError)));
    }
}

void LoginDialog::switchToSetupPage(const std::string &secret, const std::string &otpauthUri)
{
    m_setupSecret = secret;
    m_setupOtpAuthUri = otpauthUri;

    QPixmap qrPix = generateQrPixmap(QString::fromStdString(otpauthUri), 190);
    m_qrLabel->setPixmap(qrPix);
    m_secretLabel->setText(tr("Chave secreta manual: <b>%1</b>").arg(QString::fromStdString(secret)));

    m_setupTotpEdit->clear();
    m_setupErrorLabel->hide();
    m_setupStatusLabel->hide();

    setFixedSize(520, 680);
    m_stack->setCurrentWidget(m_setupPage);
    m_setupTotpEdit->setFocus();
}

void LoginDialog::onConfirm2faClicked()
{
    m_setupErrorLabel->hide();

    const QString code = m_setupTotpEdit->text().trimmed();
    if (code.length() != 6) {
        showSetupError(tr("Digite o código de 6 dígitos gerado no aplicativo autenticador."));
        return;
    }

    const QString email = m_emailEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    const std::string serverUrl = structura_license::GetLicenseServerUrl();
    const std::string installDir = structura_license::GetAppInstallDir();
    const uint8_t *releasePubKey = structura_license::GetReleasePublicKey();

    m_confirm2faButton->setEnabled(false);
    m_backToLoginButton->setEnabled(false);
    m_setupStatusLabel->show();
    QCoreApplication::processEvents();

    std::string confirmErr;
    bool confirmOk = structura_license::ClientTwoFAConfirm(
        serverUrl,
        email.toStdString(),
        password.toStdString(),
        code.toStdString(),
        confirmErr
    );

    if (!confirmOk) {
        m_confirm2faButton->setEnabled(true);
        m_backToLoginButton->setEnabled(true);
        m_setupStatusLabel->hide();
        showSetupError(translateError(confirmErr));
        return;
    }

    // 2FA confirmado com sucesso! Realiza o login imediatamente
    structura_license::ClientSession newSession;
    std::string loginErr;
    bool loginOk = structura_license::ClientLogin(
        serverUrl,
        email.toStdString(),
        password.toStdString(),
        code.toStdString(),
        installDir,
        releasePubKey,
        newSession,
        loginErr
    );

    m_confirm2faButton->setEnabled(true);
    m_backToLoginButton->setEnabled(true);
    m_setupStatusLabel->hide();

    if (loginOk) {
        finishLogin(newSession);
    } else {
        showSetupError(translateError(loginErr));
    }
}

void LoginDialog::onBackToLoginClicked()
{
    setFixedSize(480, 490);
    m_stack->setCurrentWidget(m_loginPage);
}

void LoginDialog::onCancelClicked()
{
    reject();
}

void LoginDialog::setWorking(bool working)
{
    m_emailEdit->setEnabled(!working);
    m_passwordEdit->setEnabled(!working);
    m_totpEdit->setEnabled(!working);
    m_loginButton->setEnabled(!working);
    m_cancelButton->setEnabled(!working);

    if (working) {
        m_statusLabel->show();
    } else {
        m_statusLabel->hide();
    }
}

void LoginDialog::showError(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorLabel->show();
}

void LoginDialog::showSetupError(const QString &message)
{
    m_setupErrorLabel->setText(message);
    m_setupErrorLabel->show();
}

QString LoginDialog::translateError(const std::string &errorCode)
{
    if (errorCode == "SERVER_OFFLINE_OR_TIMEOUT") {
        return tr("Não foi possível conectar ao servidor de licenciamento. Verifique sua conexão à internet ou firewall.");
    }
    if (errorCode == "PASSWORD_UNCHANGED") {
        return tr("A nova senha deve ser diferente da senha atual.");
    }
    if (errorCode == "TOO_MANY_ATTEMPTS" || errorCode == "HTTP_429") {
        return tr("Muitas tentativas falhas a partir desta rede. Aguarde alguns minutos e tente novamente.");
    }
    if (errorCode == "INVALID_REQUEST" || errorCode == "HTTP_422") {
        return tr("Os dados informados estão em formato inválido. Confira o e-mail e o código 2FA (6 dígitos).");
    }
    if (errorCode == "LOGIN_REJECTED" || errorCode == "INVALID_SERVER_RESPONSE" || errorCode == "SERVER_SIGNATURE_INVALID") {
        // NAO e erro de credencial: o servidor respondeu algo inesperado ou nao autenticado.
        return tr("Resposta inválida do servidor de licenciamento (%1). Verifique o endereço do servidor e atualize o aplicativo.")
            .arg(QString::fromStdString(errorCode));
    }
    if (errorCode == "DEVICE_IN_USE") {
        return tr("Acesso negado: a licença já está em uso em outro computador. Encerre a sessão no outro dispositivo antes de continuar.");
    }
    if (errorCode == "CLIENT_OUTDATED") {
        return tr("Esta versão do aplicativo foi revogada. Instale a versão mais recente.");
    }
    if (errorCode == "INTEGRITY_FAIL") {
        return tr("O servidor não reconheceu a integridade desta instalação. Reinstale o aplicativo.");
    }
    if (errorCode == "LICENSE_INACTIVE") {
        return tr("Nenhuma licença ativa encontrada para esta conta (expirada, suspensa ou inexistente).");
    }
    if (errorCode == "CREDENCIAIS" || errorCode == "BAD_CREDENTIALS") {
        return tr("E-mail ou senha incorretos (ou conta temporariamente bloqueada após várias tentativas). Verifique suas credenciais.");
    }
    if (errorCode == "REQUIRES_2FA_TOTP" || errorCode == "BAD_TOTP" || errorCode == "TOTP_REQUIRED") {
        return tr("Código de autenticação 2FA inválido ou expirado. Digite o código de 6 dígitos gerado no aplicativo autenticador.");
    }
    if (errorCode == "REQUIRES_2FA_SETUP" || errorCode == "ENROLL_2FA") {
        return tr("Sua conta requer a configuração da autenticação em duas etapas (2FA).");
    }
    if (errorCode == "LOCKED" || errorCode == "ACCOUNT_LOCKED") {
        return tr("Conta temporariamente bloqueada devido a repetidas tentativas incorretas. Aguarde 15 minutos e tente novamente.");
    }
    if (errorCode == "VM_NOT_ALLOWED") {
        return tr("Execução em máquina virtual / sandbox não autorizada para esta licença. Contate o administrador para inclusão na allowlist.");
    }
    if (errorCode == "INTEGRITY_CHECK_FAILED") {
        return tr("Falha na verificação de integridade dos arquivos do programa (manifesto ausente ou binários alterados). Reinstale o aplicativo.");
    }
    if (errorCode == "CONCURRENT_ACCESS" || errorCode == "DEVICE_BOUND" || errorCode == "DEVICE_LIMIT") {
        return tr("Acesso negado: a licença já está em uso em outro computador. Encerre a sessão no outro dispositivo antes de continuar.");
    }
    if (errorCode == "LICENSE_EXPIRED") {
        return tr("Sua licença ou assinatura expirou. Acesse o portal para renovação.");
    }
    if (errorCode == "LICENSE_SUSPENDED" || errorCode == "LICENSE_REVOKED") {
        return tr("Esta licença foi suspensa ou revogada pelo administrador.");
    }

    return tr("Falha na autenticação: %1").arg(QString::fromStdString(errorCode));
}

} // namespace Structura::UI
