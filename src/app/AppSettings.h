#pragma once

#include "app/ProductInfo.h"

#include <QSettings>
#include <QString>

namespace Structura::App {

/// Configuracoes do usuario desta edicao (opcoes visuais etc.). Usa organizacao e aplicacao proprias: o QSettings() padrao, sem organizacao,
/// nao grava nada.
inline QSettings appSettings()
{
    const QString id = QString::fromUtf8(Structura::Product::kAppId);
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, id, id);
}

namespace Options {
inline constexpr const char *kNodeScreenSize = "visual/nodeScreenSize";   ///< raio do no na tela, em pixels
inline constexpr double kNodeScreenSizeDefault = 5.0;
inline constexpr double kNodeScreenSizeMin = 2.0;
inline constexpr double kNodeScreenSizeMax = 16.0;
} // namespace Options

} // namespace Structura::App
