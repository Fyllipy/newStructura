#pragma once

#include "design/ShearConnections.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Emenda parafusada de barra axial (chapa, cantoneira) com cobrejuntas, na tracao ou compressao: parafusos (corte, contato), secao liquida
/// com Ct, rasgamento, cobrejuntas (escoamento, ruptura, esbeltez do trecho livre). NBR 8800:2024, 5.2, 5.3, 6.3 e 6.5. Unidades: N, mm, MPa.
struct CoverSpliceInput {
    enum Shape { FlatBar = 0, Angle = 1 };

    int shape {FlatBar};
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    // barra
    double area {0.0};               ///< area bruta (mm2)
    double thickness {10.0};         ///< espessura da parte ligada (mm)
    double width {100.0};            ///< largura da chapa, ou da aba ligada da cantoneira (mm)
    double eccentricity {0.0};       ///< cantoneira: centroide - face de ligacao (mm); Ct = 1 - ec/lc
    SteelProps bar;
    // cobrejuntas
    int covers {2};                  ///< 1 = uma cobrejunta (corte simples), 2 = duas (corte duplo)
    double coverThickness {8.0};     ///< de cada uma (mm)
    double coverWidth {100.0};
    SteelProps cover;
    // parafusos de cada lado da junta
    int rows {3};
    int lines {1};
    double pitch {60.0};
    double gauge {50.0};
    double endDistance {35.0};       ///< furo extremo - extremidade da barra (mm)
    double edgeDistance {30.0};      ///< linha de furos - borda (mm)
    double gap {10.0};
    // esforco
    double axial {0.0};              ///< N, + tracao
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkCoverSplice(const CoverSpliceInput &input);
std::vector<Part> partsCoverSplice(const CoverSpliceInput &input);
/// Comprimento de cada cobrejunta (mm).
double coverSpliceLength(const CoverSpliceInput &input);

/// Emenda de tubo (circular) por chapas de extremidade parafusadas em circulo: tracao/compressao e momento fletor.
struct TubeFlangeInput {
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    double tubeDiameter {114.3};
    double tubeThickness {6.0};
    SteelProps tube;
    double plateDiameter {240.0};    ///< diametro externo da chapa de extremidade
    double plateThickness {20.0};
    SteelProps plate;
    int bolts {6};
    double boltCircle {190.0};       ///< diametro do circulo de parafusos
    double weldLeg {6.0};            ///< filete tubo - chapa (externo)
    double weldFw {485.0};
    double axial {0.0};              ///< + tracao
    double shear {0.0};
    double moment {0.0};             ///< N.mm
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

std::vector<Check> checkTubeFlange(const TubeFlangeInput &input);
std::vector<Part> partsTubeFlange(const TubeFlangeInput &input);

} // namespace Structura::Design::Connections
