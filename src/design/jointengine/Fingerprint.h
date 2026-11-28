#pragma once

// FINGERPRINTS em tres niveis (J1; ver docs/architecture/joint_engine/FINGERPRINT_E_INVALIDACAO.md):
//   CFP configuracao fisica  : topologia + dados de configuracao (geometria, materiais, uniões, parametros constitutivos, contato, contorno);
//   AFP analise mecanica     : CFP + acoes + modelos/versoes + estrategia/parametros de solucao + hipoteses + origem dos parametros;
//   RFP resposta             : hash do CONTEUDO numerico de uma MechanicalResponse (sem proveniencia);
//   NFP classificacao        : RFP + norma + edicao + criterio + parametros estruturais.
// Mudar so a norma de classificacao muda so o NFP: a analise permanece valida. Serializacao canonica (ordenada, numeros em 17 digitos,
// ausente = "~"), SHA-256. Determinismo entre execucoes e independencia da ordem de partes/uniões/chaves.

#include "design/jointengine/Response.h"
#include "design/jointengine/Topology.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Structura::Design::JointEngine {

/// SHA-256 em hexadecimal minusculo.
[[nodiscard]] std::string sha256Hex(const std::string &data);

/// Dados de configuracao alem da topologia. `legacyValues` guarda valores de `NodeConnection::values`, CUJA ORIGEM NAO E REGISTRADA no legado.
struct ConfigurationData {
    std::map<std::string, Quantity> quantities;
    std::map<std::string, std::string> texts;
    std::map<std::string, double> legacyValues;
};

struct AnalysisData {
    ActionSet actions;
    std::vector<std::pair<std::string, std::string>> models;   ///< (id, versao) de cada modelo usado
    std::string strategy;
    SolveControls controls;
    std::map<std::string, Quantity> parameters;                ///< parametros constitutivos efetivos (com origem)
    std::vector<std::string> assumptions;
};

struct ClassificationKey {
    std::string standard;
    std::string edition;
    std::string criterion;
    std::map<std::string, Quantity> parameters;
};

[[nodiscard]] std::string configurationFingerprint(const JointTopology &topology, const ConfigurationData &data);
[[nodiscard]] std::string analysisFingerprint(const std::string &configurationFp, const AnalysisData &data);
[[nodiscard]] std::string responseFingerprint(const MechanicalResponse &response);
[[nodiscard]] std::string classificationFingerprint(const std::string &responseFp, const ClassificationKey &key);

struct FingerprintSet {
    std::string configuration;
    std::string analysis;
    std::string response;
    std::string classification;
};

/// O que esta obsoleto ao comparar o conjunto armazenado com o atual.
struct Staleness {
    bool analysis {false};
    bool classification {false};
    [[nodiscard]] bool current() const { return !analysis && !classification; }
};
/// CFP mudou => analise e classificacao obsoletas; so AFP => analise e classificacao pendentes de reexecucao; so NFP => so a classificacao.
/// Apos reexecutar a analise, a classificacao so e refeita se o novo RFP/NFP diferir (use `classificationFingerprint`).
[[nodiscard]] Staleness compare(const FingerprintSet &stored, const FingerprintSet &current);

} // namespace Structura::Design::JointEngine
