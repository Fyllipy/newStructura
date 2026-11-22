# PLAN — Implementação do Aparato de Segurança e Licenciamento (Structura)

Base: `SPEC.md`. Método spec-driven: SPEC e PLAN **já aprovados** → implementar marco a marco, com testes e revisão adversarial ao fim de cada um.

Convenção: cada tarefa tem critério de aceite (CA). Não avançar de marco com CA em aberto.

---

## M0 — Setup e infraestrutura
- [x] Repositório do **servidor** separado do solver; estrutura `server/app`, testes, CI.
- [x] `gen_keys.py`: gerar chave de assinatura Ed25519 do servidor + chaves AES/HMAC do CPF.
- [x] Segredos via `.env` / secret manager; nunca no repositório.
- [x] Scanner no CI (pip-audit / Trivy / Semgrep).
- CA: servidor sobe local; `GET /pubkey` responde; segredos fora do git.

## M1 — Auth e núcleo de licenciamento
- [x] Modelo de dados (users, licenses, devices, payments, audit_log, abuse_flags).
- [x] Senha Argon2id; CPF cifrado (AES-256-GCM) + HMAC para busca.
- [x] `POST /auth/login` (valida senha+CPF, estado/validade da licença, bind de dispositivo, emite token curto assinado).
- [x] `POST /auth/logout` (libera vínculo). `POST /license/heartbeat` (revalida/renova).
- [x] 2FA (TOTP) no login do usuário.
- CA: login/logout/heartbeat funcionam; token expira; 1 máquina ativa; CPF nunca em claro no log.

## M2 — Integridade e fingerprint
- [x] Gerador de **manifesto assinado** (hash SHA-256 por arquivo + contagem por pasta).
- [x] Cliente: verificação na inicialização e periódica; atestação enviada ao servidor.
- [x] Servidor valida a atestação; divergência → nega.
- CA: adicionar/remover/alterar arquivo na pasta é detectado e bloqueia o uso.

## M3 — Validação de cálculo (equilíbrio) + keystone K
- [x] `equilibrium.py`: recompor ΣF/ΣM aplicados a partir das entradas (cargas nodais + de barra).
- [x] `POST /compute/validate`: valida token+integridade, checa equilíbrio, assina `K`. Fail-closed.
- [x] Cache do lado aplicado por `model_hash`.
- [x] Cliente: soma reações, monta `result_digest`, verifica `K` (pub embutida), deriva `s=HKDF(K)`.
- CA: sem licença/with equilíbrio quebrado → sem `K`; export inutilizável sem `K`.

## M4 — Integração do cliente (C++)
- [x] `license_client` no solver: login/logout/heartbeat, fingerprint, chamada de validação por Calcular.
- [x] Amarrar `s` à chave de sessão de `sealPointer`/`unsealPointer`; separar slots preview vs export.
- [x] Offline = somente-leitura.
- CA: cálculo só com licença; export travado por `K`; offline abre e visualiza, não calcula.

## M5 — Anti-tamper / anti-debug / ofuscação
- [x] Estender `antiDebugPoison` ao EXE; chamadas em vários pontos; `poison` misturado em `s`.
- [x] Build: Obfuscator-LLVM, strip, `-fvisibility=hidden`; cifra de strings/constantes.
- [x] DLLs cifradas em repouso (se aplicável), chave de sessão em memória.
- CA: debugger/breakpoint/injeção degradam silenciosamente o resultado; binários sem símbolos legíveis.

## M6 — Detecção de abuso
- [x] Uso concorrente (<30s), viagem impossível/velocity (<10s saltos de sub-rede), excesso de fingerprints (>3 em 24h) com resposta graduada e auto-suspensão (3+ flags em 7 dias).
- [x] Detecção de VM/hypervisor (CPUID bit 31, vendors, drivers) inserida na cadeia de poison do executável (não booleano confiado), com allowlist e token HMAC do admin.
- [x] Kill switch cobrindo o export (`keystone.exp` estrito nos 4 métodos de finalização) + marca d'água forense assinada pelo servidor (HMAC com domínio próprio) estampada em DXF, PDF, Memorial e Quantitativos.
- CA: segundo dispositivo simultâneo tratado; VM bloqueia com exceção por allowlist; exports com kill switch temporal e rastreabilidade pericial.

## M7 — Painel admin
- [x] Criar/suspender/revogar/editar expiração; ver e liberar dispositivos; fila de flags; allowlist VM.
- [x] Auth forte + **2FA obrigatório**; audit log de ações.
- CA: todas as ações administrativas funcionam e ficam auditadas.

## M8 — Pagamentos
- [ ] `POST /webhook/payment` (Mercado Pago/Stripe), verificação de assinatura, idempotente.
- [ ] Pagamento confirmado → estende validade +1 mês e marca `ativa`.
- CA: webhook renova a licença; reenvio do mesmo evento não duplica.

## M9 — Hardening, LGPD, pentest, deploy
- [ ] Rate limiting, erros genéricos, rotação de chaves, certificate pinning.
- [ ] LGPD: minimização, direitos do titular, retenção/descarte, mascaramento de CPF.
- [ ] Deploy no tier gratuito (Oracle Always Free / Fly / Render) + DB (Supabase/Neon/Turso).
- [ ] **Pentest** antes do lançamento; correção dos achados.
- CA: checklist de aceite da SPEC §15 todo verde; pentest sem achados críticos abertos.

---

### Revisão adversarial (ao fim de cada marco)
Assuma o papel de atacante e descreva como burlaria o que acabou de construir (remover checagem local, falsificar atestação, reusar token, compartilhar licença, debugar o fluxo de `K`). Corrija antes de seguir.
