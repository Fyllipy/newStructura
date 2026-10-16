#pragma once

#include <string>

#include "solver_types.h"

bool parseInputFile(const std::string& filePath, SolverInput& input, std::string& errorMessage);
