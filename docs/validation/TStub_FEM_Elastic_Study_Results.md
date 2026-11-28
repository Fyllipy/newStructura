# Estudo MEF elástico da rigidez inicial do T-stub — resultados (Fase C6.3)

**Estado: PARCIALMENTE CONCLUÍDA.** O MEF foi executado, verificado contra uma solução analítica e convergido; mas a rigidez resultante depende fortemente de hipóteses que as publicações **não fornecem** (furo, espessura da arruela, filete, `t_w`). Logo ele **não decide** entre a rigidez de Piluso/Faella e a da EN; apenas fornece uma faixa e tendências. **Não é validação do T-stub, de nenhum espécime de [P2] nem profissional.** Código: `research/tstub_fem/`; saídas `saida_v2_*.txt`; gráfico `docs/validation/figures/tstub_fem_stiffness.svg`.

## 1. Auditoria do fluxo Mecway (antes de modelar)
| Item | Constatação |
|---|---|
| Fluxo existente (`docs/validacao_mecway/`) | só **cascas Q4 lineares**; contato, sólidos e não linearidade **nunca foram usados nem validados** |
| Contato no Mecway | `contact2` `bondedtie`/`unilateralelastic` com o solver **CalculiX** (penalidade, `normalstiffness`); o exemplo `Samples/BoltedJoint.liml` roda em modo headless (730 nós) |
| Elementos de mola/gap | existem (Spring, Gap), mas o formato `.liml` deles **não está nos exemplos**; não usados |
| Exportação a Python | script IronPython (`mecway.exe arq.liml script x.py`) grava CSV de deslocamentos; **reações não são exportadas** em modo headless (variável vazia) → o estudo usa **controle de força** |
| Calibrações feitas | face 2 do hexaedro = topo, face 1 = base (contato funciona; verificado com bloco contra base rígida: penetração 1,05 µm a 210 MPa = `p/k_n`); força aplicada a um conjunto de nós é **total**, não por nó |
| Riscos | erros viram diálogo modal (usado timeout); penalidade de contato (testada: 2·10⁴ a 2·10⁶ GPa/m); tempo cresce ≈ N^1,5 |
**Verificação do fluxo:** balanço 3D de placa larga (`b/t=8,8`), carga na ponta, malhas 6×6×2 e 10×10×4: flecha 0,0799 e 0,0812 mm × 0,0804 mm do limite de placa larga (1−ν²) e 0,0879 mm de Timoshenko (viga): o MEF fica a +1 % do limite de placa e abaixo de Timoshenko, como esperado (`saida_verify_pipeline.txt`).

## 2. Modelo (hipóteses explícitas — H1…H6; nada disto é a geometria experimental)
Elemento T de um T-stub de duas abas, **quarto de modelo** (simetria em `x`, plano dos parafusos, e em `y`, plano médio da alma); hexaedros de 8 nós com modos incompatíveis; `E=210 GPa`, `ν=0,3`; elástico.
- **H1 simetrias** e **H2 contato:** a aba contra um **plano rígido sem atrito** em `z=0`, equivalente ao par de abas iguais e oposto (válido com atrito nulo e carregamento simétrico). A alavanca emerge do contato (`Q=B−F` verificado: sem contato, `B_q=F_q` com 1,5·10⁻⁶).
- **H3 parafuso:** coluna axial fina sobre o centro de uma **arruela de aço** (`d_w`) que repousa na aba em contato unilateral sem atrito (pode inclinar e descolar); `E_cA_col=EA_b/2`; comprimento `L_b/2`, `L_b=2t_f+(t_h+t_n)/2`; só rigidez axial; sem flexão do parafuso e sem pré-tensão (*snug*).
- **H4** sem plasticidade/grandes deslocamentos. **H5** alma com `t_w` e `h_w` assumidos (4–16 mm; 15–60 mm testados). **H6** furo quadrado de lado `d_b+2 mm` (folga assumida); espessura da arruela 5 mm (assumida).
- **Geometria:** a do espécime 7 de [P2] (`t_f=13,9; m=52,45; n=71,7; b=122,8; d_w=56; M27`) **sem filete** (`r=0`) e com `t_w`, `h_w`, furo e arruela assumidos. **Não é o espécime 7.** Idem as geometrias 1 e 12.
Primeira versão (**v1, descartada**): coluna maciça colada no quadrado da arruela → `K=351–359 kN/mm`; era um **artefato** (travava a rotação da aba: com "parafuso rígido" dava 733 kN/mm, acima do limite de uma viga biengastada `E b t_f³/m³ ≈ 480`). Mantida só como sensibilidade (`saida_v1_*_ARTEFATO.txt`). Isto mostra que o MEF pode errar por ×2–5 com uma modelagem de parafuso inadequada.

## 3. Convergência (espécime 7, modelo v2, `F_q=25 kN`, rigidez `K=F/(2d)` do T-stub)
| Malha | elem. | nós | `K` (kN/mm) | variação | tempo |
|---|---|---|---|---|---|
| h6/nz2 | 1 209 | 2 324 | 74,16 | — | 11 s |
| h4/nz3 | 2 794 | 4 719 | 72,70 | −1,97 % | 21 s |
| h3/nz4 | 5 620 | 8 694 | 71,57 | −1,55 % | 47 s |
| h2,5/nz5 | 8 516 | 12 401 | 71,36 | **−0,30 %** | 85 s |
Critério: variação < 0,5 % entre os dois últimos níveis (atendido). A malha h4/nz3 das varreduras está ≈ 1,9 % acima do valor convergido (~71,4): as **comparações relativas** entre variantes valem; os valores absolutos das tabelas abaixo têm esse viés. (A v1 convergiu para 351,4 em 5 níveis, mas por ser artefato não serve.) Não se usou convergência visual de tensões.

## 4. Influência de cada fator (malha h4/nz3, espécime 7 sem filete; referência `K=72,7 kN/mm`, `B_q=29,0`, `Q_q=4,0 kN` para `F_q=25 kN`)
| Variação | `K` (kN/mm) | Δ | Leitura |
|---|---|---|---|
| **Espessura da arruela 3 / 5 / 8 mm** | 36,7 / 72,7 / 103,1 | −50 % / — / +42 % | **efeito dominante**; dado ausente em [P2] |
| **Sem furo na aba** | 131,0 | +80 % | o furo reduz muito a rigidez (aba maciça sob a arruela é irreal) |
| Parafuso rígido (só flexão da aba) | 74,8 | +2,9 % | o alongamento do parafuso contribui pouco (≈ 3 %) |
| Sem contato (sem alavanca) | 62,7 | −13,8 % | o contato aumenta a rigidez em ≈ 16 %; `Q_q`=4,0 kN (16 % de `F_q`) |
| Rigidez do contato 2·10⁴ / 2·10⁶ GPa/m | 69,8 / 74,0 | −4,0 % / +1,8 % | sensibilidade à penalidade ≲ 4 % |
| `t_w` 4 / 12 / 16 mm | 72,0 / 71,9 / 71,0 | −1,0 / −1,1 / −2,4 % | efeito pequeno |
| `h_w` 15 / 60 mm | 71,3 / 72,5 | −1,9 / −0,3 % | desprezível |
| Parafuso colado v1 (artefato) | 355,8 | ×4,9 | modelagem inadequada |
**Condições de apoio/comprimento de referência:** `t_w` e `h_w` (apoio) pouco influem; a *posição* do apoio (`m`) e a largura influenciam (seção 5). Conclusão sobre "flexão × parafuso × contato": para esta geometria a rigidez é dominada pela **flexão da aba com a introdução da carga pela arruela** (a arruela e o furo importam mais que o alongamento do parafuso).

## 5. Comparação com as formulações (mesma geometria e carga; kN/mm; Piluso com `r=0`)
**Tendências:**
- **Largura `b`** (`m=52,45; t_f=13,9`): MEF 51,2 (b=40), 53,6, 66,2, 72,7 (122,8), **75,2 (160)**, 73,5 (200), 66,6 (260); Piluso `K_i` 38,7 → 141,1 (satura em `b=150,2`); EN com `l_eff=min(2πm; 4m+1,25n; b)` cresce até 398 (`b=260`). O MEF **satura e depois cai** (a aba larga, apoiada por um parafuso central, tem partes externas menos apoiadas): **o conceito de largura efetiva limitada (Piluso) é qualitativamente coerente; a EN com `l_eff` crescente com `b` não** (o `l_eff` da EN é limitado só por `b`, `2πm` (329 mm) e `4m+1,25n` (299 mm); ele cresce com `b` até 299 mm).
- **Espessura `t_f`:** expoente efetivo do MEF entre pontos: 1,88 → 1,56 → 1,27 → 1,09 (não 3): `K` do MEF cresce muito menos que `t_f³` (aba grossa: domina o parafuso/arruela/cisalhamento). Piluso e EN assumem `t_f³` (EN(beff)/MEF vai de 1,7 a 4,3).
- **Distância `m`:** expoente efetivo do MEF ≈ −1,2 (de 120,7 a 38,2 kN/mm para `m` de 30 a 80 mm) contra `−3` das fórmulas; para `m` grande (80 mm) Piluso (33,5) e MEF (38,2) quase coincidem; para `m` pequeno (30 mm, `m/t_f=2,2`) as fórmulas superestimam por ×4–6 (zona de aba espessa/curta, domínio de cisalhamento).
**Três geometrias** (modelo com furo e arruela de 5 mm; **não** são os espécimes):
| Espécime (geom.) | MEF | Piluso `K_i` | EN (`l_eff=b_eff`) | Piluso/MEF | EN/MEF |
|---|---|---|---|---|---|
| 1 | 115,6 | 255,8 | 411,6 | 2,21 | 3,56 |
| 7 | 72,7 | 116,2 | 201,8 | 1,60 | 2,78 |
| 12 | 57,2 | 54,9 | 96,6 | 0,96 | 1,69 |
No espécime 7, sem o furo o MEF dá 131,0 (Piluso/MEF 0,89, EN/MEF 1,54): **a faixa de MEF com hipóteses plausíveis (73–131) contém `K_i` de Piluso (116) e fica abaixo da EN (202)**. Com arruela de 3 a 8 mm a faixa vai de 37 a 103 (com furo).

## 6. Conclusão e limites
- Para as geometrias analisadas, **a rigidez da EN com `l_eff=b_eff` excede o MEF em todas as hipóteses testadas** (1,5–3,6×); a de Piluso/Faella é maior que o MEF com furo (0,96–2,2×) e inferior com aba maciça (0,89). **Nenhuma das fórmulas é confirmada ou refutada**: o MEF não foi feito para os espécimes reais e depende de dados ausentes (arruela, furo, filete).
- O que o MEF sustenta de modo robusto (aparece em todas as variantes): (i) a rigidez **satura com a largura**; (ii) cresce **bem menos que `t_f³`** na faixa de espessuras testada; (iii) o alongamento do parafuso é secundário (≈ 3 %) e a **alavanca eleva a rigidez ≈ 16 %** neste modelo.
- Limites: elástico (não valida rótulas, resistência, deformação última, ruptura de parafusos, pós-pico); frictionless, sem pré-tensão, sem filete (rigidez real de perfil laminado é maior), contato por penalidade, `E`, `ν` nominais, um parafuso por lado no meio da largura, geometria assumida.
- **Para fechar:** (i) repetir com o espécime real (dimensões completas de [P2] ou dados de Faella 1998: `t_w`, arruela, furo, filete); (ii) incluir filete e pré-tensão; (iii) comparar com rigidez **medida**; (iv) só depois discutir `l_eff` da EN. Nenhum ajuste de fórmula foi feito.
