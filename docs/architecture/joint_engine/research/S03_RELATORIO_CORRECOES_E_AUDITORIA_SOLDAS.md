# S-03 — Correções normativas e auditoria dos comprimentos de solda

Referência: **NBR 8800:2024** (PDF em `normas/`). Sem validação profissional. Não foram integrados ICR, P2 nem modelos constitutivos; a demanda do `SupportWeld` **não** foi alterada.

## 1. Correções realizadas
| Item | Arquivo | Mudança |
|---|---|---|
| S-03.1 | `src/design/SupportWeld.cpp` | `minimumFilletLeg`: `t <= 6.35` → `t <= 6.3` (Tabela 11); `maximumFilletLeg`: `t < 6.35` → `t < 6.3` (6.2.6.2.2). Comentários citam a norma; `SupportWeld.h` documenta as faixas |
| S-03.2 | `SupportWeld.cpp`, `MomentConnection.cpp`, `TrussNodeConnection.cpp`, `src/tests/golden/numerics.txt` | rótulos de cláusula (ver §3); **só texto** |
| S-03.3 | `SupportWeld.{h,cpp}` | `supportWeldRuns()` + verificação de 6.2.6.2.3 (ver §4) |
| Elegibilidade | `jointengine/ResistanceWrappers.{h,cpp}` | wrapper registra perna geométrica × efetiva; verificações de 6.2.6.2.3 e de perna passam a `ConfrontedWithStandard` na resistência, **mantendo `approvalEligible = false`** |
| Testes | `TestWeldLegLimits.cpp` (reescrito), `TestSupportWeldLengths.cpp` (novo), `TestJointEngineResistanceWrappers.cpp` | ver §6 |

## 2. S-03.1 — limite de perna (6,3 mm)
Aplicação literal: Tabela 11 — até 6,3 → 3 mm; > 6,3 até 12,5 → 5 mm; > 12,5 até 19 → 6 mm; > 19 → 8 mm. 6.2.6.2.2 — perna máxima: `t` se `t < 6,3`; `t − 1,5` se `t ≥ 6,3`. Os `QEXPECT_FAIL` foram **removidos**; os pontos de fronteira (6,29; 6,30; 6,31–6,34; 6,35; 6,36; 12,49/12,5/12,51; 18,99/19/19,01) são testes normais (`QTest` com tabela de dados).
**Consequência para peças de 6,35 mm (1/4 pol):** a perna mínima passa de **3 mm para 5 mm**; a perna máxima na borda passa de 6,35 mm para **4,85 mm**. Os módulos `Connections`, `ShearConnections` e `BeamExtras` já aplicavam 6,3; agora **todos concordam** (teste `otherModulesAgreeWithSupportWeld`).
**Propostas paramétricas do catálogo:** `ConnectionCatalogVariants` chama `minimumFilletLeg`/`maximumFilletLeg`; para partes de 6,35 mm (cantoneiras `L` e `T 51×4,7` do catálogo; chapa de apoio de 1/4 pol informada pelo usuário) a perna proposta mínima passa a ser **5 mm** (antes 3). Nenhum outro módulo foi alterado; `TestConnectionCatalog` e os goldens de propostas **não mudaram** (nenhum caso de teste usa partes em 6,3 < t ≤ 6,35).

## 3. S-03.2 — rótulos normativos e golden
| Local | Antes | Depois |
|---|---|---|
| `SupportWeld` mesas (penetração total) e memória | `6.2.4` / `(6.2.4)` | `Tabela 9 (penetracao total)` / `(Tabela 9)` |
| `SupportWeld` perna máxima (mesas e alma) | `6.2.5.2` | `6.2.6.2.2` |
| `SupportWeld` alma (metal-base) | `6.2.5 e 5.4.3` | `Tabela 9 e 6.5.5` |
| `MomentConnection` solda de penetração total das mesas | `6.2.4` | `Tabela 9 (penetracao total)` |
| `TrussNodeConnection` solda de topo gousset–banzo (normal, cisalhamento) | `6.2.4` | `Tabela 9 (penetracao total)` |
| idem, interação normal × cisalhamento | `6.2.4` | `Tabela 9 (penetracao total); interacao: criterio do programa` |
**Diff antes/depois** do `numerics.txt`: `research/S03_diff_numerics_golden.txt` (52 linhas de diff, **15 linhas alteradas**). **Verificação automática:** cópia anterior (scratchpad) × atual — mesmo número de linhas; nas linhas `chk`, só a coluna da cláusula mudou (nome e campos numéricos idênticos); nas linhas de memória, só `(6.2.4)` → `(Tabela 9)`. **Alterações fora de cláusula/referência: 0.** Nenhum outro golden foi tocado (`momento_rotacao.txt` com SHA-256 idêntico).
Rótulos não alterados por estarem corretos: `6.2.5` (genérico, 6.2.5.1), `6.2.5 e Tabela 9`, `6.5.5-a/-b` (`ShearConnections`), `5.4.3` quando se refere ao cortante de alma de viga (outros módulos).

## 4. S-03.3 — comprimento efetivo mínimo (6.2.6.2.3)
**Redação conferida (NBR 8800:2024, p. 80):** "O comprimento efetivo de uma solda de filete (ver 6.2.2.2), dimensionada para uma solicitação de cálculo qualquer, não pode ser inferior a quatro vezes seu tamanho da perna e a 40 mm ou, então, para efeito de cálculo, a perna da solda não pode ser considerada maior que 25 % de seu comprimento. Quando forem usadas somente soldas de filete longitudinais nas ligações extremas de chapas planas tracionadas, o comprimento de cada filete não pode ser menor que a distância transversal entre eles (ver 5.2.5-d))." 6.2.2.2-c: comprimento efetivo = comprimento total de dimensão uniforme, incluindo retornos.
**Interpretação adotada (decisão metodológica registrada):** `L ≥ 40 mm` é **sempre exigido** (sem alternativa de cálculo); a relação `L ≥ 4a` tem a **alternativa normativa**: se `L < 4a`, a perna considerada no cálculo é `a_ef = L/4` (não há reprovação). A leitura literal "ou, então" poderia dispensar também os 40 mm; adotei a leitura conservadora. **A regra dos filetes exclusivamente longitudinais de ligações extremas de chapas tracionadas não se aplica** à solda viga–apoio (registrado).
**Implementação:** `supportWeldRuns()` devolve os trechos contínuos: mesas (face externa `bf` e duas faces internas `(bf − tw)/2`, só se filete) e alma (dois filetes `d − 2tf`), cada um com `geometricLeg`, `effectiveLeg = min(a, L/4)`, `belowFourLegs`, `belowMinimumLength`. No `checkSupportWeld`:
- se algum trecho tem `L < 4a`, a resistência do metal da solda usa a **perna efetiva** por trecho (soma `Σ n·0,707·a_ef·L`); com todos os trechos conformes mantém-se **exatamente a expressão anterior** (resultados idênticos);
- se `L < 40 mm`: verificação `... - comprimento efetivo minimo (mesas|alma)` (cláusula `6.2.6.2.3`), que **reprova**;
- se a perna efetiva foi acionada: verificação informativa `... - perna efetiva (mesas|alma)` registrando separadamente a **perna geométrica informada** e a **perna efetiva considerada** (por trecho).
- As verificações de perna mínima/máxima continuam usando a perna **geométrica**.
- **Emissão condicional:** nenhuma verificação é acrescentada quando todos os trechos atendem, **para não alterar o `numerics.txt`** (o golden imprime todas as verificações e suas linhas de memória, e você autorizou só alteração textual de cláusulas). A perna geométrica/efetiva é sempre exposta por `supportWeldRuns()` e, no wrapper, em `parameters` (`geometricLeg.*`, `effectiveLeg.*`, `shortestRun.*`). **Decisão que fica com você:** emitir sempre a verificação (exigiria acrescentar linhas ao golden).
**Exemplo numérico** (verificado em teste): `bf = 101`, `tw = 5`, perna 12,5 mm → trecho interno 48 mm < 4×12,5 = 50 → `a_ef = 12,0` nas faces internas; `Σ te·L` = 0,707·(12,5·101 + 2·12·48) = 1707,0 mm² contra 1740,9 mm² sem a alternativa (**−1,95 % na resistência do metal da solda**).

## 5. S-03.4 — auditoria dos demais módulos (nada implementado)
| Módulo | Comprimento efetivo usado | Perna usada | Verificação existente | 6.2.6.2.3 | Dados | Divergência | Recomendação |
|---|---|---|---|---|---|---|---|
| `Connections.cpp` (`checkFilletWeld`) | `length` = comprimento **total** do grupo (informado pelo chamador) | `leg` | `Comprimento minimo efetivo` = `L ≥ max(4a; 40)` (hard); perna mín. (limite 6,3) e máx. | **parcial**: só a primeira alternativa (sem `a_ef = L/4`); compara o comprimento **agregado**, não por trecho | `length`, `leg` presentes | sem alternativa de perna efetiva; agregado mascara trechos curtos | implementar `a_ef` e aceitar trechos; chamadores passarem trechos |
| `EndPlateConnection.cpp` | mesas `2bf − tw`; alma `2(d − 2tf − 2a)` | `weldFlangeLeg`, `weldWebLeg` | chama `checkFilletWeld` mas **só repassa** "Metal da solda" e "Perna minima" (`appendChecks` filtrado) | **não aplicada** (verificação de comprimento e perna máxima descartadas) | dados suficientes | verificações de `checkFilletWeld` descartadas | repassar "Comprimento" e "Perna maxima" |
| `MomentConnection.cpp` | mesas e alma por `WeldInput` | `flangeWeldLeg`, alma | repassa **todas** as verificações de `checkFilletWeld` | **sim, como `checkFilletWeld`** (agregado, 1ª alternativa) | suficientes | idem `Connections` | idem |
| `FlexEndPlate.cpp` | `w.length` do grupo | `leg` | repassa todas | sim (agregado) | suficientes | idem | idem |
| `BasePlate.cpp` | mesas: `w.length`; alma: `2(d − 2tf)` | `leg` | repassa "Metal", "Perna" e "Comprimento" — **comprimento só para o grupo das mesas** (`w`), não para `wv` (alma) | **parcial** | suficientes | alma sem verificação de comprimento | chamar "Comprimento" também para a alma |
| `TrussConnections.cpp` (gusset, solda chapa–apoio) | `gussetWidth` | `weldLeg` | `checkFilletWeld` (todas) | sim (agregado) | suficientes | idem | idem |
| `ShearConnections.cpp` | dois filetes de altura `H` (chapa) | `weldLeg` | método elástico, metal-base, perna mín./máx. (limite 6,3 ✓) | **não aplicada** | `H`, `weldLeg` presentes | sem verificação de 6.2.6.2.3 | acrescentar (`H ≥ max(4a; 40)` ou `a_ef`) |
| `CoverSplice.cpp` | `π·d` (filete contínuo em volta do tubo) | `weldLeg` | solda tubo–chapa; perna mín. | **não aplicada** (satisfeita na prática: perímetro ≫ 40 mm) | `tubeDiameter`, `weldLeg` | **tabela de perna mínima incompleta**: usa `> 12,5 ? 6 : 5`, ignorando 3 mm (≤ 6,3) e **8 mm (> 19 mm)** — para partes > 19 mm exige 6 mm onde a NBR exige 8 mm (não conservador) | corrigir para a Tabela 11 completa; opcionalmente 6.2.6.2.3 |
| `TrussNodeConnection.cpp` | barra–chapa: `lengthHeel`, `lengthToe`; barra–banzo direto: `lengthHeel + lengthToe`; gousset–banzo: `L` | `weldLeg`, `chordWeldLeg` | `Comprimento minimo do filete (>= 4 a e >= 40 mm)` (cláusula `6.2.6.2`) sobre `min(heel, toe)` **só no modo chapa soldada**; perna mín. do gousset–banzo com **Tabela 11 completa (6,3 ✓)** | **parcial**: 4a tratado como exigência rígida (sem `a_ef`); **ausente** no modo "barra direto ao banzo" e no filete gousset–banzo | comprimentos presentes | rótulo `6.2.6.2` (deveria ser `6.2.6.2.3`); sem alternativa; cobertura incompleta | rotular `6.2.6.2.3`, aplicar a todos os modos, acrescentar `a_ef` |
| `BeamExtras.cpp` | solda alma–mesa contínua ao longo da viga; lamela com comprimento `ext`; extremidade `a_l` (5.4.4.2) | `webWeldLeg`, `coverWeldLeg` | perna mín. com Tabela 11 completa (6,3 ✓); lamelas: critérios de 5.4.4.2 | **não aplicada**; solda contínua longa; trechos de lamela e extremidade podem ser curtos | `ext`, `a_l` presentes | sem 6.2.6.2.3 nas lamelas | aplicar aos trechos de lamela/extremidade; avaliar também 6.2.2.2-d (fator β de soldas longitudinais) |
| `PinConnection.cpp`, `StiffenerDesign.cpp`, `ColdFormedConnections.cpp` | não auditados em detalhe | — | — | **não auditado** | — | — | auditar em fase própria |
**Observação transversal:** `6.2.2.2-d` (fator de redução β = 1,2 − 0,002·ℓ/d ≤ 1,0, ≥ 0,6, e ℓ ≤ 180·d_w quando ℓ > 300·d_w, para filetes longitudinais sem distribuição uniforme de tensão — sobreposições, enrijecedores de apoio) **não foi encontrado** em nenhuma das verificações auditadas; sua aplicabilidade ao `TrussNodeConnection` (barra–chapa, filetes longitudinais) e a enrijecedores é **questão metodológica em aberto**.
**Não implementado em nenhum desses módulos (conforme S-03.4).**

## 6. S-03.5 — demanda do `SupportWeld` (documentação; formulação inalterada)
`F_mesa = |M|/(d − tf) + |N|/2`; `V` uniforme nos dois filetes da alma (`V/(2·(d − 2tf))`); comprimentos plenos das faces. Hipóteses e limitações (vide S-02 §5): (1) binário das mesas sem contribuição da alma ao momento; (2) `N` dividido igualmente, em módulo, entre as mesas e sem parcela na alma; (3) `V` só nos filetes da alma, tensão uniforme; (4) **sem interação** entre `V`, `M` e `N` nos filetes (alma só sob `V`; mesas só sob força normal com θ = 90°); (5) **sem excentricidade** (solda direta na face do apoio); (6) transferência na alma: sem parcela de `M`/`N`, sem variação ao longo da altura; (7) **compatibilidade do metal da solda** (Tabelas 8 e 9) e **passes** (nota a da Tabela 11) **não verificados**; (8) comprimentos de perna plena em toda a face (sem retornos, início/término, raio de concordância). Nenhuma demanda foi alterada; o ICR (já implementado em `Connections.h`) **não** foi integrado.

## 7. Elegibilidade (mantida)
Todas as verificações do wrapper de solda têm a **resistência/limite confrontado** com a NBR 8800:2024, mas **`approvalEligible = false`** porque a demanda é idealizada pelo legado (ou, nas verificações de perna e de comprimento, os comprimentos assumem perna plena e a nota a da Tabela 11 não é verificável) e as condições de aplicação (compatibilidade do metal da solda, passes, interação) não estão verificadas. Gusset (Whitmore: método complementar AISC) e tubular (CIDECT: complementar) continuam fora da conformidade NBR.

## 8. Testes
- `ctest` completo: **42/42** (35 originais + 4 J0–J3 + wrappers + `TestWeldLegLimits` + `TestSupportWeldLengths` = 42; estavam 41 antes desta fase).
- `TestWeldLegLimits`: tabelas de dados (21 espessuras para a perna mínima, 12 para a máxima), fronteiras 6,29/6,30/6,31/6,35/6,36/12,5/19, consequência para 6,35 mm, concordância entre módulos — todos passam como testes normais.
- `TestSupportWeldLengths`: trechos com perna geométrica × efetiva, comprimento suficiente (nada acrescentado), `L = 4a` exato (sem redução), `L` logo abaixo de `4a` (perna efetiva, informativa), redução quantificada da resistência, fronteira de 40 mm (39,99 / 40 / 40,01 / 20), reprovação sem alternativa, alma, perna mínima/máxima sobre a perna geométrica. A regra dos filetes longitudinais de chapas tracionadas não se aplica ao modelo: nenhum teste.
- `TestJointEngineResistanceWrappers`: equivalência bit a bit com o legado (inclusive com perna efetiva) e novo teste do registro de perna geométrica × efetiva.
- Golden: apenas textual (diff acima); `TestGoldenNumerics` passa.

## 9. Pendências
1. Decidir se a verificação de 6.2.6.2.3 deve ser **sempre emitida** (altera o golden) ou só em desvio (como está).
2. Confirmar a interpretação dos 40 mm (hard) e a aplicação **por trecho** (e não pelo comprimento agregado, como em `checkFilletWeld`).
3. Autorizar correções nos demais módulos (§5), em especial a tabela de perna mínima do `CoverSplice` (não conservadora acima de 19 mm) e as verificações descartadas em `EndPlateConnection`/`BasePlate`.
4. Questão metodológica 6.2.2.2-d (fator β) para filetes longitudinais.
5. Demanda do `SupportWeld` (S-03.5) e uso do ICR: fase própria.

## 10. Estado do Git
Branch `implantacao_dll`; sem commit. Arquivos alterados nesta fase além dos de testes/CMake: `SupportWeld.{h,cpp}`, `MomentConnection.cpp`, `TrussNodeConnection.cpp` (rótulos), `ResistanceWrappers.{h,cpp}`, `src/tests/golden/numerics.txt` (15 linhas, só cláusulas), `TestWeldLegLimits.cpp`, `TestSupportWeldLengths.cpp` (novo), `TestJointEngineResistanceWrappers.cpp`, `src/tests/CMakeLists.txt`. **Intactos (SHA-256):** `TStub*`, `JointComponents.cpp`, `JointClassification.*`, `momento_rotacao.txt`; sem mudanças em C7, persistência, UI, `normas/`.
