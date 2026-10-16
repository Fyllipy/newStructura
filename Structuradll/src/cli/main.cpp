#include <chrono>
#include <iostream>
#include <string>

#include "input_parser.h"
#include "output_writer.h"
#include "solver_core.h"

namespace {
std::string getOutputFileName(const std::string& inputFileName) {
    std::string outputFileName = inputFileName;
    const size_t dotPos = outputFileName.rfind('.');
    if (dotPos != std::string::npos) {
        outputFileName.replace(dotPos, std::string::npos, ".out");
    }
    else {
        outputFileName += ".out";
    }
    return outputFileName;
}

void printBanner() {
    std::cout << "================================================\n";
    std::cout << "                   STRUCTURA\n";
    std::cout << "    3D SPATIAL FRAME ANALYSIS PROGRAM\n";
    std::cout << "================================================\n";
}
} // namespace

int main(int argc, char* argv[]) {
    printBanner();

    const std::string inputFileName = (argc > 1) ? argv[1] : "input.txt";
    const std::string outputFileName = getOutputFileName(inputFileName);

    std::cout << "Using input file: " << inputFileName << '\n';

    SolverInput input;
    SolverOutput output;
    std::string errorMessage;

    auto startTime = std::chrono::high_resolution_clock::now();

    if (!parseInputFile(inputFileName, input, errorMessage)) {
        std::cerr << "Input parsing error: " << errorMessage << '\n';
        return 1;
    }

    SolverCore solver;
    if (!solver.run(input, output, errorMessage)) {
        std::cerr << "Solver error: " << errorMessage << '\n';
        return 1;
    }

    if (!writeOutputFile(outputFileName, input, output, errorMessage)) {
        std::cerr << "Output writing error: " << errorMessage << '\n';
        return 1;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "Analysis completed successfully.\n";
    std::cout << "Output written to: " << outputFileName << '\n';
    std::cout << "Execution time: " << durationMs << " ms\n";

    return 0;
}
