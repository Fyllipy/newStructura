#pragma once

#include "design/Connections.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Aco de uma chapa, cantoneira ou da alma da viga (MPa).
struct SteelProps {
    double fy {250.0};
    double fu {400.0};
};

/// Peca da lista de materiais da ligacao.
struct Part {
    std::string description;
    int quantity {0};
    double massEach {0.0};   ///< kg (0 para itens medidos em outra unidade)
    double length {0.0};     ///< mm (solda: comprimento total por unidade)
    std::string unit {"un"};
    double totalMass() const { return quantity * massEach; }
};

/// Chapa de cisalhamento ("shear tab"): chapa soldada ao apoio nos dois lados e parafusada na alma da viga, uma coluna de parafusos.
struct ShearTabInput {
    BoltGrade grade;
    double db {20.0};                ///< mm
    bool threadInShearPlane {true};
    int rows {3};                    ///< parafusos na coluna
    double pitch {70.0};             ///< mm
    double edgeV {35.0};             ///< furo extremo - borda superior/inferior da chapa (mm)
    double edgeH {35.0};             ///< linha de furos - borda livre da chapa (mm)
    double boltLineToWeld {60.0};    ///< a: linha de furos - face do apoio (excentricidade, mm)
    double beamSetback {12.0};       ///< folga entre a extremidade da viga e o apoio (mm)
    SteelProps plate;
    double plateThickness {9.5};     ///< mm
    SteelProps web;
    double webThickness {6.0};       ///< alma da viga (mm)
    double beamDepth {0.0};          ///< altura da viga (mm); 0 = nao verifica a altura da chapa
    double weldLeg {6.0};            ///< perna do filete (mm)
    double weldFw {485.0};           ///< MPa
    double supportThickness {12.0};  ///< espessura da peca de apoio onde a solda se liga (mm)
    SteelProps support;
    double shear {0.0};              ///< cortante de calculo V (N)
    double axial {0.0};              ///< esforco axial de calculo na viga, N (+ tracao); entra como cortante nos parafusos e tracao na chapa/solda
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

std::vector<Check> checkShearTab(const ShearTabInput &input);
std::vector<Part> partsShearTab(const ShearTabInput &input);

/// Altura da chapa (mm).
double plateHeight(const ShearTabInput &input);

/// Cantoneira dupla: duas cantoneiras iguais, parafusadas na alma da viga (corte duplo) e no apoio (corte simples).
struct DoubleAngleInput {
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    int rows {3};
    double pitch {70.0};
    double edgeV {35.0};             ///< furo extremo - extremidade da cantoneira (mm)
    double gaugeWeb {50.0};          ///< linha de furos da alma - aresta da cantoneira, aba da alma (mm)
    double gaugeSupport {50.0};      ///< idem, aba do apoio
    double webEndDistance {30.0};    ///< linha de furos da alma - extremidade da viga (mm)
    double supportEdgeDistance {35.0}; ///< linha de furos do apoio - borda da aba (mm)
    SteelProps angle;
    double angleThickness {9.5};
    SteelProps web;
    double webThickness {6.0};
    SteelProps support;
    double supportThickness {12.0};  ///< alma ou mesa do apoio onde as cantoneiras se parafusam (mm)
    double beamDepth {0.0};
    double shear {0.0};              ///< N
    double axial {0.0};              ///< N (+ tracao): cisalhamento nos parafusos da alma e tracao nos parafusos do apoio
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkDoubleAngle(const DoubleAngleInput &input);
std::vector<Part> partsDoubleAngle(const DoubleAngleInput &input);
double angleLength(const DoubleAngleInput &input);

/// Maior razao das verificacoes (>1 = nao atende).
double worstRatio(const std::vector<Check> &checks);
/// Check que governa.
const Check *governing(const std::vector<Check> &checks);

/// Propoe a chapa de cisalhamento mais leve que atende (varre diametro, linhas, espessura e perna); `found` = false se nenhuma.
struct ShearTabProposal {
    bool found {false};
    ShearTabInput input;
    double maxRatio {0.0};
};
ShearTabProposal proposeShearTab(ShearTabInput base);

struct DoubleAngleProposal {
    bool found {false};
    DoubleAngleInput input;
    double maxRatio {0.0};
};
DoubleAngleProposal proposeDoubleAngle(DoubleAngleInput base);

} // namespace Structura::Design::Connections
