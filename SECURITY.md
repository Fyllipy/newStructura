# SECURITY.md — Como o aparato de segurança do Structura funciona (e como mantê-lo)

> **Atualizações pós-auditoria (nov/2026):** CPF deixou de ser fator de login; 2FA é configurado pelo usuário no primeiro
> acesso; senha temporária com troca obrigatória; telemetria antifraude; painel admin com CSP, sessão de 30 min e proteção contra
> *path traversal*; servidor de produção com chaves próprias atrás de HTTPS. Detalhes e operação em
> `docs/LICENCIAMENTO_E_SERVIDOR.md`, `docs/PAINEL_ADMIN.md` e `docs/DEPLOY_SERVIDOR.md`.

> Este documento explica o sistema **como ele está implementado hoje**, para quem vai operar, dar manutenção
> ou auditar o produto. Para o desenho original e os critérios de aceite, ver `SPEC.md`. Para o histórico de
> marcos e decisões, ver `PLAN.md` e `server/REVIEW.md` (log adversarial, marco a marco). Este arquivo não
> substitui os outros três — é o "mapa + runbook" para quem precisa operar o sistema sem ler tudo de novo.

---

## 1. Princípio central

**Zero trust no cliente.** O programa (EXE + DLLs do solver) roda na máquina do usuário e pode ser lido,
depurado e patchado por quem tiver acesso físico a ela. Por isso, nenhuma decisão que importa — "pode
calcular", "pode exportar", "a licença está válida" — é decidida só pelo cliente. O cliente coleta e envia
evidências; o **servidor decide**, assina a decisão, e o cliente só consegue produzir uma saída finalizável se
tiver essa assinatura em mãos. Meta realista: tornar o crack **caro**, não "impossível".

Tudo no sistema segue **fail-closed**: qualquer falha (rede, assinatura, integridade, equilíbrio, expiração)
bloqueia a ação em vez de liberar por padrão.

---

## 2. Visão geral do fluxo

```
login (e-mail+senha+2FA; CPF fica cadastrado, não é fator) ──► license_token (curto, assinado, 1 dispositivo por licença)
        │
        ├─► heartbeat semanal (renova o token; detecta suspensão/revogação quase em tempo real)
        │
        ├─► a cada "Calcular" (Nível A):
        │     cliente roda o solver local → envia reaction_totals + model_hash ao servidor
        │     servidor recomputa o lado aplicado e checa equilíbrio global (ΣF=0, ΣM=0)
        │     se OK → servidor assina o Keystone K
        │     cliente deriva s = HKDF-SHA256(K) e usa s para desselar os slots de export/finalização
        │
        └─► módulos de Nível B (detalhamento, memorial):
              cliente chama /license/check a cada uso → precisa de ok=true (licença válida + online)
              não tem keystone; só gate de licença
```

Dois níveis de proteção (ver `SPEC.md` §7.1 para a justificativa):

| Nível | Módulos | Trava |
|---|---|---|
| **A** (com keystone) | cálculo, desenhos/pranchas, DXF/PDF, dimensionamento/verificações normativas, quantitativos | `K` assinado pelo servidor sobre o resultado; sem `K`, o `_z8r4q` (gatekeeper) nunca entrega os ponteiros reais dos slots de export |
| **B** (só gate online) | detalhamento, memorial de cálculo | `/license/check` precisa responder `ok=true`; offline = bloqueado |

---

## 3. Mapa de componentes

### Servidor (`server/app/`)
| Arquivo | Responsabilidade |
|---|---|
| `main.py` | Todos os endpoints HTTP (`/auth/*`, `/license/*`, `/compute/validate`, `/admin/*`) |
| `crypto.py` | Todas as primitivas de assinatura/cifra e os domínios de separação (ver §4) |
| `licensing.py` | Regras de vínculo de dispositivo, 1-máquina-por-licença, liberação por heartbeat velho |
| `integrity.py` | Verificação do manifesto de integridade do cliente; chave de cifra da DLL por versão |
| `manifest.py` / `manifest_cli.py` | Publicação/rotação/revogação de versões do manifesto assinado |
| `equilibrium.py` | Recomputo do lado aplicado e checagem de equilíbrio global |
| `model_cache.py` | Cache de `model_hash → (ΣF_aplicado, ΣM_aplicado)` (Redis ou memória) e anti-replay de nonce |
| `abuse.py` | Concorrência, velocity, excesso de fingerprints, allowlist de VM, auto-suspensão, marca d'água forense |
| `totp.py` | TOTP (2FA) próprio, RFC 6238 |
| `users.py` | Provisionamento de usuários (política de senha/CPF/e-mail) |
| `models.py` | Esquema ORM (`User`, `License`, `Device`, `AbuseFlag`, `VMAllowlist`, `IntegrityManifest`, `AuditLog`, ...) |

### Cliente (`client/`)
| Arquivo | Responsabilidade |
|---|---|
| `license_client.cpp/h` | Login, logout, heartbeat, `/compute/validate`, `/license/check` — toda a conversa HTTP com o servidor e verificação das assinaturas que voltam |
| `winhttp_client.cpp/h` | Cliente HTTP nativo (WinHTTP), fail-closed em timeout/erro |
| `vm_detect.cpp/h` | Detecção local de hypervisor/VM (CPUID + drivers de guest) e poison criptográfico |
| `dll_cipher.cpp/h` | Cifra/decifra das DLLs do solver em repouso (AES via TweetNaCl `crypto_secretbox`) |
| `export_gatekeeper.cpp/h` | Protege os 4 módulos de Nível A fora do solver (DXF, PDF, verificações normativas, quantitativos); checa `keystone.exp` (killswitch) e estampa a marca d'água |
| `tweetnacl.c/h`, `ed25519_compact.cpp/h` | Biblioteca de criptografia (TweetNaCl) e wrapper de verificação Ed25519 |
| `hkdf.h`, `sha256.h` | Primitivas auxiliares |
| `json_helper.h` | Parsing manual do JSON da wire (sem dependência externa) |
| `tests/` | Suítes por marco (`test_m4_client.cpp`, `test_m5_antidebug.cpp`, `test_m6_abuse.cpp`, ...) |

### No solver (`Structuradll/src/core/`)
| Arquivo | Responsabilidade |
|---|---|
| `solver_security.h` | Primitivas de seal/unseal de ponteiros, classificação de slots Preview vs Export, anti-debug (`antiDebugPoison`), geração de nonce |
| `bridge_dll/interface_dll.cpp/h` | O gatekeeper real `_z8r4q` (único export da DLL) — dispatch table autenticada; `_z8r4q_unlock`/`_z8r4q_lock` reselam os slots de export |

---

## 4. Criptografia: o que assina o quê, e com qual domínio

**Este é o ponto que mais já causou bug real neste projeto** (ver §8 "Lição aprendida"). Toda assinatura no
sistema usa o padrão `Ed25519_sign(priv, domínio ‖ SHA256(contexto))`, com um domínio de separação **exclusivo
por uso** — isso impede que a assinatura de um propósito seja reaproveitada (replay) para outro. Os domínios
vivem em `server/app/crypto.py` e **têm que bater byte a byte** com a constante usada no lado do cliente que
verifica.

| Uso | Domínio (servidor, `crypto.py`) | Onde o cliente verifica |
|---|---|---|
| Token de licença | `DOM_TOKEN = b"structura-token-v1\x00"` | `license_client.cpp` (via `authenticate_token` no servidor; o cliente não reassina, só guarda e reenvia) |
| Keystone `K` (Nível A) | `DOM_KEYSTONE = b"structura-keystone-v1\x00"` | `license_client.cpp::RequestComputeKeystone` |
| Passe de Nível B | `DOM_GATE = b"structura-gate-v1\x00"` | `license_client.cpp::CheckFeatureGateLevelB` |
| Token de admin | `DOM_ADMIN_TOKEN = b"structura-admin-token-v1\x00"` | `get_current_admin` (servidor; o painel não reverifica, só reenvia o token) |
| Manifesto de integridade | `DOM_MANIFEST` | `integrity_client.cpp` |
| Marca d'água forense | `DOM_WATERMARK = b"structura-watermark-v1\x00"` (em `abuse.py`) | Não é verificada pelo cliente — é só estampada na saída; a verificação pericial é feita depois, no servidor, via `abuse.verify_watermark_token` |
| Cifra da DLL em repouso | chave simétrica por versão (`DLL_RELEASE_KEY`, ver §6), não é assinatura Ed25519 | `dll_cipher.cpp` |

**Chave pública do servidor:** só ela vai embutida no binário do cliente (`SERVER_PUBKEY_B64`, impressa por
`server/gen_keys.py`). A chave privada nunca saiu do servidor/secret manager.

---

## 5. Validação do cálculo (equilíbrio global)

O solver nunca é "confiado" diretamente. A cada clique em Calcular:

1. O cliente soma as reações (`outNodeReactions6`) em `reaction_totals` e manda, junto com `model_hash` (e o
   modelo completo, só na primeira vez que esse hash aparece — depois fica em cache por `model_hash` no
   servidor).
2. O servidor recomputa o **lado aplicado** (ΣF e ΣM de cargas nodais + resultantes de cargas de barra) — é
   barato, não precisa resolver o sistema linear de novo.
3. Compara `ΣF_aplicado + ΣF_reações ≈ 0` e `ΣM_aplicado + ΣM_reações ≈ 0` dentro de uma tolerância
   (`EQUILIBRIUM_REL_TOL`/`EQUILIBRIUM_ABS_FLOOR` no `.env`). Um solver adulterado (resultado inventado) quase
   sempre quebra essa identidade física.
4. Só se isso passar (e a licença/integridade também passarem) o servidor assina `K`.

`K` nunca carrega os resultados em si — só um hash deles (`result_digest`). O cliente verifica a assinatura e
confere que o `result_digest` recomputado localmente bate com o que foi assinado, antes de usar `K` para
qualquer coisa.

---

## 6. DLL cifrada em repouso e rotação de chave por versão

- `rds.dll`/`mefs.dll` (plaintext) nunca são distribuídos — o instalador carrega `rds.enc`/`mefs.enc`
  (gerados por `server/tools/encrypt_dll.py`).
- Cada **versão do manifesto de integridade** (`IntegrityManifest.version`) tem sua própria
  `DLL_RELEASE_KEY` (`IntegrityManifest.dll_release_key`, derivada por
  `integrity.derive_dll_key_for_version`). O login só entrega essa chave (`LoginResponse.dll_key`) se o
  manifesto da versão do cliente estiver com status `current` ou `allowed`.
- Se o manifesto estiver `revoked` (versão descontinuada/vulnerável), o login falha com `426
  CLIENT_OUTDATED` **antes** de qualquer chave ser considerada — a chave antiga nunca decifra o `.enc` de uma
  versão nova (o nonce/tag do AEAD é por arquivo, então usar a chave errada falha com erro de MAC).
- O cliente decifra a DLL **em memória**, grava num arquivo temporário só para o `LoadLibrary` conseguir
  carregar (Windows exige um caminho de arquivo), e apaga imediatamente depois (com fallback de
  `MOVEFILE_DELAY_UNTIL_REBOOT` se o delete falhar enquanto a DLL está mapeada). Nunca fica em disco além
  desse instante.
- **Em build de release** (`STRUCTURA_DEV_BUILD=OFF`, o padrão), não existe fallback para `.dll` em texto
  claro — se o `.enc` não existir ou a chave não vier do servidor, o carregamento falha fechado. O fallback
  só existe em build de dev (ver `README.md`, seção "Build de desenvolvimento local").

**Pendência conhecida, não bloqueadora:** `derive_dll_key_for_version` hoje reaproveita o mesmo
`SERVER_SIGNING_SEED_B64` usado para assinar Ed25519, como material de uma HMAC para fins diferentes. O ideal
é uma semente própria (`DLL_KEY_SEED_B64`) — registrado como item de hardening para o M9, não afeta a
segurança operacional hoje porque os domínios de uso (assinatura vs. derivação simétrica) não se misturam.

---

## 7. Anti-tamper no cliente

- **Anti-debug** (`antiDebugPoison`, em `solver_security.h`, estendido ao EXE): verifica PEB, chama
  `NtQueryInformationProcess`, confere registradores de debug de hardware, mede timing com RDTSC, e varre
  bytes de prólogo de várias funções críticas reais procurando `0xCC` (breakpoint) ou `0xE9` (hook inline).
  Em vez de abortar (o que avisaria o atacante), ele **envenena silenciosamente** a chave de sessão `s` — a
  saída passa a ser sutilmente errada (geometria, densidade, resistências) em vez de travar com erro óbvio.
- **Detecção de VM** (`vm_detect.cpp`): CPUID (bit de hypervisor + assinatura do fabricante) e drivers de
  guest conhecidos (VirtualBox, VMware, Hyper-V, KVM, Xen). Se detectar VM sem um `vm_allow_token` válido
  (emitido pelo servidor só quando há entrada na `VMAllowlist` para aquela licença+fingerprint), também
  alimenta o **mesmo mecanismo de poison** do anti-debug — a decisão nunca depende de um booleano que o
  cliente poderia simplesmente mentir ao servidor.
- **Ofuscação**: Obfuscator-LLVM (`-mllvm -fla/-bcf/-sub`), strip de símbolos, `-fvisibility=hidden` — tudo
  configurado em `cmake/SecurityHardening.cmake`. `OBF_STR` cifra strings sensíveis (endpoints, domínios) em
  tempo de compilação.
- **Nenhum desses mecanismos é "inquebrável"** — a meta é custo, não impossibilidade. Por isso o killswitch
  e a revogação server-side (§6, §9) são a linha de defesa que realmente importa: mesmo que o anti-tamper
  local seja contornado numa máquina específica, a próxima verificação no servidor (heartbeat, compute,
  check) ainda pode negar.

---

## 8. Detecção de abuso e ações administrativas

Implementado em `server/app/abuse.py`, chamado em `/auth/login`, `/license/heartbeat`, `/compute/validate` e
`/license/check`:

| Padrão detectado | Reação |
|---|---|
| Concorrência (2 fingerprints/IPs em <30s para a mesma licença) | Bloqueia a requisição em conflito (`409`), sem suspender na 1ª vez |
| Excesso de fingerprints (>3 distintos em 24h) | Exige reautenticação com 2FA na próxima tentativa |
| Velocity / viagem impossível (<10s entre sub-redes incompatíveis) | Bloqueia a requisição (`403`) |
| VM/sandbox sem allowlist | Bloqueia (ver §7) |
| **3 ou mais flags de qualquer tipo em 7 dias** | **Suspende a licença automaticamente** (`status="suspensa"`) — reativação só por admin |

Tudo isso gera `AbuseFlag`, revisável no painel admin (`PATCH /admin/abuse/{id}`).

### Lição aprendida (para não repetir)

Durante a revisão do M6, encontrei um bug que **quebrava toda a verificação do Keystone**: o servidor
serializava `exp`/`ts` como string ISO-8601, e o parser JSON do cliente (`JsonGetInt`) fazia
`std::stoi("2026-11-10T19:35:57Z")`, que parava no primeiro caractere não numérico e devolvia `2026` em vez da
época Unix real — fazendo a assinatura nunca bater. Isso passou por vários testes porque os testes C++
construíam o `KeystoneArtifact` direto em memória, sem nunca passar pelo parsing JSON real.

**Takeaway operacional:** qualquer teste de um caminho criptográfico que não serialize/deserialize pela wire
real (JSON de verdade, não um struct montado à mão) pode estar testando uma ilusão. Da mesma revisão também
saiu que o domínio do Keystone no cliente estava **literalmente diferente** do servidor desde o M4
(`STRUCTURA_KEYSTONE_V1` vs. `structura-keystone-v1\0`) — ambos os bugs já foram corrigidos e re-verificados
byte a byte, mas é o tipo de coisa que só aparece testando ponta a ponta contra o formato real da wire.

---

## 9. Painel administrativo

- Hoje é só API REST (`/admin/*` em `main.py`), autenticada por um token próprio (`DOM_ADMIN_TOKEN`,
  distinto do token de licença comum — um não serve para o outro) com 2FA obrigatório no login
  (`/admin/auth/login`).
- Toda rota exige `Depends(get_current_admin)` — não há rota administrativa aberta.
- Toda ação administrativa gera uma entrada em `AuditLog` (imutável na camada da aplicação).
- Funcionalidades: criar/listar/suspender/revogar/estender licenças; listar e liberar dispositivos; revisar
  fila de `AbuseFlag`; gerenciar `VMAllowlist`.
- **Sem interface gráfica própria ainda** — use a documentação interativa do FastAPI
  (`http://127.0.0.1:8000/docs`), autenticando com o botão "Authorize" e o `admin_token`. Uma interface web
  dedicada está em desenvolvimento (ver `PLAN.md`/pedidos em andamento).

---

## 10. Runbook de operação

### Rodar o servidor localmente
Ver `server/README.md` (inclui os problemas comuns de `;` no cmd.exe e política de execução do PowerShell).

### Gerar/rotacionar chaves do servidor
```
cd server
python gen_keys.py          # sem --write: só imprime, para colar no secret manager
python gen_keys.py --write  # cria/atualiza .env local; recusa sobrescrever por segurança
```
A chave pública impressa (`SERVER_PUBKEY_B64`) é a única que deve ir para o código do cliente.

### Publicar uma nova versão de release (bump de versão)
1. Suba `STRUCTURA_VERSION_MAJOR/MINOR/PATCH` em `cmake/ProductInfo.cmake`.
2. Publique uma entrada de `IntegrityManifest` para a nova versão com status `current` (via
   `server/manifest_cli.py` ou o fluxo equivalente) — isso também gera a `DLL_RELEASE_KEY` daquela versão.
   Rebaixe a versão anterior para `allowed` (não `revoked`, para não cortar quem ainda não atualizou).
3. Compile em release (`STRUCTURA_DEV_BUILD=OFF`, o padrão) para a edição desejada
   (`-DSTRUCTURA_EDITION=COMMERCIAL` ou `DEMO` — ver `README.md`).
4. Re-gere `rds.enc`/`mefs.enc` com `server/tools/encrypt_dll.py`, usando a `DLL_RELEASE_KEY` da nova versão
   — nunca reaproveite o `.enc` de uma versão anterior.
5. Gere o instalador de cada edição via `installer/structura.nsi` (comando documentado no cabeçalho do
   próprio arquivo, usa o `build/generated/product.nsh` gerado pelo CMake).

### Revogar uma versão vulnerável/descontinuada
Marque o `IntegrityManifest` daquela versão como `revoked`. No próximo login/heartbeat, qualquer cliente
nessa versão recebe `426 CLIENT_OUTDATED` e não recebe mais `dll_key` nenhuma.

### Suspender/revogar uma licença específica
Via `PATCH /admin/licenses/{id}` (ou o painel, quando existir) — efeito imediato: o próximo
heartbeat/compute/check daquela licença já falha fechado.

### Liberar um dispositivo travado (sem esperar os 7 dias automáticos)
`DELETE /admin/devices/{id}`. **Atenção:** isso também limpa o `DeviceAccessLog` recente daquela licença — ou
seja, reseta o histórico usado para detecção de velocity/concorrência, não só o vínculo de dispositivo.

### Autorizar um cliente legítimo rodando em VM
`POST /admin/vm-allowlist` com `license_id` + `fingerprint`. Sem isso, qualquer detecção local de VM
(§7) envenena a chave de sessão, mesmo que o servidor nunca tenha sido informado (a detecção é local,
independente do que o cliente reporta).

### Rodar a suíte de testes completa
```
cd server ; pytest -q                              # testes do servidor (chaves efêmeras, SQLite em memória)
```
Os testes C++ (`client/tests/*.cpp`) são compilados e rodados junto com o resto do projeto via CTest — ver
`DEVELOPER_GUIDE.md`/`CMakeLists.txt` para os targets exatos.

---

## 11. Pendências conhecidas (backlog de hardening, não bloqueadoras)

- Separar a semente usada para `DLL_RELEASE_KEY` da semente de assinatura Ed25519 (`DLL_KEY_SEED_B64` próprio) — §6.
- `vm_allow_token` hoje só é validado por formato (32 hex chars) no cliente, não por HMAC — mitigado pela
  detecção local de VM ser independente, mas um binário já patchado poderia cravar um valor fixo. Ver
  `server/REVIEW.md` M6/M9.
- Itens gerais do M9 (rate limit por IP, `docs_url` desligado em produção, rotação de `id` de chave, cadeia
  de hash no `audit_log`, pentest antes do lançamento) — ver `PLAN.md`.

Para o estado atualizado de cada marco e o que já foi corrigido, `server/REVIEW.md` é a fonte da verdade —
este documento é o resumo operacional, não substitui a leitura dele quando for investigar algo específico.
