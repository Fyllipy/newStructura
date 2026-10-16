#pragma once

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"

#include <QString>
#include <QVector>

#include <vector>

namespace Structura::App::Services {

/**
 * @brief Combinacoes lineares de resultados por caso de carga.
 *
 * O solver e linear: cada caso e resolvido separadamente e as combinacoes
 * sao a soma dos resultados (deslocamentos, reacoes e esforcos nas barras)
 * ponderados pelos fatores. O fluxo de dados para o solver nao muda.
 */
class CombinationService
{
public:
    struct Term {
        const Structura::App::SolverBridge::Result *result {nullptr};
        double factor {1.0};
    };

    /// Soma ponderada de resultados obtidos sobre o mesmo modelo geometrico.
    static Structura::App::SolverBridge::Result combine(const std::vector<Term> &terms);

    /// Modelo unico com as cargas dos casos ponderadas pelos fatores (para analises nao lineares, que nao admitem superposicao).
    static Structura::App::SolverBridge::ModelData
    mergeModels(const std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> &terms);

    /// Combinacoes definidas; se nao houver, uma combinacao implicita "Combo 1" com todos os casos (fator 1).
    static QVector<Structura::App::ProjectData::LoadCombination>
    effectiveCombinations(const Structura::App::ProjectData::ProjectExtras &extras);

    /// Casos definidos; se nao houver, o caso padrao.
    static QVector<Structura::App::ProjectData::LoadCase>
    effectiveCases(const Structura::App::ProjectData::ProjectExtras &extras);
};

} // namespace Structura::App::Services
