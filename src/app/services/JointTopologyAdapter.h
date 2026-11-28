#pragma once

// ADAPTADOR da topologia do Joint Mechanical Engine (J1): DERIVA `JointTopology` e `ConfigurationData` a partir da ligacao legada
// (`NodeConnection` + catalogo) e das barras que chegam ao no. NAO altera `NodeConnection`, o catalogo, a persistencia ou o formato do projeto;
// nada e gravado. O que nao pode ser derivado vira `DataGap` (nunca preenchido por default) e os caminhos de carga NAO sao inferidos.
// Derivacao de nivel 1: partes e tipos de uniao conhecidos pelo catalogo; o pareamento exato das partes por uniao e os caminhos de carga
// ficam como lacunas declaradas.

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"
#include "design/jointengine/Capability.h"
#include "design/jointengine/Fingerprint.h"
#include "design/jointengine/Topology.h"

#include <vector>

namespace Structura::App::Services {

class JointTopologyAdapter
{
public:
    /// Barras (ids de barra do modelo) que chegam ao no `solverNodeId` (id do SOLVER, nao o externo). Ordem do modelo.
    [[nodiscard]] static std::vector<int> memberIdsAtNode(const Structura::App::SolverBridge::ModelData &model, int solverNodeId);

    /// Topologia derivada da ligacao. `memberIdsAtNode` = barras que chegam ao no (inclui `connection.memberId`); pode estar vazio (a barra
    /// da ligacao e sempre uma parte). Tipo desconhecido => `Unavailable` com justificativa.
    [[nodiscard]] static Structura::Design::JointEngine::JointTopology derive(const Structura::App::ProjectData::NodeConnection &connection,
                                                                              const std::vector<int> &memberIdsAtNode);

    /// Dados de configuracao: valores legados (origem nao registrada), tipo, extremidade, acos e, se informado, o E da viga.
    [[nodiscard]] static Structura::Design::JointEngine::ConfigurationData configurationData(const Structura::App::ProjectData::NodeConnection &connection,
                                                                                             double beamYoungModulusMPa /*NaN = ausente*/);

    /// Capacidade MECANICA da tipologia (separada da topologia): so ha modelo onde `JointComponentService::isSupported` (metodo dos componentes EN);
    /// todas as demais recebem estado explicito de indisponibilidade com justificativa e lista do que falta.
    [[nodiscard]] static Structura::Design::JointEngine::MechanicalCapability mechanicalCapability(const Structura::App::ProjectData::NodeConnection &connection);

    /// Fingerprint de configuracao (CFP) da ligacao: topologia derivada + dados.
    [[nodiscard]] static std::string configurationFingerprint(const Structura::App::ProjectData::NodeConnection &connection, const std::vector<int> &memberIdsAtNode,
                                                              double beamYoungModulusMPa);
};

} // namespace Structura::App::Services
