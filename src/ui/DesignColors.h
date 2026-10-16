#pragma once

#include <QColor>

namespace Structura::UI {

/// Cor do aproveitamento (eta): verde ate 0,7; amarelo ate 0,9; laranja ate 1,0; vermelho acima.
inline QColor designEtaColor(double eta)
{
    if (eta > 1.0) {
        return QColor(QStringLiteral("#c0392b"));
    }
    if (eta > 0.9) {
        return QColor(QStringLiteral("#e67e22"));
    }
    if (eta > 0.7) {
        return QColor(QStringLiteral("#d4ac0d"));
    }
    return QColor(QStringLiteral("#27ae60"));
}

} // namespace Structura::UI
