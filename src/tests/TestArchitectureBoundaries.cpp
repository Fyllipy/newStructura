#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class TestArchitectureBoundaries : public QObject
{
    Q_OBJECT

private:
    static std::string readAllText(const fs::path& filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }

        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    static std::vector<fs::path> collectSourceFiles(const fs::path& root)
    {
        std::vector<fs::path> files;
        if (!fs::exists(root)) {
            return files;
        }

        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto extension = entry.path().extension().string();
            if (extension == ".h" || extension == ".hpp" || extension == ".cpp") {
                files.push_back(entry.path());
            }
        }

        return files;
    }

    static bool containsAny(const std::string& text, const std::vector<std::string>& patterns)
    {
        for (const auto& pattern : patterns) {
            if (text.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private slots:
    void testUiHeadersDoNotIncludeConcreteSceneController()
    {
        const fs::path sourceRoot = fs::u8path(STRUCTURA_SOURCE_DIR) / "src" / "ui";
        const auto files = collectSourceFiles(sourceRoot);
        QVERIFY2(!files.empty(), "No source files found under src/ui.");

        for (const auto& filePath : files) {
            const auto extension = filePath.extension().string();
            if (extension != ".h" && extension != ".hpp") {
                continue;
            }

            const auto content = readAllText(filePath);
            const bool includesConcreteSceneController =
                content.find("#include \"viz/SceneController.h\"") != std::string::npos;

            QVERIFY2(!includesConcreteSceneController,
                     qPrintable(QString("UI header must not include viz/SceneController.h: %1")
                                    .arg(QString::fromStdString(filePath.string()))));
        }
    }

    void testAppHeadersDoNotIncludeConcreteSceneController()
    {
        const fs::path sourceRoot = fs::u8path(STRUCTURA_SOURCE_DIR) / "src" / "app";
        const auto files = collectSourceFiles(sourceRoot);
        QVERIFY2(!files.empty(), "No source files found under src/app.");

        for (const auto& filePath : files) {
            const auto extension = filePath.extension().string();
            if (extension != ".h" && extension != ".hpp") {
                continue;
            }

            const auto content = readAllText(filePath);
            const bool includesConcreteSceneController =
                content.find("#include \"viz/SceneController.h\"") != std::string::npos;

            QVERIFY2(!includesConcreteSceneController,
                     qPrintable(QString("App header must not include viz/SceneController.h: %1")
                                    .arg(QString::fromStdString(filePath.string()))));
        }
    }

    void testAppHeadersDoNotIncludeUiPresenterTypes()
    {
        const fs::path sourceRoot = fs::u8path(STRUCTURA_SOURCE_DIR) / "src" / "app";
        const auto files = collectSourceFiles(sourceRoot);
        QVERIFY2(!files.empty(), "No source files found under src/app.");

        for (const auto& filePath : files) {
            const auto extension = filePath.extension().string();
            if (extension != ".h" && extension != ".hpp") {
                continue;
            }

            const auto content = readAllText(filePath);
            const bool includesUiPresenterTypes =
                content.find("#include \"ui/MainWindowPresenter.h\"") != std::string::npos;

            QVERIFY2(!includesUiPresenterTypes,
                     qPrintable(QString("App header must not include ui/MainWindowPresenter.h: %1")
                                    .arg(QString::fromStdString(filePath.string()))));
        }
    }

    void testAppHeadersDoNotIncludeVizSceneController()
    {
        const fs::path sourceRoot = fs::u8path(STRUCTURA_SOURCE_DIR) / "src" / "app";
        const auto files = collectSourceFiles(sourceRoot);
        QVERIFY2(!files.empty(), "No source files found under src/app.");

        for (const auto& filePath : files) {
            const auto extension = filePath.extension().string();
            if (extension != ".h" && extension != ".hpp") {
                continue;
            }

            const auto content = readAllText(filePath);
            const bool includesVizSceneController =
                content.find("#include \"viz/SceneController.h\"") != std::string::npos;

            QVERIFY2(!includesVizSceneController,
                     qPrintable(QString("App header must not include viz/SceneController.h: %1")
                                    .arg(QString::fromStdString(filePath.string()))));
        }
    }

    void testCoreModelDoesNotDependOnUiOrApp()
    {
        const fs::path sourceRoot = fs::u8path(STRUCTURA_SOURCE_DIR) / "src" / "core" / "model";
        const auto files = collectSourceFiles(sourceRoot);
        QVERIFY2(!files.empty(), "No source files found under src/core/model.");

        const std::vector<std::string> forbiddenIncludes = {
            "#include \"ui/",
            "#include \"app/",
            "#include \"../ui/",
            "#include \"../app/"
        };

        for (const auto& filePath : files) {
            const auto content = readAllText(filePath);
            const bool hasForbidden = containsAny(content, forbiddenIncludes);

            QVERIFY2(!hasForbidden, qPrintable(QString("Forbidden include dependency found in %1").arg(QString::fromStdString(filePath.string()))));
        }
    }

    void testVizDoesNotDependOnUi()
    {
        const fs::path sourceRoot = fs::u8path(STRUCTURA_SOURCE_DIR) / "src" / "viz";
        const auto files = collectSourceFiles(sourceRoot);
        QVERIFY2(!files.empty(), "No source files found under src/viz.");

        const std::vector<std::string> forbiddenIncludes = {
            "#include \"ui/",
            "#include \"../ui/"
        };

        for (const auto& filePath : files) {
            const auto content = readAllText(filePath);
            const bool hasForbidden = containsAny(content, forbiddenIncludes);

            QVERIFY2(!hasForbidden, qPrintable(QString("Forbidden include dependency found in %1").arg(QString::fromStdString(filePath.string()))));
        }
    }
};

QTEST_APPLESS_MAIN(TestArchitectureBoundaries)
#include "TestArchitectureBoundaries.moc"
