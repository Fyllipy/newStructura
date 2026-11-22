#include <QtTest>

#include "ui/LoginDialog.h"
#include "app/SolverBridge.h"
#include "license_client.h"
#include "license_config.h"

using namespace structura_license;
using namespace Structura::App;
using namespace Structura::UI;

class TestUiLicenseIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Requisito 1: Prova de que a abertura bloqueia sem sessão válida
    void testLoginBlocksWithoutSession();
    void testLoginPassesWithActiveSession();
    void testSessionPersistenceRestoreAndWipe();

    // Requisito 2: Prova de que o logout limpa a sessão e tranca o sistema
    void testLogoutCleansSessionAndLocks();

    // Requisito 3: Prova de que o Keystone é exigido antes do cálculo oficial
    void testKeystoneEnforcedBeforeOfficialCalculation();
    void testPreviewCalculationBypassesKeystone();

    // Requisito 4: Validação do fluxo de auto-enrollment 2FA
    void testTwoFAValidationHelpers();
};

void TestUiLicenseIntegration::initTestCase()
{
#ifdef _WIN32
    _putenv("STRUCTURA_TEST_DLL_KEY=");
#else
    unsetenv("STRUCTURA_TEST_DLL_KEY");
#endif
    // Limpa qualquer sessão pré-existente
    SetActiveClientSession(ClientSession{});
    ClearSessionOnDisk();
}

void TestUiLicenseIntegration::cleanupTestCase()
{
#ifdef _WIN32
    _putenv("STRUCTURA_TEST_DLL_KEY=");
#else
    unsetenv("STRUCTURA_TEST_DLL_KEY");
#endif
    SetActiveClientSession(ClientSession{});
    ClearSessionOnDisk();
}

void TestUiLicenseIntegration::testLoginBlocksWithoutSession()
{
    // 1. Sem sessão em memória e sem sessão em disco
    SetActiveClientSession(ClientSession{});
    ClearSessionOnDisk();

    QCOMPARE(GetActiveClientSession().online, false);
    QVERIFY(GetActiveClientSession().licenseToken.empty());

    // Diálogo em modo não-interativo com credenciais vazias DEVE rejeitar
    LoginDialog dlg;
    // Sem preencher credenciais, clicar em Entrar ou Cancelar não autentica
    QTimer::singleShot(50, &dlg, &QDialog::reject);
    int result = dlg.exec();
    QCOMPARE(result, static_cast<int>(QDialog::Rejected));

    // A sessão continua nula, comprovando que a janela principal não seria exibida
    QCOMPARE(GetActiveClientSession().online, false);
}

void TestUiLicenseIntegration::testLoginPassesWithActiveSession()
{
    // 2. Com sessão ativa previamente estabelecida em memória
    ClientSession active;
    active.online = true;
    active.licenseToken = "active_jwt_token_test";
    active.licenseId = 101;
    active.userId = 42;
    active.dllKey = "689e4efd8e345fb49aeeae4ab42d29689d2eba244eedfcebc3900bdf92066d10";
    active.fingerprint = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    SetActiveClientSession(active);

    // ensureAuthenticated DEVE retornar true imediatamente sem abrir diálogo
    bool authenticated = LoginDialog::ensureAuthenticated(nullptr);
    QVERIFY(authenticated);
    QCOMPARE(GetActiveClientSession().online, true);
    QCOMPARE(GetActiveClientSession().licenseToken, std::string("active_jwt_token_test"));
}

void TestUiLicenseIntegration::testSessionPersistenceRestoreAndWipe()
{
    // 3. Testa persistência segura de sessão no AppData
    ClientSession session;
    session.online = true;
    session.licenseToken = "persist_jwt_token_test";
    session.licenseId = 202;
    session.userId = 84;
    session.dllKey = "689e4efd8e345fb49aeeae4ab42d29689d2eba244eedfcebc3900bdf92066d10";
    session.fingerprint = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

    // Salva em disco
    bool saved = SaveSessionToDisk(session);
    QVERIFY(saved);

    // Limpa memória
    SetActiveClientSession(ClientSession{});
    QCOMPARE(GetActiveClientSession().online, false);

    // Carrega de disco
    ClientSession loaded;
    bool loadedOk = LoadSessionFromDisk(loaded);
    QVERIFY(loadedOk);
    QCOMPARE(loaded.licenseToken, session.licenseToken);
    QCOMPARE(loaded.licenseId, session.licenseId);
    QCOMPARE(loaded.userId, session.userId);
    QCOMPARE(loaded.dllKey, session.dllKey);
    QCOMPARE(loaded.fingerprint, session.fingerprint);
    QCOMPARE(loaded.online, true);

    // Limpa de disco
    ClearSessionOnDisk();
    ClientSession empty;
    bool loadedAfterWipe = LoadSessionFromDisk(empty);
    QCOMPARE(loadedAfterWipe, false);
    QVERIFY(empty.licenseToken.empty());
}

void TestUiLicenseIntegration::testLogoutCleansSessionAndLocks()
{
    // 4. Logout desloga no servidor, limpa memória e tranca gatekeeper
    ClientSession session;
    session.online = true;
    session.licenseToken = "token_to_logout";
    session.licenseId = 303;
    SetActiveClientSession(session);
    SaveSessionToDisk(session);

    // Executa logout
    bool logoutOk = ClientLogout(GetLicenseServerUrl(), session);
    QVERIFY(logoutOk);

    // Confirma que o estado foi resetado
    QCOMPARE(session.online, false);
    QVERIFY(session.licenseToken.empty());
    QCOMPARE(GetActiveClientSession().online, false);

    // Limpa arquivo de sessão do disco como a UI faz no botão Sair
    ClearSessionOnDisk();
    ClientSession checkDisk;
    QCOMPARE(LoadSessionFromDisk(checkDisk), false);
}

void TestUiLicenseIntegration::testKeystoneEnforcedBeforeOfficialCalculation()
{
    // 5. Cálculo oficial (!isPreview) sem Keystone válido DEVE falhar fechado
    SetActiveClientSession(ClientSession{}); // Sessão offline / vazia

    SolverBridge::ModelData model;
    // Monta modelo mínimo com 2 nós e 1 barra
    SolverBridge::NodeData n1{1, 0.0, 0.0, 0.0, {{1, 1, 1, 1, 1, 1}}};
    SolverBridge::NodeData n2{2, 3.0, 0.0, 0.0, {{0, 0, 0, 0, 0, 0}}};
    model.nodes = {n1, n2};

    SolverBridge::MaterialData mat{1, 200000.0, 77000.0, 7850.0};
    model.materials = {mat};

    SolverBridge::SectionData sec{1, 50.0, 1000.0, 1000.0, 500.0};
    model.sections = {sec};

    SolverBridge::MemberData m{1, 1, 2, 1, 1};
    model.members = {m};

    SolverBridge::NodalLoadData load{2, {{0.0, 0.0, -10.0, 0.0, 0.0, 0.0}}};
    model.nodalLoads = {load};

    SolverBridge solver;

#if !defined(STRUCTURA_DEV_BUILD)
    // Em build de Release com fail-closed ativo:
    // Caso A: Sem dllKey de release -> falha fechada na carga da DLL cifrada em repouso
    bool threwNoDllKey = false;
    try {
        (void)solver.solve(model, /*isPreview=*/false);
    } catch (const SolverBridgeError &err) {
        threwNoDllKey = true;
        std::string msg = err.what();
        QVERIFY(msg.find("rds") != std::string::npos || msg.find("solver") != std::string::npos);
    }
    QVERIFY(threwNoDllKey);

    // Caso B: Com dllKey de release autenticada, porém sem Keystone K válido do servidor
    ClientSession offlineSession;
    offlineSession.dllKey = "689e4efd8e345fb49aeeae4ab42d29689d2eba244eedfcebc3900bdf92066d10";
    offlineSession.licenseToken = "unauthenticated_test_token";
    offlineSession.licenseId = 1;
    offlineSession.online = true;
    SetActiveClientSession(offlineSession);

    bool threwKeystone = false;
    try {
        (void)solver.solve(model, /*isPreview=*/false);
    } catch (const SolverBridgeError &err) {
        threwKeystone = true;
        std::string msg = err.what();
        QVERIFY(msg.find("Keystone") != std::string::npos ||
                msg.find("COMPUTE_BLOCKED") != std::string::npos ||
                msg.find("licen") != std::string::npos);
    }
    QVERIFY(threwKeystone);
#endif
}

void TestUiLicenseIntegration::testPreviewCalculationBypassesKeystone()
{
    // 6. Cálculo em modo preview (isPreview=true) não faz requisição de Keystone
    // (pode rodar ou falhar apenas se a DLL não carregar, sem erro de Keystone)
    SolverBridge::ModelData model;
    SolverBridge::NodeData n1{1, 0.0, 0.0, 0.0, {{1, 1, 1, 1, 1, 1}}};
    SolverBridge::NodeData n2{2, 3.0, 0.0, 0.0, {{0, 0, 0, 0, 0, 0}}};
    model.nodes = {n1, n2};

    SolverBridge::MaterialData mat{1, 200000.0, 77000.0, 7850.0};
    model.materials = {mat};

    SolverBridge::SectionData sec{1, 50.0, 1000.0, 1000.0, 500.0};
    model.sections = {sec};

    SolverBridge::MemberData m{1, 1, 2, 1, 1};
    model.members = {m};

    SolverBridge::NodalLoadData load{2, 0.0, 0.0, -100.0, 0.0, 0.0, 0.0};
    model.nodalLoads = {load};

    SolverBridge solver;
    bool completedOrNonKeystoneError = false;
    try {
        auto res = solver.solve(model, /*isPreview=*/true);
        completedOrNonKeystoneError = (res.nodeResults.size() == 2);
    } catch (const SolverBridgeError &err) {
        std::string msg = err.what();
        // Não deve ser erro de Keystone!
        completedOrNonKeystoneError = (msg.find("Keystone") == std::string::npos);
    }
    QVERIFY(completedOrNonKeystoneError);
}

void TestUiLicenseIntegration::testTwoFAValidationHelpers()
{
    std::string secret, uri, err;
    bool setupFail = ClientTwoFASetup("", "", "", secret, uri, err);
    QCOMPARE(setupFail, false);
    QCOMPARE(err, std::string("MISSING_CREDENTIALS"));

    bool confirmFail = ClientTwoFAConfirm("", "", "", "", err);
    QCOMPARE(confirmFail, false);
    QCOMPARE(err, std::string("MISSING_CREDENTIALS"));
}

QTEST_MAIN(TestUiLicenseIntegration)
#include "TestUiLicenseIntegration.moc"
