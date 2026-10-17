#include "ui/BusyProgress.h"
#include "ui/ConnectionPreview.h"
#include "ui/ProfileBrowser.h"
#include "ui/ProfileEditorWidget.h"
#include "ui/ProfileLibrary.h"
#include "ui/ProfileManagerDialog.h"
#include "ui/ProfilePickerDialog.h"
#include "ui/SectionPlacementPanel.h"
#include "ui/SectionPlacementWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFontDatabase>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QTimer>

using namespace Structura::UI;
using Category = ProfileLibrary::Category;
using SectionInfo = Structura::App::ProjectData::SectionInfo;

class TestProfileUi : public QObject
{
    Q_OBJECT

private:
    static QString outputDir()
    {
        const QString dir = QDir::tempPath() + QStringLiteral("/structura_profile_ui");
        QDir().mkpath(dir);
        return dir;
    }

private slots:
    void connectionExamplesDrawSomethingForEveryType()
    {
        const QString dir = qEnvironmentVariable("STRUCTURA_PREVIEW_DIR");
        for (int type = 0; type <= 5; ++type) {
            const QPixmap pixmap = Structura::UI::ConnectionPreview::example(type, QSize(300, 200));
            QVERIFY(!pixmap.isNull());
            const QImage image = pixmap.toImage();
            int different = 0;
            for (int y = 0; y < image.height(); y += 4) {
                for (int x = 0; x < image.width(); x += 4) {
                    different += image.pixelColor(x, y) != image.pixelColor(0, 0) ? 1 : 0;
                }
            }
            QVERIFY2(different > 40, qPrintable(QString::number(type)));
            if (!dir.isEmpty()) {
                pixmap.save(QStringLiteral("%1/conn%2.png").arg(dir).arg(type));
            }
        }
    }

    void busyRunsJobInBackgroundAndPropagates()
    {
        using Structura::UI::runBusy;
        QWidget owner;
        owner.show();
        // valor devolvido e progresso informado
        const int value = runBusy(&owner, QStringLiteral("Teste"), QStringLiteral("detalhe"), [](Structura::UI::BusyProgress &progress) {
            progress.report(1, 2, QStringLiteral("metade"));
            return 42;
        });
        QCOMPARE(value, 42);
        QVERIFY(!Structura::UI::isBusy());
        // tarefa longa: a janela aparece e a interface continua processando eventos
        int ticks = 0;
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, [&ticks]() { ++ticks; });
        timer.start(10);
        runBusy(&owner, QStringLiteral("Teste longo"), QString(), []() { QThread::msleep(500); });
        QVERIFY(ticks > 5);
        // excecoes voltam para a thread da interface
        bool caught = false;
        try {
            runBusy(&owner, QStringLiteral("Falha"), QString(), []() -> int { throw std::runtime_error("erro de teste"); });
        } catch (const std::runtime_error &error) {
            caught = QString::fromUtf8(error.what()) == QLatin1String("erro de teste");
        }
        QVERIFY(caught);
        QVERIFY(!Structura::UI::isBusy());
        // chamada aninhada executa direto
        const int nested = runBusy(&owner, QStringLiteral("Externo"), QString(), []() { return 1; });
        QCOMPARE(nested, 1);
    }

    void categoriesSeparateRolledAndColdFormed()
    {
        QCOMPARE(ProfileLibrary::categoryOf(QStringLiteral("W Laminado")), Category::Laminado);
        QCOMPARE(ProfileLibrary::categoryOf(QStringLiteral("U Inclinado Laminado")), Category::Laminado);
        QCOMPARE(ProfileLibrary::categoryOf(QStringLiteral("C Formado")), Category::MoldadoAFrio);
        QCOMPARE(ProfileLibrary::categoryOf(QStringLiteral("Tubular Retangular")), Category::MoldadoAFrio);
        QCOMPARE(ProfileLibrary::categoryOf(QStringLiteral("Retangular")), Category::Concreto);
        QCOMPARE(ProfileLibrary::categoryOf(QString()), Category::SemGeometria);
        QCOMPARE(ProfileLibrary::typeLabel(QStringLiteral("W Laminado")), QStringLiteral("W"));
        QCOMPARE(ProfileLibrary::typeLabel(QStringLiteral("Cartola Formado")), QStringLiteral("Cartola"));
        QVERIFY(!ProfileLibrary::familiesIn(Category::Laminado).isEmpty());
        QVERIFY(!ProfileLibrary::familiesIn(Category::MoldadoAFrio).isEmpty());
        // Toda familia pertence a exatamente uma categoria
        int total = 0;
        for (Category category : ProfileLibrary::categories()) {
            total += ProfileLibrary::familiesIn(category).size();
        }
        QCOMPARE(total, Structura::Visualization::SectionShapes::families().size());
    }

    void browserFiltersByCategoryTypeAndSearch()
    {
        ProfileBrowser browser;
        browser.resize(420, 520);
        browser.show();
        const auto *wEntry = &ProfileLibrary::entries().first();
        browser.setCurrent(wEntry->id);
        QCOMPARE(browser.currentId(), wEntry->id);
        QVERIFY(browser.currentSection().has_value());

        const auto chips = browser.findChildren<QPushButton *>();
        QPushButton *formed = nullptr;
        for (auto *button : chips) {
            if (button->text().startsWith(ProfileLibrary::categoryName(Category::MoldadoAFrio))) {
                formed = button;
            }
        }
        QVERIFY(formed);
        formed->click();
        // O perfil laminado selecionado some da lista filtrada
        QVERIFY(browser.currentId().isNull() || browser.currentId() != wEntry->id);
        browser.grab().save(outputDir() + QStringLiteral("/browser_cold_formed.png"));
    }

    void projectSectionsAreMergedWithLibrary()
    {
        const auto &entry = ProfileLibrary::entries().first();
        SectionInfo same = ProfileLibrary::makeSection(entry);
        same.uuid = QUuid::createUuid(); // equivalente carregado de arquivo, com outro id
        SectionInfo custom;
        custom.uuid = QUuid::createUuid();
        custom.name = QStringLiteral("Viga especial");
        custom.area = 0.01;
        custom.iz = custom.iy = custom.j = 1e-4;

        ProfileBrowser browser;
        browser.setProjectSections({same, custom});
        browser.setCurrent(same.uuid);
        QVERIFY(browser.currentInProject());
        QCOMPARE(browser.currentId(), entry.id);
        browser.setCurrent(custom.uuid);
        QCOMPARE(browser.currentId(), custom.uuid);
        QCOMPARE(browser.currentSection()->name, QStringLiteral("Viga especial"));
    }

    void editorCreatesValidatedProfile()
    {
        ProfileEditorWidget editor;
        editor.resize(820, 520);
        editor.setExistingNames({QStringLiteral("Existente")});
        editor.setSelectedBarCount(2);
        editor.show();
        QSignalSpy spy(&editor, &ProfileEditorWidget::createRequested);

        auto *create = [&]() -> QPushButton * {
            for (auto *button : editor.findChildren<QPushButton *>()) {
                if (button->text() == QStringLiteral("Criar perfil")) {
                    return button;
                }
            }
            return nullptr;
        }();
        QVERIFY(create);
        QVERIFY(create->isEnabled());
        editor.grab().save(outputDir() + QStringLiteral("/editor_default.png"));
        create->click();
        QCOMPARE(spy.count(), 1);
        const auto info = spy.at(0).at(0).value<SectionInfo>();
        QVERIFY(!info.family.isEmpty());
        QVERIFY(info.area > 0.0);
        QVERIFY(info.iz > 0.0);
        QVERIFY(!info.name.isEmpty());
        QVERIFY(!info.geometry.isEmpty());
    }

    void editorBlocksDuplicateNames()
    {
        ProfileEditorWidget editor;
        editor.show();
        QPushButton *create = nullptr;
        for (auto *button : editor.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("Criar perfil")) {
                create = button;
            }
        }
        QVERIFY(create);
        QVERIFY(create->isEnabled());
        QLineEdit *name = nullptr;
        for (auto *edit : editor.findChildren<QLineEdit *>()) {
            if (edit->placeholderText() == QStringLiteral("Nome do perfil")) {
                name = edit;
            }
        }
        QVERIFY(name);
        editor.setExistingNames({name->text()});
        QVERIFY(!create->isEnabled());
    }

    void editorDirectPropertiesMode()
    {
        ProfileEditorWidget editor;
        editor.resize(820, 520);
        editor.show();
        QPushButton *direct = nullptr;
        for (auto *button : editor.findChildren<QPushButton *>()) {
            if (button->text() == ProfileLibrary::categoryName(Category::SemGeometria)) {
                direct = button;
            }
        }
        QVERIFY(direct);
        direct->click();
        QSignalSpy spy(&editor, &ProfileEditorWidget::createRequested);
        for (auto *button : editor.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("Criar perfil")) {
                QVERIFY(button->isEnabled());
                button->click();
            }
        }
        QCOMPARE(spy.count(), 1);
        const auto info = spy.at(0).at(0).value<SectionInfo>();
        QVERIFY(info.family.isEmpty());
        QVERIFY(info.geometry.isEmpty());
        QVERIFY(qFuzzyCompare(info.area, 50.0e-4));
        editor.grab().save(outputDir() + QStringLiteral("/editor_direct.png"));
    }

    void managerAndPickerRender()
    {
        ProfileManagerDialog manager;
        manager.setProjectSections({});
        manager.setSelectedBarCount(3);
        manager.show();
        manager.grab().save(outputDir() + QStringLiteral("/manager_browse.png"));
        manager.showCreatePage();
        manager.grab().save(outputDir() + QStringLiteral("/manager_create.png"));

        ProfilePickerDialog picker({}, ProfileLibrary::entries().at(5).id, true);
        picker.show();
        picker.grab().save(outputDir() + QStringLiteral("/picker.png"));

        SectionPickerButton button;
        button.setCurrent(ProfileLibrary::entries().at(5).id);
        QVERIFY(button.text().contains(ProfileLibrary::entries().at(5).name));
        button.setMixed();
        QVERIFY(button.text().contains(QStringLiteral("variadas")));
    }
    void placementPanelEditsAndRenders()
    {
        SectionPlacementPanel panel(nullptr);
        SectionInfo section;
        section.name = QStringLiteral("W 310 x 32.7");
        section.family = QStringLiteral("W Laminado");
        section.geometry = {{QStringLiteral("d"), 313.0}, {QStringLiteral("bf"), 102.0}, {QStringLiteral("tw"), 6.6},
                            {QStringLiteral("tf"), 10.8}, {QStringLiteral("r"), 10.0}};
        section.iy = 1.0e-6;
        section.iz = 6.5e-5;
        SectionPlacementPanel::Target target;
        target.barId = QUuid::createUuid();
        target.externalId = 7;
        target.section = section;
        target.hasSection = true;
        target.placement.memberId = 7;
        target.placement.rotationDeg = 30.0;
        target.placement.reference = 7;
        target.placement.offsetB = 0.02;
        panel.setTargets({target});
        panel.show();
        QSignalSpy spy(&panel, &SectionPlacementPanel::placementEdited);

        auto *rotation = panel.findChildren<QDoubleSpinBox *>().first();
        rotation->setValue(45.0);
        QCOMPARE(spy.count(), 1);
        const auto patch = spy.takeFirst().at(1).value<Structura::App::ProjectData::MemberPlacementPatch>();
        QVERIFY(patch.rotationDeg.has_value());
        QCOMPARE(*patch.rotationDeg, 45.0);

        QVERIFY(panel.grab().save(outputDir() + QStringLiteral("/placement_panel.png")));
        SectionPlacementWidget mini;
        mini.setCompact(true);
        mini.setInteractive(false);
        mini.setSection(section);
        mini.setPlacement(90.0, 3, 0.0, 0.0);
        mini.resize(190, 150);
        QVERIFY(mini.grab().save(outputDir() + QStringLiteral("/placement_mini.png")));
    }
};

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qRegisterMetaType<SectionInfo>("SectionInfo");
    qRegisterMetaType<Structura::App::ProjectData::MemberPlacementPatch>("Structura::App::ProjectData::MemberPlacementPatch");
    QApplication app(argc, argv);
    // O plugin offscreen nao traz fontes: carrega uma do sistema para as capturas terem texto legivel.
    if (QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf")) >= 0) {
        app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    }
    TestProfileUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestProfileUi.moc"
