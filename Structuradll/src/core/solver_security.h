#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(_WIN32) && defined(_M_X64) && defined(_MSC_VER)
#include <intrin.h>
#endif

namespace solver_sec {

constexpr uint64_t rotl64(uint64_t x, unsigned r) noexcept {
    r &= 63U;
    return r == 0U ? x : ((x << r) | (x >> (64U - r)));
}

constexpr uint64_t rotr64(uint64_t x, unsigned r) noexcept {
    r &= 63U;
    return r == 0U ? x : ((x >> r) | (x << (64U - r)));
}

constexpr uint64_t mix64(uint64_t x) noexcept {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

// ============================================================================
// XOLLVM / OLLVM-style Mixed Boolean-Arithmetic (MBA) & Opaque Predicates
// Impede simplificacao algebrica por descompiladores (IDA Hex-Rays / Ghidra)
// e modelos de IA ao substituir operacoes lineares por identidades mistas.
// ============================================================================

inline uint64_t mbaAdd(uint64_t x, uint64_t y) noexcept {
    volatile uint64_t vx = x;
    volatile uint64_t vy = y;
    const uint64_t a = vx ^ vy;
    const uint64_t b = vx & vy;
    return (a | (b << 1U)) + (a & (b << 1U));
}

inline uint64_t mbaXor(uint64_t x, uint64_t y) noexcept {
    volatile uint64_t vx = x;
    volatile uint64_t vy = y;
    return mbaAdd(vx | vy, ~(vx & vy)) + 1ULL;
}

inline uint64_t mbaSub(uint64_t x, uint64_t y) noexcept {
    volatile uint64_t vx = x;
    volatile uint64_t vy = ~y;
    return ((vx ^ vy) + ((vx & vy) << 1U)) + 1ULL;
}

// Predicado opaco matematicamente sempre VERDADEIRO:
// Para qualquer inteiro x: 7*(x mod 65536)^2 - 1 nunca e quadrado perfeito (mod 7),
// e x*(x+1) e sempre par. Como v vem de estado volatil, o compilador e o IDA
// nao conseguem provar estaticamente que o ramo falso e inalcancavel.
inline bool opaqueTrue(uint64_t seed) noexcept {
    volatile uint64_t v = seed ^ 0x5DEECE66DULL;
    const uint64_t x = v & 0xFFFFULL;
    const uint64_t lhs = 7ULL * x * x - 1ULL;
    const uint64_t rhs = (v >> 16U) & 0xFFFFULL;
    const uint64_t evenProd = (v * (v + 1ULL)) & 1ULL;
    return (lhs != rhs * rhs) && (evenProd == 0ULL);
}

inline bool opaqueFalse(uint64_t seed) noexcept {
    return !opaqueTrue(seed);
}

// ============================================================================
// Ofuscacao de Strings e Constantes IEEE-754 em Tempo de Compilacao
// ============================================================================

template <std::size_t N, uint64_t Key>
class ObfString {
private:
    std::array<unsigned char, N> m_enc {};

    static constexpr unsigned char keystream(std::size_t idx) noexcept {
        const uint64_t h = mix64(Key ^ (static_cast<uint64_t>(idx) * 0x9E3779B97F4A7C15ULL));
        return static_cast<unsigned char>((h & 0xFFU) ^ ((h >> 19) & 0xFFU) ^ 0xA7U);
    }

public:
    constexpr ObfString(const char (&plain)[N]) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            m_enc[i] = static_cast<unsigned char>(plain[i]) ^ keystream(i);
        }
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#endif
    std::string str() const {
        std::string out(N > 0 ? N - 1 : 0, '\0');
        const volatile unsigned char* src = m_enc.data();
        for (std::size_t i = 0; i + 1 < N; ++i) {
            out[i] = static_cast<char>(src[i] ^ keystream(i));
        }
        return out;
    }
};

constexpr uint64_t fnv1aCompileTime(const char* s, std::size_t len) noexcept {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= static_cast<unsigned char>(s[i]);
        h *= 0x100000001B3ULL;
    }
    return h;
}

#define OBF_STR(lit) \
    ([]() -> std::string { \
        constexpr uint64_t _k = ::solver_sec::mix64( \
            (static_cast<uint64_t>(__LINE__) * 0x517CC1B727220A95ULL) ^ \
            ::solver_sec::fnv1aCompileTime((lit), sizeof(lit)) ^ \
            0xC6BC279692B5C323ULL); \
        static constexpr ::solver_sec::ObfString<sizeof(lit), _k> _obf(lit); \
        return _obf.str(); \
    }())

// ============================================================================
// Motor Anti-Debugging & Anti-Tampering Multivetor (Windows x64)
// Sem entradas na Import Table para APIs de debug (resolvidas via OBF_STR).
// Quando detecta debugger, hardware/software breakpoints ou single-stepping,
// retorna uma mascara de envenenamento ("poison mask" != 0) que corrompe
// silenciosamente as chaves de sessao e ponteiros de funcao.
// ============================================================================

namespace detail_adbg {

#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
#if defined(_MSC_VER)
extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* lpModuleName);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
#else
extern "C" void* __stdcall GetModuleHandleA(const char* lpModuleName);
extern "C" void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
#endif

struct alignas(16) RawContextX64 {
    uint64_t PHome[6];        // 0x00..0x2F
    uint32_t ContextFlags;    // 0x30
    uint32_t MxCsr;           // 0x34
    uint16_t Segs[6];         // 0x38..0x43
    uint32_t EFlags;          // 0x44
    uint64_t Dr0;             // 0x48
    uint64_t Dr1;             // 0x50
    uint64_t Dr2;             // 0x58
    uint64_t Dr3;             // 0x60
    uint64_t Dr6;             // 0x68
    uint64_t Dr7;             // 0x70
    uint8_t   Pad[1232 - 0x78];
};
#endif

// Verifica os primeiros bytes de uma funcao em busca de:
// - Software breakpoints: INT 3 (0xCC), INT 3 de 2 bytes (0xCD 0x03), UD2 (0x0F 0x0B)
// - Inline hooks: JMP rel32 (0xE9), JMP rel8 (0xEB), JMP [rip+disp32] (0xFF 0x25), MOV RAX; JMP RAX (0x48 0xB8)
inline uint64_t scanFunctionBreakpoint(const void* fnPtr) noexcept {
    if (fnPtr == nullptr) {
        return 0ULL;
    }
    const auto* bytes = reinterpret_cast<const volatile unsigned char*>(fnPtr);
    // Verifica os primeiros 24 bytes antes de qualquer instrucao RET (0xC3 / 0xC2)
    for (int i = 0; i < 24; ++i) {
        const unsigned char b = bytes[i];
        if (b == 0xC3U || b == 0xC2U) {
            break;
        }
        if (b == 0xCCU) {
            return 0xBAD0CC0000000001ULL ^ static_cast<uint64_t>(i);
        }
        if (i + 1 < 24 && b == 0xCDU && bytes[i + 1] == 0x03U) {
            return 0xBAD0CD0300000002ULL;
        }
        if (i + 1 < 24 && b == 0x0FU && bytes[i + 1] == 0x0BU) {
            return 0xBAD00F0B00000004ULL;
        }
        if (b == 0xE9U) {
            return 0xBAD0E90000000003ULL ^ static_cast<uint64_t>(i);
        }
        if (b == 0xEBU) {
            return 0xBAD0EB0000000005ULL ^ static_cast<uint64_t>(i);
        }
        if (i + 1 < 24 && b == 0xFFU && bytes[i + 1] == 0x25U) {
            return 0xBAD0FF2500000006ULL;
        }
        if (i + 11 < 24 && b == 0x48U && bytes[i + 1] == 0xB8U && bytes[i + 10] == 0xFFU && bytes[i + 11] == 0xE0U) {
            return 0xBAD048B800000007ULL;
        }
    }
    return 0ULL;
}

} // namespace detail_adbg

// Motor Anti-Debugging Multivetor com CFF (Control-Flow Flattening)
inline uint64_t antiDebugPoison(const void* const* fnEntries, size_t count) noexcept {
#if defined(STRUCTURA_DEV_BUILD)
    // Somente variante de desenvolvimento/teste. A DLL comercial nao contem este caminho.
    if (const char* env = std::getenv("STRUCTURA_TEST_DLL_KEY")) {
        if (std::strlen(env) == 64) {
            return 0ULL;
        }
    }
#endif
    uint64_t poison = 0ULL;

    // 1. Varredura de Software Breakpoints (0xCC) e Inline Hooks (0xE9) nos prologos das funcoes criticas
    if (fnEntries != nullptr && count > 0) {
        for (size_t i = 0; i < count; ++i) {
            poison ^= detail_adbg::scanFunctionBreakpoint(fnEntries[i]);
        }
    }

#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    // Medição de ciclos para armadilha de temporização RDTSC (Anti-Tracing / Single-Stepping)
#if defined(_MSC_VER)
    const uint64_t tscStart = __rdtsc();
#else
    uint32_t lo1 = 0, hi1 = 0;
    __asm__ volatile("rdtsc" : "=a"(lo1), "=d"(hi1));
    const uint64_t tscStart = (static_cast<uint64_t>(hi1) << 32) | lo1;
#endif

    // Maquina de estados achatada (Control-Flow Flattening estilo XOLLVM -fla)
    uint32_t state = 0x1A3F5C01U;
    while (state != 0U) {
        switch (state) {
        case 0x1A3F5C01U: {
            // 2. Leitura direta da PEB via registrador GS (sem chamar IsDebuggerPresent)
#if defined(_MSC_VER)
            const auto* peb = reinterpret_cast<const unsigned char*>(__readgsqword(0x60));
#else
            const unsigned char* peb = nullptr;
            __asm__ volatile("movq %%gs:0x60, %0" : "=r"(peb));
#endif
            if (peb != nullptr) {
                // PEB->BeingDebugged (+0x02)
                if (peb[0x02] != 0U) {
                    poison ^= 0xD3B60001A5A5F00DULL;
                }
                // PEB->NtGlobalFlag (+0xBC): FLG_HEAP_ENABLE_TAIL_CHECK (0x10) | FREE_CHECK (0x20) | VALIDATE_PARAMETERS (0x40)
                const uint32_t ntGlobalFlag = *reinterpret_cast<const uint32_t*>(peb + 0xBC);
                if (peb[0x02] != 0U && (ntGlobalFlag & 0x70U) == 0x70U) {
                    poison ^= 0xD3B600025A5ACAFEULL;
                }
            }
            state = opaqueTrue(poison ^ 0x11ULL) ? 0x4E82B102U : 0x7F00DEADU;
            break;
        }
        case 0x4E82B102U: {
            // 3. Consultas ao Kernel via ntdll!NtQueryInformationProcess (resolvida dinamicamente com OBF_STR)
            using NtQueryInfoProcFn = long(__stdcall*)(void*, uint32_t, void*, uint32_t, uint32_t*);
            static NtQueryInfoProcFn s_ntQuery = nullptr;
            static bool s_resolvedNt = false;
            if (!s_resolvedNt) {
                const std::string ntdllName = OBF_STR("ntdll.dll");
                if (void* hNtdll = detail_adbg::GetModuleHandleA(ntdllName.c_str())) {
                    const std::string fnName = OBF_STR("NtQueryInformationProcess");
                    s_ntQuery = reinterpret_cast<NtQueryInfoProcFn>(detail_adbg::GetProcAddress(hNtdll, fnName.c_str()));
                }
                s_resolvedNt = true;
            }
            if (s_ntQuery != nullptr) {
                void* const currentProcess = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
                // ProcessDebugPort (0x07)
                uintptr_t debugPort = 0;
                if (s_ntQuery(currentProcess, 7U, &debugPort, sizeof(debugPort), nullptr) == 0 && debugPort != 0) {
                    poison ^= 0xD3B6000399887766ULL;
                }
                // ProcessDebugObjectHandle (0x1E)
                void* debugObject = nullptr;
                if (s_ntQuery(currentProcess, 0x1EU, &debugObject, sizeof(debugObject), nullptr) == 0 && debugObject != nullptr) {
                    poison ^= 0xD3B6000411223344ULL;
                }
            }
            state = opaqueTrue(poison ^ 0x22ULL) ? 0x6C09D303U : 0x7F00DEADU;
            break;
        }
        case 0x6C09D303U: {
            // 4. Inspecao de Hardware Breakpoints (registradores de debug Dr0, Dr1, Dr2, Dr3)
            using GetCurrentThreadFn = void*(__stdcall*)();
            using GetThreadContextFn = int(__stdcall*)(void*, detail_adbg::RawContextX64*);
            static GetCurrentThreadFn s_getCurThread = nullptr;
            static GetThreadContextFn s_getThreadCtx = nullptr;
            static bool s_resolvedK32 = false;
            if (!s_resolvedK32) {
                const std::string k32Name = OBF_STR("kernel32.dll");
                if (void* hK32 = detail_adbg::GetModuleHandleA(k32Name.c_str())) {
                    const std::string fnCur = OBF_STR("GetCurrentThread");
                    const std::string fnCtx = OBF_STR("GetThreadContext");
                    s_getCurThread = reinterpret_cast<GetCurrentThreadFn>(detail_adbg::GetProcAddress(hK32, fnCur.c_str()));
                    s_getThreadCtx = reinterpret_cast<GetThreadContextFn>(detail_adbg::GetProcAddress(hK32, fnCtx.c_str()));
                }
                s_resolvedK32 = true;
            }
            if (s_getCurThread != nullptr && s_getThreadCtx != nullptr) {
                alignas(16) detail_adbg::RawContextX64 ctx {};
                ctx.ContextFlags = 0x00100010U; // CONTEXT_AMD64 | CONTEXT_DEBUG_REGISTERS
                if (s_getThreadCtx(s_getCurThread(), &ctx) != 0) {
                    if ((ctx.Dr0 | ctx.Dr1 | ctx.Dr2 | ctx.Dr3) != 0ULL) {
                        poison ^= 0xD3B60005AABBCCDDULL ^ ctx.Dr0 ^ ctx.Dr1;
                    }
                }
            }
            state = opaqueTrue(poison ^ 0x33ULL) ? 0x7D11E404U : 0x7F00DEADU;
            break;
        }
        case 0x7D11E404U: {
            // 5. Armadilha de temporização RDTSC (Anti-Tracing / Single-Stepping)
#if defined(_MSC_VER)
            const uint64_t tscEnd = __rdtsc();
#else
            uint32_t lo2 = 0, hi2 = 0;
            __asm__ volatile("rdtsc" : "=a"(lo2), "=d"(hi2));
            const uint64_t tscEnd = (static_cast<uint64_t>(hi2) << 32) | lo2;
#endif
            if (tscEnd > tscStart && (tscEnd - tscStart) > 50000000ULL) {
                // Tracing/single-stepping sob debugger interativo detectado
                poison ^= 0xD3B6000677889900ULL;
            }
            state = 0U;
            break;
        }
        default:
            poison ^= 0xBADF00DULL;
            state = 0U;
            break;
        }
    }
#endif

    return poison;
}

inline uint64_t antiDebugPoison(const void* fn1 = nullptr, const void* fn2 = nullptr) noexcept {
    const void* arr[2] = { fn1, fn2 };
    size_t count = 0;
    if (fn1 != nullptr) count = 1;
    if (fn2 != nullptr) count = 2;
    return antiDebugPoison(arr, count);
}

template <typename... Args>
inline uint64_t antiDebugPoisonMulti(const void* fn1, const void* fn2, const void* fn3, Args... rest) noexcept {
    const void* arr[] = { fn1, fn2, fn3, rest... };
    return antiDebugPoison(arr, sizeof...(rest) + 3);
}

// Injeta silenciosamente a máscara de envenenamento (poison) no segredo s da sessão.
// Se poison != 0, corrompe a chave de forma irreversível e não-linear, sem exceções.
inline void poisonKeystoneSecret(uint8_t secretS[32], uint64_t poison) noexcept {
    if (poison == 0ULL || secretS == nullptr) {
        return;
    }
    uint64_t head = 0;
    std::memcpy(&head, secretS, sizeof(head));
    head ^= poison;
    std::memcpy(secretS, &head, sizeof(head));

    for (size_t i = 8; i < 32; i += 8) {
        uint64_t chunk = 0;
        std::memcpy(&chunk, secretS + i, sizeof(chunk));
        chunk ^= mix64(poison ^ (static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL));
        std::memcpy(secretS + i, &chunk, sizeof(chunk));
    }
}

// Dominios criptograficos distintos para cada solver (RDS vs MEF) e app
constexpr uint64_t kDomainRds       = 0x7A4E9B13D82C65F1ULL;
constexpr uint64_t kDomainMefs      = 0x3F89C247E16B5A0DULL;
constexpr uint64_t kDomainAppExport = 0x51A7F0C3E94D2B81ULL;
constexpr uint64_t kQuarantineMask  = 0xBAADF00DDEADBEEFULL;
constexpr uint64_t kQuarantineDomain = kDomainRds ^ kQuarantineMask;

// Reconstroi a chave mestre de 256 bits em tempo de execucao usando MBA (Mixed Boolean-Arithmetic)
inline void loadMasterKey(uint64_t domain, uint64_t outKey[4]) noexcept {
    volatile uint64_t a = 0x8B3F19D4E62A7C55ULL;
    volatile uint64_t b = 0x4D9A2E71C308F56BULL;
    volatile uint64_t c = 0xE175C803A94B62DFULL;
    volatile uint64_t d = 0x29F46B8DA71E35C3ULL;
    const uint64_t k0 = mbaXor(a, domain);
    const uint64_t k1 = mbaAdd(b, rotl64(domain, 17));
    const uint64_t k2 = mbaXor(c, rotr64(domain, 11));
    const uint64_t k3 = mbaXor(d, a * 0x9E3779B97F4A7C15ULL);
    outKey[0] = mix64(k0);
    outKey[1] = mix64(k1);
    outKey[2] = mix64(k2);
    outKey[3] = mix64(k3);
}

inline void arxRound(uint64_t& v0, uint64_t& v1, uint64_t& v2, uint64_t& v3) noexcept {
    v0 = mbaAdd(v0, v1);
    v1 = rotl64(v1, 13) ^ v0;
    v0 = rotl64(v0, 32);
    v2 = mbaAdd(v2, v3);
    v3 = rotl64(v3, 16) ^ v2;
    v0 = mbaAdd(v0, v3);
    v3 = rotl64(v3, 21) ^ v0;
    v2 = mbaAdd(v2, v1);
    v1 = rotl64(v1, 17) ^ v2;
    v2 = rotl64(v2, 32);
}

// Calcula a tag de autenticacao com Control-Flow Flattening (maquina de estados estilo XOLLVM -fla)
// e armadilha de tempo RDTSC contra single-stepping em debuggers.
inline uint64_t computeAuthTag(uint64_t nonce, uint64_t domain) noexcept {
#if defined(_WIN32) && defined(_M_X64) && defined(_MSC_VER)
    const uint64_t tscStart = __rdtsc();
#endif
    uint64_t k[4];
    loadMasterKey(domain, k);
    uint64_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;

    // Dispatcher achatado (Control-Flow Flattening)
    uint32_t state = 0x3101U;
    while (state != 0U) {
        switch (state) {
        case 0x3101U:
            v0 = mbaXor(k[0], 0x736F6D6570736575ULL);
            v1 = mbaXor(k[1], 0x646F72616E646F6DULL);
            v2 = mbaXor(k[2], 0x6C7967656E657261ULL);
            v3 = mbaXor(k[3], nonce);
            state = opaqueTrue(v0 ^ nonce) ? 0x5202U : 0x9999U;
            break;
        case 0x5202U:
            for (int i = 0; i < 6; ++i) {
                arxRound(v0, v1, v2, v3);
            }
            state = opaqueFalse(v1 ^ domain) ? 0x9999U : 0x7303U;
            break;
        case 0x7303U:
            v0 = mbaXor(v0, nonce);
            v2 = mbaXor(v2, 0xFFULL);
            for (int i = 0; i < 6; ++i) {
                arxRound(v0, v1, v2, v3);
            }
            state = 0U;
            break;
        default:
            v0 ^= 0xDEADULL;
            state = 0U;
            break;
        }
    }

    uint64_t tag = (v0 ^ v1 ^ v2 ^ v3) | 1ULL;
#if defined(_WIN32) && defined(_M_X64) && defined(_MSC_VER)
    const uint64_t tscDelta = __rdtsc() - tscStart;
    // Operacao puramente em registradores leva < 2.000 ciclos.
    // Se ultrapassar 250.000.000 ciclos (~80 ms), ha single-stepping / trace humano no debugger.
    if (tscDelta > 250000000ULL) {
        tag ^= mix64(tscDelta);
    }
#endif
    return tag;
}

inline uint64_t deriveSessionKey(uint64_t nonce, uint64_t authTag, uint64_t domain) noexcept {
    return mix64(mbaXor(nonce, rotl64(authTag, 23)) ^ mix64(domain ^ 0x55AA55AA55AA55AAULL)) | 1ULL;
}

inline uint64_t slotKeystream(uint64_t nonce, uint32_t slot, uint64_t domain) noexcept {
    uint64_t k[4];
    loadMasterKey(domain, k);
    const uint64_t s = mbaAdd(static_cast<uint64_t>(slot), 1ULL) * 0x9E3779B97F4A7C15ULL;
    uint64_t v0 = mbaXor(k[0], nonce);
    uint64_t v1 = mbaXor(k[1], s);
    uint64_t v2 = mbaXor(k[2], rotl64(nonce, 29));
    uint64_t v3 = mbaXor(k[3], rotr64(s, 19));
    for (int i = 0; i < 4; ++i) {
        arxRound(v0, v1, v2, v3);
    }
    return v0 ^ v1 ^ v2 ^ v3;
}

inline uint64_t sealPointer(uintptr_t rawPtr, uint64_t nonce, uint32_t slot, uint64_t domain) noexcept {
    const uint64_t k1 = slotKeystream(nonce, slot, domain);
    const uint64_t k2 = mix64(mbaXor(k1, 0xA5A55A5AA5A55A5AULL));
    const unsigned r = static_cast<unsigned>((k1 >> 58U) & 63U);
    const uint64_t x = rotl64(mbaXor(static_cast<uint64_t>(rawPtr), k1), r);
    return mbaXor(x, k2);
}

template <typename Fn>
inline Fn unsealPointer(uint64_t sealed, uint64_t nonce, uint32_t slot, uint64_t domain) noexcept {
    const uint64_t k1 = slotKeystream(nonce, slot, domain);
    const uint64_t k2 = mix64(mbaXor(k1, 0xA5A55A5AA5A55A5AULL));
    const unsigned r = static_cast<unsigned>((k1 >> 58U) & 63U);
    const uint64_t x = rotr64(mbaXor(sealed, k2), r);
    return reinterpret_cast<Fn>(static_cast<uintptr_t>(mbaXor(x, k1)));
}

// Cifra de fluxo involutiva (auto-inversa) sobre strings C terminadas em nulo:
// preserva '\0' final e garante que nenhum byte != 0 se torne '\0'.
inline void cipherCString(char* buf, std::size_t maxLen, uint64_t sessionKey) noexcept {
    if (buf == nullptr || maxLen == 0 || sessionKey == 0) {
        return;
    }
    for (std::size_t i = 0; i < maxLen && buf[i] != '\0'; ++i) {
        const uint64_t h = mix64(sessionKey ^ (static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL));
        unsigned char k = static_cast<unsigned char>((h & 0xFFU) ^ ((h >> 17U) & 0xFFU));
        if (k == 0U) {
            k = 0x5AU;
        }
        const unsigned char b = static_cast<unsigned char>(buf[i]);
        if (b != k) {
            buf[i] = static_cast<char>(b ^ k);
        }
    }
}

inline uint64_t generateNonce(const void* stackAnchor) noexcept {
    static uint64_t s_counter = 0x3C6EF372FE94F82AULL;
    s_counter = mbaAdd(s_counter, 0x9E3779B97F4A7C15ULL);
    const auto now = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(stackAnchor));
    return mix64(now ^ rotl64(addr, 21) ^ s_counter) | 1ULL;
}

// Assinatura unica do gatekeeper exportado pelas DLLs (unico ponto de entrada visivel)
using GatekeeperFn = int (*)(uint64_t nonce, uint64_t authTag, uint64_t* outTable, uint32_t slotCount);

enum RdsSlot : uint32_t {
    Rds_ClearShells = 0,
    Rds_GetBuckling,
    Rds_GetConstraintForces,
    Rds_GetHingeResults,
    Rds_GetLinkResults,
    Rds_GetModal,
    Rds_GetNonlinearCurve,
    Rds_GetNonlinearSummary,
    Rds_GetShellResults,
    Rds_GetWarnings,
    Rds_GetWarping,
    Rds_SetAnalysisOptions,
    Rds_SetBuckling,
    Rds_SetConstraints,
    Rds_SetEndSprings,
    Rds_SetLinks,
    Rds_SetMemberActions,
    Rds_SetModal,
    Rds_SetNonlinearOptions,
    Rds_SetOptions,
    Rds_SetPlasticHinges,
    Rds_SetRigidZones,
    Rds_SetShearAreas,
    Rds_SetShearCenters,
    Rds_SetShellAlpha,
    Rds_SetShellOrthotropy,
    Rds_SetShells,
    Rds_SetSolverOptions,
    Rds_SetTaper,
    Rds_SetTensionOnly,
    Rds_SetWarpingMembers,
    Rds_SetWarpingSections,
    Rds_Solve,
    Rds_SolveV2,
    Rds_SolveV3,
    Rds_SolveV4,
    Rds_SlotCount
};

enum MefsSlot : uint32_t {
    Mefs_Create = 0,
    Mefs_Destroy,
    Mefs_Version,
    Mefs_AddMaterial,
    Mefs_SetMaterialOrthotropy,
    Mefs_AddNode,
    Mefs_AddShell,
    Mefs_AddBeam,
    Mefs_AddSpring,
    Mefs_AddTie,
    Mefs_AddTieMasked,
    Mefs_AddCase,
    Mefs_LoadNode,
    Mefs_LoadShellPressure,
    Mefs_LoadShellBody,
    Mefs_LoadShellThermal,
    Mefs_LoadBeamUniform,
    Mefs_Solve,
    Mefs_SpringIterations,
    Mefs_MaxWarp,
    Mefs_GetDisplacements,
    Mefs_GetReactions,
    Mefs_GetShellResults,
    Mefs_GetBeamForces,
    Mefs_SlotCount
};

using GatekeeperUnlockFn = int (*)(uint64_t nonce, uint64_t s_mask, uint64_t* outTable, uint32_t slotCount);
using GatekeeperLockFn = int (*)(uint64_t nonce, uint64_t* outTable, uint32_t slotCount);

// Classificacao autoritativa dos slots de Export/Finalizacao vs Calculo/Preview
// Rds_GetConstraintForces e Rds_GetWarnings sao estritamente PREVIEW
// (necessarios para calculo do equilibrio global reaction_totals e warnings de modelo).
inline bool isRdsExportSlot(uint32_t slot) noexcept {
    switch (slot) {
    case Rds_GetBuckling:
    case Rds_GetHingeResults:
    case Rds_GetLinkResults:
    case Rds_GetModal:
    case Rds_GetNonlinearCurve:
    case Rds_GetNonlinearSummary:
    case Rds_GetShellResults:
    case Rds_GetWarping:
        return true;
    default:
        return false;
    }
}

inline bool isMefsExportSlot(uint32_t slot) noexcept {
    switch (slot) {
    case Mefs_GetShellResults:
    case Mefs_GetBeamForces:
        return true;
    default:
        return false;
    }
}

// Computa o desvio de poison para os módulos Nível A fora do solver
// (DXF, PDF/Memorial, Dimensionamento NBR 8800/14762, Quantitativos).
// Se o segredo s derivado de um Keystone K válido for fornecido e consistente: desvio = 0.0 (100% exato).
// Se invocado sem autorização (s_mask == 0 ou secretS nulo/adulterado por debugger bypass):
// retorna desvio substancial (poison > 0) para corrupção silenciosa dos dados físicos.
inline double computeKeystonePoison(uint64_t s_mask, const uint8_t* secretS) noexcept {
#if defined(STRUCTURA_DEV_BUILD)
    // Somente variante de desenvolvimento/teste. A DLL comercial nao contem este caminho.
    if (const char* env = std::getenv("STRUCTURA_TEST_DLL_KEY")) {
        if (std::strlen(env) == 64) {
            return 0.0;
        }
    }
#endif
    if (s_mask == 0 || secretS == nullptr) {
        return 0.50; // desvio não-linear de 50%
    }
    uint64_t check = 0;
    std::memcpy(&check, secretS, sizeof(check));
    if (check != s_mask) {
        return 0.35; // desvio por inconsistência de keystream
    }
    uint64_t acc = 0;
    for (size_t i = 0; i < 32; i += 8) {
        uint64_t chunk = 0;
        std::memcpy(&chunk, secretS + i, sizeof(chunk));
        acc ^= chunk;
    }
    if (acc == 0) {
        return 0.40;
    }
    return 0.0;
}

} // namespace solver_sec
