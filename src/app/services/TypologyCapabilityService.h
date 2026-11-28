#pragma once

// INCREMENTO 2: o que o programa REALMENTE oferece para cada tipologia de ligação — separado em quatro capacidades independentes, sem restringir a escolha:
//   (1) configurável (o módulo paramétrico define seus componentes);
//   (2) dimensionamento normativo disponível (há verificações de resistência);
//   (3) modelo mecânico disponível (há modelo fundamentado de rigidez/resistência da ligação, hoje o método dos componentes EN para algumas tipologias);
//   (4) geração de curva M–θ disponível (só onde há modelo mecânico que a produz).
// A lista é a MESMA do configurador existente (tipos legados + `ConnectionCatalog`); nada é duplicado. As capacidades vêm de `ConnectionCatalog`, do serviço
// de componentes e do adaptador do JME (`JointTopologyAdapter::mechanicalCapability`). O comportamento requerido NÃO filtra a lista.

#include "app/ProjectDataTypes.h"
#include "design/jointengine/Capability.h"

#include <QString>
#include <QVector>

#include <string>
#include <vector>

namespace Structura::App::Services {

struct TypologyRow {
    int type {0};                 ///< id do configurador (tipo legado 0..7 ou id >= 100 do catálogo)
    QString name;
    QString group;
    bool configurable {true};
    bool normativeDesign {false};
    Structura::Design::JointEngine::ValidityStatus mechanical {Structura::Design::JointEngine::ValidityStatus::NotYetAvailable};
    std::string mechanicalModelId;   ///< vazio se não há modelo mecânico
    bool momentRotation {false};     ///< curva M–θ disponível (só com modelo mecânico que a produz)
    std::string justification;
    std::vector<std::string> missingFoundations;
};

/// Ligação já definida no nó (módulo paramétrico) com o que o JME deriva dela.
struct DefinedConnection {
    int id {0};
    int memberId {0};
    int type {0};
    QString typeName;
    std::string topologyStatus;       ///< Complete | Partial | Unavailable
    int topologyGaps {0};
    Structura::Design::JointEngine::ValidityStatus mechanical {Structura::Design::JointEngine::ValidityStatus::NotYetAvailable};
    bool momentRotation {false};
    std::string mechanicalModelId;    ///< modelo que responde (EnComponentMethod | NbrResistanceOnly | vazio)
    std::string fingerprint;          ///< CFP (12 primeiros caracteres)
};

class TypologyCapabilityService
{
public:
    /// Todas as tipologias configuráveis (tipos legados do configurador + catálogo), cada uma com as quatro capacidades.
    [[nodiscard]] static std::vector<TypologyRow> list();
    /// Capacidades de uma tipologia (tipo legado ou id do catálogo).
    [[nodiscard]] static TypologyRow row(int type);
    /// Descreve as ligações cadastradas no nó pelo JME (topologia derivada, capacidade, fingerprint). `memberIdsAtNode` = barras que chegam ao nó.
    [[nodiscard]] static std::vector<DefinedConnection> describeAtNode(const QVector<Structura::App::ProjectData::NodeConnection> &connections, int nodeId,
                                                                       const std::vector<int> &memberIdsAtNode, double beamYoungModulusMPa);
};

} // namespace Structura::App::Services
