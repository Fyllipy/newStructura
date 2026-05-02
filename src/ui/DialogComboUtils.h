#pragma once

#include <QComboBox>
#include <QPair>
#include <QVector>
#include <QVariant>
#include <QUuid>

namespace Structura::UI {

inline void populateUuidCombo(QComboBox *combo,
                              const QVector<QPair<QUuid, QString>> &options,
                              const QString &emptyLabel)
{
    combo->clear();
    combo->addItem(emptyLabel, QVariant());
    for (const auto &option : options) {
        combo->addItem(option.second, option.first);
    }
}

inline int indexForUuid(const QVector<QPair<QUuid, QString>> &options, const QUuid &id)
{
    if (id.isNull()) {
        return 0;
    }
    for (int i = 0; i < options.size(); ++i) {
        if (options[i].first == id) {
            return i + 1;
        }
    }
    return 0;
}

} // namespace Structura::UI
