#pragma once

#include "design/ShearConnections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Ligacoes soldadas de trelicas tubulares (CIDECT Design Guide 1, 2a ed., 2008: Tabelas 4.1, 4.3, 6.1 e 7.1).
/// Banzo circular (CHS) com diagonais circulares, ou chapa transversal (T/X). Resistencias de calculo N* ja incluem
/// os coeficientes de seguranca (o guia as fornece como resistencia de calculo).
struct TubularJointInput {
    enum Kind { TY = 0, X = 1, KGap = 2, KOverlap = 3 };
    enum BraceKind { Circular = 0, TransversePlate = 1 };

    int kind {KGap};
    int braceKind {Circular};
    // banzo
    double d0 {114.3};
    double t0 {6.0};
    double fy0 {355.0};
    double fu0 {490.0};
    double chordAxial {0.0};        ///< N (+ tracao) na face de ligacao
    double chordMoment {0.0};       ///< N.mm (momento no banzo, incluindo excentricidade do no)
    // diagonal 1 (a que se sobrepoe, em K com sobreposicao) ou unica diagonal
    double d1 {60.3};               ///< diametro (ou largura b1 da chapa)
    double t1 {4.0};
    double fy1 {355.0};
    double theta1 {45.0};           ///< graus
    double n1 {0.0};                ///< modulo da forca axial (N)
    // diagonal 2 (K)
    double d2 {60.3};
    double t2 {4.0};
    double fy2 {355.0};
    double theta2 {45.0};
    double n2 {0.0};
    double gap {20.0};              ///< afastamento g entre as diagonais (K com afastamento), mm
    double overlap {50.0};          ///< sobreposicao Ov em % (K com sobreposicao)
    bool hiddenToeWelded {false};   ///< pe oculto da diagonal sobreposta soldado (Ov_limit 80%) ou nao (60%)
    double eccentricity {0.0};      ///< e do no (mm); positivo para fora do eixo do banzo
};

std::vector<Check> checkTubularJoint(const TubularJointInput &input);
std::vector<Part> partsTubularJoint(const TubularJointInput &input);

} // namespace Structura::Design::Connections
