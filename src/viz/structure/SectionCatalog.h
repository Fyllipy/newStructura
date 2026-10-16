#pragma once

#include "viz/structure/SectionShapes.h"

#include <QString>
#include <QVector>

namespace Structura::Visualization {

/// Perfil comercial do catalogo (dimensoes em mm).
struct CatalogProfile {
    QString name;
    SectionShapes::Parameters parameters;
};

/// Catalogo de perfis de aco laminados (W, I, U, L e T), carregado do recurso `:/data/catalog_steel.json`.
class SectionCatalog
{
public:
    /// Perfis da familia, na ordem do catalogo (vazio se a familia nao tem catalogo).
    static const QVector<CatalogProfile> &profiles(const QString &family);
    static bool hasCatalog(const QString &family) { return !profiles(family).isEmpty(); }
};

} // namespace Structura::Visualization
