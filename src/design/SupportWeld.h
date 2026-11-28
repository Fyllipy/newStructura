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

/// Perna minima de filete (NBR 8800:2024, Tabela 11) em funcao da espessura da parte mais fina ligada (mm): ate 6,3 -> 3; ate 12,5 -> 5; ate 19 -> 6; acima de 19 -> 8.
double minimumFilletLeg(double thinnerPart);
/// Perna maxima do filete ao longo da borda de uma chapa de espessura t (NBR 8800:2024, 6.2.6.2.2): t se t < 6,3 mm; t - 1,5 mm se t >= 6,3 mm.
double maximumFilletLeg(double thickness);

/// Comprimento efetivo minimo de um filete (NBR 8800:2024, 6.2.6.2.3): 40 mm.
constexpr double kMinimumFilletRunLength = 40.0;

/// Trecho continuo de filete (6.2.6.2.3). `geometricLeg` e a perna informada; `effectiveLeg` e a perna considerada no calculo:
/// `min(geometricLeg, length/4)` (alternativa normativa: para efeito de calculo a perna nao pode ser considerada maior que 25 % do comprimento).
struct WeldRun {
    std::string name;
    int count {1};                 ///< numero de trechos iguais
    double length {0.0};           ///< comprimento do trecho (mm)
    double geometricLeg {0.0};
    double effectiveLeg {0.0};
    bool belowFourLegs {false};    ///< length < 4 x perna geometrica (aciona a perna efetiva)
    bool belowMinimumLength {false};   ///< length < 40 mm (nao ha alternativa de calculo: reprova)
};

struct SupportWeldRuns {
    std::vector<WeldRun> flange;   ///< vazio se as mesas sao de penetracao total
    std::vector<WeldRun> web;
};

/// Trechos de filete do arranjo (mesas: face externa `bf` e duas faces internas `(bf - tw)/2`; alma: dois filetes `d - 2 tf`), com perna geometrica e efetiva.
SupportWeldRuns supportWeldRuns(const SupportWeldInput &input);

} // namespace Structura::Design::Connections
