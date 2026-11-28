#pragma once

#include "app/ProjectDataTypes.h"
#include "design/JointClassification.h"
#include "design/JointComponents.h"

#include <string>
#include <vector>

namespace Structura::App::Services {

/// MODELO FISICO de uma ligacao do projeto (`NodeConnection`, do modulo de ligacoes): um unico conjunto de dados (geometria e acos cadastrados) alimenta a rigidez
/// (EN 1993-1-8, 6.3), a resistencia por componentes (6.2) e a curva momento-rotacao (6.3.1). Alem disso, o momento resistente e confrontado com as
/// verificacoes da NBR 8800:2024 que o programa faz para a mesma ligacao (NIVEL C): elas NAO alteram a curva (nivel A); apenas sinalizam, em `nbrBelowCurve`,
/// que a ligacao cadastrada nao sustenta o Mj,Rd da curva.
///
/// O modelo estrutural equivalente (liberacao, mola, rotula bilinear) e derivado daqui por `Structura::Design::JointComponents::deriveEquivalent`
/// e nunca altera este resultado.
class JointComponentService
{
public:
    /// Dados das barras ligadas (perfis I/H) para o MODELO PRELIMINAR: nenhuma ligacao dimensionada e necessaria.
    struct PreliminaryMembers {
        double beamDepth {0.0}, beamBf {0.0}, beamTf {0.0}, beamTw {0.0}, beamFy {250.0};   ///< mm, MPa
        double colDepth {0.0}, colBf {0.0}, colTf {0.0}, colTw {0.0}, colRadius {0.0}, colFy {250.0};
        double youngModulusMPa {200000.0};
    };
    /// Configuracao de REFERENCIA do modelo preliminar (0 = regra automatica). Nao e uma ligacao dimensionada.
    struct PreliminarySettings {
        int kind {1};                 ///< 0 = mesas soldadas; 1 = chapa de extremidade (fileiras na extensao)
        double plateThickness {0.0};  ///< mm (0 = igual ao diametro do parafuso)
        double boltDiameter {0.0};    ///< mm (0 = 20)
        int tensionRows {0};          ///< fileiras na extensao (0 = 1)
        double beta {1.0};            ///< parametro de transformacao: 1 = viga de um lado; 0 = vigas dos dois lados, momentos iguais
        bool webStiffened {false};
        double doublerThickness {0.0};
    };

    /// INCREMENTO 6: ligacao COMPOSTA EM SERIE (ex.: viga-pilar flangeada fora do no = mesas soldadas ao pilar + emenda por chapas de extremidade). Cada parte e uma ligacao
    /// com modelo proprio; a rotacao total e a soma das rotacoes (1/S = soma 1/Si) e o momento resistente e o menor (a junta falha pela parte mais fraca).
    struct Part {
        std::string label;
        double stiffnessKnm {0.0};    ///< Sj,ini da parte (kN.m/rad)
        double resistanceKnm {0.0};   ///< Mj,Rd da parte (kN.m)
        std::string governing;        ///< componente de teto da parte
    };

    struct Result {
        std::vector<Part> parts;              ///< vazio = ligacao simples; 2 partes = composicao em serie (Incremento 6)
        bool supported {false};
        /// INCREMENTO 6: so a RESISTENCIA (Mj,Rd pelas verificacoes da NBR 8800:2024 da propria ligacao, nivel C). Sem rigidez, sem curva M-theta: o metodo dos componentes
        /// da tipologia nao esta implementado (ex.: perfis T). Nao e resposta mecanica por componentes.
        bool resistanceOnly {false};
        std::string nbrGoverning;             ///< verificacao da NBR que governa Mj,Rd (nivel C)
        std::string message;                  ///< por que nao ha resultado (tipologia sem modelo) ou observacao
        std::string fingerprint;              ///< identifica a ligacao e o E usados: muda quando o usuario altera qualquer dado
        Structura::Design::JointClass::JointBehavior behavior;
        Structura::Design::JointComponents::StiffnessResult stiffness;
        Structura::Design::JointComponents::Resistance resistance;   ///< por componentes (EN)
        Structura::Design::JointComponents::CurveMetrics metrics;
        double momentResistanceKnm {0.0};     ///< Mj,Rd da curva (nivel A) = componentes EN; so se estes falham usa-se o da NBR
        double momentResistanceEnKnm {0.0};   ///< por componentes (EN 1993-1-8, 6.2.7)
        double momentResistanceNbrKnm {0.0};  ///< verificacoes da NBR 8800:2024 da ligacao (V = N = 0); 0 = nao determinado
        Structura::Design::JointClass::Direction direction {Structura::Design::JointClass::Direction::Positive};   ///< sentido do momento deste resultado
        bool symmetricGeometry {false};       ///< a entrada fisica deste sentido e IDENTICA a do sentido oposto (simetria demonstrada pela configuracao, nao presumida)
        std::string directionNote;            ///< como o sentido foi montado (mesa tracionada, fileiras participantes)
        std::string family;                   ///< descricao da tipologia
        bool preliminary {false};             ///< modelo preliminar (configuracao de referencia, nao dimensionada)
        std::vector<std::string> hypotheses;  ///< hipoteses e dominio de validade do modelo preliminar
        bool nbrCheckAvailable {false};       ///< NIVEL C: as verificacoes da NBR da ligacao cadastrada foram avaliadas
        bool nbrBelowCurve {false};           ///< NIVEL C: Mj,Rd das verificacoes da NBR < Mj,Rd da curva (a ligacao cadastrada nao sustenta a curva)
        std::vector<std::string> memory;      ///< memoria do calculo (componentes, Mj,Rd, curva)
    };

    /// `youngModulusMPa`: E do aco da viga (modelo). Ligacoes de tipologia sem modelo de componentes devolvem `supported = false`.
    [[nodiscard]] static Result evaluate(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa);

    /// INCREMENTO 5: resposta de UM sentido do momento. Positivo = mesa SUPERIOR tracionada (fileiras da extensao da chapa acima da mesa); negativo = mesa INFERIOR
    /// tracionada (fileiras abaixo da mesa inferior, que no sentido positivo sao as da zona comprimida). O sentido negativo NAO e o espelho presumido da curva positiva:
    /// monta-se o modelo fisico com as fileiras realmente existentes naquele lado (e as verificacoes da NBR com as zonas trocadas). `evaluate(c, E)` = sentido positivo, inalterado.
    [[nodiscard]] static Result evaluate(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa, Structura::Design::JointClass::Direction direction);

    /// MODELO PRELIMINAR (nivel A): comportamento rotacional a partir dos perfis ligados e de uma configuracao de referencia com hipoteses declaradas,
    /// sem exigir ligacao dimensionada. O Mj,Rd e o do modelo de referencia (EN 1993-1-8, 6.2); nao ha verificacao construtiva (nivel C).
    [[nodiscard]] static Result evaluatePreliminary(const PreliminaryMembers &members, const PreliminarySettings &settings);

    /// A tipologia (tipo da ligacao) tem modelo de componentes implementado?
    [[nodiscard]] static bool isSupported(const Structura::App::ProjectData::NodeConnection &connection);
    /// Tipologia viga-pilar com so a resistencia (nivel C, NBR 8800:2024): perfis T parafusados (143).
    [[nodiscard]] static bool hasResistanceOnlyModel(const Structura::App::ProjectData::NodeConnection &connection);

    /// Identificador do estado da ligacao (tipo, extremidade, parafuso, todos os valores) e do E: serve para invalidar resultados dependentes.
    [[nodiscard]] static std::string fingerprint(const Structura::App::ProjectData::NodeConnection &connection, double youngModulusMPa);
};

} // namespace Structura::App::Services
