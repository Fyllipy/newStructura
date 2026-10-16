#include <iostream>
#include <string>
#include <chrono>
#include "Structure.h"

/*
 * Structura
 *
 * Solver para a disciplina de Introdução a métodos computacionais
 * Calculo de porticos espaiais utilizando o método da rigidez direta
 * Aluno: João Fyllipy de Lima Nunes
 */
int main(int argc, char* argv[]) {
    std::cout << "================================================" << std::endl;
    std::cout << "                   STRUCTURA" << std::endl;
    std::cout << "    3D SPATIAL FRAME ANALYSIS PROGRAM" << std::endl;
    std::cout << "    Desenvolvido no contexto da disciplina de" << std::endl;
    std::cout << "    introducao a mecanica computacional de" << std::endl;
    std::cout << "    estruturas, 2025.1" << std::endl;
    std::cout << "    Aluno: Joao Fyllipy de Lima Nunes" << std::endl;
    std::cout << "================================================" << std::endl;

    // Default input file name
    std::string inputFileName = "input.txt";

    // Check for command line argument
    if (argc > 1) {
        inputFileName = argv[1];
    }

    std::cout << "\nUsing input file: " << inputFileName << std::endl;

    try {
        // Create structure object
        Structure structure;

        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();

        // Step 1: Read input file
        std::cout << "\n1. Reading input file..." << std::endl;
        if (!structure.readInputFile(inputFileName)) {
            std::cerr << "Failed to read input file. Exiting." << std::endl;
            return 1;
        }

        // Step 2: Assemble global stiffness matrix
        std::cout << "\n2. Assembling global stiffness matrix..." << std::endl;
        structure.assembleGlobalStiffnessMatrix();

        // Step 3: Assemble global load vector
        std::cout << "\n3. Assembling global load vector..." << std::endl;
        structure.assembleGlobalLoadVector();

        // Step 4: Apply boundary conditions
        std::cout << "\n4. Applying boundary conditions..." << std::endl;
        structure.applyBoundaryConditions();

        // Step 5: Solve system of equations
        std::cout << "\n5. Solving system of equations..." << std::endl;
        if (!structure.solve()) {
            std::cerr << "Failed to solve system of equations. Exiting." << std::endl;
            return 1;
        }

        // Step 6: Calculate reactions
        std::cout << "\n6. Calculating reaction forces..." << std::endl;
        structure.calculateReactions();

        // Step 7: Post-process (calculate internal forces)
        std::cout << "\n7. Post-processing (calculating internal forces)..." << std::endl;
        structure.postProcess();

        // End timing
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Step 8: Print results to console
        structure.printResults();

        // Step 9: Save results to output file
        std::string outputFileName = inputFileName;
        size_t dotPos = outputFileName.rfind('.');
        if (dotPos != std::string::npos) {
            outputFileName.replace(dotPos, std::string::npos, ".out");
        }
        else {
            outputFileName += ".out";
        }
        structure.saveResultsToFile(outputFileName);

        std::cout << "\nAnalysis completed successfully!" << std::endl;
        std::cout << "Total execution time: " << duration.count() << " ms" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "\nError during analysis: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "\nUnknown error occurred during analysis." << std::endl;
        return 1;
    }

    return 0;
}