# S-02 — Relatório de saneamento normativo F-SOL e F-GUS

Referência normativa: **NBR 8800:2024** (PDF fornecido, `normas/`). Sem validação profissional. **Nenhum código legado foi modificado** nesta entrega: S-02.1, S-02.2 e S-02.4 exigem decisão (ver §7) e S-02.3 exige autorização adicional, conforme a sua instrução. Foram alterados apenas o wrapper de elegibilidade (código novo do motor) e testes.

## 1. Fontes e itens examinados
NBR 8800:2024: Tabela 3 (γ), Tabela 9, 6.2.5.1, 6.2.5.2(a)(b)(c), 6.2.6.1–6.2.6.2.5 (Tabelas 10 e 11), 6.5.5, 6.5.6, 5.4.3 (título "Força cortante resistente de cálculo"). Código: `SupportWeld.cpp`, `Connections.cpp` (`checkFilletWeld`, ICR), `ShearConnections.cpp`, `BeamExtras.cpp`, `CoverSplice.cpp`, `MomentConnection.cpp`, `TrussNodeConnection.cpp`, `TrussConnections.cpp`, `BasePlate.cpp`, `ConnectionCatalogVariants.cpp`, `data/catalog_steel.json`, `src/tests/golden/numerics.txt`, histórico Git.

## 2. S-02.1 — limite de perna: 6,35 mm (código) × 6,3 mm (NBR)
**Norma (lida na imagem e no texto):** Tabela 11 — "Abaixo de 6,3 e até 6,3: 3 mm; acima de 6,3 até 12,5: 5; acima de 12,5 até 19: 6; acima de 19: 8". 6.2.6.2.2: perna máxima ao longo de bordas: `t` se `t < 6,3 mm`; `t − 1,5 mm` se `t ≥ 6,3 mm`. Unidade mm; precisão 1 casa decimal (os limites 12,5, 19 coincidem com o código).
**Código:** `SupportWeld.cpp` usa `t <= 6.35` (mín.) e `t < 6.35` (máx.). **Os demais módulos já usam 6,3** (`Connections.cpp:429–435`, `ShearConnections.cpp:236–239`, `BeamExtras.cpp:131`, também a perna máxima em `Connections.cpp:435`): há **inconsistência interna** para a mesma tabela.
**Origem do 6,35:** não documentada no código (nenhum comentário); o arquivo entrou no commit `1348ead` (31/10/2026, "Adicionada verificação de incêndio", commit amplo). **Hipótese (não provada):** 1/4 pol = 6,35 mm, convenção AISC/ASTM; o valor 6,35 aparece em todo o repositório como espessura nominal de cantoneiras e chapas de 1/4 pol.
**Impacto (analítico, verificado por teste):** só difere para espessura da parte mais fina `t` no intervalo:
- perna **mínima**, `6,3 < t ≤ 6,35`: legado exige **3 mm**, NBR exige **5 mm** (o legado é **menos conservador**);
- perna **máxima**, `6,3 ≤ t < 6,35`: legado permite até `t` (≈ 6,3), NBR limita a `t − 1,5` (≈ 4,8) (menos conservador).
Em `data/catalog_steel.json` (820 campos de espessura): `L` laminadas e `T 51×4,7` têm exatamente **6,35 mm** (10 perfis); nenhuma W/I/U está em (6,3; 6,35]; `CHS 141,3×6,3` tem 6,3 (coincide). Como `SupportWeld` usa `min(tf|tw, supportThickness)`, o efeito prático ocorre com **chapa/apoio de 6,35 mm (1/4 pol)** informado pelo usuário e nas **propostas de perna** de `ConnectionCatalogVariants` (usa `minimumFilletLeg`/`maximumFilletLeg`), que hoje podem propor 3 mm onde a NBR exige 5 mm.
**Goldens:** os casos de `SupportWeld` em `numerics.txt` (linhas ~2000–2016 e ~2758) têm `tw = 5,5 mm` (< 6,3): **a correção não altera nenhum número do golden**.
**Testes de fronteira criados** (`TestWeldLegLimits.cpp`): 6,29; 6,30; 6,31–6,34; 6,35; 6,36; 12,5; 12,51; 19; 19,01. Os 9 pontos divergentes estão como `QEXPECT_FAIL` (divergência conhecida: passam a XPASS quando o legado for corrigido, forçando a remoção da marca).
**Proposta de correção (aguardando aprovação):** em `SupportWeld.cpp`, `t <= 6.35` → `t <= 6.3` e `t < 6.35` → `t < 6.3`, literalmente conforme a Tabela 11/6.2.6.2.2; adicionar comentário com a referência. Consequência a comunicar ao usuário: **chapas/cantoneiras de 1/4 pol (6,35 mm) passam a exigir filete mínimo de 5 mm no `SupportWeld`**, em linha com o resto do programa e com a leitura literal da norma. Alternativa (não recomendada): manter 6,35 e documentá-lo como exceção — contradiz a norma.

## 3. S-02.2 — referências de cláusula
| Local | Rótulo atual | Rótulo conforme NBR 8800:2024 | Observação |
|---|---|---|---|
| `SupportWeld.cpp:62` (penetração total, mesas) | `6.2.4` | `Tabela 9 (penetração total, tração normal)`, 6.2.5.1 | 6.2.4 trata dos requisitos do metal da solda |
| `SupportWeld.cpp:73` | `6.2.5 e Tabela 9` | correto; pode citar `6.2.5.2(a)` | — |
| `SupportWeld.cpp:84,108` (perna máxima) | `6.2.5.2` | `6.2.6.2.2` | 6.2.5.2 é o procedimento alternativo de resistência |
| `SupportWeld.cpp:100` (metal-base da alma) | `6.2.5 e 5.4.3` | `Tabela 9 / 6.5.5` | a Tabela 9 manda o metal-base atender a 6.5; 5.4.3 é cortante de vigas |
| `SupportWeld.cpp:95,118` | `6.2.5` | aceitável (6.2.5.1) | pode ser refinado |
| `MomentConnection.cpp:35` (`6.2.4`) e `TrussNodeConnection.cpp:170` (`6.2.4`) | `6.2.4` | `Tabela 9 (penetração total)` | mesmo caso |
| `ShearConnections.cpp:179,182,376,379` (`6.5.5-a/-b`) | correto | — | confere com 6.5.5 a) e b) |
| `Connections.cpp` (`6.2.5.2-c`, `6.2.6.2.1 / Tab. 11`, `6.2.6.2.2`, `6.2.6.2.3`) | correto | — | confere |
**Dependência de golden (decisão necessária):** os rótulos aparecem no texto de `src/tests/golden/numerics.txt` (coluna da cláusula, ex.: `... | 6.2.4 | ...`, linhas ~2000, 2003, 2006, 2008, 2010, 2758–2764 e as de `MomentConnection`/`TrussNodeConnection`). **Corrigir os rótulos altera o golden** (só o texto da cláusula; valores numéricos idênticos). Não alterei o código nem o golden. **Proposta:** autorizar a regravação **restrita à coluna da cláusula**, verificável por `diff` que mostre apenas essas células e zero diferença numérica.

## 4. S-02.3 — comprimento mínimo efetivo (6.2.6.2.3)
**Exigência (NBR):** o comprimento efetivo de um filete, para uma solicitação de cálculo qualquer, não pode ser inferior a **4 vezes a perna e a 40 mm**; **ou**, para efeito de cálculo, a perna **não pode ser considerada maior que 25 % do comprimento**. Para filetes **só longitudinais** em ligações extremas de chapas tracionadas, cada filete não pode ser menor que a distância transversal entre eles (ver 5.2.5-d).
**Implementação atual:** `checkFilletWeld` (`Connections.cpp:440–442`) verifica `L ≥ max(4a; 40)` — **apenas a primeira alternativa** (não a redução da perna a 25 % de L, nem a regra dos filetes longitudinais de 5.2.5-d). É chamado por `BasePlate`, `EndPlateConnection`, `FlexEndPlate`, `MomentConnection` e `TrussConnections` (solda chapa-apoio).
**Verificações que NÃO a aplicam:** `SupportWeld` (mesas e alma), `ShearConnections` (método elástico), `CoverSplice`, `TrussNodeConnection`. **Dados:** em `SupportWeld` os comprimentos são derivados da seção (`2·bf − tw` e `d − 2·tf`) e a perna é dado de entrada ⇒ **dados suficientes**; nos demais, os comprimentos são entradas ou derivados — **a confirmar caso a caso** (não auditado em detalhe nesta fase). **Lacuna documentada; não implementada** (aguarda autorização). Proposta: reutilizar a verificação de `checkFilletWeld` em `SupportWeld`.

## 5. S-02.4 — demanda idealizada nas soldas do `SupportWeld` (auditoria; nada alterado)
**Modelo do legado:** `F_mesa = |M|/(d − tf) + |N|/2` (binário das mesas, `N` dividido igualmente); cortante `V` pelos dois filetes da alma de altura `d − 2tf`, tensão uniforme `V/(2·h_w)`; comprimento dos filetes das mesas `2·bf − tw`.
| # | Hipótese do legado | Avaliação | Validade da **demanda** (separada da resistência) |
|---|---|---|---|
| 1 | Momento resistido só pelo binário das mesas, braço `d − tf` | ignora a contribuição da alma ao momento (conservador para as mesas); braço entre eixos das mesas, correto para seção I | válida como idealização conservadora; **não é da NBR** (a norma não prescreve a distribuição) |
| 2 | `N` dividido igualmente entre as mesas, soma `|N|` em ambas | exige `N` atuando no centróide; com `N` somado em módulo a ambas as mesas é conservador para a mesa comprimida; ignora a parcela de `N` na alma | válida/conservadora; sinal de `N` descartado |
| 3 | `V` só na alma, tensão uniforme | distribuição uniforme de cisalhamento na alma é aproximação usual (a alma toma o grosso do cortante) | válida como idealização; sem interação com o momento |
| 4 | Filetes da alma só sob `V` | ignora que, perto das mesas, os filetes da alma também transferem parcela de `M`/`N` | **lacuna** (não conservadora localmente) |
| 5 | Filetes das mesas: só força normal | ignora a componente de cisalhamento transversal nas mesas e a interação (`6.2.5.2`: resultante do grupo) | **lacuna**: o ângulo `θ = 90°` assume resultante normal ao eixo |
| 6 | Comprimentos cheios `2·bf − tw` e `d − 2·tf` | assume filetes de perna plena em todas as faces, sem reduzir início/término ou raio de concordância | idealização; depende de execução |
| 7 | Excentricidade | **ignorada** (trecho de viga soldado direto na face do apoio; sem excentricidade de cortante em relação ao grupo) | válida só se não houver excentricidade; **não verificada** pelo legado |
| 8 | Compatibilidade com a geometria real | `bf`, `tf`, `tw`, `d` da viga; apoio por `supportThickness` (só verifica cisalhamento no apoio, não flexão local da mesa/alma do pilar) | **lacuna** (verificações locais do apoio estão em `SupportChecks`, não aqui) |
| 9 | Penetração total das mesas | resistência = metal-base da mesa (Tabela 9); **não verifica a compatibilidade do metal da solda (Tabela 8)** | **condição de aplicação não verificada** |
**Resistência vs. demanda:** a **resistência** de cada elemento (Tabela 9, 6.2.5.2(a), 6.5.5, Tabela 11) foi conferida com a NBR; a **demanda** é idealização de engenharia do legado, não conferida com a norma. A NBR 8800:2024 admite, para grupos excêntricos no plano, o **ICR** (6.2.5.2(b)) — que **já está implementado em `Connections.cpp`** (`IcrInput`, `WeldSegment`); uma alternativa de demanda mais rigorosa seria usá-lo. **Correção do P1:** ao contrário do que o relatório P1 afirma, o ICR **existe** no legado (módulo genérico `checkFilletWeld`/`IcrInput`), apenas **não é usado** por `SupportWeld`.
**Propostas (não implementadas):** (a) documentar as hipóteses 1–9 no memorial; (b) adicionar interação cisalhamento–normal nas mesas e parcela de `M`/`N` nos filetes da alma; (c) opcionalmente usar o ICR do módulo genérico; (d) verificar compatibilidade do metal da solda (Tabela 8).

## 6. Lógica de proveniência e elegibilidade (revisada — código novo do motor)
`WrappedCheck` passa a ter quatro eixos independentes: **resistência** (`provenance`), **demanda** (`DemandOrigin`: dado de entrada / idealizada pelo legado / derivada e confrontada), **condições** (`conditionsVerified`) e **base normativa** (`StandardBasis`: NBR 8800:2024 / complementar AISC / complementar CIDECT / EN independente / não classificada). `confrontedWithOwnSource` = resistência confrontada **e** demanda não idealizada **e** condições verificadas; `approvalEligible` = `confrontedWithOwnSource` **e** base NBR 8800:2024.
Resultado atual: **nenhuma verificação é `approvalEligible`**.
- **Solda:** resistência confrontada com a NBR (Tabela 9, 6.2.5.2(a), 6.5.5), mas demanda idealizada e condições (Tabela 8, 6.2.6.2.3) não verificadas ⇒ não elegível; o motivo é registrado por verificação.
- **Gusset:** Whitmore/pseudo-coluna é método **complementar (AISC)**, não consta na NBR ⇒ nunca conformidade NBR; demais verificações `ReproducedFromCode` ⇒ não elegíveis.
- **Tubular:** CIDECT DG1 é **complementar**; Tabela 4.1/4.2 confrontadas ⇒ `confrontedWithOwnSource = true` (CHS, `fy0 ≤ 355`, faixa de validade satisfeita), **mas `approvalEligible = false`** (não é NBR); fora da faixa de validade, nem a própria fonte é confrontada.
- EN e métodos complementares **não** são promovidos a conformidade NBR.

## 7. Testes
- Total `ctest`: **41/41** (35 anteriores + 4 J0–J3 + 1 wrappers + `TestWeldLegLimits`); sem regravação de goldens; hashes de `TStub*`, `JointComponents`, `JointClassification` e goldens idênticos ao baseline.
- `TestJointEngineResistanceWrappers`: equivalência bit a bit dos wrappers com o legado (inalterada) + novos testes dos quatro eixos de elegibilidade (incluindo `fy0 > 355`, faixa de validade violada, solda com resistência confrontada porém não elegível).
- `TestWeldLegLimits`: 8 casos passam e **9 pontos de fronteira** estão `XFAIL` documentando a divergência do `SupportWeld` (e a inconsistência com `checkFilletWeld` em 6,35 mm).

## 8. Divergências encontradas (consolidado)
1. Limite 6,35 × 6,3 mm no `SupportWeld` (mín. e máx.) e inconsistência com `Connections.cpp`, `ShearConnections.cpp`, `BeamExtras.cpp`.
2. Rótulos de cláusula incorretos (`6.2.4`, `5.4.3`, `6.2.5.2`) em `SupportWeld`, `MomentConnection` e `TrussNodeConnection`.
3. 6.2.6.2.3: implementado só parcialmente em `checkFilletWeld` e ausente em outros módulos de solda.
4. Demanda idealizada do `SupportWeld` com lacunas (alma só sob `V`, mesas sem cisalhamento/interação, compatibilidade da solda não verificada, excentricidade ignorada).
5. Afirmação do P1 de que o ICR não estava implementado no legado: **incorreta** (corrigida aqui).

## 9. Correções realizadas
Somente no código **novo** do motor (wrappers e testes). Nenhuma correção em legado.

## 10. Pendências e decisões que dependem de você
| ID | Pergunta | Recomendação |
|---|---|---|
| S-02.1 | Aprovar a correção do limite para 6,3 mm no `SupportWeld` (mín. e máx.), com a consequência de chapas de 6,35 mm exigirem filete mín. de 5 mm? | **Aprovar** (literal da NBR; alinha módulos). Remover os `QEXPECT_FAIL` na mesma entrega |
| S-02.2 | Aprovar a correção dos rótulos de cláusula, o que **altera `numerics.txt` apenas na coluna da cláusula** (valores idênticos, verificável por diff)? | **Aprovar** com diff de verificação |
| S-02.3 | Autorizar implementar 6.2.6.2.3 completo (4a e 40 mm, ou perna ≤ 25 % de L) em `SupportWeld`, e auditar os demais módulos de solda? | **Autorizar** em fase própria (pode alterar goldens se casos violarem) |
| S-02.4 | Escolher como tratar a demanda do `SupportWeld`: só documentar as hipóteses, ou migrar para o ICR/`checkFilletWeld` (altera resultados e goldens)? | **Documentar agora**; migrar só com decisão metodológica e revalidação |

## 11. Estado do Git
Branch `implantacao_dll`; sem commit. Alterados nesta fase: `ResistanceWrappers.{h,cpp}`, `TestJointEngineResistanceWrappers.cpp`, `src/tests/CMakeLists.txt` (registro de `TestWeldLegLimits`), novo `TestWeldLegLimits.cpp`, este relatório. **Intactos (SHA-256/diff):** `SupportWeld.cpp`, `TrussConnections.cpp`, `TubularJoints.cpp`, `Connections.cpp`, `TStub*`, `JointComponents.cpp`, `JointClassification.*`, goldens, C7, persistência, UI, `normas/`.
