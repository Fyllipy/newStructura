#pragma once

#include "app/ProductInfo.h"   // gerado por cmake/ProductInfo.cmake (fonte unica da identidade)

#include <QString>

namespace Structura::App::Product {

/// Nome da edicao exibido ao usuario: "Structura::Metalicas" ou "Structura::Metalicas Demo".
inline QString displayName() { return QString::fromUtf8(Structura::Product::kDisplayName); }

/// Nome comercial do produto, sem o sufixo da edicao.
inline QString productName() { return QString::fromUtf8(Structura::Product::kProductName); }

/// Versao para o usuario (ex.: "0.8 beta").
inline QString versionDisplay() { return QString::fromUtf8(Structura::Product::kVersionDisplay); }

/// "Structura::Metalicas 0.8 beta" (edicao demonstrativa inclui o " Demo").
inline QString displayNameWithVersion() { return displayName() + QLatin1Char(' ') + versionDisplay(); }

/// Texto usado como "criador/autor" em arquivos exportados (PDF, DOCX, IFC).
inline QString creatorTag() { return displayNameWithVersion(); }

} // namespace Structura::App::Product
