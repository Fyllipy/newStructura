# Relatório P1 — F-SOL e F-GUS (wrappers de resistência, proveniência e divergência de Whitmore)

Sem validação profissional. Não foram implementados: ICR, distribuição entre barras, rigidez de soldas ou de nós tubulares, leis F–δ/M–θ, flambagem fora do plano, novos modelos constitutivos.

## 1. Arquivos
**Criados:** `src/design/jointengine/ResistanceWrappers.{h,cpp}` (`WeldSupportResistance`, `GussetResistance`, `TubularResistance`; `FormulationProvenance`, `WrappedCheck`, `ResistanceReport`); `src/tests/TestJointEngineResistanceWrappers.cpp`; documentos `research/P1_CONTORNO_CHAPA_DE_NO_ESPECIFICACAO.md` e este relatório.
**Alterados:** `CMakeLists.txt` e `src/tests/CMakeLists.txt` (inserções); `research/P0_F_GUS...md` (correção do título de Whitmore 1952).
**Não alterados (SHA-256 idêntico/sem diff):** `SupportWeld.cpp`, `TrussConnections.cpp`, `TubularJoints.cpp`, `TStub*`, `JointComponents.cpp`, `JointClassification.*`, goldens, C7, persistência, UI.

## 2. Wrappers e equivalência numérica
Cada wrapper **delega** a `checkSupportWeld`, `checkGusset` e `checkTubularJoint` e registra proveniência por verificação. Teste `TestJointEngineResistanceWrappers` (3 entradas por modelo): nome, cláusula, unidade, demanda, resistência, razão, status, motivo e memória do legado **idênticos bit a bit**; `whitmoreWidth`, `freeLength` e `gussetWidth` recuperados do legado; tubular: `incluídas + excluídas = legadas` (nada some em silêncio), mesma ordem. Resultado: **40/40** testes (35 anteriores + 4 de J0–J3 + 1 de P1), goldens não regravados.

## 3. Proveniência (níveis)
| Verificação | Nível atribuído | Elegível a aprovação |
|---|---|---|
| Filete das mesas ("Tabela 9") | `PartiallyDocumented` — fator `1,0 + 0,5 sen^1,5 θ` conferido com [NG04] eq. (1); texto da NBR 8800:2024 e γ **não** conferidos | não |
| Whitmore: tração | `PartiallyDocumented` — `L_w` conferida com [CB07] Tab. 1; limitação por `gussetWidth` e resistências NBR **não** conferidas | não |
| Whitmore: compressão | `PartiallyDocumented` — ver §5 | não |
| Demais (parafusos, block shear, solda ao apoio, alma, etc.) | `ReproducedFromCode` | não |
| Tubular (equações/limites CIDECT numerados) | `ReproducedFromCode` — CIDECT DG1 **não lido** | não |
Nenhuma verificação é `ConfrontedWithStandard`/`DomainVerified`; por regra do tipo, **todas** têm `approvalEligible = false`. O legado continua decidindo a aprovação onde já decide; o motor não promove nenhum resultado.
**Tubular — exclusões:** verificações sem equação/limite CIDECT numerado único (ex.: referências "4.1 / 4.2", "Tab. 4.3 nota geral") ficam em `excluded` com motivo.

## 4. Fontes efetivamente lidas (PDFs obtidos de acesso aberto)
| Fonte | Páginas lidas | Uso |
|---|---|---|
| Ng, Deng, Grondin & Driver (2004), AISC EJ, 2º trim., pp. 39– | pp. 39–42 (revisão e início do programa experimental) | eq. (1) do filete; faixa experimental |
| Chambers & Bartley (2007), AISC EJ, 3º trim., pp. 255– | **pp. 255–262 (integral)** | `L_w`, bordas, pseudo-coluna, K 0,5–1,2 |
| Thornton (1984), AISC EJ 21(3), pp. 139–148 | **integral (pp. 139–148)** | origem de K = 0,65; comprimento da tira; solda em C (método elástico) |
**Não obtidas** (limitação registrada; modelos correspondentes **permanecem bloqueados**): EN 1993-1-8:2005 e 2024 (pagas, sem cópia legítima aberta); NBR 8800:2024 (ABNT, idem); CIDECT DG1 2008 (só cópias de terceiros nos resultados — não usadas; registro legítimo: TU Delft repository, *J. Wardenier, Y. Kurobane, J.A. Packer*); AISC Design Guide 29 (sem acesso; reimpressão do AISC retornou 403); Neis (1985) e discussão de 1987 (ASCE, só resumo); Lesik & Kennedy (1990) e Butler–Pal–Kulak (1972) (não localizados abertos; o relatório U. Alberta SER 159 de 1988 é outro documento); Whitmore (1952), *Experimental Investigation of Stresses in Gusset Plates*, Univ. of Tennessee Eng. Exp. Station Bulletin 16 (PDF não localizado; título corrigido — o título usado na P0 estava errado).

## 5. Equações conferidas e divergências (Whitmore / flambagem)
**Conferido:**
1. `V_θ/V_0 = 1,0 + 0,50 sen^1,5 θ` — [NG04] p. 39, eq. (1); o legado usa o fator com θ = 90° (1,5) e `weldAngleDeg`.
2. `L_w = 2 L tan 30° + s` — [CB07] Tab. 1; legado `whitmoreWidth = (linhas−1)·g + 2·L·tan30°` (o legado trata a distância entre linhas como `(linhas−1)·g`, equivalente a `s` quando as linhas são equidistantes).
3. **Origem de K = 0,65:** [THO84] p. 147: tira de 1 in. de chapa entre a seção de Whitmore e o ponto de trabalho tratada como **coluna biengastada (K = 0,65)**; o comentário do legado ("extremidade engastada com deslocamento lateral impedido, K = 0,65") é compatível.
**Divergências/lacunas (registradas, nada corrigido):**
- **D-1 (K):** [CB07] p. 255 relata K de 0,5 a 1,2, precisão variável, método sempre conservador — **não determina valor de projeto**; K = 0,65 tem origem em [THO84], mas isso **não** o valida frente à NBR 8800:2024 (não lida).
- **D-2 (comprimento):** [THO84] p. 147 usa l₁ da seção de Whitmore ao ponto de trabalho e **adverte** que usar l₁ pode ser contra a segurança, sugerindo média de l₁, l₂, l₃ (com ressalva); [CB07] p. 255 relata uso de média, máximo ou L₂. O legado usa um único `freeLength` informado (distância do apoio à linha de furos mais próxima). **Correspondência com l₁/L₂ plausível, não demonstrada.**
- **D-3 (largura além das bordas):** [CB07] p. 256: a parte da área de Whitmore que sai de uma **borda livre** é descontada; se entra na viga/pilar, usa-se largura **ponderada por `t F_y`** (Thornton & Kane via [CB07]); além disso o cisalhamento de bloco pode governar quando a largura excede a chapa ([CB07] eq. 4, Williams & Richard). O legado **limita** `L_w` a `gussetWidth` (largura da chapa na solda ao apoio) — critério **diferente** (não é geometria da borda livre) e **sem** ponderação. Sem o contorno (D da autorização) não é possível reproduzir o tratamento de [CB07].
- **D-4:** [THO84] calcula a tensão de Whitmore com **área bruta** (sem descontar furos) para o escoamento; o legado usa `Agw` (bruta) para escoamento e `Anw` para ruptura — compatível, mas a combinação NBR não foi conferida.
- **D-5 (solda):** [THO84] p. 142 usa o **método elástico clássico** (AISC Manual Part 4) para solda em C e indica o método inelástico por Brandt (1982, AISC EJ 19(3), pp. 150–159, **não lido**); o legado usa grupos simples (mesas e alma) — sem ICR (fora do escopo).

## 6. Limitações e dados ausentes
Contorno da chapa, ângulos/geometria das barras, ponto de trabalho, grupo de solda genérico, penetração parcial, processo/tenacidade, βw, limites β/γ/τ de tubulares (ver P0). Wrappers não definem rigidez, distribuição de esforços nem F–δ/M–θ.

## 7. Estado do Git
Branch `implantacao_dll`; somente arquivos novos/CMake como acima; legados de design, goldens e C7 intactos (hashes conferidos).

## 8. Itens que permanecem bloqueados
Resistência com fundamentação normativa (EN 1993-1-8, NBR 8800:2024, CIDECT DG1, DG 29); leis constitutivas de solda (Neis, Lesik–Kennedy, Butler–Pal–Kulak); ICR; distribuição entre barras; rigidez de nós tubulares; flambagem fora do plano; avaliação conforme de D-1…D-3 contra a NBR; Whitmore (1952) original. Contorno da chapa: especificado, **não** implementado.

---

# Adendo — confronto com as normas fornecidas em `normas/` (PDFs entregues pelo usuário)

Textos extraídos para o scratchpad (nada gravado em `normas/`); equações confirmadas **na imagem das páginas**. Cópias de terceiros (ex.: CIDECT) foram fornecidas pelo usuário e assim registradas.

## A. Fontes agora lidas
| Fonte | Trechos lidos | Observação |
|---|---|---|
| ABNT NBR 8800:2024 | Tabela 3 (p. 25); Tabelas 8–9 e 6.2.5.1–6.2.5.2 (pp. 76–79); 6.2.6 e Tabelas 10–11 (p. 80); 6.5.5–6.5.6 | **busca textual: "Whitmore" e "gusset" sem ocorrência** — a norma **não traz** o método de Whitmore |
| EN 1993-1-8:2005 | 4.5.1–4.5.3 e Tabela 4.1 (pp. 42–44); 6.3 Tabela 6.11, Nota 3; 2.? e 6.4 por texto | **só a edição 2005** foi entregue; a de 2024 **não** |
| CIDECT DG1, 2ª ed. (2008) — cópia do usuário | Tabela 4.1 e 4.2 (pp. 32–33 do guia) | demais tabelas (4.3, 6.1, 7.1) **não lidas** |
| AISC Design Guide 29 | exemplos com flambagem de Whitmore (PDF pp. 59, 111/189, 204) | método geral (capítulos teóricos) **não lido por inteiro** |

## B. Equações conferidas
- **NBR 8800:2024, Tabela 9 e 6.2.5.2(a):** `F_w,Rd = 0,6 f_w A_w [1 + 0,5 sen^1,5 θ] / γ_w2`; penetração total, tração normal: metal-base `A_MB f_y/γ_a1`; filete: "metal-base deve atender 6.5"; `γ_w2 = 1,35` (normal)/`1,15` (excepcional); `γ_a1 = 1,10`, `γ_a2 = 1,35` (Tabela 3). **Conferem com o legado** (`SupportWeld`).
- **6.5.5:** `R_d = 0,60 A_g f_y/γ_a1` e `0,60 A_nv f_u/γ_a2` — conferem com as verificações de metal-base do legado.
- **Tabela 11** (3/5/6/8 mm) e perna máxima (6.2.6.2.2: `t` se `t < 6,3`; `t − 1,5` se `≥ 6,3`) conferem **exceto** o limite: legado usa 6,35 mm onde a norma usa 6,3 mm.
- **EN 1993-1-8:2005, 4.5.3.2 (4.1), 4.5.3.3 (4.2–4.4) e Tabela 4.1:** `[σ⊥² + 3(τ⊥² + τ∥²)]^0,5 ≤ f_u/(β_w γ_M2)` e `σ⊥ ≤ 0,9 f_u/γ_M2`; `f_vw,d = (f_u/√3)/(β_w γ_M2)`; β_w = 0,8 (S235), 0,85 (S275), 0,9 (S355), 1,0 (S420/S460) — **confirmados** (antes só por fonte secundária). O legado **não** implementa a EN.
- **CIDECT DG1 2008, Tabela 4.1:** `N_i* = Q_u Q_f f_y0 t0²/sen θ_i` (4.1); punção `0,58 f_y0 π d_i t0 k_a/sen θ_i`, `k_a = (1+sen θ_i)/(2 sen θ_i)` (4.2–4.3); `Q_u` X `2,6[(1+β)/(1−0,7β)]γ^0,15` (4.4), T/Y `2,6(1+6,8β²)γ^0,2` (4.5), K com afastamento `1,65(1+8β^1,6)γ^0,3[1+1/(1,2+(g/t0)^0,8)]` (4.6); `Q_f = (1−|n|)^C1`, `C1 = 0,45−0,25β` (compressão, T/Y/X), `0,25` (K), `0,20` (tração) (4.7); faixa de validade (`0,2 ≤ d_i/d_0 ≤ 1,0`, `t_i ≤ t_0`, `e/d_0 ≤ 0,25`, `θ_i ≥ 30°`, `g ≥ t_1+t_2`, `f_yi ≤ f_y0`, `f_y ≤ 0,8 f_u`, `f_y ≤ 460`, `2γ ≤ 50` (X: 40), `d_i/t_i ≤ 50`) — **conferem com o legado** para CHS.

## C. Achados novos que alteram a P0
1. **A NBR 8800:2024 contém o método do centro instantâneo de rotação com lei de deformação do filete** (6.2.5.2(b)): `Δ_mi = 0,209 (θ_i + 2)^−0,32 d_wi`, `Δ_ui = 1,087 (θ_i + 6)^−0,65 d_wi ≤ 0,17 d_wi`, `f(p) = [p(1,9 − 0,9p)]^0,3`, `p = Δ/Δ_m`; e a combinação 6.2.5.2(c). Isso **é verificação de resistência** (a norma o usa para a resistência de grupos excêntricos), **não rigidez inicial**; o estado "equações de ICR: não documentado" da P0 passa a **Documentado (norma)** para resistência; **continua sem implementação** (restrição desta fase) e **sem** lei de rigidez.
2. **EN 1993-1-8 prescreve solda rígida na rigidez da junta:** Tabela 6.11, Nota 3: para soldas (componente k19) "o coeficiente de rigidez deve ser tomado como infinito" e o componente não entra em `S_j`. É **convenção normativa**, não demonstração física; e 4.? exige que soldas tenham capacidade de deformação adequada, "ductilidade das soldas **não deve ser considerada**". Portanto a hipótese "solda rígida" do modelo EN existente é **normativamente prevista pela EN para S_j**, mas deve seguir rotulada como convenção.
3. **K da flambagem de Whitmore depende do vínculo:** AISC DG29 usa K = 0,50 (chapa apoiada nas duas bordas; Gross 1990), K = 0,65 (Dowswell) e K = 1,2 (possibilidade de deslocamento lateral); o legado fixa 0,65 sem representar os vínculos.
4. **DG29 trata a seção de Whitmore que atravessa a junta:** efetiva na viga/pilar quando `t_w ≥ t_g` (exemplos), diferente do legado (limita a `gussetWidth`).
5. **Rótulos de cláusula do legado divergentes da NBR:** `6.2.4` (penetração total), `5.4.3` (metal-base da alma) e `6.2.5.2` (perna máxima) — a norma traz esses itens na Tabela 9/6.2.5, 6.5.5 e 6.2.6.2.2.

## D. Efeito sobre os wrappers (conferência aplicada, testada 40/40)
| Wrapper | Mudança de proveniência |
|---|---|
| Solda | mesas (penetração total), mesas (filete), alma (filetes), metal-base da alma e do apoio: **`ConfrontedWithStandard`** (elegíveis; demanda idealizada do legado **não** conferida); pernas mín./máx.: `PartiallyDocumented` (limite 6,35 × 6,3; rótulo) |
| Gusset | permanecem `PartiallyDocumented`/`ReproducedFromCode` — a NBR não traz Whitmore; K e comprimento não confrontáveis com norma; **nenhuma elegível** |
| Tubular (CHS) | eq. 4.1, 4.2 e faixa de validade da Tabela 4.1: **`ConfrontedWithStandard`** (elegíveis, exceto `f_y0 > 355`, que aciona a nota (**) não lida → `PartiallyDocumented`); demais (Tabela 4.3 de K com sobreposição, chapa transversal 7.x): `ReproducedFromCode`; o **EN 1993-1-8 seção 7 não foi comparado** |
O motor continua **sem** decidir aprovação: a elegibilidade só marca que a formulação foi confrontada com a norma citada; a decisão de aprovação segue no legado.

## E. Itens que seguem bloqueados
Neis (1985) e discussão de 1987, Lesik & Kennedy (1990), Butler–Pal–Kulak (1972), Whitmore (1952) — **não fornecidos nem localizados abertos**; EN 1993-1-8:2024 — **não fornecida**; leis constitutivas e rigidez de soldas; ICR (equações agora documentadas na NBR, **implementação vetada**); distribuição entre barras; rigidez de nós tubulares; flambagem fora do plano; Tabelas 4.3, 6.1 e 7.1 do CIDECT; método geral do DG29 (UFM etc.).


> **Correção posterior (S-02):** a frase "ICR ... implementação vetada/não implementado no legado" refere-se ao `SupportWeld`; o legado possui o ICR do módulo genérico (`Connections.h`, `IcrInput`, `WeldSegment`), não utilizado pelo `SupportWeld`. Os níveis de elegibilidade da seção D foram **revistos** no S-02 (`S02_RELATORIO_SANEAMENTO_F_SOL_F_GUS.md`): nenhuma verificação é `approvalEligible`.
