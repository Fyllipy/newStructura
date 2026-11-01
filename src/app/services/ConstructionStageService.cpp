#include "ConstructionStageService.h"

#include "CombinationService.h"

#include <algorithm>
#include <utility>

namespace Structura::App::Services {

using Structura::App::SolverBridge;

SolverBridge::ModelData ConstructionStageService::stageModel(const SolverBridge::ModelData &base, const Stage &stage)
{
    SolverBridge::ModelData model = base;
    model.nodalLoads = stage.loads.nodalLoads;
    model.memberLoads = stage.loads.memberLoads;
    model.memberMoments = stage.loads.memberMoments;
    model.memberActions = stage.loads.memberActions;
    model.shellLoads = stage.loads.shellLoads;

    const bool restrict = !stage.activeMembers.empty();
    auto inactive = [&](int memberId) { return restrict && stage.activeMembers.count(memberId) == 0; };

    int nextSection = 0;
    for (const auto &s : model.sections) {
        nextSection = std::max(nextSection, s.id);
    }
    std::map<std::tuple<int, double, double>, int> clones;
    for (auto &member : model.members) {
        Reduction r;
        const auto it = stage.reductions.find(member.id);
        if (it != stage.reductions.end()) {
            r = it->second;
        }
        if (inactive(member.id)) {
            r = {kInactiveStiffness, kInactiveStiffness};
        }
        if (r.area == 1.0 && r.inertia == 1.0) {
            continue;
        }
        const auto key = std::make_tuple(member.sectionId, r.area, r.inertia);
        auto found = clones.find(key);
        if (found == clones.end()) {
            const auto src = std::find_if(model.sections.begin(), model.sections.end(), [&](const auto &s) { return s.id == member.sectionId; });
            if (src == model.sections.end()) {
                continue;
            }
            SolverBridge::SectionData copy = *src;
            copy.id = ++nextSection;
            copy.area *= r.area;
            copy.iy *= r.inertia;
            copy.iz *= r.inertia;
            copy.j *= r.inertia;
            copy.shearAreaY *= r.inertia;
            copy.shearAreaZ *= r.inertia;
            copy.warpingConstant *= r.inertia;
            model.sections.push_back(copy);
            found = clones.emplace(key, copy.id).first;
        }
        member.sectionId = found->second;
    }
    if (restrict) {
        auto drop = [&](auto &vec) {
            vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const auto &l) { return inactive(l.memberId); }), vec.end());
        };
        drop(model.memberLoads);
        drop(model.memberMoments);
        drop(model.memberActions);
    }
    return model;
}

ConstructionStageService::Output ConstructionStageService::run(const SolverBridge::ModelData &base, const QVector<Stage> &stages, const Solver &solver)
{
    Output out;
    std::vector<CombinationService::Term> terms;
    for (const Stage &stage : stages) {
        out.perStage.push_back(solver(stageModel(base, stage)));
    }
    for (const auto &r : out.perStage) {
        terms.push_back({&r, 1.0});
    }
    out.accumulated = CombinationService::combine(terms);
    out.notes << QStringLiteral("Acumulacao incremental linear: cada fase usa a rigidez da propria fase; barras inativas tem rigidez %1 vezes a original.").arg(kInactiveStiffness);
    return out;
}

} // namespace Structura::App::Services
