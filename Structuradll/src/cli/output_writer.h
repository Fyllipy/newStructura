#pragma once

#include <string>

#include "solver_types.h"

bool writeOutputFile(
    const std::string& filePath,
    const SolverInput& input,
    const SolverOutput& output,
    std::string& errorMessage);
