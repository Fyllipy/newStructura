#pragma once

#include "design/ShearConnections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Chapa tracionada ligada por pino e o proprio pino (ABNT NBR 8800:2024, 5.2.6, 6.4 e 6.6.2). Unidades: N, mm, MPa.
struct PinPlateInput {
    // chapa
    double plateThickness {16.0};    ///< t (mm)
    double plateWidth {120.0};       ///< b (mm), furo na meia distancia das bordas
    double endDistance {60.0};       ///< a: borda do furo - extremidade da barra, paralela a forca (mm)
    SteelProps plate;
    // pino e furo
    double pinDiameter {40.0};       ///< dp (mm)
    double holeDiameter {40.5};      ///< dh (mm)
    bool rotationAllowed {false};    ///< o pino tambem permite rotacao relativa: dh <= dp + 1 mm
    bool machinedHole {true};        ///< furo mandrilado ou broqueado (6.6.2); senao a pressao de contato nao pode ser transmitida pelo pino (6.6.3)
    SteelProps pin;
    // arranjo do pino
    int shearPlanes {2};             ///< 2 = cisalhamento duplo (chapa central entre duas externas); 1 = simples (duas chapas)
    double otherThickness {10.0};    ///< espessura de cada chapa externa (corte duplo) ou da outra chapa (corte simples), mm
    double gap {2.0};                ///< folga entre chapas adjacentes (mm)
    // acao
    double tension {0.0};            ///< N
    double gammaA1 {1.10};
    double gammaA2 {1.35};
};

std::vector<Check> checkPinPlate(const PinPlateInput &input);
std::vector<Part> partsPinPlate(const PinPlateInput &input);

/// Momento fletor maximo do pino com tensoes de contato uniformes na espessura de cada parte (6.4.1), N.mm.
double pinBendingMoment(const PinPlateInput &input);

/// Fadiga de parafusos e barras redondas rosqueadas (NBR 8800:2024, Anexo H, H.5-b): faixa de variacao de tensoes de tracao na area liquida efetiva
/// Abe = pi/4 (db - 0,9382 p)^2 contra a faixa admissivel da categoria G (Tabela H.1, caso 8.5: Cf = 3,9e8, sigma_TH = 48 MPa): (Cf / N)^(1/3) >= sigma_TH.
struct BoltFatigueInput {
    double db {20.0};                ///< diametro nominal (mm)
    double pitch {2.5};              ///< passo da rosca (mm)
    int bolts {1};                   ///< parafusos que dividem a faixa de forca
    double forceRange {0.0};         ///< faixa de variacao da forca axial na ligacao (N), incluindo o efeito de alavanca
    double cycles {2.0e6};           ///< numero de ciclos N (0 = vida infinita: so o limite sigma_TH)
    bool preloaded {true};           ///< juntas com protensao inicial e material so de aco na pega: alternativa de 20 % da forca (H.5)
    double staticDesignForce {0.0};  ///< forca obtida da combinacao de H.2.1 (N); usada na alternativa de 20 % quando `preloaded`
};
std::vector<Check> checkBoltFatigue(const BoltFatigueInput &input);

/// Fadiga do metal-base de ligacoes parafusadas e de chapas ligadas por pino (H.5-a, Tabela H.1, casos 2.1 a 2.4):
/// 1 = 2.1/2.2 (categoria B), 2 = 2.3 (categoria C, ligacoes parafusadas exceto chapas por pino), 3 = 2.4 (categoria E, chapas ligadas por pino).
struct MaterialFatigueInput {
    int tableCase {3};
    double stressRange {0.0};    ///< faixa de variacao de tensoes no metal-base (MPa)
    double cycles {2.0e6};       ///< N (0 = vida infinita)
};
std::vector<Check> checkMaterialFatigue(const MaterialFatigueInput &input);

/// Requisitos especiais de fabricacao e montagem da fadiga (H.6.1 a H.6.7), como item informativo do memorial.
Check fatigueFabricationRequirements();

} // namespace Structura::Design::Connections
