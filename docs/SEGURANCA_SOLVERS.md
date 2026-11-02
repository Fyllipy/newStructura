# Segurança e Proteção contra Engenharia Reversa dos Solvers (`rds.dll` e `mefs.dll`)

Este documento descreve a arquitetura de segurança, ofuscação, criptografia, proteção anti-debugging e blindagem de binários PE implementada nos dois motores de cálculo numérico do **Structura::Metalicas**:

1. **`rds.dll` (`Structuradll/`)** — Solver de análise estrutural pelo Método da Rigidez Direta (1ª e 2ª ordem, não linearidade física e geométrica, empenamento de Vlasov, flambagem, análise modal e acoplamento com cascas).
2. **`mefs.dll` (`Femdll/`)** — Solver pelo Método dos Elementos Finitos para cascas/chapas quadriláteras Q4 (membrana com modos incompatíveis + flexão de Mindlin MITC4) e barras espaciais.

---

## 1. Visão Geral da Arquitetura

O objetivo da arquitetura é impedir tanto o **consumo não autorizado das DLLs** por softwares de terceiros quanto a **reconstrução estática ou dinâmica (inclusive assistida por IA)** das formulações matemáticas dos solvers, sem alterar os resultados numéricos nem degradar o desempenho do aplicativo principal (`Structura.exe`).

```
┌────────────────────────────────────────────────────────────────────────────┐
│                   Aplicação Principal (Structura.exe)                      │
│         SolverBridge (src/app/SolverBridge.cpp)                            │
│         ShellSolverBridge (src/app/ShellSolverBridge.cpp)                  │
└───────────────┬────────────────────────────────────────────▲───────────────┘
                │ 1. Resolve único símbolo (_z8r4q / _z8m2p) │
                │ 2. Envia (nonce, authTag ARX 256-bit)      │ 5. Devolve tabela de ponteiros
                ▼                                            │    selados por slot + strings
┌────────────────────────────────────────────────────────────┴───────────────┐
│                     DLL Protegida (rds.dll / mefs.dll)                     │
│  • 1 único export PE (_z8r4q ou _z8m2p)                                    │
│  • Seções renomeadas (.s0 .. .s5), sem Rich Header, sem DOS Stub, sem PDB  │
│  • Compilada com /GR- (sem RTTI) e /GL + /LTCG (inlining global)           │
│  • Anti-Debugger multivetor (PEB GS, NtQueryInformationProcess, Dr0..3,    │
│    varredura de INT3 0xCC nos prólogos e armadilha de tempo RDTSC)         │
│  • Ofuscação estilo XOLLVM (Control-Flow Flattening, MBA, Opaque Preds)    │
│  • Strings literais cifradas em tempo de compilação (OBF_STR)              │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Mapa de Arquivos da Infraestrutura de Segurança

| Arquivo | Papel na Arquitetura |
|---|---|
| `Structuradll/src/core/solver_security.h` | Cabeçalho central compartilhado entre os dois solvers e as classes `Bridge` da aplicação. Contém a cifra de strings `OBF_STR`, primitivas MBA/predicados opacos estilo XOLLVM, motor anti-debugger `antiDebugPoison`, autenticação ARX de 12 rodadas (`computeAuthTag`), selagem de ponteiros (`sealPointer`/`unsealPointer`), cifra de fluxo em trânsito (`cipherCString`) e enums de slots (`RdsSlot` e `MefsSlot`). |
| `Structuradll/src/bridge_dll/interface_dll.h` | Oculta as 36 funções `structura_*` da tabela de exportação PE (`#define SOLVER_DLL_API` vazio) e declara exclusivamente o gatekeeper `_z8r4q`. |
| `Structuradll/src/bridge_dll/interface_dll.cpp` | Implementa o gatekeeper `_z8r4q` com fluxo achatado (*Control-Flow Flattening*), verifica a chave de sessão `g_rdsSessionKey` e o estado anti-debug em `structura_solve_v4`, e cifra `errorBuffer` e `warnings` antes de cruzar a fronteira da DLL. |
| `Femdll/include/fem_interface.h` | Oculta as 24 funções `fem_*` da tabela de exportação PE (`#define FEM_API` vazio) e declara exclusivamente o gatekeeper `_z8m2p`. |
| `Femdll/src/fem_api.cpp` | Implementa o gatekeeper `_z8m2p` com fluxo achatado, bloqueia `fem_create` e `fem_solve` sem sessão autenticada ou sob depuração e cifra o buffer de erro em `fem_solve`. |
| `src/app/SolverBridge.cpp` | Realiza o handshake com `_z8r4q` (`kDomainRds`), dessela os ponteiros sob demanda para cada um dos 36 slots e decifra `errorBuffer` e `warnings`. |
| `src/app/ShellSolverBridge.cpp` | Realiza o handshake com `_z8m2p` (`kDomainMefs`), dessela os ponteiros dos 24 slots na estrutura `Api` e decifra mensagens de erro do MEF. |
| `Structuradll/headers/load.h` | Substitui `std::dynamic_pointer_cast` por despacho virtual (`asNodalLoad()` / `asBarLoad()`), permitindo compilar todo o solver com `/GR-` (RTTI desabilitado). |
| `tools/protect_pe.py` | Script pós-build que limpa cabeçalhos e metadados do executável PE (`rds.dll` e `mefs.dll`) e recalcula o CheckSum oficial do Windows. |
| `src/tests/TestProjectIoAndSolver.cpp` | Inclui o teste automatizado `solverDllsEnforceExportHidingAndCryptographicAuthentication`, garantindo que símbolos legados nunca vazem e que tags forjadas sejam rejeitadas. |

---

## 3. Manual Técnico de Funcionamento (Camada por Camada)

### Camada 1 — Ocultação da Tabela de Exportação PE e Gatekeeper Único

Em uma DLL C/C++ convencional, todas as funções marcadas com `__declspec(dllexport)` aparecem nominalmente no `IMAGE_EXPORT_DIRECTORY`. Ferramentas como `dumpbin /exports`, Dependency Walker, IDA Pro ou Ghidra listam imediatamente `structura_solve_v4`, `structura_set_warping_sections`, `fem_add_shell`, etc., entregando de bandeja o mapa semântico do programa.

Para eliminar esse vetor:
- Em `interface_dll.h` e `fem_interface.h`, as macros `SOLVER_DLL_API` e `FEM_API` foram definidas como vazias. Todas as funções `structura_*` (36 funções) e `fem_*` (24 funções) permanecem como funções internas privadas à DLL.
- Apenas uma função gatekeeper por DLL recebe `__declspec(dllexport)`:
  - **Em `rds.dll`:** `int _z8r4q(uint64_t nonce, uint64_t authTag, uint64_t* outTable, uint32_t slotCount)`
  - **Em `mefs.dll`:** `int _z8m2p(uint64_t nonce, uint64_t authTag, uint64_t* outTable, uint32_t slotCount)`

---

### Camada 2 — Handshake Criptográfico ARX (256-bit) e Ponteiros Selados por Slot

Quando `SolverBridge` ou `ShellSolverBridge` carrega a DLL via `QLibrary`, ocorre o seguinte protocolo:

1. **Geração de Nonce Dinâmico (`solver_sec::generateNonce`):**
   Combina o relógio de alta resolução (`std::chrono::high_resolution_clock`), o endereço de memória da stack atual (randomizado pelo ASLR do Windows) e um contador estático evoluído por MBA, produzindo um `nonce` de 64 bits único a cada chamada.

2. **Reconstrução Volátil da Chave Mestra de 256 bits (`solver_sec::loadMasterKey`):**
   A chave mestra de 256 bits não existe como um array estático na seção `.rdata`. Ela é reconstruída em registradores a partir de 4 palavras de 64 bits marcadas com `volatile` combinadas via **Mixed Boolean-Arithmetic (`mbaXor`, `mbaAdd`)** e difusão `SplitMix64` (`mix64`) com o identificador de domínio da DLL:
   - `kDomainRds  = 0x7A4E9B13D82C65F1ULL`
   - `kDomainMefs = 0x3F89C247E16B5A0DULL`
   Isso garante que um token válido para `rds.dll` seja inválido para `mefs.dll` e vice-versa.

3. **Cálculo da Tag de Autenticação (`solver_sec::computeAuthTag`):**
   Executa **12 rodadas ARX** (*Add-Rotate-Xor* inspiradas no SipHash-2-4/ChaCha, implementadas em `arxRound`) sobre o estado inicializado com a chave de 256 bits e o `nonce`.

4. **Selagem Individual dos Ponteiros de Função (`solver_sec::sealPointer` / `unsealPointer`):**
   Se `authTag == expectedTag`, o gatekeeper preenche `outTable[0 .. N-1]`. Porém, **ele não grava os endereços reais das funções na tabela** (o que permitiria a um atacante fazer um dump da memória após o handshake e ver uma vtable limpa).
   Cada endereço `rawPtr` no índice `slot` é cifrado com um keystream exclusivo de 4 rodadas ARX (`slotKeystream(nonce, slot, domain)`) e rotacionado por um deslocamento dependente da chave:
   $$\text{sealed} = \text{rotl}_{r}\!\left(\text{rawPtr} \oplus k_1,\; r\right) \oplus k_2, \quad r = (k_1 \gg 58) \bmod 64$$
   A aplicação principal dessela cada ponteiro individualmente (`unsealPointer`) no instante em que precisa invocá-lo.

5. **Guarda de Sessão Ativa (`g_rdsSessionKey` / `g_mefsSessionKey`):**
   Mesmo que um atacante descobrisse o endereço em memória de `structura_solve_v4`, `fem_create` ou `fem_solve` e tentasse chamá-las diretamente pulando o gatekeeper, essas funções verificam se a chave de sessão global foi derivada (`g_rdsSessionKey != 0` / `g_mefsSessionKey != 0`) e abortam com código `99` (`nullptr` em `fem_create`).

---

### Camada 3 — Criptografia de Strings em Tempo de Compilação (`OBF_STR`) e em Trânsito (`cipherCString`)

Assistentes de engenharia reversa baseados em IA dependem fortemente das strings de erro (ex.: *"Unstable structure (mechanism)"*, *"Torsion with warping is not combined with the modal analysis"*, *"Shell load references unknown shell"*) para localizar e rotular funções no binário descompilado.

1. **Em repouso no disco (`OBF_STR`):**
   A macro `OBF_STR("literal")` instancia em tempo de compilação (`constexpr`) um objeto `solver_sec::ObfString<sizeof(lit), Key>`, onde `Key` é derivada do hash FNV-1a da própria string misturado com `__LINE__`.
   - O compilador grava na seção `.rdata` **apenas os bytes cifrados**. A string original em texto claro jamais é emitida no `.obj` ou na `.dll`.
   - Em tempo de execução, `ObfString::str()` (marcada com `__declspec(noinline)`) lê os bytes através de um ponteiro `const volatile unsigned char*` (impedindo que o otimizador `/O2 /LTCG` desfaça a cifra em tempo de compilação) e reconstrói a `std::string` temporária na stack.

2. **Em trânsito pela fronteira da DLL (`solver_sec::cipherCString`):**
   Quando ocorre um erro ou aviso no solver, o texto decifrado é copiado para o `errorBuffer` / `warnings` e imediatamente recifrado com a chave dinâmica da sessão (`g_rdsSessionKey` / `g_mefsSessionKey`) usando `cipherCString`.
   - `cipherCString` é uma cifra de fluxo involutiva ($E_k(E_k(x)) = x$) que **preserva o byte nulo (`\0`)**: se o byte de keystream $k_i$ coincidir com o caractere $b_i$, o caractere é mantido sem gerar um `\0` prematuro.
   - `SolverBridge` e `ShellSolverBridge` aplicam `cipherCString(..., sessionKey)` ao receber o buffer, recuperando a mensagem original de forma transparente para a interface gráfica e para os testes.

---

### Camada 4 — Motor Anti-Debugger e Anti-Tampering Furtivo (`antiDebugPoison`)

Implementado em `solver_security.h` e invocado tanto nos gatekeepers (`_z8r4q` e `_z8m2p`) quanto nos pontos de entrada de cálculo (`structura_solve_v4` e `fem_solve`), o motor anti-debugger opera em **5 vetores independentes** sem expor nenhuma API de depuração na Import Table da DLL:

1. **Inspeção Direta da PEB via Registrador `GS` (`__readgsqword(0x60)`):**
   Lê diretamente da estrutura `PEB` (Process Environment Block) da arquitetura Windows x64:
   - `PEB + 0x02` (`BeingDebugged`);
   - `PEB + 0xBC` (`NtGlobalFlag`, verificando os bits `0x70` = `FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS`).
   Como não chama `kernel32!IsDebuggerPresent`, hooks de API comuns em debuggers não interceptam a leitura.

2. **Consultas ao Kernel via `ntdll!NtQueryInformationProcess` (Resolução Dinâmica Ofuscada):**
   Os nomes `"ntdll.dll"` e `"NtQueryInformationProcess"` são decifrados em tempo de execução via `OBF_STR` e resolvidos com `GetModuleHandleA` + `GetProcAddress`. O motor consulta:
   - `ProcessDebugPort` (`InfoClass = 0x07`): detecta porta de depuração ativa no kernel;
   - `ProcessDebugObjectHandle` (`InfoClass = 0x1E`): detecta a existência de um objeto de debug anexado ao processo.

3. **Detecção de Hardware Breakpoints (`Dr0..Dr3`):**
   Resolve dinamicamente `GetCurrentThread` e `GetThreadContext` (com `ContextFlags = CONTEXT_AMD64 | CONTEXT_DEBUG_REGISTERS = 0x00100010`) e verifica se `(Dr0 | Dr1 | Dr2 | Dr3) != 0`.

4. **Varredura de Software Breakpoints (`0xCC` / `INT 3`) nos Prólogos (`scanFunctionBreakpoint`):**
   Lê diretamente os primeiros 24 bytes de código de máquina em memória das funções críticas (`_z8r4q`, `_z8m2p`, `structura_solve_v4`, `structura_solve`, `fem_solve`, `fem_create`) antes da primeira instrução `RET` (`0xC3`). Se encontrar `0xCC` (`INT 3`) ou `0xCD 0x03`, detecta que um debugger inseriu um breakpoint de software na função.

5. **Armadilha de Tempo por Ciclos de Clock (`__rdtsc`) em `computeAuthTag`:**
   Mede a contagem de ciclos de CPU (`__rdtsc()`) antes e depois das 12 rodadas ARX. Em execução normal (mesmo em máquinas sob carga alta), 12 rodadas em registradores levam menos de $2.000$ ciclos. Se o delta ultrapassar $250.000.000$ de ciclos (~80 ms), identifica que o código está sendo executado passo a passo (*single-stepping* F7/F8) ou sob instrumentação pesada.

6. **Resposta por Envenenamento Criptográfico (*Stealth Poisoning*):**
   Em vez de chamar `exit()` ou `abort()` no local da detecção — o que criaria um salto condicional óbvio para o analista neutralizar com `NOP` no IDA/x64dbg — `antiDebugPoison` retorna uma máscara de 64 bits (`poison = 0` em ambiente limpo; `poison != 0` sob análise). Essa máscara é misturada via `mbaXor` ao `nonce`, à chave de sessão e aos ponteiros selados, fazendo com que a autenticação falhe ou os ponteiros fiquem inutilizáveis longe do ponto onde a detecção ocorreu.

---

### Camada 5 — Transformações Estilo XOLLVM / OLLVM (`-fla`, `-sub`, `-bcf`)

Para derrotar descompiladores modernos (IDA Hex-Rays, Ghidra) e agentes de IA que analisam o grafo de fluxo de controle (CFG), foram aplicadas três técnicas clássicas do **OLLVM / XOLLVM**:

1. **Instruction Substitution / Mixed Boolean-Arithmetic (`mbaAdd`, `mbaXor`, `mbaSub`):**
   Substitui operações aritméticas e lógicas simples por identidades mistas booleanas-aritméticas ancoradas em variáveis `volatile`:
   $$x + y = (x \oplus y) + 2(x \wedge y) = \bigl((x \oplus y) \lor 2(x \wedge y)\bigr) + \bigl((x \oplus y) \land 2(x \wedge y)\bigr)$$
   $$x \oplus y = (x \lor y) + \sim(x \wedge y) + 1$$
2. **Bogus Control Flow / Predicados Opacos (`opaqueTrue`, `opaqueFalse`):**
   Constrói condições baseadas em invariantes da teoria dos números avaliados sobre sementes voláteis de tempo de execução:
   - $7(x \bmod 2^{16})^2 - 1 \neq y^2$ (nunca é quadrado perfeito, pois $y^2 \equiv 0, 1, 2, 4 \pmod 7$, enquanto $-1 \equiv 6 \pmod 7$);
   - $x(x + 1) \equiv 0 \pmod 2$.
   O compilador e o descompilador não conseguem provar estaticamente que o ramo alternativo é morto, mantendo blocos falsos no grafo descompilado.
3. **Control-Flow Flattening (Achatamento de Fluxo por Máquina de Estados):**
   As rotinas críticas (`antiDebugPoison`, `computeAuthTag`, `_z8r4q` e `_z8m2p`) foram reescritas como despachadores de máquina de estados `while (state != 0) { switch (state) { ... } }` com transições governadas por predicados opacos.
4. **Suporte Nativo a Passes LLVM (`-mllvm`) no CMake:**
   Tanto `Structuradll/CMakeLists.txt` quanto `Femdll/CMakeLists.txt` detectam automaticamente se o compilador é baseado em Clang (`CMAKE_CXX_COMPILER_ID MATCHES "Clang"`) e ativam `-mllvm -fla -mllvm -sub -mllvm -bcf` para compilação direta com um toolchain `clang-cl` + XOLLVM.

---

### Camada 6 — Hardening de Compilação (`/GR-`, `/GL`, `/LTCG`) e Scrubber PE (`tools/protect_pe.py`)

1. **Flags de Compilação e Linkedição MSVC (`Structuradll/CMakeLists.txt` e `Femdll/CMakeLists.txt`):**
   - `/GR-`: Desativa completamente o **RTTI** (*Run-Time Type Information*). Remove todas as estruturas `RTTICompleteObjectLocator` e strings de nomes de classes (`.?AVMember@@`, `.?AVNode@@`, `.?AVSolverCore@@`, etc.) do binário.
   - `/GL` + `/LTCG` (*Whole Program Optimization* / *Link-Time Code Generation*): Funde e faz *inlining* agressivo de funções através de todas as unidades de tradução (`.cpp`), dissolvendo as fronteiras entre métodos e classes em blocos monolíticos de instruções SIMD AVX/SSE2 do Eigen.
   - `/DEBUG:NONE` + `/NOCOFFGRPINFO` + `/BREPRO`: Impede a geração de caminhos de `.pdb`, grupos COFF e timestamps reais.

2. **Pós-Processamento Binário PE (`tools/protect_pe.py`):**
   Executado automaticamente como `POST_BUILD` ao gerar `rds.dll` e `mefs.dll`, realiza 6 transformações estruturais no arquivo PE:
   - **Limpeza do DOS Stub e do MSVC Rich Header (`0x40 .. e_lfanew`):** Apaga a mensagem `"!This program cannot be run in DOS mode."` e a tabela `Rich` da Microsoft (que revelaria a versão exata do Visual Studio e o número de objetos C++).
   - **Limpeza de Metadados COFF e Optional Header:** Zera `TimeDateStamp`, `PointerToSymbolTable`, `NumberOfSymbols` e `Major/MinorLinkerVersion`.
   - **Erradicação do Diretório de Debug (`DataDirectory[6]`):** Localiza qualquer payload residual de debug no arquivo, sobrescreve seus bytes com `0x00` e zera a entrada `IMAGE_DIRECTORY_ENTRY_DEBUG`.
   - **Anonimização do Diretório de Exportação (`DataDirectory[0]`):** Zera o timestamp de exportação e substitui o nome interno da DLL (`"rds.dll"` / `"mefs.dll"`) por `"_"`.
   - **Ofuscação dos Nomes de Seções PE:** Renomeia `.text`, `.rdata`, `.data`, `.pdata`, `.rsrc`, `.reloc` para `.s0`, `.s1`, `.s2`, `.s3`, `.s4`, `.s5`.
   - **Recálculo do CheckSum PE:** Calcula e grava o checksum oficial de 32 bits no cabeçalho opcional do PE.

---

## 4. Situação Atual do Código e Auditoria Binária

### 4.1 Resultado da Auditoria nos Binários Gerados (`rds.dll` e `mefs.dll`)

| Item Auditado (`dumpbin` / `strings`) | Antes da Proteção | Situação Atual (`rds.dll` / `mefs.dll`) |
|---|---|---|
| Símbolos `structura_*` exportados em `rds.dll` | 36 funções em texto claro | **0** (apenas `_z8r4q`, ordinal 1) |
| Símbolos `fem_*` exportados em `mefs.dll` | 24 funções em texto claro | **0** (apenas `_z8m2p`, ordinal 1) |
| Strings de erro/aviso legíveis em `.rdata` | > 80 mensagens em texto claro | **0** (100% cifradas via `OBF_STR`) |
| Descritores de classes C++ RTTI (`.?AV...`) | Presentes para todas as classes | **0** (`/GR-` ativo) |
| Cabeçalho `Rich` da Microsoft e DOS Stub | Presentes | **Removidos (`0x00` em `0x40..e_lfanew`)** |
| Diretório de Debug (`IMAGE_DIRECTORY_ENTRY_DEBUG`) | Presente (`coffgrp` / PDB) | **0 RVA / 0 Size (payload zerado)** |
| Nomes das seções PE | `.text`, `.rdata`, `.data`, `.pdata`, `.rsrc`, `.reloc` | **`.s0`, `.s1`, `.s2`, `.s3`, `.s4`, `.s5`** |
| APIs de Anti-Debug na Import Table | N/A | **0** (`ntdll.dll` / `NtQueryInformationProcess` / `GetThreadContext` ocultas) |
| Testes unitários (`solver_tests`, `fem_tests`, 19 suítes `ctest`) | 100% aprovados | **100% aprovados (0 falhas)** |

### 4.2 Avaliação do Nível de Dificuldade contra Engenharia Reversa (Escala 0 a 10)

- **Nota Geral Atual: ~8,5 / 10** (contra **1,5 / 10** antes da implementação).
  - **Contra reutilização/importação das DLLs por terceiros: 9,0 / 10** — Sem o protocolo ARX de 256 bits, o domínio correto, o desdobramento MBA de ponteiros por slot e a decifração de buffers de sessão, a DLL é uma caixa-preta inoperante.
  - **Contra engenharia reversa estática + IA (IDA Pro / Ghidra + LLMs): 8,5 / 10** — Não há nomes de funções, strings de domínio, RTTI ou fronteiras de classes para ancorar a inferência da IA, e os pontos de entrada apresentam fluxo de controle achatado (*Control-Flow Flattening*) com aritmética booleana-mista (MBA) e predicados opacos.
  - **Contra engenharia reversa dinâmica (x64dbg / Frida): 8,0 / 10** — Breakpoints de software (`0xCC`), breakpoints de hardware (`Dr0..Dr3`), flags da PEB, portas de debug do kernel (`NtQueryInformationProcess`) e *single-stepping* (`__rdtsc`) envenenam silenciosamente as chaves criptográficas e os ponteiros da sessão sem revelar um ponto único de falha.

---

## 5. Guia de Manutenção para Desenvolvedores

### 5.1 Como recompilar as DLLs protegidas
A proteção e o pós-processamento PE (`tools/protect_pe.py`) são executados **automaticamente** pelo CMake ao compilar os alvos em modo `Release`, copiando as DLLs blindadas diretamente para a raiz do repositório:

```powershell
# 1. Recompilar e blindar mefs.dll (Femdll)
cmake --build Femdll/build --config Release

# 2. Recompilar e blindar rds.dll (Structuradll)
cmake --build Structuradll/build/vs --config Release --target solver_dll solver_tests

# 3. Recompilar o aplicativo principal e executar todos os testes
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### 5.2 Como adicionar uma nova função à API de `rds.dll` ou `mefs.dll`
1. Adicione o identificador do novo slot ao enum `RdsSlot` (antes de `Rds_SlotCount`) ou `MefsSlot` (antes de `Mefs_SlotCount`) em `Structuradll/src/core/solver_security.h`.
2. Declare a função com `SOLVER_DLL_API` (em `interface_dll.h`) ou `FEM_API` (em `fem_interface.h`) — ela permanecerá oculta da tabela PE.
3. No gatekeeper `_z8r4q` (`interface_dll.cpp`) ou `_z8m2p` (`fem_api.cpp`), registre a função chamando `seal(solver_sec::NovoSlot, &minha_nova_funcao);`.
4. No consumidor (`SolverBridge.cpp` ou `ShellSolverBridge.cpp`), recupere o ponteiro desselado via `resolveSlot(solver_sec::NovoSlot)` ou `unseal(solver_sec::NovoSlot)`.

### 5.3 Como rotacionar as chaves mestras a cada versão de lançamento
Para invalidar qualquer análise prévia entre versões do **Structura::Metalicas**, basta alterar as 4 constantes `volatile uint64_t a, b, c, d` dentro de `loadMasterKey` (e/ou `kDomainRds` e `kDomainMefs`) em `Structuradll/src/core/solver_security.h` e recompilar `Femdll`, `Structuradll` e `Structura`.
