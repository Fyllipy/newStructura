# Barras compostas e torção com empenamento (seções abertas)

### Como o Structura calcula: modelo, fórmulas, marchas de cálculo, hipóteses e validações (ABNT NBR 8800:2024)

Este livro complementa o *Dimensionamento de estruturas de aço com perfis laminados, soldados e tubulares*
(`LIVRO_DIMENSIONAMENTO_PERFIS_LAMINADOS.md`). Trata de dois assuntos que a auditoria de normas (`AUDITORIA_NORMAS.md`) apontou como
ausentes: as **barras compostas** (NBR 8800, 5.3.6) e a **torção em seções abertas** (NBR 8800, 5.5.2.3), que exigiu acrescentar o
**empenamento** como sétimo grau de liberdade das barras do solver.

---

## Sumário

**Parte A — Barras compostas**
1. Configurações, catálogo e criação pelo usuário
2. Propriedades da seção composta
3. Compressão (5.3.6)
4. Tração (5.2.8.2)
5. Flexão (D.2.4) e cortante (5.4.3)
6. Parâmetros de projeto e persistência
7. Hipóteses e limitações

**Parte B — Torção com empenamento**
8. Teoria de Vlasov: o que o solver passa a calcular
9. O elemento de barra com sétimo grau de liberdade
10. Condições de contorno do empenamento
11. Cargas e resultados
12. Verificação de seções I/H (5.5.2.3)
13. Como ativar e como interpretar
14. Validação numérica
15. Hipóteses e limitações
16. Mapa do código

---

# Parte A — Barras compostas

## 1. Configurações, catálogo e criação pelo usuário

Barra composta é a formada por **dois perfis iguais** afastados de `e` e ligados a intervalos `ℓ` por chapas espaçadoras (parafusadas ou
soldadas). Três configurações são tratadas:

| Família (`SectionShapes`) | Configuração | Parâmetros |
|---|---|---|
| `Dupla Cantoneira T` | duas cantoneiras de abas iguais, abas verticais costas com costas (formam um T) | `b`, `t`, `e`, `r1`, `r2` |
| `Duplo U Costas` | dois U com as almas voltadas uma para a outra (forma de I) | `d`, `bf`, `tw`, `tf`, `e`, `r`, `r1`, `flange_angle_deg` |
| `Duplo U Face a Face` | dois U com as mesas voltadas uma para a outra (caixa com afastamento) | idem |

* **Geometria.** Cada família gera **dois contornos externos** (a seção tem `outerCount = 2`): o perfil base é posicionado à esquerda e o
  espelhado à direita, a `e/2` do eixo de simetria. O preenchimento e o cálculo das propriedades integram os dois contornos
  (`computeGrossSection(loops, outerCount)`); os demais consumidores gráficos usam preenchimento par-ímpar e não precisaram mudar.
* **Catálogo.** `SectionCatalog` deriva automaticamente as composições a partir dos perfis laminados: `2L 76 x 6.1`, `2U … (costas)`,
  `2U … (face a face)`, com afastamento inicial `e = 10 mm` (editável). Aparecem no navegador de perfis na categoria **Barras compostas**.
* **Criação pelo usuário.** Pelo editor de perfis o usuário escolhe uma das três famílias, informa as dimensões e o afastamento `e` e salva no
  banco do programa; o perfil passa a constar da biblioteca como qualquer outro. A categoria `Composta` do `ProfileLibrary` separa-os dos
  laminados e dos formados a frio.

## 2. Propriedades da seção composta

Todas as propriedades vêm da integração dos dois contornos, com o centroide do conjunto como origem:

```text
A = 2 A₁         Ix = 2 (I₁x + A₁ yc²)     Iy = 2 (I₁y + A₁ (e/2 + xc)²)     (teorema dos eixos paralelos pela integração do contorno)
J  = 2 J₁                                       (constante de torção: soma dos perfis, sem colaboração das chapas espaçadoras)
Cw ≈ 2 Cw₁   (perfis U)       Cw ≈ b³t³/9 (duas cantoneiras em T, centro de cisalhamento na linha média das abas)
```

`DesignSection` guarda ainda `componentArea` (área de um perfil), `rMin` (menor raio de giração **de um perfil componente**) e `gapE`
(afastamento). `rMin` é o `r_mín` da NBR 8800, 5.3.6.1, usado no limite de esbeltez entre ligações.

## 3. Compressão (NBR 8800, 5.3.6)

### 3.1 Condição de ligação (5.3.6.1)

```text
ℓ / r_mín  ≤  3/4 (L/r)₀         e   pelo menos duas ligações intermediárias, uniformemente espaçadas
```

`(L/r)₀` é o maior índice de esbeltez da barra composta atuando como unidade. Se `ℓ` não for informado, adota-se `ℓ = L/3` (duas ligações
intermediárias). A verificação aparece como `composite` na tabela de resultados; não atender reprova a barra (`η > 1`).

### 3.2 Esbeltez equivalente na flexão em torno do eixo perpendicular ao afastamento (5.3.6.3)

A flexão em torno do eixo Y (perpendicular à linha que une os perfis) deforma os perfis uns em relação aos outros e solicita as ligações. A
carga de flambagem elástica de Euler desse eixo é reduzida: `Ne,m` no lugar de `Ne,0 = π² E Iy / Ly²`.

```text
(a) parafusos com aperto normal :     Ne,m = Ne,0 · (L/r)₀² / [ (L/r)₀² + (ℓ/r_mín)² ]

(b) solda ou parafusos protendidos :  ℓ/r_mín ≤ 40 :  Ne,m = Ne,0
                                      ℓ/r_mín > 40 :  Ne,m = Ne,0 · (L/r)₀² / [ (L/r)₀² + (Ki ℓ/r_mín)² ]
                                      Ki = 0,50 (cantoneiras em T)   0,75 (U costas, em I)   0,86 (demais)
```

`Ne,m` substitui `Ney` na determinação de `N_e` (`min(Nex ; Ney,m ; Nez)`, e as formas de flexo-torção para a cantoneira dupla, que é
monossimétrica), de modo que `χ`, `λ₀` e `N_c,Rd` seguem o procedimento do Capítulo 5 do livro principal. O memorial mostra a regra aplicada
(`5.3.6.3-a` ou `-b`) e os valores de `Ne,0` e `Ne,m`.

### 3.3 Exemplo

Valores **ilustrativos** (não extraídos de um perfil específico): duas cantoneiras em T, `L = 3000 mm`, `ℓ = L/3 = 1000 mm`, `r_mín = 14,9 mm`, `(L/r)₀ = 97`:

```text
ℓ/r_mín = 1000/14,9 = 67,1  ≤ 0,75 × 97 = 72,8      → atende 5.3.6.1
parafusos com aperto normal:  Ne,m = Ne,0 · 97² / (97² + 67,1²) = 0,676 Ne,0
solda, Ki = 0,50:             Ne,m = Ne,0 · 97² / (97² + (0,5 × 67,1)²) = 0,893 Ne,0
```

## 4. Tração (5.2.8.2)

Em barra composta tracionada, `ℓ/r_mín` entre ligações adjacentes não deve ultrapassar o limite recomendado de esbeltez (300). Como em
5.2.8 o limite é recomendação, o programa emite **aviso** (não reprova). A resistência da seção segue o Capítulo 4 do livro principal com a
área bruta de **ambos** os perfis.

## 5. Flexão (Anexo D, D.2.4) e cortante (5.4.3)

* **Duas cantoneiras em T**, flexão em torno do eixo perpendicular à alma do T (D.2.4): `λ = b/t`, `λ_p = 0,54√(E/f_y)`, `λ_r = 0,91√(E/f_y)`;
  FLM da aba comprimida e FLA da aba vertical como no perfil T (Seção 6.6 do livro principal); FLT como a do T.
* **Dois U** (costas ou face a face): a NBR 8800 **não traz** resistência à flexão da configuração (5.4.1.1 só cobre U isolado e seções-caixão
  com ligação contínua). O programa marca `covered = false`, escreve a explicação no memorial e **não inventa** resistência. A tração,
  a compressão e o cortante continuam verificados.
* **Cortante:** duas cantoneiras em T, `k_v = 1,2`, `A_w = 2 b t` (5.4.3.4); duas almas de U somadas (5.4.3.1.1); duas mesas somadas (5.4.3.5).

## 6. Parâmetros de projeto e persistência

Por barra (janela de parâmetros de dimensionamento, `DesignMemberParams`) e nos padrões do projeto (`DEFAULTS`):

| Parâmetro | Significado | Padrão |
|---|---|---|
| `compositeSpacing` | distância `ℓ` entre ligações (mm); 0 = duas intermediárias (`ℓ = L/3`) | 0 |
| `compositeWelded` | ligações intermediárias soldadas ou parafusos protendidos (5.3.6.3-b) | falso |

São gravados em colunas posicionais adicionais das linhas `DEFAULTS` e de barra do `.dat` (arquivos antigos continuam válidos).

## 7. Hipóteses e limitações

* Os dois perfis são **iguais** e simétricos em relação ao eixo de ligação; o afastamento é constante.
* As chapas espaçadoras são consideradas rígidas para o cortante transversal do conjunto, **exceto** pela redução `Ne,m` da 5.3.6.3; a resistência das
  ligações (parafusos/solda das chapas espaçadoras) **não** é calculada aqui — requer o esforço transversal de 5.3.6.2 e é tarefa do catálogo de ligações.
* `Cw` da cantoneira dupla é aproximado por `b³t³/9` (paredes finas, rotação em torno da linha média das abas).
* Flexão de duas barras U: sem verificação (a norma não a cobre).
* A flexão em torno do eixo X (junto ao afastamento) usa `Ix` do conjunto e supõe ação conjunta total; a norma exige as ligações do 5.3.6.2.

---

# Parte B — Torção com empenamento

## 8. Teoria de Vlasov: o que o solver passa a calcular

Em seção aberta de paredes finas o momento de torção `T` divide-se em duas parcelas:

```text
T = T_sv + T_w            T_sv = G J θ'               (Saint-Venant: tensões de cisalhamento "circulatórias")
                          T_w  = − E Cw θ'''          (empenamento: cisalhamento nas paredes devido ao gradiente do bimomento)
B = − E Cw θ''            (bimomento)               σ_w = B ω / Cw      (tensão normal de empenamento; ω = coordenada setorial)
E Cw θ'''' − G J θ'' = m(x)                           (equação diferencial da torção; m = torque distribuído)
```

Antes, o solver só tinha `T = G J θ'` (Saint-Venant puro, o que equivale a `Cw = 0`). Com isso, o programa **não** via o bimomento nem o torque
de empenamento, e a verificação de seções abertas sob torção era omitida com aviso (Seção 8.1 do livro principal). Para seções I/H curtas ou
com empenamento impedido essa omissão é **contra a segurança**: o engaste impede o empenamento e a mesa passa a fletir lateralmente.

Convenções (face positiva, eixos locais `x` ao longo da barra, `y`, `z`, torção positiva = rotação positiva em torno de `x`):

```text
θ' = ψ   taxa de torção ("parâmetro de empenamento")
ω = − y z   (seção I: ω_máx = h b_f / 4 na extremidade da mesa, h = d − t_f)
u = − ψ ω   deslocamento axial de empenamento
```

## 9. O elemento de barra com sétimo grau de liberdade

Cada extremidade de uma barra com empenamento ganha o grau de liberdade `ψ = θ'`, além dos seis habituais. Os graus de liberdade de empenamento
ficam **depois** dos `6 × nós` graus de liberdade (`índice = 6 nós + k`) e só existem nos nós de barras que pedem empenamento — por isso não
alteram a numeração de cascas, ligações (*ties*) e molas.

### 9.1 Rigidez exata

A solução geral da equação homogênea é `θ = a₀ + a₁ x + a₂ e^{−kx} + a₃ e^{−k(L−x)}` com `k = √(G J / E Cw)`. A forma com exponenciais
decrescentes é numericamente estável para `kL` grande. Impondo `θ_i, ψ_i, θ_j, ψ_j` e calculando as forças generalizadas

```text
F = ( −T(0) ,  −E Cw θ''(0) = +B(0) ,  T(L) ,  E Cw θ''(L) = −B(L) )
```

obtém-se a matriz de rigidez **exata** `K_t` 4 × 4 (simétrica) nos graus de liberdade `(θ_i, ψ_i, θ_j, ψ_j)` (`Warping::stiffness`). Ela
substitui os termos `G J/L` do bloco de torção: a matriz local passa de 12 × 12 para **14 × 14**, com `θ` nas posições 3 e 9 e `ψ` nas
posições 12 e 13. As demais linhas (flexão, axial, cortante de Timoshenko, rigidez geométrica, zonas rígidas, liberações, excentricidades)
permanecem como estavam; as liberações de extremidade condensam apenas os graus de liberdade originais (o empenamento nunca é "liberado" por
condensação — a condição de extremidade é dada pelo modo da Seção 10).

### 9.2 Cargas

Torque distribuído uniforme `m` (momento distribuído local `mx`): a solução particular é `θ_p = − m x²/(2 G J)`; as forças de engastamento
perfeito nos quatro graus de liberdade saem de `Warping::uniformTorqueFixedEnd`. Os torques de extremidade valem `m L/2` (como em Saint-Venant) e há
um par de **bimomentos de engastamento** nos graus `ψ` (`± E Cw m/(G J)` menos a parcela de rigidez). As cargas das demais naturezas (forças,
momentos de flexão) não têm componente em `ψ`.

## 10. Condições de contorno do empenamento

A cada extremidade, em cada nó, o parâmetro `ψ` recebe uma de três condições (por barra e extremidade, via `structura_set_warping_members`):

| Modo | Significado | Quando se aplica (modo 0 = automático) |
|---|---|---|
| 0 — automático | impedido (`ψ = 0`) em nó **totalmente engastado**; **compartilhado** entre barras colineares que chegam ao nó (continuidade do empenamento); **livre** nos demais | padrão do aplicativo |
| 1 — livre | `B = 0` (empenamento livre: apoio de garfo) | — |
| 2 — impedido | `ψ = 0` | — |

Em nó com barras **colineares** a continuidade do empenamento é natural: `ψ` e `B` passam de uma barra à outra (verificado em teste). Em nó
de **ligação entre barras não colineares** (viga–pilar) o empenamento de uma barra não se transmite à outra pelo simples encontro dos eixos;
a condição "livre" (`B = 0`) é a adotada, que **não subestima** os esforços de empenamento no ponto de fixação, mas pode superestimá-los quando
a ligação é de fato rígida à torção das mesas (enrijecedores, solda de continuidade). Para esse caso o modo 2 (impedido) está disponível na DLL.

## 11. Cargas e resultados

Resultado por barra (`structura_get_warping`): taxa de torção `ψ_i`, `ψ_j` e forças generalizadas, que são os bimomentos aplicados à barra
(`+B` em I, `−B` em J). O aplicativo reconstrói, ao longo da barra, a solução fechada (estável) **a partir dos valores das extremidades**:

```text
ψ(x)   = T(x)/(G J) + h(x),   T(x) = T₁ − m x,   h(x) = a e^{−kx} + b e^{−k(L−x)}
a, b   : de h(0) = ψ_i − T₁/(G J)   e   h(L) = ψ_j − T(L)/(G J)       (2 × 2, bem condicionado)
B(x)   = − E Cw ψ'(x) = − E Cw [ − m/(G J) + k (− a e^{−kx} + b e^{−k(L−x)}) ]
T_w(x) = − G J h(x)          T_sv(x) = T(x) − T_w(x)
```

Os esforços de projeto (`Effects`) ganham `B` (N·mm²) e `T_w` (N·mm) em cada uma das 21 seções de cada combinação; os resultados e os
parâmetros de empenamento são **gravados no `.dat`** (colunas adicionais da linha `M`) e entram na impressão digital dos resultados.

## 12. Verificação de seções I/H (NBR 8800, 5.5.2.3)

A 5.5.2.3 manda comparar **tensões** obtidas pela Teoria da Elasticidade, com as combinações de cálculo, às resistentes:

```text
(a) σ_Sd ≤ f_y/γ_a1                  (b) τ_Sd ≤ 0,60 f_y/γ_a1
(c) σ_Sd ≤ χ f_y/γ_a1                (d) τ_Sd ≤ 0,60 χ f_y/γ_a1
```

### 12.1 Tensão normal

Nos quatro cantos da seção (extremidades das mesas), nos eixos locais (`y` = altura, `z` = largura; o `M_x` de projeto é o `M_z` local e o
`M_y` de projeto é o `− M_y` local):

```text
σ = N/A − M_x y/I_x − M_y z/I_y + B ω/C_w          ω = − sy sz h b_f/4  (sy, sz = ±1 conforme o canto)
```

A tração máxima é comparada a `f_y/γ_a1` (**a**) e a compressão máxima a `χ f_y/γ_a1` (**c**). Os momentos usados são os **amplificados** do
dimensionamento (Anexo C: `M_Sd = B1 M_nt + B2 M_lt`) e `N` é o `N_Sd` correspondente. O resultado é o critério `warping_normal`.

**Hipótese sobre χ (declarada).** A norma diz que `χ` é o da 5.3.3 com `λ₀ = √(f_y/σ_e)` e `σ_e` a tensão crítica normal do estado-limite de
instabilidade em questão. O programa adota como `χ` o **menor fator** entre os estados-limite de instabilidade global e local já avaliados por
5.3 e 5.4 para a mesma seção: `χ_axial = N_c,Rd/(A f_y/γ_a1)` (se `N < 0`), `χ_x = M_x,Rd/(W_x f_y/γ_a1)` e `χ_y = M_y,Rd/(W_y f_y/γ_a1)` (se há
momento no eixo). É uma envoltória conservadora, pois uma só tensão normal é comparada ao pior dos modos.

### 12.2 Tensão de cisalhamento

Somam-se **em módulo** (sentidos relativos desconhecidos, envoltória conservadora):

```text
mesa :  τ = T_sv t_f/J  +  T_w h b_f²/(16 C_w)  +  V_z b_f²/(8 I_y)          ≤ 0,60 f_y/γ_a1                       (b)
alma :  τ = T_sv t_w/J  +  V_y Q/(I_x t_w)                                     ≤ 0,60 χ_v f_y/γ_a1,  χ_v = V_Rd/V_pl   (d)
Q = b_f t_f (d − t_f)/2 + t_w (d/2 − t_f)²/2             (momento estático da meia seção)
```

`T_w h b_f²/(16 C_w)` é o cisalhamento de empenamento no encontro mesa–alma (momento estático setorial `S_ω = t_f h b_f²/16`). O resultado é o
critério `warping_shear`. `χ_v` é a razão entre a resistência ao cortante da alma (5.4.3, que já contém a flambagem por cisalhamento) e a de
escoamento.

### 12.3 Seções abertas quaisquer: a linha média e o motor setorial

A verificação vale para **todas as seções abertas de paredes finas** com linha média conhecida: I/H, U laminado, T, L e os formados a frio U, C (enrijecido),
Z, L e cartola. A linha média (segmentos retos com espessura, em forma de árvore) é montada em `centerLineOf` (`DesignSection.cpp`) na orientação
canônica e reorientada pela **reflexão que reproduz o produto de inércia e a posição do centroide da seção desenhada**; as inércias servem apenas de
verificação grosseira (a linha média não reproduz raios de concordância nem mesas inclinadas). O motor `computeSectorial` (`Sectorial.cpp`) calcula, nos eixos
locais (`y` = altura, `z` = −largura):

```text
ω₀(s)  = ∫ (y dz − z dy)            polo no centroide, ao longo da árvore (CCW de y para z positivo)
z_s, y_s : de ∫ω_S y dA = ∫ω_S z dA = 0, ω_S = ω₀ − y_s z + z_s y     (resolve também com pequeno produto de inércia)
ω_n    = ω_S − (∫ω_S dA)/A          coordenada setorial principal normalizada;     Cw = ∫ ω_n² dA
S_y, S_z, S_ω = ∫ y dA, ∫ z dA, ∫ ω_n dA  da parte da seção além de cada ponto (momentos estáticos que dão o fluxo de cisalhamento)
```

A tensão normal em **todos os pontos de amostragem** (9 por segmento) e as de cisalhamento valem

```text
σ = N/A − M_x y/I_x − M_y z/I_y + B ω_n / Cw
τ = |T_sv| t/J  +  |V_y| |S_y|/(I_x t) + |V_z| |S_z|/(I_y t)  +  |T_w| |S_ω|/(Cw t)          (soma em módulo)
```

Para o I essas expressões reduzem-se às de 12.1 e 12.2 (testado: `ω = y z` nas mesas, `ω_máx = h b_f/4`, `Cw = t_f h² b_f³/24`). No U o centro de cisalhamento
sai fora da alma, à distância `e₀ = 3 b² t_f/(6 b t_f + h t_w)` da linha média (conferido). O `Cw` da linha média é o adotado também pelo solver
(`AnalysisService`), de modo que `B` e `ω_n/Cw` são coerentes.

**Seções maciças** (barra redonda e retangular): `τ_T = 16 T/(π D³)` (redonda) e `τ_T = T/(α h b²)` (retangular; α de Timoshenko interpolado em h/b),
mais `τ_V = 4/3 V/A` (redonda) ou `3/2 V/A` (retangular); `τ ≤ 0,60 f_y/γ` (critério `torsion`).

**Formados a frio (NBR 14762):** a norma não traz verificação de torção; aplica-se o mesmo critério de tensões elásticas, com `γ` da flexão (9.8.2) em
`f_y/γ`. É uma extensão do critério da NBR 8800, declarada como tal no memorial.

**Sem empenamento** (opção desligada): a barra é verificada só por Saint-Venant (`T_sv = T`) e o programa **avisa** que as tensões de empenamento
foram desprezadas — em perfis abertos com empenamento restringido isso é contra a segurança. Ligada, o solver recebe o `Cw` da linha média, exceto nas pernas concorrentes
(L, T, cantoneira formada), cujo `Cw` é desprezível. Tubulares seguem em 5.5.2.1/5.5.2.2 (Capítulo 8 do livro principal); barras compostas não são tratadas aqui.

## 13. Como ativar e como interpretar

* **Ativar:** janela *Parâmetros de dimensionamento*, grupo "Efeitos de segunda ordem e serviço" → "Torção com empenamento (Vlasov) nas barras de seção I/H". Refaz a análise.
  No `.dat`: linha `WARPING_TORSION 1`. Quando a opção está ligada e a seção da barra é I/H, o solver recebe `Cw` da seção
  (`DesignSection::Cw` convertido para as unidades do modelo) e o modo automático de contorno.
* **Interpretar:** nos resultados da barra aparecem `warping_normal` (σ máximo e χ) e `warping_shear` (τ da mesa e da alma), cada um com as
  parcelas de `T_sv`, `T_w`, `B`. A tabela de combinações indica a seção e a combinação governantes.
* **Não combina com** flambagem elástica linear nem análise modal: nessas execuções o solver usa Saint-Venant e emite aviso.

## 14. Validação numérica

Os testes automatizados (`TestProjectIoAndSolver`, `TestDesign`) comparam o solver com a solução fechada de Vlasov, balanço em I (`E = 2×10¹¹`,
`G = 8×10¹⁰`, `J = 10⁻⁵ m⁴`, `Cw = 10⁻⁶ m⁶`, `L = 2 m`, `k = 2 m⁻¹`):

| Caso | Fechada | Solver | Erro relativo |
|---|---|---|---|
| torque `T` na ponta: `θ(L) = T/(GJ) [L − tanh(kL)/k]` | `T/(GJ)[L − tanh(kL)/k]` | igual | < 10⁻⁹ |
| mesmo caso: bimomento no engaste | `T tanh(kL)/k` | igual | < 10⁻⁹ |
| torque uniforme `m`: `θ(L)` e `B(0)` da equação `E Cw ψ'' − G J ψ = −m (L−x)` | integração analítica | igual | < 10⁻⁸ |
| balanço dividido em 2 barras colineares | igual ao de 1 barra | continuidade de `ψ` e de `B` no nó | < 10⁻⁹ |
| fim a fim (balanço W 300 × 150 × 8 × 12, `L = 3 m`, `T = 5 kN·m`): `B(0)`, `T_w(0) = T` (engaste: `T_sv = 0`), `σ_w = B ω/Cw` | fórmulas acima | igual | < 10⁻⁶ |

Também há verificação do elemento isolado (`Warping::stiffness` simétrica; soma dos torques de engastamento `= −mL`). Sem empenamento o
mesmo modelo reproduz Saint-Venant (`θ = T L/(G J)`).

## 15. Hipóteses e limitações

* **Seções não duplamente simétricas** (U, Z, C, cartola): o solver não modela o **acoplamento flexo-torcional** (centro de cisalhamento fora do centroide) nem a
  excentricidade das cargas transversais em relação ao centro de cisalhamento: o torque deve ser informado pelo usuário (momento em torno de `x`). As tensões de
  empenamento e de cisalhamento usam a linha média (paredes finas): erro típico de poucos por cento em perfis laminados de mesas espessas ou inclinadas.
* **Sem efeito de Wagner** nem acoplamento flexo-torcional de segunda ordem: a rigidez geométrica não considera o empenamento. A flambagem
  por torção/flexo-torção do **elemento** continua tratada pelas fórmulas de `N_e` e `M_cr` (5.3 e Anexo D).
* **Empenamento nas ligações viga–pilar**: modo automático (livre) por não haver continuidade dos eixos; ver Seção 10.
* **Cargas aplicadas fora do centro de cisalhamento** geram torque só se o usuário aplicar o momento distribuído/concentrado em torno de `x` (como
  já era na Saint-Venant). Excentricidades de ligação são tratadas pelo recurso de excentricidade da seção.
* **Cisalhamento do bimomento** e **bimomento em trechos com variação de seção**: não há seção variável.
* **Instabilidade (c)/(d):** `χ` por envoltória (Seção 12.1), não por análise de flambagem da seção sob o estado de tensão combinado.
* **Zonas rígidas e deslocamento da seção:** os termos de empenamento atuam sobre a parte flexível; o empenamento não é transportado pelas zonas rígidas.
* A modal e a flambagem linear com empenamento **não estão implementadas** (aviso).

## 16. Mapa do código

| Assunto | Local |
|---|---|
| Rigidez exata, cargas e avaliação de `θ`, `ψ`, `B`, `T_sv`, `T_w` | `Structuradll/src/warping.cpp`, `headers/warping.h` |
| Matriz 14 × 14, condensação de liberações, esforços internos | `Structuradll/src/member.cpp` (`hasWarping`, `getFullLocalStiffnessMatrix`, `calculateInternalForces`) |
| Atribuição dos graus de liberdade de empenamento, montagem | `Structuradll/src/core/solver_core.cpp` (`runPass`) |
| Exportações da DLL | `structura_set_warping_sections`, `structura_set_warping_members`, `structura_get_warping` (`interface_dll.h`) |
| Ponte com o aplicativo | `src/app/SolverBridge.*` (`SectionData::warpingConstant`, `MemberData::warping`, `MemberResult::warping*`) |
| Opção do projeto e persistência | `DesignSettings::warpingTorsion`, `DatModelIoService`, `ResultsPersistence` |
| `B` e `T_w` por seção | `DesignService::collectEffects` |
| Verificação 5.5.2.3 | `src/design/BarDesign.cpp` (`warping_normal`, `warping_shear`) |
| Barras compostas | `SectionShapes`, `SectionCatalog`, `DesignSection`, `Nbr8800.cpp` (5.3.6, D.2.4), `BarDesign.cpp` (`composite`) |
