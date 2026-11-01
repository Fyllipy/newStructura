#pragma once

#include "design/BarDesign.h"

namespace Structura::Design {

/// Parametros do sistema de ligacao entre as banzos (cantoneiras) de um pilar de quatro cantoneiras (SectionKind::LatticedAngles).
struct LatticeSystem {
    int type {0};                 ///< 0 = trelica simples (uma diagonal por painel em cada face, em zigue-zague); 1 = trelica dupla em X; 2 = presilhas (chapas de ligacao)
    double panel {0.0};           ///< a: distancia entre os pontos de ligacao da trelica ou eixos das presilhas, ao longo da barra (mm); 0 = adota o maior dos afastamentos dos banzos (diagonais a ~45 graus)
    double battenDepth {150.0};   ///< hb: altura da presilha ao longo da barra (mm)
    double battenThickness {8.0}; ///< tb: espessura da presilha (mm)
};

/// Propriedades geometricas dos banzos de um pilar de quatro cantoneiras em caixa (obtidas da secao e do perfil de cada banzo).
struct LatticeGeometry {
    double chordArea {0.0};       ///< area de um banzo (mm2)
    double chordIx {0.0};         ///< inercia de um banzo em relacao ao eixo proprio paralelo a X da secao (mm4)
    double chordIy {0.0};
    double chordRMin {0.0};       ///< raio de giracao minimo de um banzo (mm)
    double h0x {0.0};             ///< distancia entre os centroides dos banzos na direcao X (mm)
    double h0y {0.0};             ///< idem na direcao Y
};

/// Geometria dos banzos a partir da secao de quatro cantoneiras (`section`) e de uma cantoneira (`chord`).
LatticeGeometry latticeGeometry(const DesignSection &section, const DesignSection &chord);

/// Resultados intermediarios de um eixo (expostos para teste).
struct LatticeAxisResult {
    double h0 {0.0};              ///< distancia entre as linhas de banzos (mm)
    double panel {0.0};           ///< a (mm)
    double diagonalLength {0.0};  ///< d = sqrt(a^2 + h0^2) (mm); 0 nas presilhas
    double inertiaEff {0.0};      ///< Ief = n A (h0/2)^2 + mu n I_banzo (mm4)
    double mu {1.0};              ///< fator das presilhas (1 nas trelicas)
    double shearStiffness {0.0};  ///< Sv (N)
    double euler {0.0};           ///< Ne,0 = pi^2 E Ief / L^2 (N)
    double Ne {0.0};              ///< Ne,m = Ne,0 / (1 + Ne,0/Sv) (N)
};

/// Rigidez efetiva e carga de flambagem de um eixo: `bendingAboutX` = flexao em torno do eixo X (deslocamento em Y; banzos a +-h0y/2).
LatticeAxisResult latticeAxis(const LatticeGeometry &g, const LatticeSystem &sys, double diagonalArea, double E, double effectiveLength, bool bendingAboutX);

/// Dimensiona o pilar treliçado ou de presilhas de quatro cantoneiras (procedimento aceito pela comunidade tecnico-cientifica, NBR 8800:2024, 5.3.6.4):
/// rigidez ao cisalhamento da ligacao (Timoshenko; EN 1993-1-1, 6.4), imperfeicao e0 = L/500, forca nos banzos, diagonais ou presilhas.
/// `in.section` e a secao de quatro cantoneiras; `in.latticeChord` e `in.latticeDiagonal` (diagonal; pode ser nulo nas presilhas) sao as cantoneiras.
BarDesignOutput designLatticedColumn(const BarDesignInput &in, const LatticeSystem &system);

} // namespace Structura::Design
