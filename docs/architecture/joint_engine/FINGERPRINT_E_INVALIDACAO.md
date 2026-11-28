# Fingerprint e invalidação em três níveis (especificação)

Estende `JointComponentService::fingerprint` (hoje: tipo, extremidade, parafuso, todos os `values` e E — um nível só). Proposta; nada implementado.

## 1. Níveis
| Nível | Identifica | Entra no hash | **Não** entra |
|---|---|---|---|
| **CFP — configuração física** | a ligação como objeto | topologia (`JointTopology`), partes e seus ids, geometria, materiais (propriedades usadas), tipos de união (`LinkKind`), parâmetros constitutivos **declarados na configuração** (ex.: rigidez fornecida de uma solda), condições de contato, condições de contorno | esforços, modelos, normas |
| **AFP — análise mecânica** | uma execução | `CFP` + esforços/`ActionSet` + `(modelId, modelVersion)` de cada componente usado + estratégia de solução e seus parâmetros (tolerâncias, passos) + hipóteses ativas + origem (`Origin`) dos parâmetros | norma/edição de classificação |
| **NFP — classificação normativa** | uma classificação | `AFP` **(só a parte consumida: a resposta, não o caminho)** → na prática o *hash do `MechanicalResponse`* + norma + edição + critério + parâmetros estruturais usados (Kb, Kc, contraventamento, etc.) | demais parâmetros da análise |

Hash: SHA-256 de uma serialização canônica (campos ordenados, números em formato fixo de 17 dígitos significativos, strings UTF-8), truncado para exibição; o texto canônico é guardado em modo de depuração para diagnosticar divergência. Determinístico entre execuções.

## 2. Regras de invalidação
1. Mudou **CFP** ⇒ AFP e NFP obsoletos.
2. Mudou algo só de **AFP** (esforços, versão de modelo, tolerância de solução, hipótese) ⇒ NFP obsoleto (porque depende da resposta); CFP permanece.
3. Mudou só **norma/edição/critério/parâmetros estruturais** da classificação ⇒ **somente NFP** obsoleto; **a análise mecânica permanece válida** (não é reexecutada). Isso é garantido por NFP depender do hash da *resposta* e não dos parâmetros de análise.
4. Resultado obsoleto nunca é exibido como atual: a UI/relatório recebem `Stale{level}`.
5. Versão de modelo muda (ex.: C7.1 → C7.2) ⇒ AFP muda; CFP não.
6. Dados **ausentes** entram no hash como ausentes (marcador), não como zero: preencher o dado muda o hash.

## 3. Compatibilidade
O fingerprint legado continua existindo para `JointComponentService` (não alterado). O CFP é calculado por um adaptador a partir de `NodeConnection` + `JointTopology` (derivada ou persistida) e **pode incluir** o texto do fingerprint legado como componente, evitando divergência. Testes de contrato (futuros): (a) alterar um único `values` altera CFP/AFP/NFP; (b) alterar norma de classificação altera só NFP; (c) alterar esforço altera AFP e NFP; (d) permutar a ordem de partes/links com mesmo conteúdo **não** altera o hash (canonicalização); (e) preencher dado ausente altera o hash.
