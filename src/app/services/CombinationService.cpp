#include "CombinationService.h"

namespace Structura::App::Services {

Structura::App::SolverBridge::Result CombinationService::combine(const std::vector<Term> &terms)
{
    using Result = Structura::App::SolverBridge::Result;
    Result combined;
    bool initialized = false;
    bool shellInitialized = false;

    for (const Term &term : terms) {
        if (!term.result) {
            continue;
        }
        const Result &source = *term.result;
        if (!initialized) {
            // Estrutura (ids, ordem, vetor auxiliar K) vem do primeiro termo; valores zerados.
            combined = source;
            for (auto &node : combined.nodeResults) {
                node.displacements.fill(0.0);
                node.reactions.fill(0.0);
            }
            for (auto &member : combined.memberResults) {
                member.localForces.fill(0.0);
            }
            initialized = true;
        }
        if (source.nodeResults.size() != combined.nodeResults.size()
            || source.memberResults.size() != combined.memberResults.size()) {
            continue;
        }
        for (std::size_t i = 0; i < source.nodeResults.size(); ++i) {
            for (std::size_t dof = 0; dof < 6; ++dof) {
                combined.nodeResults[i].displacements[dof] += term.factor * source.nodeResults[i].displacements[dof];
                combined.nodeResults[i].reactions[dof] += term.factor * source.nodeResults[i].reactions[dof];
            }
        }
        for (std::size_t i = 0; i < source.memberResults.size(); ++i) {
            for (std::size_t k = 0; k < 12; ++k) {
                combined.memberResults[i].localForces[k] += term.factor * source.memberResults[i].localForces[k];
            }
        }
        // cascas acopladas: nos proprios e resultantes dos elementos
        if (!shellInitialized) {
            shellInitialized = true;
            for (auto &node : combined.shellNodeResults) {
                node.displacements.fill(0.0);
                node.reactions.fill(0.0);
            }
            for (auto &element : combined.shellResults) {
                element.values.fill(0.0);
            }
        }
        if (source.shellNodeResults.size() == combined.shellNodeResults.size() && source.shellResults.size() == combined.shellResults.size()) {
            for (std::size_t i = 0; i < source.shellNodeResults.size(); ++i) {
                for (std::size_t dof = 0; dof < 6; ++dof) {
                    combined.shellNodeResults[i].displacements[dof] += term.factor * source.shellNodeResults[i].displacements[dof];
                    combined.shellNodeResults[i].reactions[dof] += term.factor * source.shellNodeResults[i].reactions[dof];
                }
            }
            for (std::size_t i = 0; i < source.shellResults.size(); ++i) {
                for (std::size_t k = 0; k < 40; ++k) {
                    combined.shellResults[i].values[k] += term.factor * source.shellResults[i].values[k];
                }
            }
        }
    }
    return combined;
}

QVector<Structura::App::ProjectData::LoadCase>
CombinationService::effectiveCases(const Structura::App::ProjectData::ProjectExtras &extras)
{
    if (!extras.loadCases.isEmpty()) {
        return extras.loadCases;
    }
    return {Structura::App::ProjectData::LoadCase{Structura::App::ProjectData::defaultLoadCaseName(),
                                                  Structura::App::ProjectData::defaultLoadCaseName(), false}};
}

Structura::App::SolverBridge::ModelData CombinationService::mergeModels(
    const std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> &terms)
{
    Structura::App::SolverBridge::ModelData merged;
    bool first = true;
    for (const auto &term : terms) {
        if (!term.first) {
            continue;
        }
        if (first) {
            merged = *term.first;
            merged.nodalLoads.clear();
            merged.memberLoads.clear();
            merged.memberMoments.clear();
            merged.shellLoads.clear();
            first = false;
        }
        for (auto load : term.first->nodalLoads) {
            for (double &v : load.values) {
                v *= term.second;
            }
            merged.nodalLoads.push_back(load);
        }
        for (auto load : term.first->memberLoads) {
            load.qx *= term.second;
            load.qy *= term.second;
            load.qz *= term.second;
            merged.memberLoads.push_back(load);
        }
        for (auto moment : term.first->memberMoments) {
            moment.mx *= term.second;
            moment.my *= term.second;
            moment.mz *= term.second;
            merged.memberMoments.push_back(moment);
        }
        for (auto load : term.first->shellLoads) {
            for (double &v : load.v) {
                v *= term.second;
            }
            merged.shellLoads.push_back(load);
        }
    }
    return merged;
}

QVector<Structura::App::ProjectData::LoadCombination>
CombinationService::effectiveCombinations(const Structura::App::ProjectData::ProjectExtras &extras)
{
    if (!extras.combinations.isEmpty()) {
        return extras.combinations;
    }
    Structura::App::ProjectData::LoadCombination implicit;
    implicit.name = QStringLiteral("Combo 1");
    for (const auto &loadCase : effectiveCases(extras)) {
        implicit.factors.append({loadCase.name, 1.0});
    }
    return {implicit};
}

} // namespace Structura::App::Services
