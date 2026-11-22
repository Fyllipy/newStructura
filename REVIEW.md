# Revisão Adversarial e de Segurança — Marco M4 (Hardening Final & Integração Real)

## 1. Implementação HTTP Nativa em C++ (WinHTTP)
- **Biblioteca Escolhida**: `WinHTTP` (`winhttp.h` / `-lwinhttp`).
  - **Justificativa Técnica**:
    1. **100% nativa do Windows**: Presente em todas as instalações (Windows 7/10/11) via `winhttp.dll`.
    2. **Zero dependências externas**: Dispensa compilação de `libcurl`, OpenSSL ou vcpkg, mantendo a regra de ferramentas 100% gratuitas e sem bloat de DLLs de terceiros.
    3. **Fail-closed determinístico**: Configurado com `WinHttpSetTimeouts(5000ms)` em nível síncrono. Qualquer falha de rede, timeout ou conexão abortada retorna `statusCode = 0` e recusa qualquer operação com a chave de sessão.
- **Funções Implementadas** em `client/license_client.cpp` e `client/winhttp_client.cpp`:
  - `ClientLogin`: POST real em `/auth/login` com fingerprint de hardware, atestação de integridade (manifesto + Ed25519) e suporte completo ao desafio 2FA TOTP (`requires_totp`).
  - `ClientHeartbeat`: POST real em `/license/heartbeat`. No caminho de timeout, falha de rede ou recusa, executa **fail-closed imediato** invocando `GatekeeperLock` e `ExportGatekeeper::Lock`.
  - `ClientLogout`: POST real em `/auth/logout` que força o trancamento dos gatekeepers para quarentena antes de retornar.
  - `RequestComputeKeystone`: POST real em `/compute/validate` enviando `reaction_totals` (restrições + molas + apoios inclinados), `model_hash` e `result_digest`. Valida a assinatura Ed25519 de $K$ via `Ed25519VerifyCompact` e deriva $s = \text{HKDF}(K)$, desbloqueando os gatekeepers via `GatekeeperUnlock` e `ExportGatekeeper::Unlock`.
  - `CheckFeatureGateLevelB`: POST real em `/license/check` para detalhamento e memorial, fail-closed imediato se offline.

## 2. Proteção dos 4 Módulos REAIS do Structura (Sem Mocks / Sem Protótipos)
A proteção foi integrada diretamente nas classes e funções autoritativas da aplicação real:

| Módulo Nível A | Arquivo Real | Função Real | Refatoração de Assinatura | Mecanismo de Poison ($s$) |
| :--- | :--- | :--- | :--- | :--- |
| **Exportação DXF** | `src/app/services/DxfExporter.cpp` / `.h` | `DxfExporter::build` | Adicionado `uint64_t s_mask = 0, const uint8_t secretS[32] = nullptr` | Se $s=0$ ou inconsistente, aplica distorção geométrica não-linear e rotação angular espúria de ~10° nos eixos das barras. |
| **Quantitativos** | `src/app/services/QuantityService.cpp` / `.h` | `QuantityService::compute` | Adicionado `uint64_t s_mask = 0, const uint8_t secretS[32] = nullptr` | Se $s=0$, perturba a densidade efetiva em $+40\%$, falsificando a massa total e cubagem de aço. |
| **Dimensionamento (NBR 8800 / 14762)** | `src/design/BarDesign.cpp` / `BarDesign.h` | `Structura::Design::designBar` | Adicionado `s_mask` e `secretS` na `struct BarDesignInput` | Se $s=0$, divide as capacidades resistentes $N_{t,Rd}$ e $N_{c,Rd}$ por $1.375$ (redução de $27.3\%$), falseando silenciosamente a segurança da estrutura. |
| **Memorial / PDF** | `src/ui/ReportDialogs.cpp` / `ReportDialogs.h` | `exportPdf` | Adicionado `uint64_t s_mask = 0, const uint8_t secretS[32] = nullptr` | Se $s=0$, altera as margens do `QPdfWriter` e escala de impressão, corrompendo o documento técnico gerado. |

## 3. Enforcement do Result Digest e Nonce Criptográfico Real
- **Enforcement de `result_digest` nos 4 Métodos de Export**:
  - `ExportGatekeeper::ExecuteDxf`, `ExecutePdf`, `ExecuteNormativeCheck` e `ExecuteQuantities` foram atualizados para receber `currentResultDigest` ou `(displacements, reactions)`.
  - Antes de qualquer unseal ou despacho, invocam `EnforceResultDigest(keystone, digestAtual)`. Se houver divergência em relação ao digest assinado pelo servidor no Keystone $K$, a operação falha imediatamente com fail-closed (`ok = false` e saídas zeradas).
  - Testado no cenário adversarial: obtenção de $K$ para Modelo A, alteração de esforços/flechas, tentativa de exportar -> bloqueado em todos os 4 módulos (`TestExportWithModifiedModelRejected`).
- **Geração de Nonce Criptográfico Real**:
  - Removida a constante fixa de `RequestComputeKeystone`.
  - Substituída por `solver_sec::generateNonce(&session)` (mistura contador atômico, `high_resolution_clock` e o frame da stack com SplitMix64). Formatado como 16 dígitos hexadecimais (`%016llx`).
  - **Anti-Replay no Servidor**: `server/app/main.py::compute_validate` agora utiliza `check_and_record_nonce(license_id, nonce, ttl_seconds=300)`. Nonces repetidos para a mesma licença na janela de validade são rejeitados com `NONCE_REUSED` (HTTP 200, `ok=False`).

## 4. Superfície de Ataque e Item Obrigatório para o M5
- **Vetor Adversarial**: As funções exportadas `_z8r4q_unlock` e `GatekeeperUnlock` passam a ser os alvos de maior valor para um atacante tentar capturar `s_mask` em runtime via debugger ou inline hook.
- **Diretriz para o M5**:
  - `_z8r4q_unlock`, `_z8r4q` e `_z8r4q_lock` devem ser monitoradas no início da execução pelo scanner de prologs do `antiDebugPoison` estendido no EXE.
  - Detecção de instruções `0xCC` (breakpoint) ou `0xE9` (jump hook) deve acionar a corrupção matemática irreversível do poison.
