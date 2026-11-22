# SPEC — Aparato de Segurança e Licenciamento (Structura)

> Especificação de implementação. Stack do cliente: **C++ nativo** (EXE + `rds.dll` / solvers).
> Princípio central: **zero trust no cliente**; toda decisão que importa é autoritativa no servidor.
> Meta realista: tornar o crack caro o bastante para não compensar — não "impossível".
> Escopo: proteger o **programa inteiro (Structura)** — o solver é apenas **um** módulo (modelagem, cálculo, desenhos/pranchas, DXF/PDF, memorial, dimensionamento, relatórios, quantitativos, detalhamento).

---

## 1. Objetivos de segurança

1. O cálculo só ocorre com **licença válida** verificada no servidor.
2. A **IP valiosa** (solvers e demais módulos) roda local, mas cada **saída valiosa** só é **finalizável** com participação do servidor.
3. Detectar **adulteração** de arquivos e da quantidade de arquivos na pasta.
4. Detectar **uso por terceiros** / compartilhamento de licença.
5. Servidor **à prova de leaks**, hosped­ável em tier gratuito.
6. Conformidade **LGPD** (CPF, e-mail, senha).

---

## 2. Modelo de ameaças (STRIDE resumido)

| Ameaça | Vetor | Mitigação |
|---|---|---|
| Spoofing | cliente falso, servidor falso (MITM) | TLS 1.3 + certificate pinning; respostas assinadas (Ed25519) |
| Tampering | patch no EXE/DLL, DLL injetada | manifesto assinado de hashes + contagem; atestação server-side; anti-debug |
| Repudiation | negar uso | audit log imutável |
| Info disclosure | leak de chaves/segredos/PII | segredos em secret manager; CPF cifrado; erros genéricos |
| DoS | flood de endpoints | rate limiting por IP/licença/conta |
| Elevation | burlar licença | keystone `K` server-side gating export; fail-closed |

---

## 3. Arquitetura e componentes

- **Cliente (C++)**: autentica, coleta fingerprint, verifica integridade local, roda o solver, calcula o resumo de equilíbrio, chama o servidor a cada *Calcular*, valida `K`, destrava export.
- **API (servidor)**: auth, emissão/validação de licença, atestação de integridade, verificação de equilíbrio, emissão de `K`, detecção de abuso, webhook de pagamento.
- **Banco de dados**: usuários, licenças, dispositivos, pagamentos, auditoria, flags de abuso.
- **Painel admin**: gestão de licenças/usuários/dispositivos (2FA obrigatório).

Fluxo macro: `login → token de licença (curto, assinado) → a cada Calcular: solver local + validação server-side (equilíbrio) → K assinado → destrava export`. `heartbeat` semanal revalida.

---

## 4. Modelo de licenciamento

- Identidade: **CPF + e-mail + senha**. Senha autentica (Argon2id); CPF/e-mail identificam.
- Vínculo: **1 máquina ativa por licença** (fingerprint de hardware). Logout libera; novo login vincula.
- Recuperação de vínculo preso: liberar após **7 dias** sem heartbeat, ou por ação do admin.
- Validade **mensal**; **pagamento confirmado** (webhook) renova por +1 ciclo.
- Estados: `ativa`, `expirada`, `suspensa`, `revogada`. Só `ativa` + dentro da validade libera cálculo.
- **Autoridade de tempo = servidor** (nunca o relógio do cliente).

---

## 5. Criptografia (algoritmos e onde as chaves vivem)

| Uso | Primitiva | Chave |
|---|---|---|
| Transporte | TLS 1.3 + certificate pinning | cert do servidor |
| Token de licença | assinatura EdDSA (Ed25519) ou JWT EdDSA | priv no servidor; **pub embutida no cliente** |
| Keystone `K` | Ed25519 sobre hash do contexto | priv no servidor; pub embutida no cliente |
| Derivação `s` (destrava export) | HKDF-SHA256 | derivada de `K` em runtime, só em memória |
| Senha | Argon2id (salt por usuário) | — |
| CPF em repouso | AES-256-GCM (+ HMAC-SHA256 p/ busca) | no secret manager do provedor |
| Fingerprint | SHA-256 de sinais compostos | — |
| DLL em repouso (se aplicável) | AES-256-GCM | chave de sessão entregue no login, só em memória |

Nenhum segredo no repositório ou no binário do cliente além de **chaves públicas**.

---

## 6. Validação do solver — equilíbrio global ("1+1=2")

Identidade física: em toda análise linear correta, `ΣF_aplicado + ΣF_reações = 0` e `ΣM_aplicado + ΣM_reações = 0` (momentos em torno da origem).

**O servidor recomputa o lado aplicado** a partir das entradas (barato, O(cargas)):
- `ΣF_aplicado` = Σ cargas nodais `(Fx,Fy,Fz)` + Σ resultantes de cargas de barra (q × comprimento, transformadas para o global via geometria).
- `ΣM_aplicado` (origem) = Σ `r_i × F_i` + Σ momentos aplicados `(Mx,My,Mz)` + contribuição de momento das cargas de barra.

**O cliente envia** apenas os **totais de reação** calculados de `outNodeReactions6`:
- `ΣF_reações`, e `ΣM_reações` (origem) = Σ `r_j × R_j` + Σ `(Mx,My,Mz)` das reações.

**Checagem**: `‖ΣF_aplicado + ΣF_reações‖ ≤ tol` e `‖ΣM_aplicado + ΣM_reações‖ ≤ tol`, com
`tol = 1e-6 · max(‖ΣF_aplicado‖, ‖ΣF_reações‖) + piso_absoluto`. Um solver adulterado quase sempre falha.

**Dados e cache**: os dados de entrada (cargas + geometria) vão ao servidor **uma vez por modelo**, identificados por `model_hash`; o servidor guarda `ΣF_aplicado`/`ΣM_aplicado` em cache. Nos cliques seguintes vão só `model_hash` + totais de reação. **Uma ida** por *Calcular*; nenhum resultado detalhado sai da máquina.

---

## 7. Keystone `K` e trava de finalização (via `_z8r4q`)

Após (a) licença válida e (b) equilíbrio OK, o servidor devolve:

```
K = Ed25519_sign(priv_servidor,
    SHA256(model_hash ‖ result_digest ‖ nonce ‖ license_id ‖ exp ‖ ts))
```

- `result_digest` = hash dos resultados que o cliente se compromete (deslocamentos + esforços resumidos).
- O cliente **verifica `K`** com a chave pública embutida e confere `result_digest` recomputado.
- Deriva `s = HKDF-SHA256(ikm=K, salt=nonce, info="finalize"||license_id)`.
- `s` entra na **chave de sessão** de `sealPointer`/`unsealPointer`:
  - slots de **cálculo/preview** → selados com a chave em binário (tela mostra números);
  - slots de **resultado oficial / export / impressão / relatório** → só desselam com `s`.
- Sem `K` válido, `unsealPointer` devolve ponteiros errados → export falha/inutilizável.
- Misturar também o `poison` do anti-debug em `s` (ver §9).

---

## 7.1 Escopo: programa inteiro e níveis de proteção

O Structura é um programa completo; o solver é **um** módulo. A proteção cobre tudo:

- **Direito de executar (todo o EXE):** licença + fingerprint (1 máquina), integridade de todos os arquivos + contagem da pasta, anti-debug no EXE.
- **Nível A — com keystone** (a saída só é finalizável com `K` do servidor): **cálculo, geração de desenhos/pranchas, exportação DXF/PDF, dimensionamento/verificações normativas, quantitativos**. No solver a validação é o equilíbrio (§6); nos demais, o gate mínimo é o `K` assinado sobre o `digest` da saída (+ invariante barata quando houver). `s = HKDF(K)` destrava a finalização de cada módulo via `_z8r4q`.
- **Nível B — só gate de licença (sem keystone):** **detalhamento e memorial de cálculo**. Ficam **bloqueados** se a licença não for validada no servidor ou se estiver **offline**; não amarram a saída ao `K`.

Em ambos os níveis, offline = somente-leitura: nada é produzido sem licença válida verificada.

---

## 8. Contratos de API

Todos via HTTPS (TLS 1.3). Corpo em JSON. Respostas sensíveis assinadas (Ed25519). Erros **genéricos** ao cliente; detalhe só no log.

### 8.1 `POST /auth/login`
```json
// request
{ "email": "...", "password": "...", "cpf": "...",
  "fingerprint": "sha256-hex",
  "client_version": "x.y.z",
  "integrity": { "manifest_version": 7, "files_root": "sha256-hex", "file_count": 42, "exe_hash": "sha256-hex" } }
// response 200
{ "license_token": "<assinado, TTL curto>", "expires_at": "ISO8601",
  "server_pubkey_id": "k1", "session_dll_key": "<wrapped, se usar DLL cifrada>" }
// erros: 401 CREDENCIAIS, 403 LICENSE_INACTIVE, 409 DEVICE_IN_USE, 426 CLIENT_OUTDATED, 409 INTEGRITY_FAIL
```
Regra de dispositivo: sem vínculo ativo → vincula este fingerprint; igual → ok; diferente e heartbeat < 7 dias → `409 DEVICE_IN_USE`; diferente e ≥ 7 dias → re-vincula. 2FA: se habilitado, exigir `totp` (segundo passo `POST /auth/2fa`).

### 8.2 `POST /auth/logout`
```json
{ "license_token": "..." }   // libera o vínculo de dispositivo no servidor → 204
```

### 8.3 `POST /license/heartbeat`  (semanal, em segundo plano)
```json
{ "license_token": "...", "fingerprint": "...", "integrity": { ... } }
// 200 { "license_token": "<renovado>", "expires_at": "ISO8601", "status": "ativa" }
// 403 se suspensa/revogada/expirada
```

### 8.4 `POST /compute/validate`  (a cada clique em Calcular)
```json
// request
{ "license_token": "...", "nonce": "random-hex", "fingerprint": "...",
  "integrity": { "files_root": "sha256-hex", "file_count": 42, "exe_hash": "sha256-hex" },
  "model_hash": "sha256-hex",
  "model": { /* enviado só se o servidor não tiver o model_hash em cache */
    "materials": [...], "sections": [...], "nodes": [...], "members": [...],
    "nodal_loads": [...], "member_loads": [...] },
  "reaction_totals": { "F": [Fx,Fy,Fz], "M_origin": [Mx,My,Mz] },
  "result_digest": "sha256-hex" }
// response 200
{ "ok": true, "K": "ed25519-sig-hex", "ts": "ISO8601", "exp": "ISO8601" }
// response 200 (falha de validação)
{ "ok": false, "reason": "EQUILIBRIUM_FAIL" | "INTEGRITY_FAIL" | "LICENSE_INVALID" }
```
Servidor: valida token (assinatura, exp, status, fingerprint) → valida `integrity` contra o manifesto esperado → (cacheia `model` por `model_hash` e pré-calcula o lado aplicado) → checa equilíbrio (§6) → assina `K`. **Fail-closed**: qualquer falha → sem `K`.

### 8.4b `POST /license/check`  (Nível B — detalhamento, memorial)
```json
{ "license_token": "...", "fingerprint": "...", "integrity": { ... }, "feature": "detalhamento|memorial" }
// 200 { "ok": true }
// 403 { "ok": false, "reason": "LICENSE_INVALID" | "OFFLINE" | "INTEGRITY_FAIL" }
```
Valida licença + integridade + presença online, **sem** keystone. O módulo só roda se `ok`. Sem internet → o cliente não obtém `ok` → módulo bloqueado.

### 8.5 `POST /webhook/payment`  (Mercado Pago / Stripe)
Verifica assinatura do provedor → se `paid`: estende `current_period_end` +1 mês e marca `ativa`. Idempotente por `provider_ref`.

### 8.6 Admin (atrás de auth + 2FA): 
`POST /admin/licenses` (criar), `PATCH /admin/licenses/{id}` (suspender/revogar/editar expiração),
`GET /admin/licenses/{id}/devices`, `DELETE /admin/devices/{id}` (liberar vínculo),
`GET /admin/payments`, `GET /admin/abuse` (fila de flags), `POST /admin/vm-allowlist` (liberar cliente legítimo em VM).

---

## 9. Cliente C++ — responsabilidades

- **Fingerprint**: SHA-256 de sinais compostos (CPU, placa, disco, MAC, id de instalação do SO). Tolerância à troca de 1 peça.
- **Integridade**: no start e periodicamente, recomputar SHA-256 de cada arquivo + **contagem por pasta** e comparar com o manifesto assinado; **rejeitar arquivo a mais, a menos ou alterado**. Enviar atestação ao servidor.
- **Anti-debug** (estender o `antiDebugPoison` existente): aplicar também ao **EXE**; chamar em vários pontos (start, antes de cada Calcular, no caminho de export); misturar `poison` na chave `s`. Compilar com `-fvisibility=hidden`, strip de símbolos.
- **Ofuscação**: Obfuscator-LLVM (control-flow flattening, bogus flow, substituição de instruções); cifra de strings/constantes em compile-time.
- **DLL cifrada** (se aplicável): AES-256-GCM em repouso; chave de sessão do login, decifrar só em memória.
- **Equilíbrio**: somar `outNodeReactions6` → `reaction_totals`; montar `model_hash` e `result_digest`.
- **Rede**: fail-closed; retry com backoff; nenhuma rota libera cálculo/export ao falhar a verificação.
- **Gate por módulo**: Nível A chama `/compute/validate` e exige `K` para finalizar a saída; Nível B (detalhamento, memorial) chama `/license/check` e só roda com `ok` (licença válida + online).
- **Offline**: abre em **somente-leitura** (visualiza salvos); não calcula, não exporta com `K`, e **não roda detalhamento nem memorial**.

---

## 10. Modelo de dados (servidor)

```
users(id, email UNIQUE, password_hash, cpf_enc, cpf_hmac, totp_secret_enc, status, created_at)
licenses(id, user_id FK, status, current_period_end, plan, created_at)
devices(id, license_id FK, fingerprint_hash, active BOOL, bound_at, last_heartbeat_at, released_at)
payments(id, license_id FK, provider, provider_ref UNIQUE, amount, status, paid_at)
audit_log(id, actor, action, target, metadata_masked, ts)         -- imutável
abuse_flags(id, license_id FK, kind, score, details, status, ts)
integrity_manifest(version, signed_blob)                           -- hashes + contagem, assinado
model_cache(model_hash PK, applied_F, applied_M, cached_at)        -- pode ser Redis/efêmero
```
CPF: `cpf_enc` (AES-256-GCM) + `cpf_hmac` (busca). Nunca em log/URL/erro. Mascarar no admin (`***.***.***-12`).

---

## 11. Detecção de abuso

- **1 fingerprint ativo** por licença; segundo login simultâneo → recusa.
- **Uso concorrente** (2 IPs/fingerprints ao mesmo tempo) → bloqueia + reautenticação.
- **Viagem impossível / velocity** (geo-IP incompatível no tempo; muitos fingerprints em pouco tempo) → flag.
- **Detecção de VM/sandbox** → **bloqueia** o cálculo; com **allowlist do admin** por licença/fingerprint para clientes legítimos em VM.
- **Ações graduais**: reautenticar/2FA → limitar → suspender → revogar. Tudo em auditoria.
- **Kill switch** remoto (revogação com efeito na próxima verificação; tokens curtos propagam rápido).
- **Marca d'água forense**: id da licença discreto nos resultados/exports.

---

## 12. Servidor à prova de leaks

TLS em tudo; segredos em secret manager; Argon2id para senha; AES-256-GCM em repouso para PII; prepared statements (anti-SQLi); validação de entrada; rate limiting por IP/licença/conta; respostas de licença/compute assinadas; audit log sem PII em claro; erros genéricos; dependências fixadas + scanner (pip-audit/npm audit/Trivy/Semgrep) no CI; rotação de chaves.

---

## 13. Stack gratuita

Backend FastAPI/Node/Go; hospedagem Oracle Always Free / Fly / Render / Cloud Run; DB Supabase/Neon/Turso; cripto libsodium/PyNaCl/`cryptography`; ofuscação Obfuscator-LLVM + strip + UPX (leve); pagamento Mercado Pago/Stripe (SDK grátis, só taxa por transação). **Asterisco**: certificado de assinatura de código EV é pago (opcional; a rota grátis é assinatura própria + atestação server-side, ao custo do aviso do SmartScreen).

---

## 14. LGPD

Base legal + minimização; CPF cifrado e mascarado; TLS no transporte; direitos do titular (acesso/correção/exclusão); retenção e descarte definidos. Não é aconselhamento jurídico — validar com advogado.

---

## 15. Critérios de aceite (amostra)

- [ ] Sem licença válida, o cálculo **nunca** ocorre (cliente e servidor).
- [ ] Validação de equilíbrio a cada *Calcular* + heartbeat semanal funcionando.
- [ ] Export/impressão **inutilizáveis** sem `K` válido (via `s`/`_z8r4q`).
- [ ] Adição/remoção/alteração de arquivo na pasta é detectada e bloqueia o uso.
- [ ] Token curto, assinado e verificado; relógio do cliente não decide validade.
- [ ] 1 máquina ativa; logout libera; 7 dias sem heartbeat liberam vínculo preso.
- [ ] Segundo dispositivo simultâneo é detectado e tratado.
- [ ] VM/sandbox bloqueia, com allowlist do admin.
- [ ] Nenhum segredo no repositório; senha Argon2id; CPF cifrado e mascarado.
- [ ] Admin cria/suspende/revoga/edita expiração/vê e libera dispositivos.
- [ ] Falhas de rede são fail-closed.
- [ ] Pagamento confirmado renova a validade via webhook (idempotente).
- [ ] Anti-debug ativo no EXE e nas DLLs, poison misturado em `s`.
- [ ] Pentest executado antes do lançamento.

---

## 16. Decisões travadas

1. Validação = equilíbrio global (ΣF=0), 1 ida + cache por `model_hash`.
2. Finalização travada por `K` via `_z8r4q` (`s = HKDF(K)`).
3. 1 máquina ativa por licença; logout + 7 dias sem heartbeat para liberar.
4. Offline = somente-leitura.
5. 2FA no login do usuário.
6. Anti-debug estendido ao EXE + poison em `s`.
7. Extras (grátis): pinning, marca d'água, kill switch, rotação de chaves, VM-block + allowlist admin.
8. Pentest antes do lançamento.
9. Escopo = **programa inteiro**; licença + integridade + anti-debug cobrem o EXE.
10. **Nível A (keystone):** cálculo, desenhos/pranchas, DXF/PDF, dimensionamento, quantitativos. **Nível B (só gate de licença + online, sem keystone):** detalhamento e memorial de cálculo — bloqueados sem licença válida ou offline.
