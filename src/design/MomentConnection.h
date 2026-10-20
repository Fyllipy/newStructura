#pragma once

#include "design/WebShearConnection.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Ligacao rigida viga-pilar (transmite momento): mesas da viga soldadas ao pilar (penetracao total ou filetes) ou ligadas por perfis T
/// parafusados (mesa tracionada com efeito de alavanca); alma por solda direta ao pilar, tala simples ou dupla tala.
/// O momento vira um binario nas mesas: F = M/(d - tf) +- N/2. NBR 8800:2024 (5.7, 6.2, 6.3, 6.5). Unidades: N, mm, MPa.
struct MomentConnectionInput {
    enum FlangeMode { Welded = 0, TStub = 1 };
    enum WebMode { DirectWeld = 0, SinglePlate = 1, DoublePlate = 2 };

    int flangeMode {Welded};
    int webMode {SinglePlate};
    BoltGrade grade;
    double db {20.0};
    bool threadInShearPlane {true};

    // viga
    double beamDepth {300.0};
    double beamBf {150.0};
    double beamTf {10.0};
    double beamTw {6.0};
    SteelProps beam;

    // mesas soldadas ao pilar
    bool flangeFillet {false};       ///< false = solda de penetracao total; true = filetes nos dois lados da mesa
    double flangeWeldLeg {8.0};
    double weldFw {485.0};
    double webWeldLeg {6.0};         ///< alma soldada direto no pilar (webMode = DirectWeld)

    // perfis T (flangeMode = TStub): um em cada mesa da viga
    double stubStemThickness {12.5};
    double stubFlangeThickness {16.0};
    double stubWidth {150.0};        ///< largura da aba do T (do lado do pilar), mm
    SteelProps stub;
    int stubColumnBoltsPerRow {2};   ///< parafusos por fileira na aba (ligados ao pilar)
    int stubColumnRows {1};          ///< fileiras de cada lado da alma do T
    double stubColumnPitch {70.0};
    double stubGauge {90.0};
    double stubB {40.0};             ///< eixo do parafuso - face da alma do T
    double stubA {35.0};             ///< eixo do parafuso - borda da aba
    int stubBeamBolts {4};           ///< parafusos do T na mesa da viga (alma do T)
    double stubBeamPitch {70.0};
    double stubBeamEnd {40.0};
    double stubBeamGauge {60.0};     ///< entre as duas colunas de parafusos da alma do T na mesa da viga

    // tala(s) de alma (webMode = SinglePlate / DoublePlate): verificadas como ligacao de cortante
    WebShearInput web;

    // pilar
    double colDepth {260.0};
    double colBf {146.0};
    double colTf {12.7};
    double colTw {7.6};
    double colRadius {10.0};
    SteelProps column;
    double colUnbraced {0.0};
    double doubler {0.0};            ///< chapa de reforco do painel de alma (mm), soma dos dois lados
    bool panelStiffened {false};     ///< enrijecedores transversais do pilar alinhados as mesas da viga (entram no kv do painel)
    double colAxial {0.0};           ///< NSd do pilar (N, modulo), p/ o painel de alma (5.7.7.1)
    double colShear {0.0};           ///< cortante ja atuante no pilar (N), somado algebricamente a forca das mesas
    double panelShear {-1.0};        ///< FSd do painel pelo equilibrio do no (N); >= 0 substitui forca das mesas - colShear

    double moment {0.0};
    double shear {0.0};
    double axial {0.0};              ///< + tracao
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

std::vector<Check> checkMomentConnection(const MomentConnectionInput &input);
std::vector<Part> partsMomentConnection(const MomentConnectionInput &input);

} // namespace Structura::Design::Connections
