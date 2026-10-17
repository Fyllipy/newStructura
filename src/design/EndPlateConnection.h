#pragma once

#include "design/Connections.h"
#include "design/ShearConnections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Ligacao rigida viga-pilar por chapa de extremidade (parafusos tracionados na zona da mesa tracionada, parafusos de cortante na zona comprimida).
/// NBR 8800:2024: 6.3 (parafusos, 6.3.5 efeito alavanca), 6.2 (soldas), 5.7.2 a 5.7.5 e 5.7.7 a 5.7.9 (mesa e alma do pilar).
/// Unidades: N, mm, MPa.
struct EndPlateInput {
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};

    // viga
    double beamDepth {300.0};
    double flangeWidth {150.0};
    double flangeThickness {10.0};
    double webThickness {6.0};
    SteelProps beam;

    // chapa de extremidade
    double plateWidth {160.0};
    double plateThickness {19.0};
    SteelProps plate;

    // parafusos
    int tensionRows {2};             ///< fileiras (de 2 parafusos) na zona tracionada
    int shearRows {1};               ///< fileiras (de 2 parafusos) na zona comprimida, que resistem ao cortante
    double pitch {70.0};             ///< entre fileiras (mm)
    double gauge {90.0};             ///< entre as duas colunas de parafusos (mm)
    double firstRow {40.0};          ///< face externa da mesa da viga ao eixo da primeira fileira de parafusos, fora da mesa (mm); vale nas duas zonas
    double edgeVertical {35.0};      ///< eixo do parafuso extremo a borda da chapa (mm)

    // soldas
    double weldFlangeLeg {8.0};
    double weldWebLeg {6.0};
    double weldFw {485.0};

    // pilar
    double colDepth {260.0};
    double colFlangeWidth {146.0};
    double colFlangeThickness {12.7};
    double colWebThickness {7.6};
    double colRadius {10.0};
    SteelProps column;
    double colUnbraced {0.0};        ///< comprimento destravado ℓ na altura da ligacao (mm); 0 = mesas contidas (5.7.5 dispensada)
    bool continuityPlates {false};   ///< enrijecedores horizontais alinhados com as mesas da viga (5.7.9)
    double stiffenerThickness {9.5};
    double stiffenerWidth {60.0};    ///< largura de cada enrijecedor (mm)
    double doublerThickness {0.0};   ///< chapa de reforco da alma do painel (5.7.7.2)

    // esforcos de calculo (N, N.mm); axial + tracao
    double moment {0.0};
    double shear {0.0};
    double axial {0.0};

    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

std::vector<Check> checkEndPlate(const EndPlateInput &input);
std::vector<Part> partsEndPlate(const EndPlateInput &input);

/// Altura total da chapa (mm): altura da viga mais o prolongamento acima e abaixo, ate a borda apos a ultima fileira.
double endPlateHeight(const EndPlateInput &input);

} // namespace Structura::Design::Connections
