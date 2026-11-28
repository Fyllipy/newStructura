# Compatibilidade entre a rigidez de Piluso/Faella e a rigidez EN da Fase B

Fase C2. Compara formulações; **não há rigidez experimental tabulada** em [P2] (ver §5 para o que as curvas digitalizadas permitem dizer). Código e saídas: `research/tstub_piluso/stiffness_compare.py`, `saida_stiffness_compare.txt`. Sem validação profissional.

## 1. As duas formulações
**Piluso/Faella** ([P1 (38)–(41)], apêndice de [P2]; fonte original Faella et al. 1998, **não consultada**), para o T-stub de dois elementos T com 2 parafusos *snug-tightened*:
$$K=0{,}5\,E\,\frac{b_{eff}t_f^3}{m'^3},\quad m'=m+0{,}8r-\zeta r,\ \zeta=0{,}16\frac{m+0{,}8r}{t_f}-0{,}08;\qquad \delta_y=\frac{2F_y}{K}+\frac{F_y/2+M_y/n}{E A_b/L_b},\quad K_i=\frac{F_y}{\delta_y}$$
com `b_eff=min[d_h+2m; b]`, `A_b=πd_b²/4` (no exemplo), `L_b=2t_f+(t_h+t_n)/2`.

**EN (Fase B do Structura)** (`TStubStiffness`; EN 1993-1-8, 6.3.2, Tab. 6.11): `k_4=0,9\,l_{eff}t_f^3/m^3`, `k_{10}=1,6A_s/L_b`, em série; para o espécime (duas abas T e um conjunto de parafusos): `δ=F/E\,(2/k_4+1/k_{10})`.

## 2. Comparação ponto a ponto
| Aspecto | Piluso/Faella | EN Tab. 6.11 (Fase B) |
|---|---|---|
| Origem | calibração experimental (*snug* × *preloaded*), T-stubs de perfis laminados HEA/HEB | coeficientes da norma por componente (derivação não consultada) |
| Flexão da mesa | `0,5E b_eff t_f³/m'³` **por elemento T** (2 em série → `0,25E b_eff t_f³/m'³`); `m'` inclui o enrijecimento do raio de concordância | `0,9 l_eff t_f³/m³` **por aba** (2 em série → `0,45E l_eff t_f³/m³`); `m` sem correção do raio |
| Largura | `b_eff = min[d_h+2m ; b]` | `l_eff` das Tabelas 6.4/6.6 (depende de `e`, `m`, grupo) |
| Parafuso | `δ_b = B_y/(E A_b/L_b)`, **`B_y = F/2 + M_y/n`** (inclui a alavanca) e `A_b` bruta | `k_{10}=1,6 A_s/L_b` (o fator 1,6 e a área resistente `A_s`) |
| Alavanca | explícita no esforço do parafuso | implícita nos coeficientes (origem do 1,6 não consultada) |
| Aperto | *snug-tightened* | independe (a EN diz "preloaded or non-preloaded") |
| Dados exigidos | `t_f, m, n, r, b, d_h, t_h, t_n, d_b, E, f_y, M_y` | `l_eff, t_f, m, A_s, L_b` |
| Contribui à curva | `K_i` entra em `δ_y` e como parte elástica `F/K_i` de cada ponto | só rigidez inicial (sem lei) |

Mesmas grandezas físicas, mas **com graus de liberdade diferentes**: a EN usa `l_eff` (regras geométricas de comprimento efetivo para resistência) para rigidez; Piluso usa `b_eff` (largura medida/geométrica). Não são o mesmo parâmetro (§3).

## 3. Resultados numéricos nos 12 espécimes de [P2] (rigidez do T-stub, kN/mm)
Hipóteses **minhas** (não de [P2]): `l_eff = min(2πm; 4m+1,25n)` com `e=n`; alternativa `l_eff=b_eff`; `A_s` ISO. Falta de dados (gabarito, `t_w`) impede uma escolha única de `l_eff`.
| # | `K_i` Piluso | EN (1) `l_eff` da EN | EN (2) `l_eff=b_eff` | EN(1)/P | EN(2)/P |
|---|---|---|---|---|---|
| 1 | 183 | 642 | 409 | 3,50 | 2,23 |
| 4 | 97 | 353 | 168 | 3,64 | 1,73 |
| 7 | 112 | 450 | 202 | 4,03 | 1,81 |
| 9 | 86 | 300 | 140 | 3,50 | 1,63 |
| 12 | 53 | 270 | 97 | 5,13 | 1,83 |
(12 espécimes completos em `saida_stiffness_compare.txt`.) Resultados: EN(1)/Piluso entre 3,2 e 5,1 — com `l_eff` da EN maior que a própria largura do espécime (`l_eff` 193–299 mm contra `b` 90–159 mm): **fisicamente inadmissível, inaplicável** a este T-stub; EN(2)/Piluso entre 1,6 e 2,2. Só da mesa (sem parafuso), as duas coincidem se `l_eff = 0,556\,b_{eff}(m/m')^3` (43–67 mm contra `b_eff` 90–125 mm): a EN, com `l_eff = b_eff`, é ≈ 1,8× mais rígida que Piluso para a flexão da mesa. O parafuso (2000–3700 kN/mm no modelo de Piluso) contribui pouco ao total em ambas.

## 4. Consequências de misturar (alternativa B)
Substituindo `K_i` de Piluso pela rigidez EN(2) e mantendo `δ_p` de [P1] (`δ=F/K_i+2δ_p`):
- `δ_u` cai 2,0–5,4 %: a parte elástica `F_u/K_i` é só 4–10 % de `δ_u` (dominado por `2δ_{pu}`, independente de `K`).
- `δ_y` cai ≈ 50 % (ex.: espécime 7: 1,03 → 0,57 mm; espécime 1: 0,60 → 0,27 mm) e, com ele, a posição de `δ_h`.
Logo a **capacidade de deformação última é robusta** à escolha da rigidez, mas a **forma da curva no trecho inicial não** (e `K_i` é a rigidez que a análise momento-rotação usa).

## 5. O que as curvas experimentais digitalizadas permitem concluir
O trecho junto aos eixos não é recuperável da figura (traço de ~4 px sobre o quadro; a digitalização perde os primeiros 0,3–0,9 mm). A secante no primeiro ponto digitalizado é um **limite inferior** da rigidez inicial experimental:
| # | `δ₀` (mm) | `F_exp(δ₀)` (kN) | secante exp. ≥ (kN/mm) | `K_i` Piluso | EN(2) |
|---|---|---|---|---|---|
| 1 | 0,34 | 87,5 | 257 | 183 | 409 |
| 4 | 0,89 | 91,1 | 102 | 97 | 168 |
| 7 | 0,77 | 100,7 | 131 | 112 | 202 |
| 11 | 0,55 | 86,1 | 157 | 89 | 167 |
| 12 | 0,78 | 36,5 | 47 | 53 | 97 |
Esses limites **não discriminam** as formulações: em 1, 4, 7 e 12 a rigidez experimental fica ≥ a de Piluso ou próxima; em 1, 7 e 12 ela é inferior à EN(2); no espécime 11 está próxima da EN(2). Nenhuma das duas é validada por estes dados. **Rigidez experimental inicial: NÃO AVALIADA.**

## 6. Alternativas
- **A — usar integralmente Piluso/Faella.** Vantagens: modelo coerente (rigidez, rótulas, parafuso e `δ_p` do mesmo autor, com validação em 12 ensaios e 4 pontos reproduzidos). Limitações: rigidez para parafusos *snug* (perfil laminado HEA/HEB, 2 parafusos); `b_eff`, `ζ`, `m'` de fonte não consultada; mecanismo 3 ausente; não conversa com a EN de `l_eff` (grupos de fileiras, chapas de extremidade). Consequência: duas bases normativas no mesmo componente, mas separadas.
- **B — rigidez EN (Fase B) + deformação plástica de Piluso.** Vantagens: reaproveita a Fase B e a coerência com a EN do `M_j,ini`. Limitações: **compatibilidade matemática e física não demonstrada**: as rigidezes diferem ≈ 1,6–2,2× (com `l_eff=b_eff`), o `δ_y` cai ≈ 50 % e `δ_h` perde sentido (o ponto `h` foi derivado com `K_i` de Piluso), e a EN foi derivada para outro contexto; as equações de `δ_p` não dependem de `K`, então a mistura é *formalmente* possível mas **não validada**.
- **C — dois modelos independentes, sem misturar.** Vantagens: preserva a rastreabilidade; cada um é testável contra suas próprias referências; permite comparar. Limitações: dois resultados; interface precisa dizer qual é usado.

## 7. Recomendação técnica
**Alternativa C no curto prazo**, com `K_i` de Piluso/Faella usado **somente** na lei de Piluso (modelo da Fase C), e a rigidez EN (Fase B) mantida para `S_{j,ini}` da EN. Mixagem (B) só depois que: (i) haja rigidez experimental/MEF independente que mostre qual das rigidezes reproduz o trecho inicial para o tipo de ligação alvo; (ii) se defina `l_eff` para T-stubs de ensaio e para as Tabelas 6.4/6.6 de forma consistente; (iii) se obtenha Faella et al. (1998). Justificativa: as duas formulações dão rigidezes que diferem de fator ≈ 2 para os mesmos dados, e as curvas digitalizadas não permitem escolher. **A escolha para o Structura continua dependente da validação pendente (MEF/ensaios com rigidez medida).**

## 8. Atualização (Fase C6.3): evidência de MEF elástico 3D
`docs/validation/TStub_FEM_Elastic_Study_Results.md`. Para geometrias de [P2] **sem filete e com furo, arruela e `t_w` assumidos**, o MEF (CalculiX/Mecway, convergido a 0,3 %) deu rigidez **menor** que a EN com `l_eff=b_eff` em todas as hipóteses (EN/MEF 1,5–3,6) e comparável à de Piluso/Faella (Piluso/MEF 0,89–2,2, conforme furo e geometria). Tendências robustas do MEF: a rigidez **satura com a largura** (coerente com `b_eff` limitado, não com `l_eff` crescente) e cresce **menos que `t_f³`**. A espessura da arruela (±50 %), o furo (+80 % sem ele) e o filete (não modelado) são dados ausentes que dominam o resultado: **o MEF não decide** entre as formulações. A recomendação da §7 (alternativa C) permanece.
