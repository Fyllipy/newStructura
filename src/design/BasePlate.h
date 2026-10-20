#pragma once

#include "design/ShearConnections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Disposicoes construtivas dos chumbadores (ABNT NBR 8800:2024, Tabela 18), aco ASTM A36.
struct AnchorDetail {
    double db {0.0};          ///< diametro (mm)
    const char *inches {""};
    double a1 {0.0};          ///< chumbador - borda da placa (mm)
    double a2 {0.0};          ///< entre chumbadores (mm)
    double a3 {0.0};
    double h1 {0.0};          ///< comprimento de ancoragem
    double h2 {0.0};
    double r1 {0.0};
    double r2 {0.0};
    double holeDiameter {0.0};
    double washerThickness {0.0};
    double washerSize {0.0};
    double en {0.0};          ///< distancia do chumbador a borda do bloco (mm)
    double fckMin {0.0};      ///< MPa
    double blockMin {0.0};    ///< Nb,min (mm)
};
const std::vector<AnchorDetail> &anchorTable();
const AnchorDetail &anchorFor(double db);   ///< linha da tabela de diametro mais proximo (>= db, ou o maior)

/// Base de pilar de perfil I/H em bloco de concreto com chumbadores externos as mesas (base tipo 1, ABNT NBR 8800:2024, 6.7).
struct BasePlateInput {
    // pilar
    double d {300.0};
    double bf {150.0};
    double tw {6.0};
    double tf {10.0};
    SteelProps column;
    // chumbadores
    double anchorDiameter {25.0};
    int anchors {4};               ///< 4 a 8
    double anchorFub {400.0};
    // placa
    double plateThickness {25.0};
    SteelProps plate;
    // concreto e argamassa
    double fck {25.0};             ///< MPa
    double gammaC {1.4};
    double blockN {0.0};           ///< mm; 0 = minimo da Tabela 18
    double blockB {0.0};
    double frictionCoefficient {0.45};
    // solda pilar-placa
    double weldLeg {6.0};
    double weldFw {485.0};
    // nervuras de enrijecimento (base rigida com chapa de enrijecimento / base engastada); 0 = sem
    int ribs {0};                  ///< numero de nervuras (ao longo da largura da placa)
    double ribThickness {12.5};
    double ribHeight {150.0};      ///< altura sobre a placa (mm)
    double ribLength {100.0};      ///< comprimento a partir da mesa do pilar (mm)
    double ribWeldLeg {6.0};
    // esforcos de calculo
    double axial {0.0};            ///< N (+ tracao, - compressao)
    double moment {0.0};           ///< N.mm (eixo de maior inercia)
    double shear {0.0};            ///< N
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

struct BasePlateGeometry {
    double lx {0.0};   ///< dimensao da placa na direcao do momento (mm)
    double ly {0.0};
    double a {0.0};    ///< eixo da placa - linha de chumbadores (mm)
    double m {0.0}, n {0.0}, n0 {0.0};
    double blockN {0.0}, blockB {0.0};
    double sigmaCRd {0.0};   ///< MPa (6.6.5)
    std::string caseName;    ///< C1, C2, C3, T1, T2 ou T3
};
BasePlateGeometry basePlateGeometry(const BasePlateInput &input);

/// Placa retangular b x a (a = dimensao ao longo das bordas de comprimento b... ver abaixo) sob pressao uniforme q, com as bordas x = 0, x = b e y = 0
/// simplesmente apoiadas e a borda y = a livre (`farEdgeFree`) ou simplesmente apoiada: solucao de Levy da placa de Kirchhoff (nu = 0,3).
/// Devolve o maior momento fletor por unidade de largura, |M| / (q b^2), na placa inteira. Usada nas nervuras das bases de pilar: b = espacamento
/// das nervuras, a = comprimento das nervuras a partir da mesa do pilar (borda y = 0), borda y = a livre.
double plateMomentCoefficient(double aOverB, bool farEdgeFree, double nu = 0.3);

std::vector<Check> checkBasePlate(const BasePlateInput &input);
std::vector<Part> partsBasePlate(const BasePlateInput &input);

struct BasePlateProposal {
    bool found {false};
    BasePlateInput input;
    double maxRatio {0.0};
};
BasePlateProposal proposeBasePlate(BasePlateInput base);

} // namespace Structura::Design::Connections
