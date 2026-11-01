#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Eigen/Sparse>

#include "solver_types.h"

class Node;

// Vinculos lineares entre graus de liberdade, impostos por penalidade: cada linha impoe  sum_j c_j u_j = 0.
// A rigidez da penalidade e 1e7 vezes a rigidez diagonal do grau de liberdade dependente (o primeiro da linha, com coeficiente 1) no mesmo no.
struct ConstraintRow {
    std::vector<std::pair<int, double>> entries;   // (indice global do grau de liberdade, coeficiente); o primeiro tem coeficiente 1
    double penalty = 0.0;
    bool reportReaction = false;                   // a forca do vinculo e somada as reacoes dos nos (apoios inclinados)
    int group = 0;                                 // 0 = geral, 1 = diafragma, 2 = ligacao rigida, 3 = apoio inclinado
};

// Expande diafragmas, ligacoes rigidas, apoios inclinados e vinculos gerais em linhas de penalidade. `base` e a rigidez ja montada
// (barras, cascas, molas) e fornece a escala das penalidades.
std::vector<ConstraintRow> buildConstraintRows(const SolverInput& input,
                                               const std::unordered_map<int, int>& nodeIndexById,
                                               const std::vector<std::shared_ptr<Node>>& nodes,
                                               const Eigen::SparseMatrix<double>& base,
                                               int totalDof);

void assembleConstraintStiffness(const std::vector<ConstraintRow>& rows, int totalDof, Eigen::SparseMatrix<double>& k);

// Forca (escalar) de cada vinculo: penalidade x (c . u)
std::vector<double> constraintForces(const std::vector<ConstraintRow>& rows, const Eigen::VectorXd& displacement);

// Reacoes dos vinculos sinalizados (forca que o vinculo exerce sobre a estrutura): - penalidade (c . u) c
Eigen::VectorXd constraintReactionVector(const std::vector<ConstraintRow>& rows, const Eigen::VectorXd& displacement, int totalDof);
