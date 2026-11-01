#pragma once

#include <QString>

namespace Structura::App::Services {

/**
 * @brief Migra os dados do usuario da identidade antiga ("Structura") para a atual ("Structura-Metalicas").
 *
 * Regras (nunca destrutivas):
 *  - Copia, sem sobrescrever, os arquivos de `%APPDATA%/Structura` (biblioteca de perfis e materiais) para a pasta atual.
 *  - A origem nunca e alterada nem apagada.
 *  - Roda uma unica vez: grava o marcador `.migrated-from-legacy` na pasta de destino.
 *  - So se aplica a edicao Comercial (a Demo comeca com dados proprios e vazios).
 */
class UserDataMigration
{
public:
    struct Result {
        bool ran {false};          ///< a migracao foi tentada nesta chamada
        int filesCopied {0};
        int filesSkipped {0};      ///< ja existiam no destino
    };

    /// Ponto de entrada do programa: usa as pastas e configuracoes reais; no-op na Demo ou se ja migrado.
    static Result runIfNeeded();

    /// Copia recursivamente `legacyDir` -> `currentDir` sem sobrescrever; grava o marcador. Testavel com pastas temporarias.
    static Result migrateDirectory(const QString &legacyDir, const QString &currentDir);

    /// Nome do arquivo marcador gravado no destino.
    static QString markerFileName();
};

} // namespace Structura::App::Services
