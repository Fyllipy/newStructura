#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Hipoteses de carga condicionais: expande uma combinacao-base em varias combinacoes.
 *
 * Tipos:
 *  - Optional: o caso age ou nao age (a combinacao sai com o fator do caso e sem ele).
 *  - Exclusive: exatamente um caso do grupo age por vez (posicoes de carga movel, direcoes de vento, sentidos de sismo).
 *  - Alternating: qualquer subconjunto nao vazio do grupo age (sobrecarga alternada por vao ou por pavimento, em tabuleiro de xadrez).
 *
 * Os casos de cada grupo devem existir em `factors` da combinacao-base (o fator informado e o que cada caso recebe quando age).
 * O numero de combinacoes e limitado por `maxCombinations` para evitar explosao combinatoria.
 */
class LoadHypothesisService
{
public:
    enum class Kind { Optional, Exclusive, Alternating };

    struct Hypothesis {
        QString name;
        Kind kind {Kind::Optional};
        QStringList cases;
    };

    struct Output {
        QVector<Structura::App::ProjectData::LoadCombination> combinations;
        bool truncated {false};
        QStringList notes;
    };

    static Output expand(const Structura::App::ProjectData::LoadCombination &base, const QVector<Hypothesis> &hypotheses, int maxCombinations = 500);

    /// Aplica a expansao a todas as combinacoes (as que nao citam casos de hipotese passam inalteradas).
    static Output expandAll(const QVector<Structura::App::ProjectData::LoadCombination> &bases, const QVector<Hypothesis> &hypotheses, int maxCombinations = 2000);
};

} // namespace Structura::App::Services
