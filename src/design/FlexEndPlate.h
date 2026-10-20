#pragma once

#include "design/ShearConnections.h"
#include "design/WebShearConnection.h"

#include <vector>

namespace Structura::Design::Connections {

/// Chapa de extremidade flexivel (de altura parcial): chapa soldada a alma da viga por filetes dos dois lados e parafusada ao apoio
/// (mesa ou alma de pilar, ou alma de viga principal) em duas colunas de parafusos simetricas em relacao a alma — o cortante passa pelo
/// centroide do grupo de parafusos (sem excentricidade), e a rotacao da extremidade e acomodada pela flexibilidade da chapa.
/// Verificacoes: NBR 8800:2024, 6.3 (parafusos), 6.2 (solda), 6.5 (chapa), 5.4.3 (alma da viga). Unidades: N, mm, MPa.
struct FlexEndPlateInput {
    int support {WebShearInput::ColumnFlange};

    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    int rows {3};
    double pitch {70.0};
    double gauge {90.0};          ///< entre as duas colunas de parafusos (mm)
    double edgeV {35.0};          ///< furo extremo - borda superior/inferior da chapa
    double edgeH {35.0};          ///< coluna de parafusos - borda lateral da chapa

    SteelProps plate;
    double plateThickness {9.5};
    double plateHeight {0.0};     ///< 0 = (rows - 1) pitch + 2 edgeV

    double weldLeg {6.0};         ///< filetes chapa - alma (dos dois lados)
    double weldFw {485.0};

    SteelProps web;
    double webThickness {6.0};
    double beamDepth {0.0};
    double beamTf {0.0};
    double beamFillet {0.0};
    double gap {12.0};

    SteelProps support_;
    double supportThickness {12.0};   ///< mesa ou alma do apoio onde a chapa e parafusada

    double shear {0.0};
    double axial {0.0};               ///< + tracao
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

double flexEndPlateHeight(const FlexEndPlateInput &input);
double flexEndPlateWidth(const FlexEndPlateInput &input);

std::vector<Check> checkFlexEndPlate(const FlexEndPlateInput &input);
std::vector<Part> partsFlexEndPlate(const FlexEndPlateInput &input);

} // namespace Structura::Design::Connections
