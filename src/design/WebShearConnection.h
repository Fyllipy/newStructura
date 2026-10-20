#pragma once

#include "design/ShearConnections.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Ligacao de viga por tala(s) de alma: tala simples (single plate) ou dupla tala, ligada a alma da viga por parafusos (uma ou duas linhas)
/// ou por solda, e soldada ao apoio (alma ou mesa do pilar, ou alma de viga principal com recortes nas mesas).
/// Verificacoes pela NBR 8800:2024: 6.3 (parafusos), 6.2 (soldas), 6.5 (chapas: escoamento, ruptura, rasgamento e flexao).
/// Unidades: N, mm, MPa.
struct WebShearInput {
    enum Support { BeamWeb = 0, ColumnWeb = 1, ColumnFlange = 2 };

    int plates {1};                  ///< 1 = tala simples; 2 = dupla tala (uma de cada lado da alma)
    bool webWelded {false};          ///< tala(s) soldadas a alma da viga em vez de parafusadas
    int support {ColumnFlange};

    // parafusos na alma da viga
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    int rows {3};
    int lines {1};                   ///< linhas verticais de parafusos (1 ou 2)
    double pitch {70.0};
    double gauge {60.0};             ///< entre linhas (mm)
    double edgeV {35.0};             ///< furo extremo - borda superior/inferior da tala
    double edgeH {35.0};             ///< ultima linha de furos - borda livre da tala
    double boltLineToSupport {60.0}; ///< primeira linha de furos - face do apoio (mm)

    // tala
    SteelProps plate;
    double plateThickness {9.5};     ///< cada tala (mm)
    double plateHeight {0.0};        ///< altura (mm); 0 = (rows - 1) pitch + 2 edgeV (parafusada)
    double plateLength {0.0};        ///< comprimento horizontal soldado a alma (mm), tala soldada a alma

    // solda
    double weldLegWeb {6.0};         ///< tala - alma da viga (tala soldada)
    double weldLegSupport {6.0};     ///< tala - apoio
    double weldFw {485.0};
    bool weldEnd {false};            ///< filete vertical na extremidade da tala sobre a alma (contorno em C)

    // viga ligada
    SteelProps web;
    double webThickness {6.0};
    double beamDepth {0.0};
    double beamBf {0.0};
    double beamTf {0.0};
    double beamFillet {0.0};         ///< raio de concordancia alma-mesa (mm)
    double gap {12.0};               ///< folga entre a extremidade da viga e o apoio (mm)

    // apoio
    SteelProps support_;
    double supportThickness {12.0};  ///< alma ou mesa do apoio onde a(s) tala(s) se solda(m)
    double supD {0.0};               ///< altura do perfil de apoio (mm)
    double supBf {0.0};
    double supTf {0.0};
    double supTw {0.0};
    double supFillet {0.0};
    double copeClearance {15.0};     ///< folga do recorte nas mesas (viga principal)

    // esforcos de calculo
    double shear {0.0};
    double axial {0.0};              ///< + tracao
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

/// Altura da(s) tala(s) (mm).
double webShearPlateHeight(const WebShearInput &input);
/// Comprimento horizontal da(s) tala(s) (mm).
double webShearPlateLength(const WebShearInput &input);
/// Distancia da face do apoio ao centroide do grupo de parafusos ou de solda (excentricidade do cortante, mm).
double webShearEccentricity(const WebShearInput &input);

std::vector<Check> checkWebShear(const WebShearInput &input);
std::vector<Part> partsWebShear(const WebShearInput &input);

/// Recorte necessario nas mesas da viga ligada a alma de uma viga principal: comprimento e profundidade (mm); zeros se nao ha.
struct CopeSize {
    double length {0.0};
    double depth {0.0};
    bool both {false};   ///< recorte nas duas mesas
};
CopeSize webShearCope(const WebShearInput &input);

} // namespace Structura::Design::Connections
