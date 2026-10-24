#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace Structura::UI {

/**
 * @brief Carrega um icone SVG do recurso `:/svg/<name>` e o pinta com `color`.
 *
 * Os SVGs usam `currentColor`; a cor e substituida no texto antes de renderizar.
 * O resultado e rasterizado em 2x para telas de alta densidade.
 */
QIcon svgIcon(const QString &name,
              const QColor &color = QColor(QStringLiteral("#1e232b")),
              int size = 24,
              const QColor &accent = QColor(QStringLiteral("#0d7fb9")));

/// Icone padrao das acoes da interface: tracos escuros (#1e232b) e destaque azul, rasterizado em 64 px.
QIcon actionIcon(const QString &name);

} // namespace Structura::UI
