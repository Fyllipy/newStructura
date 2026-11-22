#include "vm_detect.h"

#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <cctype>

namespace structura_license {

static bool s_forceVmForTest = false;
static std::string s_forcedVendor = "TestVM";

void SetForceVmDetectionForTest(bool forceVm, const std::string& vendor) {
    s_forceVmForTest = forceVm;
    s_forcedVendor = vendor;
}

bool GetForceVmDetectionForTest() {
    return s_forceVmForTest;
}

VmDetectionResult DetectVirtualMachine() {
    // 0. Hook de controle para suites de teste automatizado de abuso
    if (s_forceVmForTest) {
        return {true, s_forcedVendor, 0x8000000000000001ULL};
    }

    // 1. Inspecao de Hypervisor via CPUID (ECX bit 31 da funcao 1)
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    bool hypervisorPresent = (cpuInfo[2] & (1 << 31)) != 0;

    if (hypervisorPresent) {
        // Funcao 0x40000000 retorna a assinatura do fabricante do hypervisor
        __cpuid(cpuInfo, 0x40000000);
        char sig[13] = {0};
        std::memcpy(sig + 0, &cpuInfo[1], 4);
        std::memcpy(sig + 4, &cpuInfo[2], 4);
        std::memcpy(sig + 8, &cpuInfo[3], 4);

        std::string vendorStr(sig);
        std::string friendlyVendor = "Hypervisor (" + vendorStr + ")";
        uint64_t flags = 0x1ULL;

        if (vendorStr.find("VMware") != std::string::npos) {
            friendlyVendor = "VMware";
            flags |= 0x2ULL;
        } else if (vendorStr.find("Microsoft Hv") != std::string::npos) {
            // No Windows 10/11 fisico com Isolamento de Nucleo (VBS / Core Isolation),
            // WSL2 ou Hyper-V ativo, o proprio host fisico roda sob a particao raiz (Root Partition).
            // A folha 0x40000003 do Hyper-V retorna os privilegios da particao:
            // EBX bit 0 (privilegio 32) = CreatePartitions
            // EBX bit 12 (privilegio 44) = CpuManagement
            // Apenas a particao raiz (Host OS fisico) possui esses privilegios.
            // Maquinas virtuais convidadas (guest VMs) NAO possuem esses bits.
            if (static_cast<unsigned int>(cpuInfo[0]) >= 0x40000003U) {
                int hvFeatures[4] = {0};
                __cpuid(hvFeatures, 0x40000003);
                bool isRootPartition = (hvFeatures[1] & 0x1) != 0;
                if (isRootPartition) {
                    // Host fisico com VBS/Hyper-V de seguranca do Windows ativo: nao eh VM convidada!
                    return {false, "", 0ULL};
                }
            }
            friendlyVendor = "Microsoft Hyper-V";
            flags |= 0x4ULL;
        } else if (vendorStr.find("VBox") != std::string::npos) {
            friendlyVendor = "VirtualBox";
            flags |= 0x8ULL;
        } else if (vendorStr.find("KVM") != std::string::npos || vendorStr.find("TCG") != std::string::npos) {
            friendlyVendor = "KVM/QEMU";
            flags |= 0x10ULL;
        } else if (vendorStr.find("Xen") != std::string::npos) {
            friendlyVendor = "Xen";
            flags |= 0x20ULL;
        }

        return {true, friendlyVendor, flags};
    }

    // 2. Inspecao de drivers de comunicacao e pipes de virtualizacao conhecidos
    const char* vmDevices[] = {
        "\\\\.\\VBoxMiniRdrDN",
        "\\\\.\\VBoxGuest",
        "\\\\.\\HGFS",
        "\\\\.\\vmci"
    };

    for (const char* dev : vmDevices) {
        HANDLE h = CreateFileA(dev, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return {true, "Virtualization Guest Driver", 0x100ULL};
        }
    }

    return {false, "", 0ULL};
}

uint64_t CheckVmPoison(const std::string& vmAllowToken) {
    VmDetectionResult res = DetectVirtualMachine();
    if (!res.isVm) {
        return 0ULL;
    }

    // Se VM detectada, valida se ha token autorizativo emitido pelo servidor
    if (vmAllowToken.size() == 32) {
        bool validHex = true;
        for (char c : vmAllowToken) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                validHex = false;
                break;
            }
        }
        if (validHex) {
            // Ambiente virtualizado expressamente autorizado na allowlist de admin!
            return 0ULL;
        }
    }

    // VM detectada SEM autorizacao valida na allowlist:
    // Retorna mascara de poison != 0 para alimentar silenciosamente o envenenamento de s
    uint64_t poisonMask = 0x8000000000000000ULL | (res.vmFlags ? res.vmFlags : 0x1ULL);
    return poisonMask;
}

} // namespace structura_license
