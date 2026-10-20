#pragma once

#include "design/ShearConnections.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Ligacao de diagonal/montante de trelica ou contraventamento a uma chapa de no (gusset) parafusada, soldada ao banzo/pilar.
/// Verifica parafusos (cisalhamento, contato), a diagonal (secao liquida com Ct, cisalhamento de bloco) e a chapa
/// (secao de Whitmore: escoamento, ruptura e flambagem; cisalhamento de bloco) e a solda da chapa ao apoio.
struct GussetInput {
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    int rows {3};                    ///< parafusos ao longo da forca
    int lines {1};                   ///< linhas de parafusos (1 ou 2) na aba ligada
    double pitch {70.0};             ///< mm
    double gauge {60.0};             ///< entre linhas (mm), se lines = 2
    double endDistLeg {35.0};        ///< furo extremo - extremidade da diagonal (mm)
    double edgeDistLeg {30.0};       ///< linha de furos - borda da aba (mm)
    double endDistGusset {35.0};     ///< ultimo furo - ponta da chapa, no sentido da forca (mm)
    double edgeDistGusset {40.0};    ///< linha extrema - borda lateral da chapa (mm)
    // diagonal
    int bars {2};                    ///< 1 = uma peca (chapa/cantoneira simples); 2 = duas pecas com a chapa entre elas (corte duplo)
    double barArea {0.0};            ///< area bruta de UMA peca (mm2)
    double barLegThickness {6.35};   ///< espessura da aba ligada (mm)
    double barEccentricity {20.0};   ///< ec: distancia do centroide a face de ligacao (mm)
    SteelProps bar;
    // chapa
    double gussetThickness {9.5};
    double gussetWidth {200.0};      ///< largura da chapa na solda ao apoio (mm); limita a largura de Whitmore
    double freeLength {80.0};        ///< distancia do apoio a linha de furos mais proxima (comprimento da coluna de Whitmore)
    SteelProps gusset;
    // solda ao apoio
    double weldLeg {6.0};
    double weldFw {485.0};
    double weldAngleDeg {90.0};      ///< angulo entre a forca e o eixo da solda (0 = paralela, 90 = perpendicular)
    double supportThickness {12.0};
    SteelProps support;
    // esforcos de calculo (N, modulo)
    double tension {0.0};
    double compression {0.0};
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

std::vector<Check> checkGusset(const GussetInput &input);
std::vector<Part> partsGusset(const GussetInput &input);
/// Largura de Whitmore (mm), sem limitar pela largura da chapa.
double whitmoreWidth(const GussetInput &input);
/// Comprimento da chapa na direcao da forca (mm).
double gussetLength(const GussetInput &input);

struct GussetProposal {
    bool found {false};
    GussetInput input;
    double maxRatio {0.0};
};
/// Chapa/parafusos mais leves que atendem (varre diametro, parafusos por linha, linhas, espessura e solda).
GussetProposal proposeGusset(GussetInput base);

/// Emenda de viga parafusada com talas de mesa (externas, opcionalmente internas) e talas duplas de alma.
struct SpliceInput {
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    // viga
    double beamDepth {300.0};
    double flangeWidth {150.0};
    double flangeThickness {10.0};
    double webThickness {6.0};
    double webHeight {0.0};          ///< altura livre da alma (mm); 0 = d - 2 tf
    double beamArea {0.0};           ///< area bruta da viga (mm2), para repartir o esforco axial entre as mesas
    SteelProps beam;
    double gap {10.0};               ///< folga entre as extremidades (mm)
    // tala de mesa (de cada lado da emenda)
    int flangeRows {3};              ///< parafusos ao longo da viga, por lado
    int flangeLines {2};             ///< parafusos na largura
    double flangePitch {70.0};
    double flangeGauge {80.0};
    double flangeEnd {40.0};         ///< furo extremo - extremidade da tala (mm)
    double flangeEdge {35.0};
    double outerPlateWidth {150.0};
    double outerPlateThickness {12.5};
    double innerPlateThickness {0.0};   ///< 0 = sem talas internas
    double innerPlateWidth {55.0};      ///< largura de cada tala interna (duas por mesa)
    SteelProps plates;
    // tala de alma (duas, uma de cada lado)
    int webRows {3};
    int webCols {1};                 ///< colunas de parafusos de cada lado da emenda
    double webPitch {70.0};
    double webGauge {60.0};
    double webEdgeH {40.0};          ///< coluna extrema - borda da tala (mm)
    double webEdgeV {35.0};
    double webPlateThickness {6.3};
    int webPlates {2};               ///< talas de alma: 2 (uma de cada lado, perfil I) ou 1 (perfil U)
    // esforcos de calculo
    double moment {0.0};             ///< N.mm
    double shear {0.0};              ///< N
    double axial {0.0};              ///< N (+ tracao)
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkSplice(const SpliceInput &input);
std::vector<Part> partsSplice(const SpliceInput &input);
double spliceFlangePlateLength(const SpliceInput &input);
double spliceWebPlateLength(const SpliceInput &input);
double spliceWebPlateHeight(const SpliceInput &input);

struct SpliceProposal {
    bool found {false};
    SpliceInput input;
    double maxRatio {0.0};
};
SpliceProposal proposeSplice(SpliceInput base);

} // namespace Structura::Design::Connections
