#pragma once

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"

#include <vector>

namespace Structura::App::Services {

/**
 * @brief Converte resultados do solver de/para a forma gravada no .dat e valida se ainda correspondem ao modelo.
 */
class ResultsPersistence
{
public:
    /// Impressao digital (SHA-1) dos modelos de cada caso e das opcoes que alteram a analise.
    static QString fingerprint(const std::vector<const Structura::App::SolverBridge::ModelData *> &models, bool secondOrder,
                               double stiffnessReduction, double imperfectionRatio = 0.0);

    static Structura::App::ProjectData::StoredRun toStored(const QString &name, const Structura::App::SolverBridge::Result &result);

    /// Reconstroi o resultado na ordem de nos/barras de `model`; `ok` = false se faltar algum no ou barra.
    static Structura::App::SolverBridge::Result fromStored(const Structura::App::ProjectData::StoredRun &run,
                                                           const Structura::App::SolverBridge::ModelData &model, bool *ok);
};

} // namespace Structura::App::Services
