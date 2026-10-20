#pragma once

#include "design/CompositeFloor.h"
#include "design/Connections.h"
#include "design/ShearConnections.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

/// Ligacao mista de aco e concreto (viga mista semicontinua sob momento negativo): ABNT NBR 8800:2024, Anexo O e L.5.2.2.
/// Componentes: armadura da laje (O.2.3), conectores de cisalhamento na regiao de momento negativo (O.2.4) e ligacao metalica da mesa
/// inferior (O.2.5.2): chapa de extremidade de altura total (O.1), cantoneiras na alma e na mesa inferior (O.2), chapa simples na alma e
/// cantoneira na mesa inferior (O.3) ou apenas cantoneira na mesa inferior (O.4). A ligacao da alma so transmite o cortante (O.2.5.1).
/// Unidades: N, mm, MPa.
struct CompositeConnectionInput {
    enum Variant { EndPlate = 0, AnglesWeb = 1, PlateWeb = 2, BottomAngleOnly = 3 };
    enum Support { Column = 0, Beam = 1 };

    int variant {AnglesWeb};
    int support {Column};

    // perfil de aco (secao duplamente simetrica)
    double d {400.0}, bf {180.0}, tf {12.5}, tw {8.0}, radius {0.0};
    double area {0.0};              ///< mm2 (0 = pelas dimensoes)
    double inertia {0.0};           ///< Ia, mm4 (0 = pelas dimensoes)
    double fy {250.0}, fu {400.0}, steelE {200000.0};
    bool welded {false};            ///< perfil soldado: verifica a solda mesa-alma (O.3.2-b)
    double weldLeg {6.0};           ///< filete mesa-alma do perfil soldado (mm)
    double weldFw {485.0};

    // laje e concreto
    Composite::ConcreteInput concrete;
    Composite::SlabKind slabKind {Composite::SlabKind::Solid};
    double tc {120.0};              ///< altura da laje de concreto (acima da forma, se houver) (mm)
    double hF {0.0};                ///< altura da nervura da forma (0 = laje macica)
    double ribWidth {0.0};          ///< largura media da nervura da forma (mm), para Rg dos conectores em nervuras paralelas
    double span {8000.0};           ///< vao do tramo ligado (L/dt do Anexo O.4)
    double leftSpan {8000.0}, rightSpan {8000.0};   ///< vaos adjacentes ao no (zero de momento = 1/4 da soma, L.2.2.2-b)
    double leftSpacing {3000.0}, rightSpacing {3000.0};   ///< distancia ao eixo da viga adjacente de cada lado

    // armadura
    int bars {8};
    double barDiameter {12.5};      ///< CA-50, minimo 12,5 mm (O.2.3.3)
    double barSpacing {100.0};
    double fys {500.0};             ///< fyk (MPa)
    double fts {540.0};             ///< resistencia a ruptura (MPa)
    double epsSu {0.05};            ///< deformacao na ruptura da armadura isolada
    double rebarE {210000.0};
    double gammaS {1.15};
    double y {80.0};                ///< topo do perfil ao centro da armadura (mm)
    double transverseArea {0.0};    ///< armadura transversal de cada lado do pilar (mm2)

    // apoio
    double ha {260.0};              ///< largura do elemento de apoio, paralela a armadura (mm)
    double bc {260.0};              ///< largura do pilar na direcao transversal as vigas (mm)
    double a {120.0};               ///< primeiro conector - face do apoio (mm)
    bool stiffeners {true};         ///< par de enrijecedores na alma do pilar no nivel da mesa inferior (O.2.5.2.1)
    double stiffenerArea {0.0};     ///< area do par de enrijecedores (mm2)
    double columnD {260.0}, columnBf {260.0}, columnTf {12.5}, columnTw {8.0}, columnFy {250.0};

    // conectores na regiao de momento negativo
    Composite::StudInput studs;
    int connectors {0};             ///< n, entre a secao de momento maximo negativo e a de momento nulo (0 = pelo espacamento e 15 % do vao)
    double connectorSpacing {200.0};
    double krOverride {0.0};        ///< kN/mm; 0 = Tabela de O.2.4.1
    double l1 {0.0};                ///< 0 = 15 % do vao

    // ligacao metalica da mesa inferior
    double db {20.0};               ///< parafusos da cantoneira inferior
    BoltGrade grade;
    bool threadInShearPlane {true};
    int rowsL {2};                  ///< linhas de parafusos na aba horizontal da cantoneira (2 parafusos por linha)
    double boltSpacing {70.0};      ///< S, na direcao da forca
    double boltGauge {60.0};
    double boltEnd {35.0};
    double angleLeg {150.0};        ///< largura da aba horizontal (transversal a forca) (mm)
    double angleLegVertical {100.0};   ///< altura da aba vertical (mm)
    double angleT {16.0};
    double angleFy {250.0}, angleFu {400.0};
    int verticalBolts {2};          ///< parafusos da aba vertical (apoio): resistem ao cortante em O.4
    double verticalLever {50.0};    ///< eixo dos parafusos da aba horizontal - face da aba vertical (mm)
    bool endPlateCjp {true};        ///< chapa de extremidade: solda de penetracao total ou filete duplo
    double endPlateWeldLeg {10.0};  ///< filete duplo da mesa inferior a chapa (mm)

    // ligacao da alma (somente cortante, O.2.5.1)
    double webT {8.0};              ///< espessura da cantoneira, da chapa simples ou da chapa de extremidade (mm)
    double webDb {20.0};
    double webFy {250.0};
    double webLc {240.0};           ///< altura da ligacao da alma (mm)
    double webBottom {60.0};        ///< borda inferior da ligacao da alma - face inferior da viga (mm)
    double webPlateWeldLeg {6.0};   ///< filete da chapa simples ao apoio (O.3)
    bool webSlotted {true};         ///< furos pouco alongados na direcao do eixo (O.3)
    double webEndDistance {40.0};   ///< furo - extremidade da chapa na direcao do eixo (O.3)
    bool webPretensioned {true};    ///< parafusos da alma com pre-tensao (O.3)

    // carregamento e rotacao requerida (O.4)
    double betaVm {0.95};
    int loading {0};                ///< 0 = carga distribuida ou uma concentrada; 1 = duas concentradas nos tercos
    bool shored {false};
    double positiveMoment {0.0};    ///< M+Rd da viga mista no vao (N.mm); 0 = calculado automaticamente pela L.2.3.1 com os dados da laje e dos conectores do vao
    double positiveSpacing {150.0}; ///< espacamento dos conectores na regiao de momento positivo (mm)

    // solicitacoes (N, N.mm)
    double moment {0.0};
    double shear {0.0};
    double axial {0.0};

    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
};

struct CompositeConnectionResult {
    std::vector<Check> checks;
    std::vector<std::string> notes;
    double bEff {0.0};
    double as {0.0};
    double fsRd {0.0};
    double fcsRd {0.0};
    double fiRd {0.0};
    double mRd {0.0};
    double si {0.0};          ///< rigidez inicial (N.mm/rad)
    double thetaU {0.0};      ///< capacidade de rotacao (rad)
    double thetaRequired {0.0};
    double yLnp {0.0};
};

CompositeConnectionResult checkCompositeConnection(const CompositeConnectionInput &input);
std::vector<Part> partsCompositeConnection(const CompositeConnectionInput &input);

/// M+Rd da viga mista no vao (L.2.3.1, interacao completa ou parcial, com betaVm), pelos mesmos dados da ligacao: perfil, laje, conectores do vao e
/// larguras efetivas das regioes de momento positivo (L.2.2.2-a: 7/10 do vao em vao interno, 4/5 em vao extremo). N.mm; 0 se nao ha laje.
double positiveMomentResistance(const CompositeConnectionInput &input, std::string *memo = nullptr);

/// Rotacao necessaria (mrad) das Tabelas O.1 a O.3 com interpolacao linear em L/dt, fy e betaVm (O.4); false se fora da faixa tabelada.
bool requiredRotation(double lOverD, double fy, double betaVm, int loading, double &mrad);

} // namespace Structura::Design::Connections
