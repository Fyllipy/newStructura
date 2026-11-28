# Validação do modelo de Piluso et al. contra os 12 espécimes de [P2]

Fase C3. Compara o modelo **reproduzido** (`research/tstub_piluso/piluso_tstub.py`) com o experimento de [P2]. Não há ajuste de parâmetros. **Não é validação profissional do Structura:** valida (no sentido de verificar) que o modelo publicado foi reproduzido e como ele se compara com os ensaios dos autores. Saídas completas: `research/tstub_piluso/saida_validate_published.txt`, `saida_validate_experimental.txt`; gráfico: `docs/validation/figures/tstub_piluso_comparison.svg` (e `.png`).

## 1. Origem dos dados (rastreabilidade)
| Dado | Fonte | Natureza |
|---|---|---|
| Geometria (Tab. 1), aço e parafuso (Tab. 2) | [P2] Tabelas 1 e 2 | **tabulado** |
| `F_u,exp`, `δ_u,exp` (deslocamento na força máxima) e resultados teóricos publicados | [P2] Tabela 3 | **tabulado** (com a correção de rótulos dos espécimes 6 e 7, §3) |
| `E=210 000 N/mm²` | exemplo trabalhado de [P2] | hipótese de transcrição (não consta das tabelas) |
| Área do parafuso na resistência | `π d_b²/4` (bruta) | **hipótese** que reproduz a Tabela 3 (ver reconstrução §9) |
| Curva `F`–`δ` experimental | [P2] Fig. 8 | **DIGITALIZADA de figura raster 1-bit (300 dpi), não tabulada**; `research/tstub_piluso/digit/` |
Não há dados de pré-tensão, `t_w` nem gabarito. Os pontos digitalizados são a mediana de pixels do traço contínuo; a calibração dos eixos (±2 px) foi verificada: o máximo digitalizado coincide com `F_u,exp` da Tabela 3 em ≤ 1,1 % em todos os espécimes (ex.: 346,2 × 342,39; 482,0 × 479,40 kN). O trecho a < ~0,3–0,9 mm (junto aos eixos) **não é recuperável**.

## 2. Reprodução das equações e dos valores publicados (separada da comparação experimental)
**(a) Exemplo trabalhado (espécime 7, [P2] p. 702–703):** 35 grandezas (deformações, curvaturas, `M_h/M_y…M_u/M_y`, `M_y,M_u`, `β_{u,lim}`, `F_y,F_h,F_m,F_u`, `ζ`, `m'`, `K`, `δ_y`, `K_i`, `ξ_2,ξ_3`, `D(ξ_2),F(ξ_3),C`, `δ_{ph},δ_{pm},δ_{pu}`, `δ_h,δ_m,δ_u`) reproduzidas com diferença ≤ 0,06 % (maior: `δ_{ph}` 0,058 %), tolerâncias de 0,005–0,2 % conforme o arredondamento publicado. `β_u`: 0,4732 × 0,47 impresso (arredondamento).

**(b) Fórmulas fechadas `D,F,G` × integração numérica independente** da curvatura ([P1 (18)] com momento linear): diferença relativa ≤ 3,4·10⁻⁷ para ξ > ξ₁ (espécimes 1, 7, 12).

**(c) Tabela 3 de [P2], colunas teóricas (12 espécimes, sem ajustes):** `M_h/M_y, M_m/M_y, M_u/M_y`: |desvio| ≤ 0,003 %. Com a área bruta no parafuso: `β_u`, `δ_{u,th}` e `F_{u,th}` reproduzidos a ≲ 0,01 % em **11 dos 12** espécimes. Exceções: **espécime 8** (a Tabela 3 usa o mecanismo 1; o critério `β_u` dá o 2: `F_u` 319,4 × 386,0 kN; ao forçar o mecanismo 1, reproduz 385,99 kN e 45,19 mm exatamente) e **espécime 11** (`β_u` 0,3749 × 0,3651 impresso; `δ_u` e `F_u` coincidem).

## 3. Correções de rótulo da Tabela 3 (inferência documentada)
Nos espécimes 6 e 7 as linhas `F_{u,exp}` e `F_{u,th}` estão trocadas em relação à razão impressa `F_{u,th}/F_{u,exp}` e ao exemplo trabalhado (`F_u` teórico = 429,9 kN no espécime 7). Evidências: (i) o modelo reproduz 482,32 (6) e 429,90 (7) como valores **teóricos**; (ii) a curva experimental digitalizada atinge 448,6 e 482,0 kN; (iii) a Fig. 8 mostra a curva-modelo terminando em ≈ 430 kN (espécime 7). Adotado: `F_{u,exp}(6)=445,76`, `(7)=479,40`.

## 4. Comparação com o experimento
Valores por espécime (`F` em kN, `δ` em mm; mecanismo pelo critério `β_u`; espécime 8 com o mecanismo 1, como em [P2]):
| # | mec. | `F_u,exp` | `F_u,th` | th/exp | `δ_u,exp` | `δ_u,th` | th/exp | viés curva (%) | RMS (%) | máx. \|erro\| (%) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2 | 342,4 | 344,0 | 1,005 | 21,9 | 19,2 | 0,875 | −7,8 | 9,0 | 18,0 |
| 2 | 2 | 337,1 | 343,7 | 1,019 | 22,7 | 19,4 | 0,853 | −6,7 | 8,4 | 17,2 |
| 3 | 2 | 281,5 | 271,4 | 0,964 | 39,3 | 40,2 | 1,023 | −13,6 | 14,2 | 23,9 |
| 4 | 1 | 343,3 | 324,2 | 0,944 | 63,7 | 62,6 | 0,983 | −12,5 | 12,9 | 19,4 |
| 5 | 1 | 423,6 | 394,4 | 0,931 | 68,2 | 58,4 | 0,857 | −10,6 | 10,9 | 15,1 |
| 6 | 2 | 445,8 | 482,3 | 1,082 | 40,8 | 34,0 | 0,834 | +0,1 | 4,8 | 11,0 |
| 7 | 1 | 479,4 | 429,9 | 0,897 | 70,8 | 66,6 | 0,941 | −16,9 | 17,2 | 21,9 |
| 8 | 1† | 348,8 | 386,0 | 1,107 | 47,6 | 45,2 | 0,949 | −1,7 | 5,4 | 11,9 |
| 9 | 1 | 378,0 | 329,0 | 0,870 | 58,0 | 63,3 | 1,091 | −18,2 | 18,4 | 21,6 |
| 10 | 1 | 323,8 | 305,3 | 0,943 | 38,0 | 39,0 | 1,025 | −12,8 | 13,1 | 23,8 |
| 11 | 1 | 250,4 | 235,6 | 0,941 | 59,1 | 53,6 | 0,907 | −2,6 | 3,4 | 9,4 |
| 12 | 1 | 195,9 | 198,8 | 1,015 | 81,0 | 87,8 | 1,084 | −8,2 | 9,5 | 14,3 |
† mecanismo 2 pelo critério (`F_u,th`=319,4; 41,2 mm; viés −17,1 %).

**Estatísticas (12 espécimes):** `F_u,th/F_u,exp`: média 0,977, desvio-padrão 0,068, faixa 0,870–1,107; `δ_u,th/δ_u,exp`: média 0,952, dp 0,086, faixa 0,834–1,091. **Coincidem com as publicadas** por [P2] (resistência: média 0,98, dp 0,07, 0,87–1,11; deslocamento: média 0,95, dp 0,09, 0,84–1,09).

**Erro da curva** (força relativa, `(F_th−F_exp)/F_exp`, em 60 pontos igualmente espaçados em `[max(5 %·δ_u,exp; 1 mm); min(δ_u,th; fim da curva exp.)]`, excluindo o trecho junto aos eixos; o trabalho `W` é a área sob a curva até o menor deslocamento comum):
- modelo **multilinear de 4 pontos**: viés médio −9,3 % (de −18,2 a +0,1), RMS médio 10,6 %, erro máximo médio 17,3 %, `W_th/W_exp` médio 0,912 (0,821–1,007);
- **curva contínua** (mesmas equações): viés médio −3,9 %, RMS médio 7,5 % (3,9–13,4 %).
A subestimativa do modelo multilinear é sistemática: os segmentos retos ligam pontos de uma curva côncava (ver o gráfico). As métricas combinam viés (sinal), RMS (dispersão) e máximo, mais o trabalho, para não depender de um único número; elas **dependem** do trecho excluído e da calibração (±2 px ≈ ±1–2 %).

## 5. Critérios de avaliação (§6.3 do prompt)
| Critério | Resultado |
|---|---|
| 1. Reprodução das equações | **Concluída** para os mecanismos 1 e 2 (integração numérica independente; exemplo trabalhado a ≤ 0,06 %). Mecanismo 3: sem equações → **não reproduzido**. |
| 2. Reprodução dos valores publicados | **Concluída com ressalvas documentadas**: 11/12 na Tabela 3; espécime 8 (mecanismo) e `β_u` do 11. |
| 3. Curvas teóricas × experimentais | **Parcial**: estatísticas de `F_u`/`δ_u` coincidem com as de [P2]; curva multilinear subestima em média 9 %; trecho inicial (rigidez) **não avaliado**. |
| 4. Previsão do mecanismo | **11/12** corretos; o critério falha no espécime 8 (prevê 2; observado 1). |
| 5. Limitações | ver §6 |

## 6. Limitações
Apenas 12 espécimes, com `m/t_f` de 2,7 a 4,4, parafusos M20–M27, aço Fe430 (perfis laminados), carga monotônica; 11 de perfis laminados e 1 soldado. A lei do material exige a curva verdadeira completa (inclusive `ε_u` de ensaio de cupom), que raramente é reportada ([P2] p. 700); sem ela o modelo não pode ser aplicado a ensaios de terceiros. Rigidez inicial: sem referência experimental tabulada. Mecanismo 3 e parafusos pré-tensionados: fora do modelo. As curvas experimentais são digitalizadas.

**Classificação da etapa: PARCIALMENTE CONCLUÍDA** (modelo reproduzido e comparado; curva inicial e mecanismo 3 não validados).
