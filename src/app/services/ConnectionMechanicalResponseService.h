#pragma once

// INCREMENTO 3 — RESPOSTA MECÂNICA de uma ligação DEFINIDA (NodeConnection do módulo paramétrico): curva M–θ, rigidez inicial, resistência, componentes e hipóteses.
// Reutiliza o modelo mecânico existente (`JointComponentService::evaluate` = método dos componentes da EN 1993-1-8 sobre `JointComponents`) e o organiza na
// `MechanicalResponse` do JME com proveniência. NÃO cria formulação constitutiva alguma: onde não há modelo (tipologia sem método dos componentes, perfis T,
// dados ausentes) o resultado é INDISPONÍVEL e não contém números. A curva EN (6.3.1) é de FORMA FECHADA: não há iteração (convergência "não aplicável");
// a capacidade de rotação (EN 6.4) NÃO é calculada e o estado individual de cada componente ao longo da curva NÃO existe no modelo EN (a não linearidade da
// curva é do nó): estes itens são declarados indisponíveis, nunca inventados.

#include "app/ProjectDataTypes.h"
#include "design/JointClassification.h"
#include "design/jointengine/Response.h"

#include <string>
#include <vector>

namespace Structura::App::Services {

struct MechanicalComponentRow {
    std::string name;
    std::string symbol;               ///< "k1", "k4"...
    std::string clause;
    int row {0};                      ///< 0 = componente do nó; r >= 1 = fileira
    bool rigid {false};               ///< coeficiente infinito (não contribui para a flexibilidade)
    double stiffnessMm {0.0};         ///< ki (mm); só vale se !rigid
    double flexibilityPerMm {0.0};    ///< 1/ki (mm^-1); 0 se rígido
    double resistanceKn {-1.0};       ///< F,Rd (kN); < 0 = componente sem resistência própria no modelo
    std::string failureMode;
    bool governingInRow {false};      ///< fileira: componente de maior flexibilidade entre os da PRÓPRIA fileira (série)
    double shareInRow {0.0};          ///< fileira: fração (0 a 1) da flexibilidade da fileira (1/keff,r) atribuída a este componente
    double shareOfTotal {0.0};        ///< componente do nó: fração (0 a 1) da flexibilidade total 1/k1 + 1/k2 + ... + 1/keq (EN 6.3.1)
};

struct MechanicalRowRow {
    int row {0};
    double leverMm {0.0};             ///< h (mm)
    double keffMm {0.0};
    double individualKn {-1.0};       ///< Ft,r,Rd isolada
    double effectiveKn {0.0};         ///< após a distribuição (6.2.7.2)
    std::string limitedBy;
};

/// Estado do NÓ em um nível de momento (a curva EN é do nó; não há estado individual de componente).
struct MechanicalLevel {
    double momentKnm {0.0};
    double ratioToResistance {0.0};   ///< M / Mj,Rd
    double rotationRad {0.0};
    double secantKnmPerRad {0.0};
    std::string state;                ///< "elástico (M ≤ 2/3 Mj,Rd)" | "não linear (EN 6.3.1, 2/3 Mj,Rd < M ≤ Mj,Rd)"
};

struct ConnectionMechanicalResult {
    Structura::Design::JointClass::Direction direction {Structura::Design::JointClass::Direction::Positive};   ///< sentido do momento desta resposta (INCREMENTO 5)
    std::string directionLabel;       ///< "positivo (mesa superior tracionada)" | "negativo (mesa inferior tracionada)"
    bool symmetricGeometry {false};   ///< a entrada fisica deste sentido e identica a do oposto (demonstrado pela configuracao)
    std::string participants;         ///< fileiras tracionadas / mesa que participam deste sentido
    bool available {false};           ///< há resposta calculada (curva M–θ)
    bool resistanceOnly {false};      ///< so RESISTENCIA (Mj,Rd da NBR, nivel C): sem rigidez nem curva (Incremento 6)
    bool partial {false};             ///< só rigidez (resistência não determinada): `StiffnessOnly`
    Structura::Design::JointEngine::ValidityStatus status {Structura::Design::JointEngine::ValidityStatus::NotYetAvailable};
    std::string message;              ///< por que não há resultado, ou observação
    std::string family;               ///< descrição da tipologia mecânica
    Structura::Design::JointEngine::MechanicalResponse response;
    double initialStiffnessKnm {0.0};   ///< Sj,ini (kN.m/rad)
    double resistanceKnm {0.0};         ///< Mj,Rd (kN.m), método dos componentes
    double leverArmMm {0.0};
    double nbrResistanceKnm {0.0};      ///< Mj,Rd pelas verificações da NBR da ligação cadastrada (0 = não determinado)
    bool nbrBelowCurve {false};         ///< a ligação cadastrada NÃO sustenta o Mj,Rd da curva
    double rowsShareOfTotal {0.0};      ///< fração da flexibilidade total atribuída ao conjunto de fileiras (1/keq)
    std::string governingContribution;  ///< maior contribuinte à flexibilidade total (componente do nó ou "fileiras")
    std::vector<MechanicalComponentRow> components;
    std::vector<MechanicalRowRow> rows;
    std::vector<MechanicalLevel> levels;
    std::vector<std::string> hypotheses;
    std::vector<std::string> limitations;
    std::vector<std::string> memory;    ///< memória do cálculo (a do serviço de componentes)
    std::string configurationFingerprint;   ///< CFP da ligação (12 primeiros caracteres)
    std::string analysisFingerprint;        ///< AFP: CFP + modelo/versão + parâmetros do modelo (12 primeiros caracteres)
};

/// As duas respostas de uma ligacao, SEPARADAS e identificadas (nunca uma curva espelhada da outra): cada sentido tem modelo, proveniencia e fingerprints proprios.
struct ConnectionMechanicalBoth {
    ConnectionMechanicalResult positive;   ///< mesa superior tracionada
    ConnectionMechanicalResult negative;   ///< mesa inferior tracionada
    bool symmetryDemonstrated {false};     ///< os dois sentidos foram calculados e as entradas fisicas sao identicas (nao e presuncao)
    std::string note;
    [[nodiscard]] const ConnectionMechanicalResult &of(Structura::Design::JointClass::Direction d) const { return d == Structura::Design::JointClass::Direction::Positive ? positive : negative; }
};

class ConnectionMechanicalResponseService
{
public:
    /// `youngModulusMPa` = E do aço da viga (OBRIGATÓRIO e > 0: sem E não há resposta; nenhum valor padrão é assumido). `curvePoints` >= 2.
    [[nodiscard]] static ConnectionMechanicalResult evaluate(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, int curvePoints);
    /// Resposta de UM sentido do momento (o de cima e o sentido positivo, com resultados identicos aos do Incremento 3).
    [[nodiscard]] static ConnectionMechanicalResult evaluate(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, int curvePoints, Structura::Design::JointClass::Direction direction);
    /// Os dois sentidos, calculados separadamente. Um sentido que o modelo nao permite calcular fica indisponivel (sem numeros); nada e espelhado.
    [[nodiscard]] static ConnectionMechanicalBoth evaluateBoth(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, int curvePoints);
};

} // namespace Structura::App::Services
