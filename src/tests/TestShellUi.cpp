#include "shell/ShellFactory.h"
#include "shell/ShellMesh.h"
#include "ui/ProfilePropertiesDialog.h"
#include "viz/structure/SectionShapes.h"
#include "ui/ShellDialog.h"
#include "viz/structure/ShellRenderer.h"

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkWindowToImageFilter.h>

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>

using namespace Structura;
using namespace Structura::Shell;

class TestShellUi : public QObject
{
    Q_OBJECT

private:
    static QPushButton *button(QWidget *root, const QString &text)
    {
        for (auto *b : root->findChildren<QPushButton *>()) {
            if (b->text() == text) {
                return b;
            }
        }
        return nullptr;
    }

private slots:

    void drawnAssemblyRendersWithSelectionHighlight()
    {
        const QByteArray shots = qgetenv("STRUCTURA_SHOT_DIR");
        if (shots.isEmpty()) {
            QSKIP("defina STRUCTURA_SHOT_DIR para gerar a captura");
        }
        Model model;
        const double R = 3.0, H = 4.0, rise = 1.0;
        auto bottom = makeDisk({0, 0, 0}, {R, 0, 0}, {0, 1, 2});
        auto wall = makeCylinder({0, 0, 0}, R, {0, 0, H});
        const double rs = (R * R + rise * rise) / (2.0 * rise), zc = H + rise - rs, phi0 = std::asin(R / rs);
        std::vector<Vec3> profile;
        for (int i = 0; i <= 24; ++i) {
            const double phi = phi0 * (1.0 - static_cast<double>(i) / 24.0);
            profile.push_back({rs * std::sin(phi), 0.0, zc + rs * std::cos(phi)});
        }
        auto dome = makeRevolution(profile, {0, 0, 0}, {0, 0, 1});
        auto slab = makeRectangle({4.5, -2, 0}, {9, 2, 0}, {0, 1, 2});
        int id = 1;
        for (Object *o : {&*bottom, &*wall, &*dome, &*slab}) {
            o->id = id++;
            o->name = "P" + std::to_string(o->id);
            o->meshSize = 0.5;
            o->p["t"] = 0.01;
        }
        bottom->p["flip"] = 1.0;
        slab->material = 0;
        model.objects = {*bottom, *wall, *dome, *slab};
        Mesh mesh = generateMesh(model);
        RenderOptions options;
        options.highlightObject = 1;   // o costado selecionado
        const RenderMesh render = buildRenderMesh(mesh, model, nullptr, options);
        Structura::Visualization::ShellRenderer renderer;
        vtkNew<vtkRenderer> ren;
        ren->SetBackground(1, 1, 1);
        renderer.attach(ren);
        renderer.setMesh(render);
        renderer.setVisible(true);
        ren->GetActiveCamera()->SetPosition(16, -14, 10);
        ren->GetActiveCamera()->SetFocalPoint(3, 0, 2);
        ren->GetActiveCamera()->SetViewUp(0, 0, 1);
        ren->ResetCamera();
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetSize(900, 700);
        window->AddRenderer(ren);
        window->Render();
        // seleciona pelo pixel central: o costado fica no centro da imagem
        vtkNew<vtkWindowToImageFilter> grab;
        grab->SetInput(window);
        grab->Update();
        vtkNew<vtkPNGWriter> writer;
        writer->SetFileName((shots + "/shell_drawn.png").constData());
        writer->SetInputConnection(grab->GetOutputPort());
        writer->Write();
        QVERIFY(!mesh.quads.empty());
    }

    void gapBetweenCompositeProfilesChangesSectionProperties()
    {
        using Structura::Visualization::SectionShapes;
        auto params = [](double gap) {
            SectionShapes::Parameters p;
            for (const auto &parameter : SectionShapes::findFamily(QStringLiteral("Duplo U Costas"))->parameters) {
                p.insert(parameter.key, parameter.defaultValue);
            }
            p.insert(QStringLiteral("e"), gap);
            return p;
        };
        const auto narrow = SectionShapes::properties(QStringLiteral("Duplo U Costas"), params(10.0));
        const auto wide = SectionShapes::properties(QStringLiteral("Duplo U Costas"), params(60.0));
        QVERIFY(narrow && wide);
        QVERIFY(std::abs(narrow->area - wide->area) < 1e-12);       // a area nao depende do afastamento
        QVERIFY(wide->iy > narrow->iy * 1.05);                      // a inercia em torno do eixo vertical cresce (Steiner)
        QVERIFY(std::abs(wide->iz - narrow->iz) / narrow->iz < 1e-9);
    }

    void profilePropertiesDialogListsGeometricAndEngineeringValues()
    {
        for (const QString &family : {QStringLiteral("W Laminado"), QStringLiteral("Duplo U Costas")}) {
            Structura::App::ProjectData::SectionInfo section;
            section.name = family;
            section.family = family;
            const auto *spec = Structura::Visualization::SectionShapes::findFamily(family);
            QVERIFY(spec);
            for (const auto &parameter : spec->parameters) {
                section.geometry.insert(parameter.key, parameter.defaultValue);
            }
            section.area = 0.003;
            section.iz = 1e-5;
            section.iy = 1e-6;
            section.j = 1e-8;
            Structura::UI::ProfilePropertiesDialog dialog(section);
            const auto *table = dialog.findChild<QTableWidget *>();
            QVERIFY(table);
            QStringList names;
            for (int r = 0; r < table->rowCount(); ++r) {
                names << table->item(r, 0)->text();
            }
            QVERIFY(table->rowCount() > 20);
            QVERIFY(names.contains(QStringLiteral("Módulo plástico Zx")));
            QVERIFY(names.contains(QStringLiteral("Constante de empenamento Cw (coordenada setorial)")) || family != QLatin1String("W Laminado"));
            if (family == QLatin1String("Duplo U Costas")) {
                QVERIFY(names.contains(QStringLiteral("Afastamento entre os perfis")));
            }
        }
    }

    void tankAnalysisAndDesignThroughTheDialog()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/mefs.dll");
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll nao encontrada na raiz do repositorio.");
        }
        Model model;
        Object tank;
        tank.id = 1;
        tank.name = "Tanque";
        tank.kind = Kind::CylTank;
        tank.meshSize = 0.8;
        tank.p = {{"r", 3.0}, {"h", 4.0}, {"courses", 2.0}, {"t1", 0.010}, {"t2", 0.008}, {"tBottom", 0.008}, {"roof", 0.0}};
        model.objects.push_back(tank);
        Load water;
        water.caseName = "AGUA";
        water.type = Load::Hydrostatic;
        water.v = {9810.0, 3.5, 0.0, 0.0};
        model.loads.push_back(water);
        Support soil;
        soil.springPerArea = 5.0e7;
        soil.compressionOnly = true;
        soil.value = 0.0;
        model.supports.push_back(soil);
        for (int k = 0; k < 2; ++k) {
            Support p;
            p.selector = Support::Point;
            p.value = k == 0 ? 3.0 : -3.0;
            p.fixed = k == 0 ? std::array<int, 6> {1, 1, 0, 0, 0, 0} : std::array<int, 6> {0, 1, 0, 0, 0, 0};
            model.supports.push_back(p);
        }
        int renders = 0;
        RenderMesh last;
        bool valued = false;
        std::size_t triangles = 0;
        UI::ShellDialog::Callbacks callbacks;
        callbacks.caseNames = []() { return QStringList {QStringLiteral("AGUA")}; };
        callbacks.combinations = []() {
            App::ProjectData::LoadCombination combo;
            combo.name = QStringLiteral("ELU1");
            combo.factors = {{QStringLiteral("AGUA"), 1.4}};
            return QVector<App::ProjectData::LoadCombination> {combo};
        };
        callbacks.render = [&](const RenderMesh &mesh) {
            ++renders;
            valued = mesh.hasValues;
            triangles = mesh.triangles.size();
            last = mesh;
        };
        callbacks.unitsPerMeter = []() { return 1.0; };
        UI::ShellDialog dialog(model, callbacks, dll);
        dialog.show();
        auto *mesh = button(&dialog, QStringLiteral("Gerar malha"));
        auto *run = button(&dialog, QStringLiteral("Analisar"));
        QVERIFY(mesh && run);
        mesh->click();
        QVERIFY(renders >= 1);
        QVERIFY(triangles > 100);
        QVERIFY(!valued);   // so a malha
        run->click();
        QVERIFY(valued);
        QVERIFY(renders >= 2);
        // verificacao
        auto *verify = button(&dialog, QStringLiteral("Verificar"));
        QVERIFY(verify);
        verify->click();
        QTableWidget *table = nullptr;
        for (auto *t : dialog.findChildren<QTableWidget *>()) {
            if (t->columnCount() == 8 && t->horizontalHeaderItem(0) && t->horizontalHeaderItem(0)->text() == QStringLiteral("Zona")) {
                table = t;
            }
        }
        QVERIFY(table != nullptr);
        QVERIFY(table->rowCount() >= 3);   // von Mises por zona (fundo + 2 virolas) e a flecha nao entra sem servico
        QVERIFY(dialog.model().objects.size() == 1);
        const QByteArray shots = qgetenv("STRUCTURA_SHOT_DIR");
        if (!shots.isEmpty()) {
            Structura::Visualization::ShellRenderer renderer;
            vtkNew<vtkRenderer> ren;
            ren->SetBackground(1, 1, 1);
            renderer.attach(ren);
            renderer.setMesh(last);
            renderer.setVisible(true);
            ren->GetActiveCamera()->SetPosition(14, -12, 9);
            ren->GetActiveCamera()->SetFocalPoint(0, 0, 2);
            ren->GetActiveCamera()->SetViewUp(0, 0, 1);
            ren->ResetCamera();
            vtkNew<vtkRenderWindow> window;
            window->SetOffScreenRendering(1);
            window->SetSize(900, 700);
            window->AddRenderer(ren);
            window->Render();
            vtkNew<vtkWindowToImageFilter> grab;
            grab->SetInput(window);
            grab->Update();
            vtkNew<vtkPNGWriter> writer;
            writer->SetFileName((shots + "/shell_3d.png").constData());
            writer->SetInputConnection(grab->GetOutputPort());
            writer->Write();
            // capturas das abas para conferencia visual (so quando a variavel de ambiente existe)
            auto *tabs = dialog.findChild<QTabWidget *>();
            for (int i = 0; tabs && i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                QApplication::processEvents();
                dialog.grab().save(QString::fromLocal8Bit(shots) + QStringLiteral("/shell_tab%1.png").arg(i));
            }
        }
    }
};

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf")) >= 0) {
        app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    }
    TestShellUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestShellUi.moc"
