// Implementação do Gatekeeper de Exportação e Finalização (M4).
// Base: SPEC.md (§7.1, §9), PLAN.md (M4) e Validação de Arquitetura Híbrida.

#include "export_gatekeeper.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <sstream>

namespace structura_license {

namespace {

// Computa o desvio de poison a partir do segredo s da sessão:
// - Se s_mask == 0 ou secretS == nullptr -> desvio severo (0.50).
// - Se houver inconsistência entre s_mask e o vetor s -> desvio (0.35).
// - Se for genuíno e consistente -> 0.0 (cálculo 100% exato e pristino).
inline double ComputePoisonDrift(uint64_t s_mask, const uint8_t secretS[32]) noexcept {
    if (s_mask == 0 || secretS == nullptr) {
        return 0.50; // desvio não-linear de 50%
    }
    uint64_t check = 0;
    std::memcpy(&check, secretS, sizeof(check));
    if (check != s_mask) {
        return 0.35; // desvio por inconsistência de keystream
    }
    // Verificação de entropia básica (segredo não pode ser todo zero)
    uint64_t accumulator = 0;
    for (size_t i = 0; i < 32; i += 8) {
        uint64_t chunk = 0;
        std::memcpy(&chunk, secretS + i, sizeof(chunk));
        accumulator ^= chunk;
    }
    if (accumulator == 0) {
        return 0.40;
    }
    return 0.0;
}

} // namespace

ExportGatekeeper::ExportGatekeeper() {
    Initialize(0x1337C0DEULL);
}

void ExportGatekeeper::Initialize(uint64_t nonce) {
    currentNonce_ = nonce;
    unlocked_ = false;

    // Inicialização da tabela em quarentena (fail-closed imediato)
    const uint64_t quatDomain = solver_sec::kDomainAppExport ^ solver_sec::kQuarantineMask;

    dispatchTable_[ExportSlot_Dxf] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealDxfExport),
        currentNonce_,
        ExportSlot_Dxf,
        quatDomain);

    dispatchTable_[ExportSlot_Pdf] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealPdfExport),
        currentNonce_,
        ExportSlot_Pdf,
        quatDomain);

    dispatchTable_[ExportSlot_NormativeChecks] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealNormativeCheck),
        currentNonce_,
        ExportSlot_NormativeChecks,
        quatDomain);

    dispatchTable_[ExportSlot_Quantities] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealQuantityExport),
        currentNonce_,
        ExportSlot_Quantities,
        quatDomain);
}

bool ExportGatekeeper::Unlock(uint64_t nonce, uint64_t s_mask) {
    if (nonce == 0 || s_mask == 0) {
        return false;
    }
    currentNonce_ = nonce;
    const uint64_t dynamicDomain = solver_sec::kDomainAppExport ^ s_mask;

    // Reseala a tabela usando os ponteiros reais originais do gatekeeper
    dispatchTable_[ExportSlot_Dxf] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealDxfExport),
        currentNonce_,
        ExportSlot_Dxf,
        dynamicDomain);

    dispatchTable_[ExportSlot_Pdf] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealPdfExport),
        currentNonce_,
        ExportSlot_Pdf,
        dynamicDomain);

    dispatchTable_[ExportSlot_NormativeChecks] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealNormativeCheck),
        currentNonce_,
        ExportSlot_NormativeChecks,
        dynamicDomain);

    dispatchTable_[ExportSlot_Quantities] = solver_sec::sealPointer(
        reinterpret_cast<uintptr_t>(&RealQuantityExport),
        currentNonce_,
        ExportSlot_Quantities,
        dynamicDomain);

    unlocked_ = true;
    return true;
}

bool ExportGatekeeper::Lock(uint64_t nonce) {
    Initialize(nonce != 0 ? nonce : currentNonce_);
    return true;
}

// Invocação Efêmera de ExportDxf
DxfExportOutput ExportGatekeeper::ExecuteDxf(uint64_t nonce, const KeystoneArtifact& keystone,
                                            const double* displacements, size_t dispCount,
                                            const double* reactions, size_t reacCount,
                                            const std::vector<DxfBarInput>& bars) {
    std::string digestAtual = ComputeResultDigest(displacements, dispCount, reactions, reacCount);
    return ExecuteDxf(nonce, keystone, digestAtual, bars);
}

DxfExportOutput ExportGatekeeper::ExecuteDxf(uint64_t nonce, const KeystoneArtifact& keystone,
                                            const std::string& currentResultDigest,
                                            const std::vector<DxfBarInput>& bars) {
    // M6: Checagem de expiração do Keystone (Kill switch: TTL 300s)
    int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
    if (keystone.exp > 0 && nowSec > keystone.exp) {
        Lock(currentNonce_);
        return {}; // fail-closed: Keystone expirado
    }

    // 0. Enforcement estrito do result_digest, varredura anti-debug e poison de VM (M5/M6)
    uint64_t exportPoison = CheckExeAntiDebugPoison();
    uint64_t vmPoison = CheckVmPoison(GetActiveClientSession().vmAllowToken);
    if (!EnforceResultDigest(keystone, currentResultDigest) || exportPoison != 0ULL || vmPoison != 0ULL) {
        if (exportPoison != 0ULL || vmPoison != 0ULL) {
            Lock(currentNonce_);
        }
        return {}; // fail-closed: digest diverge ou processo sob debugger/VM nao autorizada
    }

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystone.sessionSecretS.data(), sizeof(s_mask));

    // 1. Unseal efêmero
    uint64_t sealed = dispatchTable_[ExportSlot_Dxf];
    uint64_t dynamicDomain = solver_sec::kDomainAppExport ^ s_mask;
    auto fn = solver_sec::unsealPointer<DxfExportFn>(sealed, nonce, ExportSlot_Dxf, dynamicDomain);

    // 2. Invocação imediata
    DxfExportOutput out;
    if (fn != nullptr) {
        out = fn(bars, s_mask, keystone.sessionSecretS.data());
    }

    // M6: Estampa a marca d'água forense assinada pelo servidor
    if (out.ok && !keystone.watermarkToken.empty()) {
        out.forensicWatermark = keystone.watermarkToken;
        out.dxfSnippet += "999\nSTRUCTURA-FORENSIC-WM: " + keystone.watermarkToken + "\n";
    }

    // 3. Destruição imediata da stack
    volatile uintptr_t wipe = reinterpret_cast<uintptr_t>(fn);
    (void)wipe;
    fn = nullptr;
    return out;
}

DxfExportOutput ExportGatekeeper::ExecuteDxf(uint64_t nonce, const KeystoneArtifact& keystone,
                                            const std::vector<DxfBarInput>& bars) {
    return ExecuteDxf(nonce, keystone, std::string{}, bars);
}

// Invocação Efêmera de ExportPdf
PdfExportOutput ExportGatekeeper::ExecutePdf(uint64_t nonce, const KeystoneArtifact& keystone,
                                            const double* displacements, size_t dispCount,
                                            const double* reactions, size_t reacCount,
                                            const std::vector<PdfSectionInput>& sections) {
    std::string digestAtual = ComputeResultDigest(displacements, dispCount, reactions, reacCount);
    return ExecutePdf(nonce, keystone, digestAtual, sections);
}

PdfExportOutput ExportGatekeeper::ExecutePdf(uint64_t nonce, const KeystoneArtifact& keystone,
                                            const std::string& currentResultDigest,
                                            const std::vector<PdfSectionInput>& sections) {
    // M6: Checagem de expiração do Keystone (Kill switch: TTL 300s)
    int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
    if (keystone.exp > 0 && nowSec > keystone.exp) {
        Lock(currentNonce_);
        return {}; // fail-closed: Keystone expirado
    }

    // 0. Enforcement estrito do result_digest, varredura anti-debug e poison de VM (M5/M6)
    uint64_t exportPoison = CheckExeAntiDebugPoison();
    uint64_t vmPoison = CheckVmPoison(GetActiveClientSession().vmAllowToken);
    if (!EnforceResultDigest(keystone, currentResultDigest) || exportPoison != 0ULL || vmPoison != 0ULL) {
        if (exportPoison != 0ULL || vmPoison != 0ULL) {
            Lock(currentNonce_);
        }
        return {}; // fail-closed: digest diverge ou processo sob debugger/VM nao autorizada
    }

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystone.sessionSecretS.data(), sizeof(s_mask));

    uint64_t sealed = dispatchTable_[ExportSlot_Pdf];
    uint64_t dynamicDomain = solver_sec::kDomainAppExport ^ s_mask;
    auto fn = solver_sec::unsealPointer<PdfExportFn>(sealed, nonce, ExportSlot_Pdf, dynamicDomain);

    PdfExportOutput out;
    if (fn != nullptr) {
        out = fn(sections, s_mask, keystone.sessionSecretS.data());
    }

    // M6: Estampa a marca d'água forense assinada pelo servidor
    if (out.ok && !keystone.watermarkToken.empty()) {
        out.forensicWatermark = keystone.watermarkToken;
        out.summaryText += "\n[MARCA DAGUA FORENSE: " + keystone.watermarkToken + "]\n";
    }

    volatile uintptr_t wipe = reinterpret_cast<uintptr_t>(fn);
    (void)wipe;
    fn = nullptr;
    return out;
}

PdfExportOutput ExportGatekeeper::ExecutePdf(uint64_t nonce, const KeystoneArtifact& keystone,
                                            const std::vector<PdfSectionInput>& sections) {
    return ExecutePdf(nonce, keystone, std::string{}, sections);
}

// Invocação Efêmera de RunNormativeChecks
NormativeCheckOutput ExportGatekeeper::ExecuteNormativeCheck(uint64_t nonce, const KeystoneArtifact& keystone,
                                                            const double* displacements, size_t dispCount,
                                                            const double* reactions, size_t reacCount,
                                                            const NormativeCheckInput& input) {
    std::string digestAtual = ComputeResultDigest(displacements, dispCount, reactions, reacCount);
    return ExecuteNormativeCheck(nonce, keystone, digestAtual, input);
}

NormativeCheckOutput ExportGatekeeper::ExecuteNormativeCheck(uint64_t nonce, const KeystoneArtifact& keystone,
                                                            const std::string& currentResultDigest,
                                                            const NormativeCheckInput& input) {
    // M6: Checagem de expiração do Keystone (Kill switch: TTL 300s)
    int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
    if (keystone.exp > 0 && nowSec > keystone.exp) {
        Lock(currentNonce_);
        return {}; // fail-closed: Keystone expirado
    }

    // 0. Enforcement estrito do result_digest, varredura anti-debug e poison de VM (M5/M6)
    uint64_t exportPoison = CheckExeAntiDebugPoison();
    uint64_t vmPoison = CheckVmPoison(GetActiveClientSession().vmAllowToken);
    if (!EnforceResultDigest(keystone, currentResultDigest) || exportPoison != 0ULL || vmPoison != 0ULL) {
        if (exportPoison != 0ULL || vmPoison != 0ULL) {
            Lock(currentNonce_);
        }
        return {}; // fail-closed: digest diverge ou processo sob debugger/VM nao autorizada
    }

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystone.sessionSecretS.data(), sizeof(s_mask));

    uint64_t sealed = dispatchTable_[ExportSlot_NormativeChecks];
    uint64_t dynamicDomain = solver_sec::kDomainAppExport ^ s_mask;
    auto fn = solver_sec::unsealPointer<NormativeCheckFn>(sealed, nonce, ExportSlot_NormativeChecks, dynamicDomain);

    NormativeCheckOutput out;
    if (fn != nullptr) {
        out = fn(input, s_mask, keystone.sessionSecretS.data());
    }

    // M6: Estampa a marca d'água forense assinada pelo servidor
    if (out.ok && !keystone.watermarkToken.empty()) {
        out.forensicWatermark = keystone.watermarkToken;
        out.reportSnippet = "[MARCA DAGUA FORENSE: " + keystone.watermarkToken + "]\n"
            + "NBR 8800 Verificacao: Taxa = " + std::to_string(out.utilizationRatio)
            + (out.compliant ? " (CONFORME)" : " (NAO CONFORME)");
    }

    volatile uintptr_t wipe = reinterpret_cast<uintptr_t>(fn);
    (void)wipe;
    fn = nullptr;
    return out;
}

NormativeCheckOutput ExportGatekeeper::ExecuteNormativeCheck(uint64_t nonce, const KeystoneArtifact& keystone,
                                                            const NormativeCheckInput& input) {
    return ExecuteNormativeCheck(nonce, keystone, std::string{}, input);
}

// Invocação Efêmera de GenerateQuantities
QuantityExportOutput ExportGatekeeper::ExecuteQuantities(uint64_t nonce, const KeystoneArtifact& keystone,
                                                        const double* displacements, size_t dispCount,
                                                        const double* reactions, size_t reacCount,
                                                        const std::vector<QuantityBarInput>& bars) {
    std::string digestAtual = ComputeResultDigest(displacements, dispCount, reactions, reacCount);
    return ExecuteQuantities(nonce, keystone, digestAtual, bars);
}

QuantityExportOutput ExportGatekeeper::ExecuteQuantities(uint64_t nonce, const KeystoneArtifact& keystone,
                                                        const std::string& currentResultDigest,
                                                        const std::vector<QuantityBarInput>& bars) {
    // M6: Checagem de expiração do Keystone (Kill switch: TTL 300s)
    int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
    if (keystone.exp > 0 && nowSec > keystone.exp) {
        Lock(currentNonce_);
        return {}; // fail-closed: Keystone expirado
    }

    // 0. Enforcement estrito do result_digest, varredura anti-debug e poison de VM (M5/M6)
    uint64_t exportPoison = CheckExeAntiDebugPoison();
    uint64_t vmPoison = CheckVmPoison(GetActiveClientSession().vmAllowToken);
    if (!EnforceResultDigest(keystone, currentResultDigest) || exportPoison != 0ULL || vmPoison != 0ULL) {
        if (exportPoison != 0ULL || vmPoison != 0ULL) {
            Lock(currentNonce_);
        }
        return {}; // fail-closed: digest diverge ou processo sob debugger/VM nao autorizada
    }

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystone.sessionSecretS.data(), sizeof(s_mask));

    uint64_t sealed = dispatchTable_[ExportSlot_Quantities];
    uint64_t dynamicDomain = solver_sec::kDomainAppExport ^ s_mask;
    auto fn = solver_sec::unsealPointer<QuantityExportFn>(sealed, nonce, ExportSlot_Quantities, dynamicDomain);

    QuantityExportOutput out;
    if (fn != nullptr) {
        out = fn(bars, s_mask, keystone.sessionSecretS.data());
    }

    // M6: Estampa a marca d'água forense assinada pelo servidor
    if (out.ok && !keystone.watermarkToken.empty()) {
        out.forensicWatermark = keystone.watermarkToken;
        out.reportSnippet = "[MARCA DAGUA FORENSE: " + keystone.watermarkToken + "]\n"
            + "Massa Total: " + std::to_string(out.totalMassKg) + " kg, Volume: "
            + std::to_string(out.totalVolumeM3) + " m3";
    }

    volatile uintptr_t wipe = reinterpret_cast<uintptr_t>(fn);
    (void)wipe;
    fn = nullptr;
    return out;
}

QuantityExportOutput ExportGatekeeper::ExecuteQuantities(uint64_t nonce, const KeystoneArtifact& keystone,
                                                        const std::vector<QuantityBarInput>& bars) {
    return ExecuteQuantities(nonce, keystone, std::string{}, bars);
}

// --- Funções Reais com Mistura Criptográfica de s nos Cálculos ---

DxfExportOutput ExportGatekeeper::RealDxfExport(const std::vector<DxfBarInput>& bars,
                                              uint64_t s_mask,
                                              const uint8_t secretS[32]) {
    DxfExportOutput out;
    out.ok = true;
    out.barCount = bars.size();

    const double poison = ComputePoisonDrift(s_mask, secretS);
    // Se poison != 0, aplica distorção geométrica não-linear e corrupção angular silenciosa
    const double scaleModifier = 1.0 + poison * 0.25;
    const double angularDrift = poison * 0.174533; // ~10 graus se adulterado

    double sumX = 0.0, sumY = 0.0, sumZ = 0.0;
    double totalLen = 0.0;
    std::ostringstream ss;
    ss << "SECTION\n2\nENTITIES\n";

    for (const auto& b : bars) {
        // Transformação com incorporação do fator s
        double dx = (b.x2 - b.x1) * scaleModifier;
        double dy = (b.y2 - b.y1) * scaleModifier;
        double dz = (b.z2 - b.z1) * scaleModifier;

        if (poison != 0.0) {
            // Rotação corrompida em torno de Z
            double rx = dx * std::cos(angularDrift) - dy * std::sin(angularDrift);
            double ry = dx * std::sin(angularDrift) + dy * std::cos(angularDrift);
            dx = rx;
            dy = ry;
        }

        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        totalLen += len;

        double midX = (b.x1 + b.x1 + dx) * 0.5;
        double midY = (b.y1 + b.y1 + dy) * 0.5;
        double midZ = (b.z1 + b.z1 + dz) * 0.5;
        sumX += midX;
        sumY += midY;
        sumZ += midZ;

        ss << "0\nLINE\n8\nBARS\n10\n" << b.x1 << "\n20\n" << b.y1 << "\n30\n" << b.z1
           << "\n11\n" << (b.x1 + dx) << "\n21\n" << (b.y1 + dy) << "\n31\n" << (b.z1 + dz) << "\n";
    }
    ss << "0\nENDSEC\n";

    if (!bars.empty()) {
        out.centerOfMass[0] = sumX / bars.size();
        out.centerOfMass[1] = sumY / bars.size();
        out.centerOfMass[2] = sumZ / bars.size();
    }
    out.totalLength = totalLen;
    out.dxfSnippet = ss.str();
    return out;
}

PdfExportOutput ExportGatekeeper::RealPdfExport(const std::vector<PdfSectionInput>& sections,
                                              uint64_t s_mask,
                                              const uint8_t secretS[32]) {
    PdfExportOutput out;
    out.ok = true;
    out.sectionCount = sections.size();

    const double poison = ComputePoisonDrift(s_mask, secretS);
    // Se poison != 0, altera silenciosamente as tensões de pico reportadas no documento
    const double stressFactor = 1.0 - poison * 0.40;
    const double dispFactor = 1.0 + poison * 0.60;

    double peakStress = 0.0;
    double maxDisp = 0.0;

    for (const auto& s : sections) {
        double combinedStress = (std::abs(s.maxBendingStress) + std::abs(s.maxAxialStress)) * stressFactor;
        if (combinedStress > peakStress) {
            peakStress = combinedStress;
        }
        double disp = std::abs(s.maxDisplacement) * dispFactor;
        if (disp > maxDisp) {
            maxDisp = disp;
        }
    }

    out.reportedPeakStress = peakStress;
    out.reportedDisplacement = maxDisp;
    std::ostringstream ss;
    ss << "RELATORIO TECNICO DE CALCULO - STRUCTURA\n";
    ss << "Tensoes maximas combinadas: " << peakStress << " MPa\n";
    ss << "Deslocamento maximo: " << maxDisp << " mm\n";
    out.summaryText = ss.str();
    return out;
}

NormativeCheckOutput ExportGatekeeper::RealNormativeCheck(const NormativeCheckInput& in,
                                                        uint64_t s_mask,
                                                        const uint8_t secretS[32]) {
    NormativeCheckOutput out;
    out.ok = true;

    const double poison = ComputePoisonDrift(s_mask, secretS);

    // Coeficiente de ponderação da resistência NBR 8800: gamma_a1 = 1.10
    // Com poison != 0, gamma_a1 é inflado desproporcionalmente, corrompendo a capacidade
    const double gamma_a1 = 1.10 * (1.0 + poison * 0.75);

    // Capacidade axial de cálculo N_Rd = (A * f_y) / gamma_a1 (kN)
    // A em m2, fy em MPa -> A * fy em MN = 1000 kN
    double NRd = (in.area * in.yieldStrengthFy * 1000.0) / gamma_a1;

    // Momento fletor resistente de cálculo M_Rd = (Z * f_y) / gamma_a1 (kN.m)
    double MRd = (in.plasticModulus * in.yieldStrengthFy * 1000.0) / gamma_a1;

    out.designAxialCapacityRd = NRd;
    out.designMomentCapacityRd = MRd;

    // Relação de interação simplificada NBR 8800:
    double ratioN = (NRd > 1e-6) ? (std::abs(in.axialForceSd) / NRd) : 999.0;
    double ratioM = (MRd > 1e-6) ? (std::abs(in.bendingMomentSd) / MRd) : 999.0;
    out.utilizationRatio = ratioN + ratioM;
    out.compliant = (out.utilizationRatio <= 1.0);
    return out;
}

QuantityExportOutput ExportGatekeeper::RealQuantityExport(const std::vector<QuantityBarInput>& bars,
                                                        uint64_t s_mask,
                                                        const uint8_t secretS[32]) {
    QuantityExportOutput out;
    out.ok = true;

    const double poison = ComputePoisonDrift(s_mask, secretS);
    const double densityFactor = 1.0 + poison * 0.50;

    double totalMass = 0.0;
    double totalVol = 0.0;
    double totalLen = 0.0;

    for (const auto& b : bars) {
        double vol = b.length * b.area;
        double mass = vol * (b.density * densityFactor);
        totalVol += vol;
        totalMass += mass;
        totalLen += b.length;
    }

    out.totalMassKg = totalMass;
    out.totalVolumeM3 = totalVol;
    out.totalLengthM = totalLen;
    return out;
}

ExportGatekeeper& GetGlobalExportGatekeeper() {
    static ExportGatekeeper s_gatekeeper;
    return s_gatekeeper;
}

} // namespace structura_license
