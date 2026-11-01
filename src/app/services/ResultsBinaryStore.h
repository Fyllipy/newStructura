#pragma once

#include "app/ProjectDataTypes.h"

#include <QIODevice>
#include <QString>
#include <QStringList>

namespace Structura::App::Services {

/**
 * @brief Persistencia binaria versionada dos resultados da analise.
 *
 * Formato: "STRR", versao (quint32), impressao digital do modelo, indice de secoes (nome, deslocamento, tamanho, soma de verificacao) e as secoes.
 * Secoes: "run:<caso ou COMB:combinacao>" (uma por hipotese), "design", "buckling" (modos de flambagem) e "modal" (modos de vibracao).
 * O indice permite ler uma hipotese sem carregar as demais (`readRun`). A versao e checada na leitura; versoes futuras sao recusadas com mensagem.
 */
class ResultsBinaryStore
{
public:
    static constexpr quint32 kVersion = 1;

    static bool write(QIODevice &device, const Structura::App::ProjectData::StoredResults &results, QString *error = nullptr);
    static bool read(QIODevice &device, Structura::App::ProjectData::StoredResults &results, QString *error = nullptr);

    /// Nomes das secoes (e a impressao digital do modelo, em `fingerprint`).
    static QStringList sections(QIODevice &device, QString *fingerprint = nullptr, QString *error = nullptr);
    /// Le so a hipotese `name` (nome do StoredRun).
    static bool readRun(QIODevice &device, const QString &name, Structura::App::ProjectData::StoredRun &run, QString *error = nullptr);
};

} // namespace Structura::App::Services
