#pragma once
// Deteccao de Hypervisor / Maquina Virtual e Poison Criptografico (Marco M6).
// Base: SPEC.md (§11), PLAN.md (M6) e Decisoes de Desenho Travadas.
//
// Mecanismo:
// 1. CPUID Hypervisor bit (ECX bit 31 da funcao 1) e vendor signature da funcao 0x40000000.
// 2. Inspecao de artefatos de virtualizacao (VMware, VirtualBox, Hyper-V, KVM/QEMU, Xen).
// 3. Insercao obrigatoria na cadeia de poison: se VM for detectada sem vm_allow_token
//    valido emitido pelo servidor na allowlist, gera mascara de poison que corrompe
//    silenciosamente a chave de sessao s (mesmo mecanismo do anti-debug).
// 4. O prologo de CheckVmPoison e incluido no array de funcoes criticas de
//    CheckExeAntiDebugPoison, garantindo que qualquer tentativa de patchear ou remover
//    a checagem de VM tambem dispare o envenenamento imediato de s.

#include <string>
#include <cstdint>

namespace structura_license {

struct VmDetectionResult {
    bool isVm = false;
    std::string vendor;   // "VMware", "Microsoft Hyper-V", "VirtualBox", "KVM/QEMU", "Xen", etc.
    uint64_t vmFlags = 0; // Mascara de flags de deteccao
};

// Realiza a varredura de hypervisor e drivers de maquina virtual
VmDetectionResult DetectVirtualMachine();

// Inspeciona se ha VM nao autorizada e gera poison criptografico (M6).
// Retorna 0ULL se ambiente fisico OU se VM estiver autorizada por vmAllowToken.
// Retorna mascara != 0ULL se VM for detectada sem autorizacao valida.
uint64_t CheckVmPoison(const std::string& vmAllowToken);

// Utilitario para testes controlados de abuso e degradacao
void SetForceVmDetectionForTest(bool forceVm, const std::string& vendor = "TestVM");
bool GetForceVmDetectionForTest();

} // namespace structura_license
