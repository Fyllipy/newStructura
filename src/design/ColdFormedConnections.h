#pragma once

#include "design/Connections.h"
#include "design/ShearConnections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Ligacao por sobreposicao de perfil formado a frio a chapa (ABNT NBR 14762:2010, Secao 10).
/// Unidades: N, mm, MPa. Aplicavel a partes ligadas com t <= 4,75 mm (10.2.1 e 10.3.1).
struct ColdFormedInput {
    enum Mode { Bolts = 0, Weld = 1, Screws = 2 };
    int mode {Bolts};

    double force {0.0};           ///< forca de cisalhamento de calculo na ligacao (N)
    double memberResistance {0.0};///< resistencia de calculo da barra (N); > 0 aplica o minimo de 50 % (10.1-a)

    double t1 {2.0};              ///< espessura do perfil (mm)
    double fy1 {345.0};
    double fu1 {450.0};
    double t2 {4.75};             ///< espessura da chapa de ligacao (mm)
    double fy2 {250.0};
    double fu2 {400.0};

    // parafusos comuns e autoatarraxantes
    double d {12.0};              ///< diametro nominal (mm)
    int n {2};                    ///< parafusos ao longo da forca
    int lines {1};                ///< linhas de parafusos
    double pitch {40.0};          ///< espacamento ao longo da forca (mm)
    double gauge {40.0};          ///< espacamento entre linhas (mm)
    double end1 {30.0};           ///< centro do furo extremo ate a extremidade do perfil, na direcao da forca (mm)
    double end2 {30.0};           ///< idem na chapa
    double edge {25.0};           ///< centro do furo ate a borda transversal (mm)
    double fub {400.0};           ///< ruptura do parafuso (Tabela 16)
    bool thread {true};           ///< plano de corte passa pela rosca
    int planes {1};

    // solda de filete (superficies planas)
    double leg {3.0};             ///< perna do filete (mm)
    double lengthLong {0.0};      ///< comprimento de filetes paralelos a forca (mm)
    double lengthTrans {0.0};     ///< comprimento de filetes normais a forca (mm)
    double fw {485.0};

    // parafusos autoatarraxantes (10.5)
    double fssRk {0.0};           ///< cisalhamento nominal do parafuso, fabricante (N); 0 = nao informado
    double ftsRk {0.0};           ///< tracao nominal do parafuso, fabricante (N); 0 = nao informado
    double headDiameter {8.0};    ///< dh (mm)
    double washerDiameter {0.0};  ///< dw (mm); 0 = sem arruela
    double washerThickness {0.0};
    double tension {0.0};         ///< tracao total nos parafusos (N), dividida igualmente
};

std::vector<Check> checkColdFormed(const ColdFormedInput &input);

std::vector<Part> partsColdFormed(const ColdFormedInput &input);

} // namespace Structura::Design::Connections
