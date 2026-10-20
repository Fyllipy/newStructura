#pragma once

#include "design/ShearConnections.h"
#include "design/TrussConnections.h"

#include <array>
#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// No de trelica plana: ate tres barras (duas diagonais e um montante) ligadas ao banzo diretamente ou por chapa de no (gousset).
/// Ligacao da barra a chapa (ou ao banzo): solda de filete ou parafusos; chapa ao banzo: solda de topo, filetes (3 ou 4 lados) ou parafusos.
/// NBR 8800:2024 (5.2, 5.3, 6.2, 6.3, 6.5). Unidades: N, mm, MPa.
struct TrussBarData {
    bool on {false};
    double force {0.0};              ///< esforco axial de calculo (N, + tracao)
    double angleDeg {45.0};          ///< angulo com o eixo do banzo (0 a 90)
    bool toRight {true};             ///< a barra se afasta do no para o lado + do banzo
    double area {0.0};               ///< area bruta (mm2)
    double legWidth {64.0};          ///< largura da aba ligada, b (mm)
    double legThickness {6.35};
    double eccentricity {20.0};      ///< ec: centroide - face de ligacao (mm)
    SteelProps steel;
    int bars {1};                    ///< pecas por barra (1 = cantoneira/perfil simples; 2 = pares com o gousset entre elas)
    // ligacao por solda de filete
    double weldLeg {5.0};
    double lengthHeel {80.0};        ///< filete do lado do calcanhar (mm)
    double lengthToe {80.0};         ///< filete do lado da ponta da aba (mm)
    // ligacao por parafusos
    int rows {3};
    int lines {1};
    double pitch {70.0};
    double gauge {60.0};
    double endDistance {35.0};
    double edgeDistance {30.0};
};

struct TrussNodeInput {
    enum BarMode { Welded = 0, Bolted = 1 };
    enum ChordMode { Direct = 0, ButtWeld = 1, FilletWeld = 2, BoltedToChord = 3 };

    int barMode {Welded};
    int chordMode {ButtWeld};
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};
    std::array<TrussBarData, 3> bars;
    // chapa de no
    double gussetThickness {9.5};
    double gussetWidth {200.0};      ///< ao longo do banzo (mm)
    double gussetHeight {200.0};     ///< do banzo ate a extremidade livre (mm)
    double freeLength {80.0};        ///< do banzo ao primeiro parafuso/solda (mm)
    SteelProps gusset;
    // gousset - banzo
    double chordWeldLeg {6.0};
    bool fourSides {false};          ///< filetes nos quatro lados (3 lados: sem o lado da extremidade livre)
    int chordBolts {4};              ///< parafusos gousset - banzo (duas colunas)
    double chordBoltPitch {70.0};
    double weldFw {485.0};
    double chordThickness {12.0};    ///< mesa do banzo onde a chapa/barra se liga
    SteelProps chord;
    // excentricidades da resultante em relacao ao centro da ligacao banzo-chapa (mm): ao longo do banzo e normal a ele
    double ex {0.0};
    double ey {0.0};
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

std::vector<Check> checkTrussNode(const TrussNodeInput &input);
std::vector<Part> partsTrussNode(const TrussNodeInput &input);

/// Componentes da resultante das barras no banzo: ao longo do banzo (Fx), normal ao banzo (Fy, + afasta da face) e momento (N.mm).
struct TrussInterfaceForces {
    double fx {0.0}, fy {0.0}, moment {0.0};
    double pull {0.0}, push {0.0};   ///< forcas normais ao banzo: arrancamento e compressao (N, soma dos modulos por sentido)
};
TrussInterfaceForces trussInterfaceForces(const TrussNodeInput &input);

} // namespace Structura::Design::Connections
