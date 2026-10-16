#pragma once

#include <string>
#include <unordered_set>

#include "solver_types.h"

class SolverCore {
public:
    bool run(const SolverInput& input, SolverOutput& output, std::string& errorMessage) const;

private:
    bool runPass(const SolverInput& input, const std::unordered_set<int>& inactiveMembers, SolverOutput& output, std::string& errorMessage) const;
};
