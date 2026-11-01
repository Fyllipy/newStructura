#include <QtTest/QtTest>

#include "app/Product.h"
#include "app/services/UserDataMigration.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using Structura::App::Services::UserDataMigration;
namespace Product = Structura::App::Product;

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class TestProductIdentity : public QObject
{
    Q_OBJECT

private slots:
    void identityIsConsistent()
    {
        QCOMPARE(Product::productName(), QStringLiteral("Structura::Metalicas"));
        QVERIFY(Product::displayName().startsWith(Product::productName()));
        QVERIFY(Product::versionDisplay().contains(QStringLiteral("beta")));   // versao atual: 0.8 beta
        QCOMPARE(QString::fromUtf8(Structura::Product::kVersion), QStringLiteral("0.8.0"));

        // cada edicao tem identificador proprio, distinto do antigo: pastas de dados e configuracoes nao se misturam
        QVERIFY(QString::fromUtf8(Structura::Product::kAppId) != QString::fromUtf8(Structura::Product::kLegacyAppId));
        QCOMPARE(QString::fromUtf8(Structura::Product::kLegacyAppId), QStringLiteral("Structura"));
        if (Structura::Product::kIsDemo) {
            QCOMPARE(Product::displayName(), QStringLiteral("Structura::Metalicas Demo"));
            QVERIFY(QString::fromUtf8(Structura::Product::kAppId).endsWith(QStringLiteral("-Demo")));
            QCOMPARE(QString::fromUtf8(Structura::Product::kEdition), QStringLiteral("DEMO"));
        } else {
            QCOMPARE(Product::displayName(), QStringLiteral("Structura::Metalicas"));
            QCOMPARE(QString::fromUtf8(Structura::Product::kEdition), QStringLiteral("COMMERCIAL"));
        }
    }

    void migrationCopiesWithoutOverwritingOrDeletingSource()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString legacy = root.filePath(QStringLiteral("Structura"));
        const QString current = root.filePath(QStringLiteral("Structura-Metalicas"));
        writeFile(legacy + QStringLiteral("/library/profiles.json"), "antigo-perfis");
        writeFile(legacy + QStringLiteral("/library/materials.json"), "antigo-materiais");
        writeFile(current + QStringLiteral("/library/materials.json"), "novo-materiais");   // ja existe: nao pode ser sobrescrito

        const auto result = UserDataMigration::migrateDirectory(legacy, current);
        QVERIFY(result.ran);
        QCOMPARE(result.filesCopied, 1);
        QCOMPARE(result.filesSkipped, 1);
        QCOMPARE(readFile(current + QStringLiteral("/library/profiles.json")), QByteArray("antigo-perfis"));
        QCOMPARE(readFile(current + QStringLiteral("/library/materials.json")), QByteArray("novo-materiais"));
        // a origem permanece intacta
        QCOMPARE(readFile(legacy + QStringLiteral("/library/profiles.json")), QByteArray("antigo-perfis"));
        QCOMPARE(readFile(legacy + QStringLiteral("/library/materials.json")), QByteArray("antigo-materiais"));
        QVERIFY(QFileInfo::exists(current + QLatin1Char('/') + UserDataMigration::markerFileName()));
    }

    void migrationRunsOnlyOnce()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString legacy = root.filePath(QStringLiteral("Structura"));
        const QString current = root.filePath(QStringLiteral("Structura-Metalicas"));
        writeFile(legacy + QStringLiteral("/library/profiles.json"), "v1");
        QVERIFY(UserDataMigration::migrateDirectory(legacy, current).ran);

        // o usuario apaga o arquivo migrado e a origem ganha outro: a segunda execucao nao recopia nada
        QVERIFY(QFile::remove(current + QStringLiteral("/library/profiles.json")));
        writeFile(legacy + QStringLiteral("/library/extra.json"), "v2");
        const auto second = UserDataMigration::migrateDirectory(legacy, current);
        QVERIFY(!second.ran);
        QCOMPARE(second.filesCopied, 0);
        QVERIFY(!QFileInfo::exists(current + QStringLiteral("/library/profiles.json")));
        QVERIFY(!QFileInfo::exists(current + QStringLiteral("/library/extra.json")));
    }

    void migrationWithoutLegacyDirDoesNothing()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto result = UserDataMigration::migrateDirectory(root.filePath(QStringLiteral("nao-existe")), root.filePath(QStringLiteral("destino")));
        QVERIFY(!result.ran);
        QVERIFY(!QFileInfo::exists(root.filePath(QStringLiteral("destino"))));   // nao cria pasta vazia a toa
    }
};

QTEST_APPLESS_MAIN(TestProductIdentity)
#include "TestProductIdentity.moc"
