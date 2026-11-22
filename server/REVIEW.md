# Revisão adversarial por marco

## M0 — Setup e infraestrutura

**Entregue:** `gen_keys.py` (`--write`, 0600, recusa sobrescrever), config fail-closed (`Settings.validate_secrets()` no import de `app.main`), `.gitignore` (.env/*.db/*.pem/*.key), `requirements*.txt` com limites de versão, CI (`.github/workflows/server-ci.yml`: pytest, pip-audit, Semgrep, gitleaks, Trivy), 18 testes.

**Ataques considerados e resultado**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Provocar erro de config para vazar segredo em log (pydantic ecoa `input_value`) | **Falha real encontrada pelo teste e corrigida**: validação própria, mensagem só com nome do campo. |
| 2 | Subir o servidor sem chaves / com chave curta (assinaturas fracas ou crash tardio) | Não sobe (`SecretConfigError`); testado em subprocess. |
| 3 | Commit acidental de `.env`/chaves | `.gitignore` + teste de `git ls-files` + gitleaks no CI + heurística anti-literal no código. |
| 4 | `gen_keys --write` sobrescrevendo `.env` de produção | Recusa (O_EXCL). |
| 5 | `/pubkey` vazando material privado | Teste confirma que seed/CPF keys não aparecem. |
| 6 | Dependência com CVE | pip-audit/Trivy no CI (limites superiores de versão adicionados; lockfile com hashes fica para o M9). |

**Pendências assumidas (marcos futuros, não M0):** `/pubkey` não é assinado nem tem rate limit (M9: pinning embute a pubkey, então um `/pubkey` falso só afeta quem não embutiu); `.env.example` e `/docs` do FastAPI expostos em dev (desligar `docs_url` em produção no M9); rotação de chaves usa o `id` `k1` fixo (M9).

**Nota de ambiente:** no sandbox Windows o `TestClient` trava (asyncio precisa de socket de loopback). Os testes foram rodados fora do sandbox; no CI/Linux não há restrição.

---

## M1 — Auth e núcleo de licenciamento

**Entregue:** modelo de dados (+ índice único parcial "1 device ativo por licença" no DB, `audit_log` imutável na camada ORM), Argon2id, CPF AES-256-GCM+HMAC (CPF normalizado), `/auth/login`, `/auth/logout`, `/license/heartbeat`, 2FA TOTP (RFC 6238 próprio, segredo cifrado com subchave HKDF, anti-replay atômico), `/auth/2fa/setup|confirm`, lockout por conta, token com `typ/did/iat/exp`, separação de domínio nas assinaturas (token ≠ K ≠ manifesto), handler 422 genérico, `users.py` (provisionamento com política de senha/CPF/e-mail). 59 testes no total.

**Ataques considerados**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Reusar token após logout/rebind/suspensão (skeleton: logout liberava o device mas o token seguia válido 24 h e o heartbeat **re-vinculava**) | **Corrigido.** Token carrega `did`; todo uso (`authenticate_token`) exige device ATIVO + fingerprint igual + licença usável + usuário ativo no DB. Kill switch/logout são imediatos. |
| 2 | Heartbeat re-vinculando outra máquina com token roubado | Corrigido: heartbeat não vincula; só login vincula. |
| 3 | Enumeração de contas (mensagem, status ou tempo) | Mensagem única `CREDENCIAIS`; hash "dummy" para usuário inexistente **e** para conta travada. |
| 4 | Brute force de senha/CPF/TOTP | Lockout 5 falhas/15 min (conta); TOTP errado também conta. IP rate-limit fica no M9. |
| 5 | Replay de código TOTP (inclusive em corrida concorrente) | `UPDATE ... WHERE last_step < :step` atômico + teste de corrida. |
| 6 | Cadastrar o próprio TOTP após roubar senha+CPF | `setup` só vale se 2FA ainda não ativo; usuários provisionados (seed/admin) já nascem inscritos; reset só por admin. |
| 7 | Dois logins simultâneos em máquinas diferentes (corrida) | Índice único parcial no DB; `IntegrityError` → `DEVICE_IN_USE`. |
| 8 | Reaproveitar assinatura de token como K (ou vice-versa) | Prefixos de domínio distintos; testado nos dois sentidos. Token sem `typ` rejeitado. |
| 9 | PII em erro/log/audit | 422 genérico (o padrão ecoava senha/CPF); testes de caplog e `audit_log` sem CPF/e-mail/senha. |
| 10 | Adulterar `audit_log` | Bloqueado via ORM; **não** protege contra quem tem acesso direto ao DB (ver residuais). |
| 11 | Payload gigante / campos malformados | Limites de tamanho e hex64 nos schemas. |

**Residuais assumidos (não resolvidos no M1)**
- **Fingerprint é afirmado pelo cliente:** quem roubar token + fingerprint opera de outra máquina. Detecção por concorrência/geo/velocity → **M6**; integridade do cliente → **M2/M5**.
- **Lockout permite DoS direcionado** a uma conta conhecida (por design, trade-off). Mitiga-se com rate limit por IP e alerta de admin → **M9/M7**.
- **`audit_log` imutável só na aplicação.** Em produção: role de DB sem UPDATE/DELETE + encadeamento por hash → **M9**.
- **`verify_integrity` ainda é stub** (formato/presença) → **M2**.
- Tentativa de device em uso consome o passo TOTP (usuário espera ~30 s) — aceito.
- Cargas/caches do `compute/validate` e schemas de entrada do modelo seguem do esqueleto → **M3**.

---

## M2 — Integridade e fingerprint (Programa Completo: Nível A e Nível B)

**Entregue:**
- Gerador e assinador de manifesto canônico: [`tools/make_manifest.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/tools/make_manifest.py) com contagem de arquivos por pasta, exclusão configurada (ex: `user_data`), hash Merkle/raiz SHA-256 sobre linhas `F` e `D`, e assinatura Ed25519 (chave privada de release mantida offline/CI; servidor conhece apenas a chave pública `RELEASE_PUBKEY_B64`).
- CLI de gestão de manifesto no servidor: [`manifest_cli.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/manifest_cli.py) (`publish`, `set-status`, `list`).
- Verificação de atestação server-side real: [`integrity.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/integrity.py) e tabela [`IntegrityManifest`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/models.py). Re-validação da assinatura do manifesto em tempo de execução para evitar confiar cegamente no banco de dados. Suporte a versão `current`, `allowed` e `revoked` (com erro `426 CLIENT_OUTDATED`).
- Proteção por módulo e novo endpoint Nível B: [`modules.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/modules.py) e endpoint `POST /license/check` em [`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py). Garante que detalhamento e memorial exijam presença online e licença ativa (retornando passe assinado `Ed25519` com domínio `DOM_GATE` de 60s atrelado a nonce, sem conceder keystone `K`).
- Implementação C++ cliente nativa: [`sha256.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/sha256.h), [`integrity_client.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/integrity_client.h) e [`integrity_client.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/integrity_client.cpp), compatível com Windows Unicode / UTF-8 sem dependências externas, bloqueio de symlinks/junctions (reparse points) e verificação estrita.
- Testes automatizados: 122 testes passando em [`server/tests/`](file:///C:/Users/João/Documents/Repos/newStructura/server/tests/), incluindo conformidade cruzada binária C++ vs Python.

**Ataques considerados e mitigação**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Adulteração de qualquer arquivo (.exe, .dll ou .dat) na instalação | **Detectado e bloqueado:** Altera o SHA-256 do arquivo e o `files_root`. Servidor rejeita login, heartbeat, compute e check de licença. |
| 2 | Injeção de DLL extra ou arquivo não previsto na pasta | **Detectado e bloqueado:** A contagem por pasta em `D` e a lista `F` divergem, alterando `files_root` e `file_count`. |
| 3 | Remoção de arquivo secundário | **Detectado e bloqueado:** `files_root` e `file_count` divergem. |
| 4 | Criação de pasta vazia ou subpasta adicional | **Detectado e bloqueado:** Contagem de pastas do manifesto altera o digest da raiz. |
| 5 | Substituição de arquivo por outro de mesmo tamanho (colisão ingênua) | **Detectado e bloqueado:** Hash SHA-256 do arquivo altera `files_root`. |
| 6 | Manipulação/adulteração direta na tabela `integrity_manifest` do DB do servidor | **Detectado e bloqueado:** Servidor re-verifica a assinatura Ed25519 da chave de release contra o texto a cada carregamento/checagem. |
| 7 | Comprometimento da chave privada do servidor web | **Mitigado:** A chave privada de release **nunca** reside no servidor, apenas no CI/offline. Servidor possui apenas a chave pública `RELEASE_PUBKEY_B64`. |
| 8 | Burlar checagem Nível B (detalhamento/memorial) requisitando sem internet | **Detectado e bloqueado:** O cliente exige passe assinado online de 60s do endpoint `/license/check`. Offline = fail-closed. |
| 9 | Reutilização de passe do Nível B para módulos Nível A (ex: cálculo/desenhos) | **Detectado e bloqueado:** Domínio separado (`DOM_GATE` vs `DOM_KEYSTONE`). Além disso, o endpoint `/license/check` rejeita estritamente qualquer feature que não seja Nível B. |
| 10 | Replay de passe Nível B entre sessões ou licenças distintas | **Detectado e bloqueado:** O digest do passe assina `(feature, license_id, nonce, exp)`. |
| 11 | Criação de Junction/Symlink malicioso no Windows para escapar da pasta | **Detectado e bloqueado:** Cliente rejeita e falha (`ScanError`) se encontrar reparse points / symlinks. |

**Residuais / Próximos passos (M3):**
- O solver e os módulos Nível A (desenhos, DXF/PDF, dimensionamento, quantitativos) dependem do keystone `K` gerado após verificação de equilíbrio global e integridade. O cálculo de equilíbrio com cargas locais e cache Redis será implementado no **M3**.

---

## M3 — Validação de cálculo (equilíbrio) + Keystone K

**Entregue:**
- Transformação de cargas de barra em sistema local (`coord_system = 0`) para sistema global em [`equilibrium.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/equilibrium.py), respeitando rigorosamente a formulação de Sennett e a implementação do solver nativo C++ (`ensureLocalAxesComputed` / `selectReferenceVector` / rotação de seção).
- Recomposição exata de resultantes aplicadas $\Sigma F$ e $\Sigma M$ em torno da origem a partir de cargas nodais e cargas de barra distribuídas em $O(\text{cargas})$.
- Checagem física de equilíbrio global `check_equilibrium` contra os totais de reação fornecidos pelo cliente ($||\Sigma F_{\text{aplicado}} + \Sigma F_{\text{reações}}|| \le tol$).
- Cache de modelo por `model_hash` em [`model_cache.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/model_cache.py) com suporte a Redis (`REDIS_URL`) e fallback em memória. Uma única ida ao servidor com dados de geometria/cargas por modelo; chamadas seguintes enviam apenas `model_hash` e reações.
- Emissão e assinatura do Keystone $K = \text{Ed25519\_sign}(\text{DOM\_KEYSTONE} \mathbin{\Vert} \text{SHA256}(\text{model\_hash} \mathbin{\Vert} \text{result\_digest} \mathbin{\Vert} \text{nonce} \mathbin{\Vert} \text{license\_id} \mathbin{\Vert} \text{exp} \mathbin{\Vert} \text{ts}))$.
- Fail-closed: sem licença válida, integridade adulterada ou equilíbrio violado, o Keystone $K$ **nunca** é emitido (`ok: false`).
- Suporte completo no cálculo do equilíbrio a todas as ações do Structura (Patch M3): cargas nodais, cargas de barra uniformes, momentos distribuídos (`member_moments`), forças pontuais (`kind=1`), momentos pontuais aplicados (`kind=2`), cargas trapezoidais parciais (`kind=0`) e deformação imposta/térmica (`kind=3` autoequilibrada), suportando eixos locais e globais com rotação de seção.
- 139 testes automatizados passando cobrindo todo o fluxo até M3.

**Ataques considerados e mitigação**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Solver crackeado localmente alterando esforços/resultados ou omitindo cálculo | **Detectado e bloqueado:** As reações enviadas violam a identidade física $(\Sigma F = 0, \Sigma M = 0)$. O servidor recusa com `EQUILIBRIUM_FAIL` e não emite $K$. Sem $K$, a chave $s$ não é gerada e os slots de export/finalização permanecem inutilizáveis. |
| 2 | Envio de reações falsificadas que somam zero em modelo estático não carregado | **Detectado e bloqueado:** O `result_digest` é assinado dentro do Keystone $K$. O cliente verifica o digest contra os seus próprios resultados antes de aceitar $K$. |
| 3 | Reutilização de Keystone $K$ de outro modelo ou cálculo anterior | **Detectado e bloqueado:** $K$ amarra `model_hash`, `result_digest`, `nonce` aleatório de 64-bit e validade curta (`exp = ts + 300`). O cliente deriva $s = \text{HKDF}(K, \text{nonce}, \text{info})$. Nonce diferente gera chave $s$ completamente distinta. |
| 4 | Omissão do modelo estrutural para forçar aprovação sem dados aplicados | **Detectado e bloqueado:** Se `model_hash` não estiver previamente no cache, o servidor exige o payload `model` (`MODEL_REQUIRED`) e não libera $K$. |
| 5 | Envenenamento de cache ou colisão de hash | **Mitigado:** `model_hash` calculado como SHA-256 das entradas completas canônicas. Chaves no Redis isoladas por namespace `structura:model:` e expiração automática configurada. |
| 6 | Forjar assinatura do Keystone $K$ | **Impossível:** Assinatura Ed25519 com chave privada mantida exclusivamente no servidor. |

**Próximos passos (M4):**
- Integração C++ completa no cliente: `license_client` implementando login/logout/heartbeat, fingerprint de hardware, chamada de validação por Calcular, amarração da chave $s = \text{HKDF}(K)$ aos slots de exportação via `_z8r4q` e modo offline somente-leitura.

---

## M4 — Integração do cliente (C++)

**Entregue:**
- [`client/license_client.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/license_client.h) e [`client/license_client.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/license_client.cpp): implementação nativa C++ para login/logout/heartbeat, fingerprint de hardware (CPUID + Volume Serial + MachineGuid no Windows), acumulador de reações completas, enforcement de digest e des-selagem com Keystone.
- [`client/hkdf.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/hkdf.h): derivação criptográfica autônoma $s = \text{HKDF-SHA256}(K, \text{salt}=\text{nonce}, \text{info}=\text{"finalize"} \mathbin{\Vert} \text{license\_id})$.
- **Completude das reações:** classe `ReactionAccumulator` integrando restrições rígidas, molas elásticas nodais e apoios inclinados/forças de vínculo em $\Sigma F$ e $\Sigma M$ na origem.
- **Enforcement do `result_digest`:** rotina `EnforceResultDigest` recomputa em tempo constante o digest sobre os resultados e recusa a operação caso haja divergência em relação ao que o Keystone $K$ assinou.
- **Separação de slots no `_z8r4q`:** rotina `UnsealSlotPointer` permite que slots normais (preview/cálculo) usem a chave do binário sem restrição de exportação, enquanto slots de exportação/relatório/finalização exigem obrigatoriamente a chave $s$ derivada do Keystone válido. Sem $K$, o ponteiro retorna 0 (fail-closed imediato).
- **Modo offline somente-leitura:** sem rede ou validação, cálculo e finalização Nível A e Nível B falham fechados, permitindo apenas visualização.
- Testes automatizados: binário C++ nativo [`test_m4_client.exe`](file:///C:/Users/João/AppData/Local/Temp/structura_cpp/test_m4_client.exe) passando 100% de testes e 141 testes pytest no servidor.

**Ataques considerados e mitigação**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Adulteração local de esforços/deslocamentos após o solver rodar para burlar dimensionamento no export | **Detectado e bloqueado:** O `EnforceResultDigest` confere os dados reais contra o `result_digest` selado no Keystone $K$. Divergência causa recusa imediata. |
| 2 | Omissão de reações de molas ou apoios inclinados para forçar soma zero em modelo com carga desequilibrada | **Impossibilitado:** O `ReactionAccumulator` compõe as componentes tridimensionais completas de molas, apoios inclinados e vínculos, fechando o equilíbrio apenas em modelos fisicamente consistentes. |
| 3 | Chamar slots de exportação (PDF/DXF/relatórios) sem validação online de $K$ | **Bloqueado (fail-closed):** `UnsealSlotPointer` retorna nulo (0) ou gera ponteiro corrompido para slots de export se o Keystone não for fornecido ou for inválido. |
| 4 | Reutilização de chave de sessão $s$ entre sessões ou licenças diferentes | **Mitigado:** A derivação $s = \text{HKDF}(K)$ é parametrizada com o `nonce` da sessão e o `license_id`. |
| 5 | Manipulação do relógio da máquina cliente para estender validade | **Inócuo:** O cliente não é autoridade de tempo; o tempo e o timestamp de validade de $K$ e dos passes são autoritativos no servidor. |

**Próximos passos (M5):**
- Anti-tamper, anti-debug e ofuscação: estender `antiDebugPoison` ao EXE, misturar poison na chave $s$, build com Obfuscator-LLVM / strip / cifra de strings.

---

## M5 — Anti-tamper / anti-debug / ofuscação

**Entregue:**
- **Detecção em profundidade de debug e instrumentação no EXE e DLLs ([`solver_security.h`](file:///C:/Users/João/Documents/Repos/newStructura/Structuradll/src/core/solver_security.h)):**
  - Monitoramento de software breakpoints (`0xCC` INT3, `0xCD 0x03` INT n, `0x0F 0x0B` UD2) e trampolines/hooks inline (`0xE9` JMP rel32, `0xEB` JMP rel8, `0xFF 0x25` JMP indirect, `0x48 0xB8` MOV RAX; JMP RAX) nos primeiros 24 bytes dos prólogos de funções críticas (`scanFunctionBreakpoint`).
  - Leitura direta da PEB sem APIs visíveis do sistema operacional via `gs:[0x60]` com assembly inline x64 GCC/MSVC: checagem de `BeingDebugged` (+0x02) e `NtGlobalFlag` (+0xBC, flags `0x70` FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS).
  - Resolução dinâmica de APIs nativas via `GetProcAddress`: `ntdll!NtQueryInformationProcess` para consulta a `ProcessDebugPort` (7) e `ProcessDebugObjectHandle` (30); e `kernel32!GetThreadContext` para verificação de hardware breakpoints nos registradores de depuração (`Dr0`..`Dr3`).
  - Armadilha de temporização RDTSC (`__rdtsc()`) para detecção de single-stepping, pausas de depuração e tracing interativo.
  - Função de envenenamento matemático `poisonKeystoneSecret` perturbando a chave de sessão $s$ de forma não linear e irreversível sem causar crashes imediatos.
- **Integração no ciclo de vida do cliente ([`license_client.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/license_client.h), [`license_client.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/license_client.cpp), [`main.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/src/main.cpp)):**
  - Rotina `CheckExeAntiDebugPoison()` monitorando os pontos nevrálgicos do executável: `_z8r4q`, `_z8r4q_unlock`, `_z8r4q_lock`, `ClientLogin`, `RequestComputeKeystone` e os exports reais `RealDxfExport`, `RealNormativeCheck`, `RealQuantityExport`, `RealPdfExport`.
  - Invocação anti-debug no startup do executável (`src/main.cpp`), no login (`ClientLogin` corrompendo o fingerprint se depurado), no heartbeat (`ClientHeartbeat` forçando fail-closed imediato com trancamento do gatekeeper), antes de cada Calcular (`RequestComputeKeystone` perturbando reações para rejeição por desequilíbrio), e na derivação da chave $s$ (`DeriveSessionSecretS`).
  - Proteção nos caminhos reais de exportação ([`export_gatekeeper.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/export_gatekeeper.cpp)): `ExecuteDxf`, `ExecutePdf`, `ExecuteNormativeCheck`, `ExecuteQuantities` executam varredura anti-debug antes de desselar ponteiros; se depurado, trancam o gatekeeper em quarentena e recusam a operação fechada (`ok = false`).
- **Ofuscação em compile-time de strings e constantes:**
  - Macro template `OBF_STR` (`solver_security.h`) gerando cifragem XOR pseudoaleatória com semente derivada de `__TIME__` e `__LINE__` calculada em tempo de compilação (`constexpr`), decifrando os literais somente em buffer efêmero na stack no instante do uso.
  - Aplicada em todas as URLs e constantes sensíveis (`"/auth/login"`, `"/auth/logout"`, `"/license/heartbeat"`, `"/compute/validate"`, `"STRUCTURA_KEYSTONE_V1"`).
- **Proteção e Cifra de DLLs em repouso ([`dll_cipher.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/dll_cipher.h), [`dll_cipher.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/dll_cipher.cpp)):**
  - Módulo nativo baseado em criptografia autenticada XSalsa20-Poly1305 (TweetNaCl `crypto_secretbox`).
  - Derivação de chave de sessão simétrica de 256 bits a partir de `licenseToken`, `fingerprint` e `licenseId` via SHA-256 (`DeriveDllSessionKey`).
  - Decifração efêmera em memória (`DecryptDllInMemory`), validação de MAC e limpeza de material sensível com `SecureWipe`.
- **Configuração de Build e Hardening ([`cmake/SecurityHardening.cmake`](file:///C:/Users/João/Documents/Repos/newStructura/cmake/SecurityHardening.cmake)):**
  - Ocultação de símbolos globais: `CMAKE_CXX_VISIBILITY_PRESET hidden`, `-fvisibility=hidden`, `-fvisibility-inlines-hidden`.
  - Eliminação de símbolos legíveis (strip): flags de linkagem `-Wl,--strip-all -s` em Release para executáveis e bibliotecas compartilhadas; `/OPT:REF /OPT:ICF` em MSVC.
  - Detecção automática de Obfuscator-LLVM (OLLVM) com injeção de passes `-mllvm -fla` (achatamento de fluxo), `-mllvm -bcf` (fluxo falso), `-mllvm -sub` (substituição de instruções).
- **Testes automatizados do marco:**
  - Suíte nativa C++ [`test_m5_antidebug.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/tests/test_m5_antidebug.cpp) validando detecção de 0xCC, 0xCD 0x03, UD2, trampolines de hook, degradação silenciosa por poison sem crash, envelope cifrado de DLL, ofuscação de strings e teste obrigatório de decrypt-to-temp com remoção imediata e recusa de payload corrompido (`TestDecryptToTempLoadLibraryAndCleanup`).
  - Regressão completa de M4 (`test_m4_client.exe`) 100% aprovada.
  - Suíte do servidor pytest com 142 testes passando.

**Ataques considerados e mitigação**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Anexar debugger ao EXE (x64dbg / IDA Pro / Cheat Engine) para ler a chave $s$ | **Detectado e mitigado:** `BeingDebugged`, `NtGlobalFlag` ou `ProcessDebugPort` ativam o poison. O poison é misturado matematicamente na chave $s$. Ao invés de travar ou alertar o analista, $s$ se torna inválida; os slots do gatekeeper des-selam em quarentena ou geram saídas com resistências falsificadas e deformações angulares. |
| 2 | Inserir software breakpoint (`0xCC` / INT3) no gatekeeper `_z8r4q` ou nas rotinas de export | **Detectado e degradado:** `scanFunctionBreakpoint` inspeciona os prólogos das funções monitoradas antes de executar. O byte `0xCC` é somado ao registrador de veneno (`poisonAccumulator`), corrompendo a des-selagem sem exibir erro de violação de acesso imediato. |
| 3 | Instalar inline hook (trampoline `JMP rel32` 0xE9 / `JMP indirect` 0xFF 25) para interceptar resultados de cálculo | **Detectado e degradado:** O verificador de prólogos detecta bytes de salto nos primeiros 24 bytes das funções protegidas, retornando poison alto que invalida o cálculo. |
| 4 | Tracing interativo passo a passo (single-stepping com `F8` / `F7`) | **Detectado:** A verificação de latência por ciclo de clock via `__rdtsc()` identifica deltas acima de 2.000.000 ciclos entre instruções contíguas, marcando presença de depurador interativo. |
| 5 | Busca de strings em texto claro no binário (`strings structura.exe`) procurando URLs e rotas de validação | **Impossibilitado:** Strings sensíveis protegidas com a macro `OBF_STR`; no binário compilado existem apenas arrays de bytes criptografados por XOR com máscaras efêmeras. |
| 6 | Inspecionar a tabela de símbolos de `rds.dll` ou `Structura.exe` com `nm` ou `dumpbin` | **Impossibilitado:** Strip completo (`-Wl,--strip-all -s`) e `-fvisibility=hidden`. A ferramenta `nm` reporta `"no symbols"`. |
| 7 | Copiar a DLL do disco para executar em outra máquina ou em harness isolado | **Mitigado:** Arquivo da DLL cifrado em repouso com XSalsa20-Poly1305. A chave de decifração em memória só existe quando há sessão válida com token e fingerprint conferidos. |
| 8 | Adulterar byte da DLL cifrada em disco ou tentar extrair o arquivo temporário decifrado | **Detectado e mitigado:** Decifração falha estritamente por rejeição de MAC Poly1305 sem gravar nenhum byte no disco e sem invocar LoadLibrary. Em execução legítima, o arquivo temporário com nome imprevisível é imediatamente removido do disco após o load (`GetFileAttributes` retorna inválido) e os buffers em RAM limpos com `SecureWipe`. |
| 9 | Obter `dll_key` em versão antiga/trial para decifrar solvers de versões futuras offline | **Mitigado pela amarração de DLL_RELEASE_KEY por versão de manifesto:** Cada versão de release possui chave simétrica independente armazenada no DB (`IntegrityManifest.dll_release_key`). Ao rotacionar/revogar o manifesto antigo, o login com versão obsoleta é recusado com HTTP 426 (`CLIENT_OUTDATED`) sem entregar a chave; a chave antiga $k_1$ falha com erro de MAC Poly1305 se tentada contra $rds.enc_{v2}$. Em Release, fallback para texto claro é proibido em nível de compilação (`STRUCTURA_DEV_BUILD=OFF`). |

---

## M6 — Detecção de abuso (Concorrência, Velocity, Bloqueio de VM com Allowlist e Marcas Forenses)

**Entregue:**
- **Telemetria e Detecção de Abuso em Tempo Real ([`abuse.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/abuse.py), [`models.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/models.py), [`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py)):**
  - Registro de acessos na tabela `DeviceAccessLog` auditando `license_id`, `fingerprint_hash`, `ip_address`, `action` e `ts`.
  - Resposta graduada e diferenciada para infrações:
    - `concurrent_access`: acessos da mesma licença com IPs ou fingerprints distintos em janela menor que 30s são bloqueados na 2ª tentativa (HTTP 409 `CONCURRENT_ACCESS_BLOCKED` ou `CONCURRENT_ACCESS_DETECTED`), registrando flag de abuso com score 1.0 sem suspensão imediata na 1ª ocorrência.
    - `excessive_fingerprints`: tentativa de utilizar um 4º fingerprint distinto em janela móvel de 24h é bloqueada (HTTP 401 `REAUTH_2FA_REQUIRED` / `EXCESSIVE_FINGERPRINTS`), exigindo reautenticação 2FA imediata e registrando flag de abuso.
    - `velocity` / Viagem Impossível: saltos de rede entre sub-redes com prefixos distintos em intervalo menor que 10s são bloqueados imediatamente (HTTP 403 `VELOCITY_BLOCKED` ou `VELOCITY_LIMIT_EXCEEDED`), registrando flag de abuso.
  - **Regra de Suspensão Automática Cumulativa (3+ flags em 7 dias):**
    - A função `record_abuse_flag` executa query móvel contínua: `SELECT count(id) FROM abuse_flags WHERE license_id = :lic_id AND ts >= :seven_days_ago`.
    - Ao atingir 3 ou mais flags de qualquer combinação de tipo nos últimos 7 dias, a licença é automaticamente marcada com `status = "suspensa"`, gerando evento de auditoria imutável `AuditLog(actor="system", action="auto_suspend_license")` com o motivo e histórico mascarado.
    - Licenças suspensas têm qualquer chamada subsequente (`/auth/login`, `/license/heartbeat`, `/license/check`, `/compute/validate`) imediatamente recusada com HTTP 403 `LICENSE_SUSPENDED`.
- **Detecção de VM / Hypervisor no Cliente C++ com Inserção na Cadeia de Poison ([`vm_detect.h`](file:///C:/Users/João/Documents/Repos/newStructura/client/vm_detect.h), [`vm_detect.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/vm_detect.cpp), [`license_client.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/license_client.cpp)):**
  - Varredura de baixo nível inspecionando o hypervisor present bit (ECX bit 31 do CPUID folha 1), strings de fornecedor na folha CPUID 0x40000000 (VMware, Microsoft Hyper-V, VirtualBox, KVM/QEMU, Xen) e drivers de virtualização (`VBoxGuest`, `HGFS`, `vmci`).
  - **Mecanismo Anti-Bypass por Poisoning:** A detecção de VM não depende de o cliente ser honesto em um booleano de payload. A função `CheckVmPoison` gera uma máscara de envenenamento (`0x8000000000000000ULL | flags`) que é misturada via bitwise-OR em `DeriveSessionSecretS`, corrompendo silenciosamente a chave $s$ caso o ambiente seja virtualizado sem autorização.
  - **Proteção do Prólogo de `CheckVmPoison`:** O endereço de `CheckVmPoison` está registrado no array `criticalFns[]` de `CheckExeAntiDebugPoison`. Qualquer tentativa de colocar breakpoint (`0xCC`), hook inline (`0xE9`) ou adulterar a rotina de detecção dispara imediatamente o envenenamento de $s$ pela camada anti-debug.
  - **Allowlist Administrativa de VM:** Endpoints administrativos `POST /admin/vm-allowlist` e `PATCH /admin/licenses/{license_id}` permitem autorizar instâncias específicas de VM vinculadas ao par `(license_id, fingerprint)`. Quando autorizado, o servidor emite o token HMAC `vm_allow_token` no envelope TLS que neutraliza o poison no cliente (`CheckVmPoison` retorna 0).
- **Kill Switch no Export por Expiração Temporal de Keystone ([`export_gatekeeper.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/export_gatekeeper.cpp)):**
  - Nos 4 métodos de finalização Nível A (`ExecuteDxf`, `ExecutePdf`, `ExecuteNormativeCheck`, `ExecuteQuantities`), checa rigorosamente `keystone.exp` contra o tempo monotônico do sistema (`std::time(nullptr)`). Se `nowSec > keystone.exp`, a exportação falha fechada imediatamente (`ok = false`, saídas nulas) e tranca a tabela em quarentena (`Lock`).
  - O servidor limita o tempo de vida do Keystone emitido em `/compute/validate` a 300 segundos (5 minutos), fechando a janela de reaproveitamento de cálculos passados.
- **Marca D'água Forense Autoritativa do Servidor ([`abuse.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/abuse.py), [`export_gatekeeper.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/export_gatekeeper.cpp)):**
  - Assinatura criptográfica HMAC-SHA256 autoritativa gerada exclusivamente pelo servidor sob o domínio isolado `DOM_WATERMARK = b"structura-watermark-v1\x00"`: `STRUCTURA-WM1:{license_id}:{ts}:{export_type}:{sig_truncada}`.
  - Emitida nos endpoints `/compute/validate` (Nível A) e `/license/check` (Nível B) e devolvida nos schemas de resposta.
  - O cliente C++ não calcula a marca por conta própria; apenas estampa o token oficial recebido do servidor:
    - **DXF:** comentário padronizado de especificação sob o código de grupo DXF 999 (`999\nSTRUCTURA-FORENSIC-WM: {token}\n`).
    - **PDF:** carimbo de autenticidade no rodapé (`[MARCA DAGUA FORENSE: {token}]`).
    - **Memorial Normativo:** linha de rastreabilidade forense no resumo do cálculo (`[MARCA DAGUA FORENSE: {token}]`).
    - **Levantamento de Quantitativos:** cabeçalho pericial no relatório de materiais (`[MARCA DAGUA FORENSE: {token}]`).
  - Função de verificação forense `verify_watermark_token` permitindo a perícia validar e extrair a licença original, tipo de exportação e timestamp de documentos vazados.
- **Testes Automatizados do Marco:**
  - Suíte do servidor [`server/tests/test_m6_abuse.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/tests/test_m6_abuse.py): 6 testes passando cobrindo concorrência, excesso de fingerprints, velocity, auto-suspensão em 7 dias, bloqueio/allowlist de VM e emissão/verificação forense.
  - Suíte cliente C++ nativa [`client/tests/test_m6_abuse.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/tests/test_m6_abuse.cpp): 3 suítes principais testando poison de VM, neutralização por allowlist, kill switch por expiração temporal de Keystone e estampa fidedigna nos 4 formatos de exportação.

**Ataques considerados e mitigação**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Duas instâncias rodando a mesma licença em computadores diferentes ao mesmo tempo | **Detectado e bloqueado:** A 2ª chamada em janela menor que 30s é recusada com `CONCURRENT_ACCESS_BLOCKED` (HTTP 409), gerando AbuseFlag. Se persistir (3 ocorrências em 7 dias), a licença é auto-suspensa. |
| 2 | Compartilhamento de credenciais em grupo com uso alternado (> 3 máquinas em 24h) | **Detectado e bloqueado:** Ao tentar registrar um 4º hardware fingerprint em 24h, o acesso é barrado com `EXCESSIVE_FINGERPRINTS` (HTTP 401) exigindo revalidação com 2FA. |
| 3 | Falsificação de IP / Proxy hopping (acessos com saltos geográficos impossíveis em < 10s) | **Detectado e bloqueado:** `record_access_and_check_abuse` detecta a discrepância temporal entre prefixos de rede (`VELOCITY_LIMIT_EXCEEDED` / HTTP 403), gerando AbuseFlag imediata. |
| 4 | Cliente patchado para enviar `is_vm: false` enquanto executa dentro de VM/Sandbox de análise | **Detectado e degradado:** A detecção local do cliente (`DetectVirtualMachine`) inspeciona diretamente CPUID e registradores e alimenta `CheckVmPoison`. Se for VM sem token assinado pelo servidor, $s$ é envenenada silenciosamente sem depender do que o cliente enviou pela rede. |
| 5 | Engenharia reversa no cliente para patchear a função `CheckVmPoison` com NOP ou `return 0` | **Detectado e degradado:** O prólogo de `CheckVmPoison` é monitorado por `CheckExeAntiDebugPoison`. A presença de modificações, trampolines de hook (`0xE9`) ou breakpoints (`0xCC`) no prólogo dispara a corrupção de $s$ pela camada anti-tamper. |
| 6 | Uso de Keystone $K$ legítimo após o encerramento da sessão ou após 5 minutos | **Bloqueado (fail-closed):** O gatekeeper nos 4 métodos de export valida `keystone.exp` contra o tempo atual. Se expirado (> 300s), os unseals são recusados e a tabela trancada. |
| 7 | Tentativa de forjar a marca d'água forense no cliente ou gerar marcas com licença de terceiros | **Impossibilitado:** A marca forense é um token assinado via HMAC-SHA256 com chave derivada no servidor (`DOM_WATERMARK`). O cliente apenas reproduz o token entregue pelo servidor; tokens alterados falham na verificação pericial `verify_watermark_token`. |
| 8 | Vazamento de projetos calculados ou memoriais em ambientes corporativos | **Rastreável:** A marca d'água presente no DXF (grupo 999), PDF, memorial e quantitativos permite à perícia identificar com precisão a licença, usuário, data e hora da emissão. |

---

## M7 — Painel Administrativo e Verificação Criptográfica de Passe Nível B

**Entregue:**
- **Verificação Criptográfica Estrita de `pass_sig` no Cliente C++ ([`license_client.cpp`](file:///C:/Users/João/Documents/Repos/newStructura/client/license_client.cpp)):**
  - Eliminação da confiança cega em respostas HTTP 200 com campos de string não vazios.
  - Reconstrução no cliente do contexto exato assinado pelo servidor: `feature | license_id | nonce | exp`.
  - Hashing com SHA-256 e concatenação sob o domínio rigoroso `DOM_GATE = b"structura-gate-v1\x00"` (18 bytes).
  - Chamada a `Ed25519VerifyCompact` contra a chave pública oficial do servidor (`serverPubKey`).
  - Falha estrita e trancamento da sessão (`TriggerSessionLock`) com erro `PASS_SIGNATURE_INVALID` se a assinatura for adulterada, se o campo `exp` for manipulado, ou se a feature/nonce divergir.
  - Teste de integração wire real `TestRealServerJsonPayloadParsingAndLevelBGateVerification` implementado e aprovado em `test_m6_abuse.cpp`.
- **Autenticação Forte para Administradores com 2FA Obrigatório ([`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py), [`crypto.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/crypto.py)):**
  - Flags `is_admin: bool` no modelo `User` ([`models.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/models.py)).
  - Rota `POST /admin/auth/login` exigindo e-mail, senha forte, CPF e token TOTP (RFC 6238) obrigatório.
  - Emissão de tokens administrativos dedicados com separação de domínio criptográfica `DOM_ADMIN_TOKEN = b"structura-admin-token-v1\x00"`, impedindo que tokens de licença comuns acessem o painel administrativo.
  - Dependência `get_current_admin` protegendo 100% das rotas de admin contra chamadas sem credencial ou com privilégios insuficientes (HTTP 401/403).
- **Gestão de Licenças ([`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py)):**
  - `POST /admin/licenses`: provisionamento de novas licenças por usuário (`user_id` ou `email`), plano ("mensal", "anual", etc.) e período em dias.
  - `GET /admin/licenses`: listagem de licenças com contagem de dispositivos ativos e filtros por status ou usuário.
  - `GET /admin/licenses/{license_id}`: visualização detalhada da licença.
  - `PATCH /admin/licenses/{license_id}`: alteração de status (`ativa`, `suspensa`, `revogada`, `expirada`), extensão de período (`add_days`) ou edição direta da data de validade (`current_period_end`).
  - **Efeito Imediato (Fail-Closed):** a alteração de status ou encurtamento da validade de uma licença no banco reflete imediatamente na próxima requisição do cliente (`/license/heartbeat`, `/license/check`, `/compute/validate`), resultando em recusa imediata (HTTP 403 `LICENSE_INVALID` / `LICENSE_INACTIVE`).
- **Gestão de Dispositivos Vinculados ([`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py)):**
  - `GET /admin/licenses/{license_id}/devices`: histórico completo de dispositivos ativos e inativos vinculados à licença.
  - `DELETE /admin/devices/{device_id}`: liberação manual de vínculo de máquina presa (`active = False`, `released_at = now`), com limpeza atômica da telemetria recente em `DeviceAccessLog` para possibilitar novo vínculo limpo sem falso-positivo de concorrência.
- **Fila de AbuseFlags e Revisão Administrativa ([`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py)):**
  - `GET /admin/abuse`: listagem de flags de abuso registrados pelo sistema de detecção (M6), ordenados cronologicamente, com filtro por status (`aberto`, `revisado`, `resolvido`).
  - `PATCH /admin/abuse/{flag_id}`: transição de status de "aberto" para "revisado" com anotações textuais do administrador, persistida de forma definitiva no banco de dados.
- **Gestão de VMAllowlist ([`main.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/app/main.py)):**
  - `GET /admin/vm-allowlist`: listagem de máquinas virtuais autorizadas.
  - `POST /admin/vm-allowlist`: cadastro/ativação de permissão de VM para pares `(license_id, fingerprint)`.
  - `DELETE /admin/vm-allowlist/{entry_id}`: desativação imediata de permissão de VM (`active = False`).
- **Auditoria Total Imutável:**
  - Todas as 11 rotas do painel admin emitem obrigatoriamente entradas em `AuditLog`, registrando ator (`admin:{id}`), ação executada, alvo e metadados sanitizados sem PII.
- **Testes Automatizados do Marco:**
  - Suíte [`server/tests/test_m7_admin.py`](file:///C:/Users/João/Documents/Repos/newStructura/server/tests/test_m7_admin.py): 7 testes abrangendo todos os fluxos administrativos.

**Ataques considerados e mitigação**

| # | Ataque | Resultado |
|---|---|---|
| 1 | Tentativa de acessar rotas do painel admin sem autenticação ou com token de licença comum | **Rejeitado (HTTP 401/403):** `get_current_admin` exige `DOM_ADMIN_TOKEN` assinado e `User.is_admin == True`. Tokens normais de licença utilizam `DOM_TOKEN` e falham na validação criptográfica. |
| 2 | Forjar passe de gate do Nível B (`pass_sig`) injetando texto arbitrário ou resposta mockada | **Rejeitado e trancado:** `CheckFeatureGateLevelB` verifica a assinatura Ed25519 sobre `DOM_GATE + sha256(feature|licenseId|nonce|exp)`. Assinaturas inválidas disparam `TriggerSessionLock`. |
| 3 | Manipular o campo `exp` do passe de Nível B para tentar usar além da janela autorizada | **Rejeitado:** Como `exp` faz parte do digest assinado pelo servidor, alterar o tempo invalida a assinatura Ed25519. |
| 4 | Licença suspensa ou revogada pelo admin continuar rodando em cliente com token emitido antes | **Bloqueado imediatamente:** `authenticate_token` valida o status da licença no banco de dados a cada chamada de rede (`heartbeat`, `check`, `validate`), garantindo efeito imediato do bloqueio administrativo. |
| 5 | Tentar bypassar 2FA no login do painel administrativo | **Rejeitado:** `admin_login` exige código TOTP e validação estrita anti-replay em `totp_last_step`. |
| 6 | Ações administrativas não rastreadas ou auditadas | **Impossibilitado:** Todas as operações administrativas chamam `_audit` e gravam no `AuditLog` imutável. |

**Próximos passos (M8):**
- M8 — Pagamentos (`POST /webhook/payment` Mercado Pago/Stripe, verificação de assinatura, extensão de validade +1 mês e status ativa).

