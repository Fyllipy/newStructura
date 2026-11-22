# SecurityHardening.cmake — Configurações de Ofuscação, Ocultação de Símbolos e Strip (Marco M5).
# Base: SPEC.md (§9), PLAN.md (M5).

message(STATUS "Configurando Hardening de Segurança (M5: Anti-Debug, Visibilidade Oculta, Strip)")

# 1. Ocultação de Símbolos Internos (-fvisibility=hidden)
# Garante que funções internas do licensing, criptografia e matemática não apareçam na tabela de exportação
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Flags de compilador para GCC e Clang / Obfuscator-LLVM
    add_compile_options(
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -fstack-protector-strong
    )

    # Detecção e suporte a flags nativas do Obfuscator-LLVM (OLLVM)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mllvm -sub" COMPILER_SUPPORTS_OLLVM_SUB)
    if (COMPILER_SUPPORTS_OLLVM_SUB)
        message(STATUS "Obfuscator-LLVM detectado! Ativando passes de ofuscação avançada (-fla, -bcf, -sub)")
        add_compile_options(
            -mllvm -fla   # Control-Flow Flattening (achatamento de fluxo)
            -mllvm -bcf   # Bogus Control Flow (blocos falsos de controle)
            -mllvm -sub   # Instruction Substitution (substituição MBA de instruções)
        )
    else()
        message(STATUS "Compilador padrão detectado. Ofuscação CFF e MBA ativas em nível de código-fonte (solver_security.h)")
    endif()

    # 2. Strip de Símbolos em Release (elimina nomes legíveis, tabela de depuração e variáveis)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--strip-all -s")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--strip-all -s")

elseif (MSVC)
    # Flags de hardening e eliminação de símbolos para MSVC
    add_compile_options(/Gy /GS)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
endif()

# 3. Flag de Build de Desenvolvimento vs Release (M5: Cifra de DLLs em Repouso)
# Em Release (padrao de distribuicao), STRUCTURA_DEV_BUILD e OFF.
# Em Release: fail-closed estrito; LoadEncryptedDll e obrigatorio (sem fallback para .dll puro).
# Para habilitar fallback exclusivamente em ambiente de desenvolvimento local:
#   cmake -DSTRUCTURA_DEV_BUILD=ON ..
option(STRUCTURA_DEV_BUILD "Habilita fallback para DLLs em texto claro (APENAS PARA DESENVOLVIMENTO LOCAL)" OFF)
if (STRUCTURA_DEV_BUILD)
    message(WARNING "ATENCAO: STRUCTURA_DEV_BUILD esta ATIVA! Fallback para DLLs em claro permitido em dev.")
    add_compile_definitions(STRUCTURA_DEV_BUILD=1)
else()
    message(STATUS "Build de producao/release seguro: STRUCTURA_DEV_BUILD DESATIVADA (fail-closed estrito sem fallback)")
endif()

