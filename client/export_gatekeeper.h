#pragma once
// Gatekeeper de Exportação e Finalização para Módulos Nível A Fora do Solver (M4).
// Base: SPEC.md (§7.1, §9), PLAN.md (M4) e Validação de Arquitetura Híbrida.
//
// Cobre os 4 módulos Nível A fora do solver:
// 1. ExportDxf: exportação DXF / desenhos
// 2. ExportPdf: geração de relatórios / memorial formatado
// 3. RunNormativeChecks: dimensionamento e verificações normativas (NBR 8800 / NBR 14762)
// 4. GenerateQuantities: quantitativos e levantamento de materiais
//
// Camadas de Defesa:
// 1. Despacho com Ponteiros Selados: A tabela m_dispatchTable guarda ponteiros selados com
//    (kDomainAppExport ^ s_mask). Unseal e invocação são estritamente efêmeros (sem cache em variável de longa duração).
// 2. Mistura Criptográfica de s nos Cálculos (Defesa Principal): Todas as transformações
//    geométricas, resistências de cálculo e densidades dependem do keystream de s. Se invocado
//    sem s válido ou com s adulterado, corrompe silenciosamente os resultados físicos (sem exceção perceptível).
// 3. Reseal / Lock Imediato: No logout, kill-switch ou falha de heartbeat, a tabela é trancada
//    de volta ao domínio de quarentena.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "license_client.h"
#include "../Structuradll/src/core/solver_security.h"

namespace structura_license {

enum ExportSlot : uint32_t {
    ExportSlot_Dxf = 0,
    ExportSlot_Pdf,
    ExportSlot_NormativeChecks,
    ExportSlot_Quantities,
    ExportSlot_Count
};

// --- Tipos de Dados dos Exporters ---

struct DxfBarInput {
    int id = 0;
    double x1 = 0.0, y1 = 0.0, z1 = 0.0;
    double x2 = 0.0, y2 = 0.0, z2 = 0.0;
};

struct DxfExportOutput {
    bool ok = false;
    size_t barCount = 0;
    double centerOfMass[3] = {0.0, 0.0, 0.0};
    double totalLength = 0.0;
    std::string dxfSnippet;
    std::string forensicWatermark; // Marca d'agua forense estampada (M6)
};

struct PdfSectionInput {
    int sectionId = 0;
    double maxBendingStress = 0.0; // MPa
    double maxAxialStress = 0.0;   // MPa
    double maxDisplacement = 0.0;  // mm
};

struct PdfExportOutput {
    bool ok = false;
    size_t sectionCount = 0;
    double reportedPeakStress = 0.0; // MPa
    double reportedDisplacement = 0.0; // mm
    std::string summaryText;
    std::string forensicWatermark; // Marca d'agua forense estampada (M6)
};

struct NormativeCheckInput {
    int memberId = 0;
    double axialForceSd = 0.0;   // N_Sd (kN)
    double bendingMomentSd = 0.0;// M_Sd (kN.m)
    double area = 0.005;         // A (m2)
    double plasticModulus = 0.0005; // Z (m3)
    double yieldStrengthFy = 345.0; // f_y (MPa)
};

struct NormativeCheckOutput {
    bool ok = false;
    double designAxialCapacityRd = 0.0; // N_Rd (kN)
    double designMomentCapacityRd = 0.0;// M_Rd (kN.m)
    double utilizationRatio = 0.0;      // max(N_Sd/N_Rd + M_Sd/M_Rd)
    bool compliant = false;             // ratio <= 1.0
    std::string reportSnippet;          // Memorial descritivo / resumo de calculo (M6)
    std::string forensicWatermark;      // Marca d'agua forense estampada (M6)
};

struct QuantityBarInput {
    int memberId = 0;
    double length = 0.0;      // m
    double area = 0.005;      // m2
    double density = 7850.0;  // kg/m3 (aço)
};

struct QuantityExportOutput {
    bool ok = false;
    double totalMassKg = 0.0;
    double totalVolumeM3 = 0.0;
    double totalLengthM = 0.0;
    std::string reportSnippet;          // Levantamento de materiais / resumo (M6)
    std::string forensicWatermark;      // Marca d'agua forense estampada (M6)
};

// Assinaturas das funções internas de exportação
using DxfExportFn = DxfExportOutput (*)(const std::vector<DxfBarInput>& bars,
                                       uint64_t s_mask,
                                       const uint8_t secretS[32]);

using PdfExportFn = PdfExportOutput (*)(const std::vector<PdfSectionInput>& sections,
                                       uint64_t s_mask,
                                       const uint8_t secretS[32]);

using NormativeCheckFn = NormativeCheckOutput (*)(const NormativeCheckInput& input,
                                                 uint64_t s_mask,
                                                 const uint8_t secretS[32]);

using QuantityExportFn = QuantityExportOutput (*)(const std::vector<QuantityBarInput>& bars,
                                                 uint64_t s_mask,
                                                 const uint8_t secretS[32]);

// --- Gatekeeper dos Módulos Externos ---
class ExportGatekeeper {
public:
    ExportGatekeeper();

    // Inicializa a tabela no handshake / load
    void Initialize(uint64_t nonce);

    // Reseala a tabela para o domínio dinâmico autenticado por s_mask
    bool Unlock(uint64_t nonce, uint64_t s_mask);

    // Tranca a tabela de volta para o domínio de quarentena (logout, kill-switch, heartbeat fail)
    bool Lock(uint64_t nonce);

    // Invocação com des-selagem efêmera e enforcement obrigatório do result_digest
    // Se o digest não bater com o Keystone K assinado pelo servidor, recusa com fail-closed (ok = false)
    // Versão com recomputação de digest a partir dos resultados atuais do modelo:
    DxfExportOutput ExecuteDxf(uint64_t nonce, const KeystoneArtifact& keystone,
                               const double* displacements, size_t dispCount,
                               const double* reactions, size_t reacCount,
                               const std::vector<DxfBarInput>& bars);

    // Versão com digest pré-calculado:
    DxfExportOutput ExecuteDxf(uint64_t nonce, const KeystoneArtifact& keystone,
                               const std::string& currentResultDigest,
                               const std::vector<DxfBarInput>& bars);

    // Versão legada sem digest -> fail-closed imediato (recusa sem verificação de integridade)
    DxfExportOutput ExecuteDxf(uint64_t nonce, const KeystoneArtifact& keystone,
                               const std::vector<DxfBarInput>& bars);

    PdfExportOutput ExecutePdf(uint64_t nonce, const KeystoneArtifact& keystone,
                               const double* displacements, size_t dispCount,
                               const double* reactions, size_t reacCount,
                               const std::vector<PdfSectionInput>& sections);

    PdfExportOutput ExecutePdf(uint64_t nonce, const KeystoneArtifact& keystone,
                               const std::string& currentResultDigest,
                               const std::vector<PdfSectionInput>& sections);

    PdfExportOutput ExecutePdf(uint64_t nonce, const KeystoneArtifact& keystone,
                               const std::vector<PdfSectionInput>& sections);

    NormativeCheckOutput ExecuteNormativeCheck(uint64_t nonce, const KeystoneArtifact& keystone,
                                               const double* displacements, size_t dispCount,
                                               const double* reactions, size_t reacCount,
                                               const NormativeCheckInput& input);

    NormativeCheckOutput ExecuteNormativeCheck(uint64_t nonce, const KeystoneArtifact& keystone,
                                               const std::string& currentResultDigest,
                                               const NormativeCheckInput& input);

    NormativeCheckOutput ExecuteNormativeCheck(uint64_t nonce, const KeystoneArtifact& keystone,
                                               const NormativeCheckInput& input);

    QuantityExportOutput ExecuteQuantities(uint64_t nonce, const KeystoneArtifact& keystone,
                                           const double* displacements, size_t dispCount,
                                           const double* reactions, size_t reacCount,
                                           const std::vector<QuantityBarInput>& bars);

    QuantityExportOutput ExecuteQuantities(uint64_t nonce, const KeystoneArtifact& keystone,
                                           const std::string& currentResultDigest,
                                           const std::vector<QuantityBarInput>& bars);

    QuantityExportOutput ExecuteQuantities(uint64_t nonce, const KeystoneArtifact& keystone,
                                           const std::vector<QuantityBarInput>& bars);

    // Ponteiros estáticos reais para verificação em testes de integridade
    static DxfExportOutput RealDxfExport(const std::vector<DxfBarInput>& bars,
                                        uint64_t s_mask,
                                        const uint8_t secretS[32]);

    static PdfExportOutput RealPdfExport(const std::vector<PdfSectionInput>& sections,
                                        uint64_t s_mask,
                                        const uint8_t secretS[32]);

    static NormativeCheckOutput RealNormativeCheck(const NormativeCheckInput& input,
                                                  uint64_t s_mask,
                                                  const uint8_t secretS[32]);

    static QuantityExportOutput RealQuantityExport(const std::vector<QuantityBarInput>& bars,
                                                  uint64_t s_mask,
                                                  const uint8_t secretS[32]);

private:
    std::array<uint64_t, ExportSlot_Count> dispatchTable_{};
    uint64_t currentNonce_ = 0;
    bool unlocked_ = false;
};

// Instância global ou de sessão para os serviços da UI
ExportGatekeeper& GetGlobalExportGatekeeper();

} // namespace structura_license
