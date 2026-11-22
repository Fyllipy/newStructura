# Integração do cliente (C++) — notas

Estes arquivos entram no **repo do solver** (Structuradll), não no repo do servidor.

## Onde conectar
- **A cada clique em Calcular:** depois de `structura_solve*`, some `outNodeReactions6` → `ReactionTotals`
  (ΣF e ΣM em torno da origem), calcule `result_digest` (hash dos resultados) e `model_hash`, e chame
  `ValidateCompute(...)`. Só conclua/mostre o entregável (export/impressão) se retornar `true` e o `K` conferir.
- **Equilíbrio:** o cliente só envia os **totais de reação**; o servidor recomputa o lado aplicado a partir
  das cargas+geometria (enviadas uma vez por modelo, em cache por `model_hash`).

## Keystone → export (via `_z8r4q`)
- `s = HKDF(K, nonce, license_id)` entra na chave de sessão de `sealPointer`/`unsealPointer`.
- Separe os slots do gatekeeper: **cálculo/preview** com a chave em binário; **export/impressão/relatório**
  só desselam com `s`. Misture também o `poison` do `antiDebugPoison` em `s`.

## Anti-debug
- Estenda o `antiDebugPoison` (de `solver_security.h`) ao **EXE**, chame em vários pontos (start, antes de
  Calcular, no export) e misture o resultado em `s`.

## Offline
- Sem internet: abre em **somente-leitura** (visualiza salvos). Não calcula, não exporta com `K`.

## Fail-closed
- Qualquer falha (rede, licença, integridade, equilíbrio) → sem `K` → sem export. Nunca "fail-open".

## Segurança de build
- Chave **pública** do servidor embutida (nunca a privada). HTTPS com certificate pinning.
- Compilar com Obfuscator-LLVM, strip de símbolos, `-fvisibility=hidden`; cifrar strings/constantes.
