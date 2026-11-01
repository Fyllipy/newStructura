#include "UserDataMigration.h"

#include "app/ProductInfo.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace Structura::App::Services {

QString UserDataMigration::markerFileName()
{
    return QStringLiteral(".migrated-from-legacy");
}

UserDataMigration::Result UserDataMigration::migrateDirectory(const QString &legacyDir, const QString &currentDir)
{
    Result result;
    const QDir source(legacyDir);
    if (legacyDir.isEmpty() || currentDir.isEmpty() || !source.exists()
        || QFileInfo(legacyDir).canonicalFilePath() == QFileInfo(currentDir).canonicalFilePath()) {
        return result;
    }
    QDir().mkpath(currentDir);
    const QDir target(currentDir);
    if (target.exists(markerFileName())) {
        return result;   // ja migrado: dados novos do usuario nao sao tocados de novo
    }

    result.ran = true;
    QDirIterator it(legacyDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString from = it.next();
        const QString relative = source.relativeFilePath(from);
        const QString to = target.filePath(relative);
        if (QFileInfo::exists(to)) {
            ++result.filesSkipped;
            continue;
        }
        QDir().mkpath(QFileInfo(to).absolutePath());
        if (QFile::copy(from, to)) {
            ++result.filesCopied;
        }
    }

    QFile marker(target.filePath(markerFileName()));
    if (marker.open(QIODevice::WriteOnly)) {
        marker.write(QByteArray("origem: ") + legacyDir.toUtf8() + '\n');
    }
    return result;
}

UserDataMigration::Result UserDataMigration::runIfNeeded()
{
    if (Structura::Product::kIsDemo) {
        return {};
    }
    const QString legacyId = QString::fromUtf8(Structura::Product::kLegacyAppId);
    const QString currentId = QString::fromUtf8(Structura::Product::kAppId);
    if (legacyId == currentId) {
        return {};
    }

    // pasta atual: <Roaming>/<kAppId>; a antiga e a irma <Roaming>/<kLegacyAppId>
    const QString currentDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir parent(currentDir);
    parent.cdUp();
    const QString legacyDir = parent.filePath(legacyId);

    // Configuracoes (QSettings): nada a migrar. Sem nome de organizacao o QSettings() padrao nao grava (AccessError), entao a identidade
    // antiga nunca persistiu configuracoes; so a biblioteca de perfis e materiais (arquivos) existe.
    return migrateDirectory(legacyDir, currentDir);
}

} // namespace Structura::App::Services
