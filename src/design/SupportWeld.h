#pragma once

#include "design/Connections.h"

#include <vector>

namespace Structura::Design::Connections {

/**
 * Solda da viga ao apoio (pilar ou viga principal) nas ligacoes "fora do no": o trecho de viga entre a face do apoio e a junta e soldado a face e
 * transmite o momento, o cortante e o axial da ligacao. Modelo (NBR 8800:2024, 6.2):
 *  - o momento e o axial sao resistidos pelas mesas: F = M / (d - tf) + |N| / 2 na mesa mais solicitada;
 *  - mesas com solda de penetracao total (a resistencia e a do metal-base, 6.2.4) ou com filetes transversais (Tabela 9, theta = 90 graus);
 *  - o cortante e resistido pelos dois filetes da alma (comprimento d - 2 tf), longitudinais ao esforco.
 * Unidades: N, N.mm, mm, MPa.
 */
struct SupportWeldInput {
    double depth {300.0};           ///< altura da viga d
    double flangeWidth {150.0};     ///< bf
    double flangeThickness {10.0};  ///< tf
    double webThickness {6.0};      ///< tw
    double beamFy {250.0};
    double beamFu {400.0};
    double supportThickness {0.0};  ///< espessura da parte do apoio onde se solda (mesa/alma); 0 = nao informada
    double supportFy {250.0};
    double supportFu {400.0};
    bool flangeFullPenetration {true};
    double flangeLeg {8.0};         ///< perna dos filetes das mesas (se nao for penetracao total)
    double webLeg {6.0};
    double fw {485.0};              ///< resistencia do metal da solda (Tabela A.4)
    double shear {0.0};             ///< N
    double axial {0.0};             ///< N, + tracao
    double moment {0.0};            ///< N.mm
    double gammaW2 {1.35};
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkSupportWeld(const SupportWeldInput &input);

/// Perna minima de filete (Tabela 11) em funcao da espessura da parte mais fina ligada (mm).
double minimumFilletLeg(double thinnerPart);
/// Perna maxima do filete na borda de uma chapa de espessura t (6.2.5.2).
double maximumFilletLeg(double thickness);

} // namespace Structura::Design::Connections
