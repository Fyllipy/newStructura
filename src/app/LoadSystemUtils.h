#pragma once

#include <QString>

namespace Structura::App {

inline QString normalizeMemberLoadSystem(const QString &system,
                                         const QString &fallback = QStringLiteral("GLOBAL"))
{
    QString value = system.trimmed();
    if (value.isEmpty()) {
        value = fallback.trimmed();
    }

    if (value.compare(QStringLiteral("LOCAL"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("L"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("LOCAL");
    }

    return QStringLiteral("GLOBAL");
}

inline bool isLocalMemberLoadSystem(const QString &system)
{
    return normalizeMemberLoadSystem(system) == QLatin1String("LOCAL");
}

} // namespace Structura::App
