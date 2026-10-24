# Dimensionamento de estruturas de aço com perfis laminados, soldados e tubulares

### Como o Structura calcula: métodos, fórmulas, marchas de cálculo e verificações (ABNT NBR 8800:2024)

> Documento técnico-didático do programa **Structura**. Descreve, passo a passo, tudo o que o módulo de
> dimensionamento faz com barras de aço **laminadas** (W, I, U, L, T), **tubulares** (circular, quadrado e retangular),
> **barras maciças**, além das **ligações**, da **base de pilar** e das **vigas e lajes mistas**.
> O companheiro deste texto é o livro de **perfis formados a frio** (`LIVRO_DIMENSIONAMENTO_PERFIS_FORMADOS_A_FRIO.md`),
> que trata da ABNT NBR 14762:2010.
>
> Todas as fórmulas aqui descritas **são as que estão implementadas** no código (`src/design`, `src/app/services`).
> Quando o programa adota uma simplificação, ela é declarada no ponto em que aparece e reunida no Capítulo 15.
> Os exemplos numéricos foram obtidos executando o próprio módulo de cálculo (os valores impressos são os do programa).

---

## Sumário

1. Visão geral: do solver ao veredito de cada barra
2. Propriedades geométricas da seção
3. Material, coeficientes de ponderação e classificação dos elementos
4. Barras tracionadas (NBR 8800, 5.2)
5. Barras comprimidas (NBR 8800, 5.3)
6. Barras fletidas (NBR 8800, 5.4, Anexos D e E)
7. Força cortante (NBR 8800, 5.4.3)
8. Torção em tubulares e combinação de esforços (NBR 8800, 5.5)
9. Efeitos de segunda ordem: B1, B2, análise de 2ª ordem e forças nocionais
10. Estados-limite de serviço: flechas e deslocamentos de andar
11. Dimensionamento automático (escolha do perfil mais leve)
12. Exemplos resolvidos de barras
13. Ligações, apoios e base de pilar
14. Vigas mistas e lajes mistas com forma de aço incorporada
15. Hipóteses, limitações e pontos de atenção
16. Apêndices (símbolos, identificadores de verificação, mapa do código)

---

## Capítulo 1 — Visão geral: do solver ao veredito de cada barra

### 1.1 Unidades e convenções

| Grandeza | Unidade interna do módulo `src/design` |
|---|---|
| comprimento | mm |
| força | N |
| momento | N·mm |
| tensão | MPa (N/mm²) |

O modelo estrutural do programa trabalha em kN e m (ou N e m, conforme o módulo de elasticidade do material atribuído).
A camada de serviço `DesignService` deduz o fator de conversão do próprio módulo de elasticidade:

```text
E ≥ 1·10¹⁰   →  material em Pa:      força em N   (fator 1)
E < 1·10¹⁰   →  material em kN/m²:   força em kN  (fator 1000)
comprimento: sempre em m (fator 1000 para mm)
```

**Sinais dos esforços** (convenção do módulo):

```text
N  > 0   tração                       N < 0   compressão
Mx > 0   comprime a fibra superior (+b da seção; eixo local y do solver)
My > 0   comprime a fibra +a (−z do solver)
Vy       cortante paralelo à altura da seção (resistido pela alma)
Vz       cortante paralelo à largura (resistido pelas mesas)
T        momento de torção
```

Eixos da seção: `a` horizontal (largura), `b` vertical (altura); **X** é o eixo horizontal (flexão no plano da altura,
maior inércia nos perfis I) e **Y** o vertical. Para a cantoneira, a seção é expressa nos **eixos principais**, que são
os mesmos eixos locais usados pelo solver.

### 1.2 Esforços ao longo da barra

O solver fornece, por combinação, os esforços de extremidade de cada barra (forças locais nos dois nós) e as cargas
distribuídas que atuam nela. O `DesignService::buildElements` reconstrói a barra (eixos locais x, y, z, esforços nas
extremidades, cargas uniformes em eixos locais ou globais) e o `UniformLoadForceEvaluator` — o mesmo avaliador usado
nos diagramas de esforços — calcula os esforços internos em **21 seções igualmente espaçadas** (`x = L·i/20`, i = 0…20):

```text
N(x), Vy(x), Vz(x), T(x), Mx(x) = Mz_solver(x), My(x) = −My_solver(x)
```

Em cada seção os valores são convertidos para N e N·mm (fator de força × fator de comprimento para momentos).
A resposta de cada **combinação** (ou caso) é armazenada em uma estrutura `ComboEffects` com o nome da combinação, as
21 seções e duas informações que alimentam o Anexo C: se há **cargas transversais** no plano de flexão em torno de X e
em torno de Y (`transverseLoadX`, `transverseLoadY`).

### 1.3 Quais combinações são verificadas

* Por padrão, as combinações **últimas** são todas aquelas cujo nome **não** começa por `ELS`.
  O usuário pode escolher outro subconjunto em *Parâmetros de dimensionamento*.
* Combinações cujo nome começa por `ELS` são de **serviço**: não entram nas verificações de resistência; alimentam as
  flechas (Capítulo 10).
* Com as **forças nocionais** ligadas (Capítulo 9), cada combinação última gera quatro variantes
  (`ELU1 +NX`, `ELU1 −NX`, `ELU1 +NY`, `ELU1 −NY`). Elas seguem a seleção da combinação de origem.

#### Geração automática das combinações (NBR 8681:2003, NBR 8800:2024 Tabelas 1 e 2)

O gerador de combinações (Carregamentos › Gerar combinações) classifica cada caso de carga e propõe os coeficientes iniciais:

| Tipo de ação | γ desfavorável | γ favorável | ψ0 | ψ1 | ψ2 |
|---|---|---|---|---|---|
| Peso próprio de estrutura metálica | 1,25 | 1,0 | — | — | — |
| Demais permanentes | 1,35 | 1,0 | — | — | — |
| Variável (sobrecarga de uso) | 1,5 | 0,0 | 0,5 | 0,4 | 0,3 |
| Vento | 1,4 | 0,0 | 0,6 | 0,3 | 0,0 |
| Temperatura | 1,2 | 0,0 | 0,6 | 0,5 | 0,3 |

(O usuário deve conferir os valores — por exemplo, ψ0 da sobrecarga depende do tipo de ocupação.) As combinações últimas
normais são

```text
ELU = Σ γg,i Fg,i + γq1 Fq1 + Σ γq,j ψ0,j Fq,j
```

com **cada ação variável como principal por vez**, **ventos mutuamente exclusivos** (um vento por combinação) e uma família
adicional com permanentes **favoráveis** quando há vento (sucção/arrancamento). As combinações de serviço seguem
`ELS-QP` (quase permanente: permanentes + Σψ2 Fq), `ELS-CF` (frequente: ψ1 na principal, ψ2 nas demais) e, se pedido, `ELS-CR` (rara).
Os nomes começam por `ELU` ou `ELS`, que é como o dimensionamento separa resistência de serviço.

### 1.4 O algoritmo `designBar` (visão geral)

Para cada barra, o programa executa — nesta ordem — o seguinte:

```text
1. Monta a seção de projeto (DesignSection) a partir do perfil atribuído; escolhe a norma (8800 ou 14762).
2. Calcula as resistências que NÃO dependem dos esforços:
      NtRd (tração) e NcRd (compressão), com os comprimentos de flambagem Lx, Ly, Lz.
3. Para cada combinação e para cada uma das 21 seções:
      3.1  amplifica os momentos por B1 (se N < 0) e por B2
      3.2  tração  OU  compressão (+ limite de esbeltez)
      3.3  flexão em X e em Y   (MRd depende de Lb, Cb e do lado comprimido)
      3.4  cortante em Y e em Z
      3.5  interação normal + flexão (5.5.1.2)
      3.6  torção (tubulares) e interações com torção
4. Para cada verificação (identificador `id`) guarda a PIOR ocorrência (maior η) e a combinação/seção em que ocorre.
5. A razão máxima η = max(η_i) é o aproveitamento da barra; o estado-limite governante, a combinação e a posição x
   que o produzem são registrados.
```

O aproveitamento η é **demanda/resistência** (η > 1 não atende). A tabela de verificações é ordenada por η decrescente.

#### Comprimentos de flambagem e comprimento destravado

```text
Lx = Kx·L   (ou o valor informado)          flambagem por flexão em torno de X
Ly = Ky·L   (ou o valor informado)          flambagem por flexão em torno de Y
Lz = Kz·L   (ou o valor informado)          flambagem por torção
Lb = 0                se a mesa comprimida está contida continuamente
Lb = valor informado  ou  L / n_trechos     comprimento destravado (n_trechos ≥ 1 trechos iguais)
```

Os parâmetros K, o número de trechos, `Lb`, a opção "Cb = 1" e as razões `An/Ag` e `B2` são **por barra**
(`[MEMBER_DESIGN]` do arquivo `.dat`), com valores padrão do projeto.

#### Fator de modificação Cb (NBR 8800, 5.4.2.3-a)

Calculado **por trecho destravado** `k` (de `x₀ = kL/n` a `x₁ = (k+1)L/n`), a partir dos momentos nas seções:

```text
Mmax = máx(|Mx| nas seções do trecho, |Mx(x₀)|, |Mx(x₁)|)
MA = |Mx|(x₀ + 0,25 Δ)     MB = |Mx|(x₀ + 0,50 Δ)     MC = |Mx|(x₀ + 0,75 Δ)       Δ = x₁ − x₀
Cb = 12,5 Mmax / (2,5 Mmax + 3 MA + 4 MB + 3 MC)  ≤ 3,0           (Rm = 1,0)
```

MA, MB e MC são **interpolados linearmente** entre as 21 seções. Se `Lb = 0`, o FLT não se aplica; se "Cb = 1" estiver
marcado (balanços etc.), Cb = 1,0.

#### Coeficiente Cm e amplificação local B1 (Anexo C, resumo; detalhes no Capítulo 9)

```text
Cm = 1,0                          se há carga transversal no plano de flexão
Cm = máx(0,40 ; 0,60 − 0,40·M1/M2)  caso contrário (M1/M2 > 0 em curvatura reversa)
B1 = máx(1,0 ; Cm / (1 − N_c,Sd / N_e))     N_e = π² E I / L²  (K = 1, no plano de flexão)
```

Se `N_c,Sd ≥ N_e`, B1 é tomado como 10⁶ (a barra não atende). Os momentos de cálculo são
`M_Sd = M · B1 · B2_barra · B2_combinação`.

---

## Capítulo 2 — Propriedades geométricas da seção

### 2.1 Contorno e integração

O perfil é descrito por contornos poligonais (mm): o primeiro contorno é o externo; os demais são furos
(tubulares). Para cada polígono o programa integra, pelo teorema de Green, em torno de um ponto qualquer:

```text
A    = ½ Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)
S_a  = (1/6) Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)(xᵢ + xᵢ₊₁)
S_b  = (1/6) Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)(yᵢ + yᵢ₊₁)
I_aa = (1/12) Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)(xᵢ² + xᵢ xᵢ₊₁ + xᵢ₊₁²)
I_bb = (1/12) Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)(yᵢ² + yᵢ yᵢ₊₁ + yᵢ₊₁²)
I_ab = (1/24) Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)(xᵢ yᵢ₊₁ + 2 xᵢ yᵢ + 2 xᵢ₊₁ yᵢ₊₁ + xᵢ₊₁ yᵢ)
```

Os furos entram com sinal negativo. A origem é transladada para o centroide (`a_c = S_a/A`, `b_c = S_b/A`) e as inércias
são levadas ao centroide por Steiner:

```text
Ix  = ∫ b² dA          (eixo horizontal X — flexão no plano da altura)
Iy  = ∫ a² dA          (eixo vertical Y)
Ixy = ∫ a b dA
rx = √(Ix/A)    ry = √(Iy/A)
Wx,topo = Ix / b_máx     Wx,base = Ix / |b_mín|     Wx = mín(Wx,topo ; Wx,base)
Wy,dir  = Iy / a_máx     Wy,esq  = Iy / |a_mín|     Wy = mín(…)
```

### 2.2 Módulos plásticos

A **linha neutra plástica** (LNP) divide a seção em duas áreas iguais. O programa a encontra por **bissecção** na
coordenada (b para o eixo X, a para o eixo Y) recortando os polígonos (Sutherland–Hodgman) acima e abaixo da linha.
O módulo plástico é a soma dos momentos estáticos das duas partes em relação à LNP:

```text
Z = S_acima + S_abaixo      (cada S = ∫ |coordenada − LNP| dA)
```

`Zx` e `Zy` alimentam os momentos de plastificação `Mpl = Z·fy`.

### 2.3 Eixos principais (cantoneira)

```text
I1,2 = (Ix + Iy)/2 ± √[ ((Ix − Iy)/2)² + Ixy² ]
θ    = ½ atan2(−2 Ixy , Ix − Iy)
r_mín = √(I2 / A)
```

A seção da cantoneira é então **reexpressa nos eixos principais** (rotação do contorno em θ; centroide recalculado).
Se a barra tem giro de seção (`rotação` do posicionamento), o sinal do ângulo é escolhido para anular `Ixy`.

### 2.4 Constantes de torção, empenamento e centro de cisalhamento

| Perfil | Constante de torção J | Empenamento Cw | Centro de cisalhamento |
|---|---|---|---|
| I / H | `(2 bf tf³ + (d − tf) tw³)/3` | `Iy (d − tf)²/4` (D.2.8-a) | no centroide (x₀ = y₀ = 0) |
| U laminado | `(2 bf tf³ + (d − tf) tw³)/3` | `tf bfm³ hm²/12 · (3 bfm tf + 2 hm tw)/(6 bfm tf + hm tw)` com `bfm = bf − tw/2`, `hm = d − tf` | afastado da alma de `e₀ = 3 bfm² tf/(6 bfm tf + hm tw)` para fora |
| T | `(bf tf³ + (d − tf/2) tw³)/3` | `(tf bf)³/144 + ((d − tf/2) tw)³/36` | `y₀ =` linha média da mesa |
| L (abas iguais) | `2 (b − t/2) t³ / 3` | 0 | no encontro das linhas médias das abas |
| Tubular circular | `π (D⁴ − Di⁴)/32` | — | centroide |
| Tubular retangular | `4 Am² t / U` (Bredt), `Am = (b−t)(h−t)`, `U = 2[(b−t)+(h−t)]` | — | centroide |
| Barra circular | `π d⁴/32` | — | — |
| Barra retangular | `a b³ [1/3 − 0,21 (b/a)(1 − b⁴/(12 a⁴))]`, a ≥ b | — | — |

(J vem do módulo de perfis `SectionShapes::properties`; Cw e o centro de cisalhamento são calculados em
`makeDesignSection`.)

### 2.5 Dimensões características e elementos planos

`makeDesignSection` guarda também as dimensões paramétricas e monta a lista de **elementos planos** usada na flambagem
local da compressão (Seção 5.3.4 da norma, Tabela 4):

| Perfil | Altura da alma h | Elementos (largura b, espessura t, grupo da Tabela 4, quantidade) |
|---|---|---|
| I/H | `h = d − 2 tf − 2 r` (r = raio de concordância do catálogo; 0 se não informado) | mesa: `bf/2`, `tf`, grupo 4, ×4 · alma: `h`, `tw`, grupo 2, ×1 |
| U | `h = d − 2 tf − 2 r` | mesa: `bf`, `tf`, grupo 4, ×2 · alma: `h`, `tw`, grupo 2 |
| L | — | aba: `b`, `t`, grupo 3, ×2 |
| T | — | mesa: `bf/2`, `tf`, grupo 4, ×2 · alma: `d`, `tw`, grupo 6 |
| Tubular retangular/quadrado | `b_plano = b − 4t` (e `h − 4t`) | mesa e alma, grupo 1, ×2 cada |
| Tubular circular | `D/t` | (tratamento próprio, 5.3.4.3) |

---

## Capítulo 3 — Material, coeficientes e classificação dos elementos

### 3.1 Aço

```text
E = 200 000 MPa        G = 77 000 MPa        (NBR 8800)
fy, fu: do material atribuído à barra (biblioteca NBR 7007 / ASTM / ZAR) ou do aço informado na barra
```

O aço de cada barra é o do **material** a ela atribuído, a menos que a barra tenha aço próprio nos parâmetros de projeto.

### 3.2 Coeficientes de ponderação das resistências (NBR 8800, Tabela 3 — combinações normais)

```text
γa1 = 1,10   escoamento, flambagem e instabilidade
γa2 = 1,35   ruptura
γw2 = 1,35   solda (metal da solda)
```

### 3.3 Tensão residual

Nos estados-limite de flambagem de vigas (Anexo D), a tensão residual é `σr = 0,30 fy` (D.2.8-e).

### 3.4 Limites de esbeltez local — Tabela 4 da NBR 8800 (compressão axial)

Para cada elemento de largura `b` e espessura `t`:

| Grupo | Elemento | (b/t)lim |
|---|---|---|
| 1 | AA — paredes planas de tubular retangular e caixão | `1,40 √(E/fy)` |
| 2 | AA — almas de perfis I, H, U | `1,49 √(E/fy)` |
| 3 | AL — abas de cantoneira | `0,45 √(E/fy)` |
| 4 | AL — mesas de perfis I, H, U, T | `0,56 √(E/fy)` |
| 5 | AL — mesas de perfis I/H soldados | `0,64 √(E kc / fy)`, `kc = 4/√(h/tw)` com 0,35 ≤ kc ≤ 0,76 |
| 6 | AL — alma de perfil T | `0,75 √(E/fy)` |

(AA = elemento apoiado nas duas bordas; AL = apoiado numa borda e livre na outra.)

Coeficientes da largura efetiva (Tabela 5): `c1 = 0,18` e `c2 = 1,31` (AA); `c1 = 0,20`, `c2 = 1,38` (tubular retangular);
`c1 = 0,22`, `c2 = 1,49` (AL, grupos 3 em diante).

---

## Capítulo 4 — Barras tracionadas (NBR 8800, 5.2)

### 4.1 Estados-limite

O dimensionamento verifica dois estados-limite, e a resistência é o menor deles:

```text
Escoamento da seção bruta:       N_t,Rd(esc) = Ag fy / γa1
Ruptura da seção líquida efetiva: N_t,Rd(rup) = Ae fu / γa2          com   Ae = Ct · An
N_t,Rd = min[ N_t,Rd(esc) ; N_t,Rd(rup) ]
```

No programa, `An/Ag` (razão da seção líquida na região da ligação, por barra) e `Ct` (coeficiente de redução da área
líquida, 5.2.5) são parâmetros de projeto, de modo que:

```text
Ae = Ct · (An/Ag) · Ag
```

Com os valores padrão (`An/Ag = 1`, `Ct = 1`) a ruptura é calculada sobre a seção bruta (sem furos); furos e
efeitos da ligação só entram por esses dois parâmetros — **as ligações e furos não são desenhados na barra**.

### 4.2 Limite de esbeltez

```text
λ = L / r_mín ≤ 300      (recomendação, 5.2.8)
```

Ultrapassar 300 em barra tracionada **gera apenas um aviso** (a norma o trata como recomendação) e não reprova a barra.

### 4.3 Marcha de cálculo (exemplo)

Barra W 200 × 15,0, aço de fy = 345 MPa e fu = 450 MPa, N_t,Sd = 120 kN, `An/Ag = 0,85`, `Ct = 0,90`:

```text
Ag = 1942 mm²
Escoamento:  Ag fy/γa1 = 1942 × 345 / 1,10 = 609,1 kN
Ruptura:     Ae = 0,90 × 0,85 × 1942 = 1485,6 mm²
             Ae fu/γa2 = 1485,6 × 450 / 1,35 = 495,2 kN
N_t,Rd = min(609,1 ; 495,2) = 495,2 kN       η = 120 / 495,2 = 0,242
```

---

## Capítulo 5 — Barras comprimidas (NBR 8800, 5.3)

### 5.1 Sequência de cálculo

```text
1) Cargas de flambagem elástica   N_ex, N_ey, N_ez (e as de flexo-torção, quando couber)  →  N_e = menor valor
2) Índice de esbeltez reduzido    λ0 = √( Ag fy / N_e )
3) Fator de redução               χ(λ0)
4) Área efetiva                   Aef  (flambagem local — 5.3.4)
5) Resistência                    N_c,Rd = χ Aef fy / γa1
```

### 5.2 Cargas de flambagem elástica (Euler e torção)

```text
N_ex = π² E Ix / Lx²                         flexão em torno de X
N_ey = π² E Iy / Ly²                         flexão em torno de Y
N_ez = [ π² E Cw / Lz² + G J ] / r0²         torção (perfis abertos);  G J / r0²  se Lz = 0 e Cw = 0

r0² = rx² + ry² + x0² + y0²                  raio de giração polar em relação ao centro de cisalhamento
```

Um comprimento nulo (`L = 0`) desativa o modo correspondente (carga infinita). Se todos os modos são infinitos, o programa
adota `χ = 1` (sem flambagem global) e `N_c,Rd = Aef fy / γa1`.

### 5.3 Seções duplamente simétricas (I, H, tubulares, maciças)

`N_e = mín(N_ex ; N_ey ; N_ez)`. O modo governante é registrado na memória ("flexão em torno de Y", "torção" etc.).
Em I/H, `x₀ = y₀ = 0`, de modo que `r0² = rx² + ry²`.

### 5.4 Seções monossimétricas (U e T) — flexo-torção (5.3.5.2)

O eixo de simetria é X no perfil U (alma vertical) e Y no perfil T. A flexão em torno do **eixo de simetria** acopla-se à torção:

```text
ρ  = 1 − (d₀ / r0)²          d₀ = x₀ (perfil U)  ou  y₀ (perfil T)

N_ft = (N_sim + N_ez) / (2 ρ) · [ 1 − √( 1 − 4 N_sim N_ez ρ / (N_sim + N_ez)² ) ]

N_e = mín( N_outro ; N_ft )         N_sim: carga de Euler em torno do eixo de simetria;  N_outro: a do outro eixo
```

### 5.4b Barras compostas (5.3.6)

Dois perfis iguais (cantoneiras em T; U em I; U face a face) ligados a intervalos `ℓ`: `ℓ/r_mín ≤ 0,75 (L/r)₀` e ao menos duas ligações intermediárias
(critério `composite`); `Ne,m` no lugar de `Ney` conforme parafusos de aperto normal (5.3.6.3-a) ou solda/protensão (5.3.6.3-b, `Ki = 0,50 / 0,75 / 0,86`).
Detalhes, catálogo, flexão (D.2.4) e cortante em `BARRAS_COMPOSTAS_E_TORCAO_COM_EMPENAMENTO.md`, Parte A.

### 5.5 Seções assimétricas e cantoneira simples

**Cantoneira conectada por uma aba (5.3.5.4)**, quando atendidas as condições `b/t ≤ 0,71 √(E/fy)` e `L_eq/r ≤ 200`:

```text
r   = √( (I1 + I2)/2 / A )                  raio de giração em torno do eixo paralelo à aba
λx  = Lx / r
L_eq = 72 r + 0,75 Lx          se λx ≤ 80
L_eq = 32 r + 1,25 Lx          se λx > 80
N_e = π² E (I1 + I2)/2 / L_eq²
```

**Fora dessas condições** (ou perfis assimétricos em geral, 5.3.5.3): `N_e` é a **menor raiz positiva** do cúbico

```text
(N_e − N_ex)(N_e − N_ey)(N_e − N_ez) − N_e² (N_e − N_ey)(x0/r0)² − N_e² (N_e − N_ex)(y0/r0)² = 0
```

resolvido por bissecção entre 0 e o menor dos três valores.

### 5.6 Fator de redução χ (5.3.3)

```text
λ0 ≤ 1,5 :   χ = 0,658^(λ0²)
λ0 > 1,5 :   χ = 0,877 / λ0²
```

### 5.7 Área efetiva — flambagem local (5.3.4)

Para cada elemento plano da Tabela 4 (grupo, `b`, `t`):

```text
Se  b/t ≤ (b/t)lim / √χ :     b_ef = b           (elemento totalmente efetivo)

Senão:
   σ_el = [ c2 (b/t)lim / (b/t) ]² · fy                     tensão de flambagem local elástica
   b_ef = b (1 − c1 √(σ_el/(χ fy))) √(σ_el/(χ fy))          largura efetiva (5.3.4.2)
```

e a perda de área é `Σ n_elementos (b − b_ef) t`, de modo que `Aef = Ag − perda`.
`c1`, `c2` e `(b/t)lim` vêm da Seção 3.4. Se todos os elementos estão abaixo do limite, `Aef = Ag` (5.3.4.1).

**Tubular circular (5.3.4.3):**

```text
D/t ≤ 0,11 E/fy :   Aef = Ag
D/t >  0,11 E/fy :  Aef = [ 0,038 E/(fy D/t) + 2/3 ] Ag          (aviso se D/t > 0,45 E/fy)
```

### 5.8 Resistência e limite de esbeltez

```text
N_c,Rd = χ Aef fy / γa1
KL/r (máximo entre X e Y) ≤ 200       (5.3.7.1): este limite ENTRA no aproveitamento, η = KL/r / 200
                                         (pode ser desligado em "Parâmetros")
```

### 5.9 Marcha de cálculo (exemplo)

Pilar W 310 × 74,0 (H): `d = 310`, `bf = 205`, `tw = 9,4`, `tf = 16,3` mm; aço fy = 345 MPa; L = 4000 mm;
`Kx = Ky = Kz = 1`. Propriedades do programa: `Ag = 9377 mm²`, `Ix = 1,626·10⁸ mm⁴`, `Iy = 2,343·10⁷ mm⁴`,
`rx = 131,7 mm`, `ry = 50,0 mm`, `J = 6,732·10⁵ mm⁴`, `Cw = 5,052·10¹¹ mm⁶`.

```text
N_ex = π² · 200000 · 1,626·10⁸ / 4000² = 20 060 kN
N_ey = π² · 200000 · 2,343·10⁷ / 4000² =  2 890 kN
r0²  = 131,7² + 50,0² = 19 838 mm²
N_ez = [π² · 200000 · 5,052·10¹¹/4000² + 77000 · 6,732·10⁵] / 19 838 = 5 755 kN
N_e  = 2 890 kN  (flexão em torno de Y)

λ0 = √(9377 × 345 / 2 890 330) = 1,058         χ = 0,658^(1,058²) = 0,626
Elementos: mesa b/t = 102,5/16,3 = 6,3 ≤ 17,0; alma b/t = 277,4/9,4 = 29,5 ≤ 45,3  →  Aef = Ag
N_c,Rd = 0,626 × 9377 × 345 / 1,10 = 1 841 kN       η_N = 900/1841 = 0,489
KL/r = 4000/50,0 = 80,0 ≤ 200                        η = 0,400
```

(o valor `(b/t)lim/√χ` para a mesa é `0,56√(E/fy)/√0,626 = 13,5/0,791 = 17,0`; para a alma `1,49 √(E/fy)/√0,626 = 35,9/0,791 = 45,3`.)

---

## Capítulo 6 — Barras fletidas (NBR 8800, 5.4 e Anexos D e E)

### 6.1 Estrutura geral

O momento resistente de cálculo é o **menor** entre os estados-limite aplicáveis à seção e ao eixo de flexão:

```text
FLT  flambagem lateral com torção       (só em flexão em torno do eixo de maior inércia, com Lb > 0)
FLM  flambagem local da mesa
FLA  flambagem local da alma
(+ escoamento/plastificação, tubulares, maciças etc.)

M_Rd = mín(estados-limite)   e   M_Rd ≤ 1,5 W fy / γa1       (5.4.2.2: validade da análise elástica)
```

onde `W` é o menor módulo elástico da seção em relação ao eixo de flexão. O programa informa qual estado-limite governa.

### 6.2 Interpolação padrão (D.2.1 e D.2.2)

Para um parâmetro de esbeltez `λ`, com limites `λp` (compacta) e `λr` (semicompacta), momento de plastificação
`Mpl = Z fy`, momento de início de escoamento `Mr` e momento crítico elástico `Mcr`:

```text
λ ≤ λp :        M_Rd = Mpl / γa1
λp < λ ≤ λr :   M_Rd = [ Mpl − (Mpl − Mr)(λ − λp)/(λr − λp) ] / γa1
λ > λr :        M_Rd = Mcr / γa1                (FLT e FLM); no FLA de vigas I (não esbeltas) usa-se Mr/γa1
```

> **Conformidade com a NBR 8800:2024 (conferida):** na edição 2024 o `Cb` **não** multiplica o parêntese da interpolação; ele
> está embutido em `Mcr` e em `λr` (D.2.1 e Tabela D.1, `λr = 1,38 Cb √(Iy J)/(ry J β1)·√(1 + √(1 + 27 Cw β1²/(Cb² Iy)))`). O programa
> implementa exatamente esse formato — a ressalva "conservador" que constava nas edições anteriores deste livro **não procede**
> para a norma 2024 (ela só valeria em comparação com o formato `Cb[…] ≤ Mpl` da NBR 8800:2008).

### 6.2b Furos na mesa tracionada (5.4.2.6)

Se `fu Afn < Yt fy Afg` (`Yt = 1,0` para `fy/fu ≤ 0,8`, senão `1,10`), o momento resistente é limitado por
`M_Rd = (1/γa1)(fu Afn/Afg) Wt`, com `Wt` o módulo elástico do lado tracionado. O programa assume `Afn/Afg` igual à razão `An/Ag` informada nos parâmetros
da barra (campo "razão An/Ag"), e acrescenta a linha ao memorial quando o limite governa.

### 6.3 Seções I e H e U — flexão em torno do eixo de maior inércia (X)

Tabela D.1, com `σr = 0,30 fy`, `W = Wx` e `Mr = (fy − σr) W`:

**FLT** (somente se `Lb > 0`):

```text
λ  = Lb / ry                               λp = 1,76 √(E/fy)
β1 = (fy − σr) W / (E J)
λr = 1,38 Cb √(Iy J) / (ry J β1) · √( 1 + √( 1 + 27 Cw β1² / (Cb² Iy) ) )
Mcr = Cb π² E Iy / Lb² · √( Cw/Iy · (1 + 0,039 J Lb² / Cw) )          (D.2.8-a)
```

**FLM** (D.2.2 e D.2.8-e/f):

```text
λ  = bf/(2 tf)  (I, H)    ou   bf/tf  (U)          λp = 0,38 √(E/fy)       λr = 0,83 √(E/(fy − σr))
Mr = (fy − σr) W            Mcr = 0,69 E W / λ²
```

**FLA** (alma de seções não esbeltas):

```text
λ = h/tw           λp = 3,76 √(E/fy)       λr = 5,70 √(E/fy)       Mr = fy W
λ ≤ λp : Mpl/γa1       λp < λ ≤ λr : interpolação com Mr = fy W       (não há ramo λ > λr: ver alma esbelta)
```

### 6.4 Vigas de alma esbelta (Anexo E)

Se o perfil é I e `h/tw > 5,70 √(E/fy)`, aplica-se o Anexo E. Com `hc = h`, `afc = bf tf`:

```text
ar  = mín(10 ; hc tw / afc)
kpg = mín(1 ; 1 − ar/(1200 + 300 ar) · (hc/tw − 5,70 √(E/fy)))
Wxc: módulo elástico Wx          My = kpg fy Wxc       Mr = kpg (fy − σr) Wxc

Escoamento da mesa tracionada:  M_Rd = Wx fy / γa1                                            (E.2.1)
FLT (Lb > 0):   λ = Lb / ry,  λp = 1,10 √(E/fy),  λr = π √(E Cb/(fy − σr)),  Mcr = Cb kpg π² E Wxc / λ²   (E.2.2)
FLM:            λ = bf/(2 tf), λp = 0,38 √(E/fy), λr = 0,95 √(kc E/(fy − σr)),
                kc = 4/√(h/tw) (0,35 ≤ kc ≤ 0,76),  Mcr = 0,90 kpg E kc Wxc / λ²                   (E.2.3)
```

Usa-se a interpolação da Seção 6.2 com `My` e `Mr` acima no lugar de `Mpl` e `Mr`. Observação: `ry` é o raio de giração da
seção bruta (aproximação do raio da mesa comprimida mais 1/3 da alma).

### 6.5 Eixo de menor inércia (Y) em I, H e U

```text
FLM:  λ = bf/(2 tf) (I,H) ou bf/tf (U);  λp = 0,38 √(E/fy);  λr = 0,83 √(E/(fy − σr));
      Mr = (fy − σr) W;  Mcr = 0,69 E W/λ²
```

No perfil **U**, a FLM só se aplica quando as pontas livres das mesas estão comprimidas (a abertura do U do lado comprimido).
Quando é a alma que está comprimida usa-se a **FLA do U** (D.2.8-d, NBR 8800:2024), com a **largura efetiva** da alma:

```text
λ = h/tw      λp = 1,12 √(E/fy)      λr = 1,40 √(E/fy)
Alma (grupo 2 da Tabela 4, χ = 1, c1 = 0,18, c2 = 1,31):  (b/t)lim = 1,49 √(E/fy)
   se h/tw > (b/t)lim:  σel = [c2 (b/t)lim/(h/tw)]² fy ;  hef = h (1 − c1 √(σel/fy)) √(σel/fy)      (5.3.4.2)
   Wef = Ief/c  da seção com a alma reduzida de (h − hef) tw  (nova linha neutra);  Wef ≤ W
   Mr = fy Wef        Mcr = Wef² fy / W        e a interpolação do Mpl a Mr entre λp e λr
```

(Antes desta revisão o programa usava `Mr = fy W` sem `Wef` e sem o ramo `λ > λr`; corrigido.)

Se nenhum estado-limite de flambagem se aplica, `M_Rd = Mpl/γa1`.

### 6.6 Perfil T

* **Eixo vertical:** FLM das mesas (`λ = bf/(2 tf)`, `λp = 0,38√(E/fy)`, `λr = 0,83√(E/(fy − σr))`, `Mcr = 0,7 E W/λ²`).
* **Eixo horizontal:** depende de quem está comprimido.
  - *Mesa comprimida:* FLM com `Mr = mín[(fy − σr) Wc ; fy Wt]` e `Mcr = 0,7 E Wc/λ²`.
  - *Ponta livre da alma comprimida:* FLA da alma do T (D.2.3), com `λ = d/tw`, `λy = 0,84√(E/fy)`, `λr = 1,52√(E/fy)`:
    `λ ≤ λy: fy W/γ`; `λy < λ ≤ λr: (1,43 − 0,515 λ √(fy/E)) fy W/γ`; `λ > λr: 1,52 E W/(λ² γ)`.
  - *FLT* (se `Lb > 0`): `B = 2,3 (d/Lb) √(Iy/J)` e `Mcr,base = 1,95 (E/Lb) √(Iy J)`.
    Alma tracionada (mesa comprimida): `Mcr = Mcr,base (B + √(1 + B²))` com interpolação e
    `λr = 1,95 E/(ry fy) · √(Iy J)/W · √(2,36 fy d W/(E J) + 1)`;
    alma comprimida (D.2.3, NBR 8800:2024): `Mcr = Mcr,base(−B + √(1 + B²))`, `r = fy W/Mcr`;
    `r ≤ 1`: `M_Rd = mín[(1,92 − 1,17√r) fy W ; 1,5 fy W]/γ`;   `r > 1`: `M_Rd = (0,92 − 0,17/r) Mcr/γ`.
    (A versão anterior do programa usava `mín(Mcr ; fy W)/γ`, mais conservador e fora do texto da norma; corrigido.)

### 6.7 Tubulares

**Retangular/quadrado** (D.2.7) — parede comprimida `bw` (plana) e alma `hw` (plana), `λ = bw/t`:

```text
FLM:  λp = 1,12 √(E/fy)   λr = 1,40 √(E/fy)
      λ ≤ λp:  Mpl/γ
      λp < λ ≤ λr:  [Mpl − (Mpl − fy W)(3,57 λ √(fy/E) − 4,0)]/γ
      λ > λr:  W_ef fy/γ, com b_ef de 5.3.4.2 (χ = 1) e W_ef = W − perda·y²/y_máx
FLA:  λ = h_plano/t;  λp = 2,42 √(E/fy);  λr = 5,70 √(E/fy)   (λ > λr: não coberto — M_Rd = 0 e aviso)
FLT (eixo X, Ix > Iy, Lb > 0): λ = Lb/ry,  λp = 0,13 E √(J A)/Mpl,  λr = 2 E Cb √(J A)/Mr,  Mcr = 2 E Cb √(J A)/λ
```

**Circular** (D.2.6): `D/t ≤ 0,07 E/fy: Mpl/γ`; `≤ 0,31 E/fy: (0,021 E/(D/t) + fy) W/γ`; acima: `0,33 E W/((D/t) γ)`
(aviso se `D/t > 0,45 E/fy`).

**Barra circular maciça** (D.2.5): `M_Rd = Mpl/γ`. **Barra retangular**: em X com `Lb > 0`, FLT com
`λp = 0,13 E √(JA)/Mpl`, `λr = 2 E Cb √(JA)/Mr`, `Mcr = 2 E Cb √(JA)/λ`; senão `Mpl/γ`.

### 6.8 Cantoneira laminada

A NBR 8800:2024 **não traz** a cantoneira simples em flexão no Anexo D (só duas cantoneiras formando T, D.2.4). Para não deixar o
momento sem verificação, o programa aplica um critério **adaptado da AISC 360-22, F10** (eixos principais), identificado como tal
nas linhas do memorial:

```text
Escoamento (limite elástico):   M_Rd = fy W / γa1                       (W mínimo em torno do eixo principal)
FLM da aba:   λ = b/t ;  λp = 0,54 √(E/fy) ;  λr = 0,91 √(E/fy)
              λ ≤ λp: não se aplica ; λp < λ ≤ λr: M_Rd = fy W (2,43 − 1,72 λ √(fy/E))/γa1 ; λ > λr: 0,71 E W/(λ² γa1)
FLT (eixo principal maior, Lb > 0):   Me = 0,46 E b² t² Cb / Lb ;  r = fy W/Me
              r ≤ 1: M_Rd = mín[(1,92 − 1,17√r) fy W ; 1,5 fy W]/γa1 ;   r > 1: (0,92 − 0,17/r) Me/γa1
```

O menor dos valores governa (limitado por 5.4.2.2). Trata-se de **critério auxiliar**, fora do texto da NBR 8800.

### 6.9 Marcha de cálculo (exemplo completo)

Viga W 410 × 53,0 (`d = 403`, `bf = 177`, `tw = 7,5`, `tf = 10,9` mm), fy = 345 MPa, L = 6000 mm, biapoiada com
carga uniforme de cálculo `w = 30 kN/m` e **dois trechos destravados** (`Lb = 3000 mm`). Propriedades:
`Ag = 6804 mm²`, `Ix = 1,860·10⁸`, `Iy = 1,009·10⁷ mm⁴`, `Wx = 923 296 mm³`, `Zx = 1 045 280 mm³`, `ry = 38,51 mm`,
`J = 207 952 mm⁴`, `Cw = Iy (d − tf)²/4 = 3,878·10¹¹ mm⁶`, `h = d − 2 tf = 381,2 mm` (o catálogo não fornece raio).

```text
Mpl = Zx fy = 1 045 280 × 345 = 360,62 kN·m          σr = 0,30 × 345 = 103,5 MPa
Mr  = (fy − σr) Wx = 241,5 × 923 296 = 222,98 kN·m

Esforços:  Msd,máx = w L²/8 = 135 kN·m (x = 3000);   Vsd,máx = w L/2 = 90 kN (x = 0)

Cb do trecho de 0 a 3000 mm:  Mmax = 135;  MA(750) = 58,7;  MB(1500) = 101,25;  MC(2250) = 126,2   (kN·m)
Cb = 12,5·135 / (2,5·135 + 3·58,7 + 4·101,25 + 3·126,2) = 1687,5 / 1297,4 = 1,30

FLT:  λ = 3000/38,51 = 77,9;   λp = 1,76 √(200000/345) = 42,4
      β1 = 241,5 × 923 296 / (200000 × 207 952) = 5,361·10⁻³ mm⁻¹
      λr = 1,38·1,30 √(1,009·10⁷ × 207 952)/(38,51 × 207 952 × 5,361·10⁻³) · √(1 + √(1 + 27·3,878·10¹¹·β1²/(1,30²·1,009·10⁷))) = 139,6
      Mcr = 1,30 π² 200000 · 1,009·10⁷/3000² · √(3,878·10¹¹/1,009·10⁷ · (1 + 0,039·207 952·3000²/3,878·10¹¹)) = 615,2 kN·m
      λp < λ < λr:  M_Rd = [360,62 − (360,62 − 222,98)(77,9 − 42,4)/(139,6 − 42,4)]/1,10 = 282,1 kN·m
FLM:  λ = 88,5/10,9 = 8,1 ≤ λp = 0,38√(E/fy) = 9,1  →  M_Rd = 360,62/1,10 = 327,8 kN·m
FLA:  λ = 381,2/7,5 = 50,8 ≤ λp = 3,76√(E/fy) = 90,5  →  M_Rd = 327,8 kN·m
Limite 5.4.2.2:  1,5 Wx fy/γa1 = 434,4 kN·m   (não governa)

M_Rd = mín(282,1 ; 327,8 ; 327,8) = 282,1 kN·m  (FLT)           η = 135 / 282,1 = 0,479
```

(Os números foram reproduzidos pelo programa: `M_Rd = 282,13 kN·m`, `Cb = 1,30`, `Mcr = 615,18 kN·m`, `η = 0,4785`.)

---

## Capítulo 7 — Força cortante (NBR 8800, 5.4.3)

### 7.1 Almas de perfis I, H e U (5.4.3.1)

```text
Vpl = 0,60 Aw fy          Aw = d tw              (área efetiva de cisalhamento: altura TOTAL × espessura da alma)
λ  = h / tw
λp = 1,10 √(kv E/fy)      λr = 1,37 √(kv E/fy)      kv = 5,34   (alma sem enrijecedores transversais)

λ ≤ λp :          V_Rd = Vpl / γa1
λp < λ ≤ λr :     V_Rd = (λp/λ) Vpl / γa1
λ > λr :          V_Rd = 1,24 (λp/λ)² Vpl / γa1
```

(O programa não considera enrijecedores transversais: `kv = 5,34` fixo.)

### 7.2 Demais seções

| Seção | Tratamento |
|---|---|
| Mesas de I/H/U (cortante `Vz`) | mesma formulação com `h = bf/2` (I) ou `bf` (U), `tw → tf`, `Aw = 2 bf tf`, `kv = 1,2` (5.4.3.5) |
| T (alma) | `h = d`, `Aw = d tw`, `kv = 1,2` (5.4.3.3); mesas do T: `h = bf/2`, `Aw = bf tf`, `kv = 1,2` |
| Tubular retangular | `kv = 5,0`, `h = ` altura da parte plana da parede, `Aw = 2 h t` (5.4.3.2) |
| Tubular circular (5.4.3.6) | `τcr = mín[ máx(1,60 E/(√(Lv/D)(D/t)^1,25) ; 0,78 E/(D/t)^1,5) ; 0,60 fy ]`, `V_Rd = 0,5 τcr Ag / γa1`, com `Lv = L/2` |
| Barra maciça | `V_Rd = 0,60 fy/γa1 · A · 3/4` (τmáx = 4V/3A ≤ 0,60 fy/γa1) |
| Cantoneira | `V_Rd = √2 · (2/3) · 0,60 fy b t / γa1` (duas abas a 45° dos eixos principais) |

### 7.3 Marcha de cálculo

Viga W 410 × 53,0 (mesmos dados do exemplo do Capítulo 6): `Aw = d tw = 403 × 7,5 = 3022,5 mm²`,
`Vpl = 0,60 × 3022,5 × 345 = 625,7 kN`; `λ = 381,2/7,5 = 50,8 ≤ λp = 1,10 √(5,34 × 200000/345) = 61,2`:

```text
V_Rd = Vpl/γa1 = 625,7/1,10 = 568,8 kN        V_Sd = 90 kN      η = 0,158
```

---

## Capítulo 8 — Torção em tubulares e interação de esforços (NBR 8800, 5.5)

### 8.1 Torção (5.5.2.1) — somente tubulares

**Circular:**

```text
W_T = π (D − t)² t / 2
T_Rd = mín{ máx[ 1,23 W_T E / ((D/t)^1,25 √(L/D)) ; 0,60 W_T E/(D/t)^1,5 ] / γa1 ; 0,60 W_T fy / γa1 }
```

**Retangular:**

```text
W_T = 2 (b_mín − t)(h_máx − t) t − 4,5 (4 − π) t³
h/t = (maior largura plana)/t      lim1 = 2,45 √(E/fy)     lim2 = 3,07 √(E/fy)
h/t ≤ lim1 :        T_Rd = 0,60 W_T fy / γa1
lim1 < h/t ≤ lim2 : T_Rd = 0,60 W_T fy (lim1/(h/t)) / γa1
h/t > lim2 :        T_Rd = 0,46 π² W_T E / ((h/t)² γa1)
```

Em perfis **abertos** e maciços com momento de torção significativo (> 0,1 kN·m), o programa só verifica quando a torção com empenamento
está ativada **e** a seção é I/H duplamente simétrica (critérios `warping_normal` e `warping_shear`, NBR 8800, 5.5.2.3 — ver
`BARRAS_COMPOSTAS_E_TORCAO_COM_EMPENAMENTO.md`, Parte B). Nos demais casos (U, L, T e maciços) a torção **não é verificada** e o programa emite um aviso.

### 8.2 Efeito da torção nas interações (5.5.2.2)

```text
T_Sd/T_Rd ≤ 0,20 :   efeito da torção desprezado
T_Sd/T_Rd > 0,20 :   η = (N_Sd/N_Rd + M_Sd/M_Rd) + (V_Sd/V_Rd + T_Sd/T_Rd)²
```

### 8.3 Flexão composta (5.5.1.2) — interação normal + flexão

Com `N_Rd` = resistência à tração (se N > 0) ou à compressão (se N < 0), e `M_Sd` já amplificado:

```text
N_Sd / N_Rd ≥ 0,2 :   η = N_Sd/N_Rd + (8/9) (Mx,Sd/Mx,Rd + My,Sd/My,Rd)        (5.5.1.2-a)
N_Sd / N_Rd < 0,2 :   η = N_Sd/(2 N_Rd) + (Mx,Sd/Mx,Rd + My,Sd/My,Rd)           (5.5.1.2-b)
```

O resultado é registrado como `Flexo-compressão`, `Flexo-tração` ou `Flexão composta (momentos)` conforme o sinal de `N`.
A interação é avaliada em **cada uma das 21 seções** e em cada combinação; aparece na tabela com o `η` da pior ocorrência.

### 8.4 Marcha de cálculo (exemplo completo, pilar do Capítulo 5)

Pilar W 310 × 74,0, L = 4000 mm, `N_c,Sd = 900 kN`, momentos de extremidade `Mx(0) = +120 kN·m` e `Mx(L) = −60 kN·m`
(curvatura reversa, sem carga transversal), `Vy = 45 kN`, fy = 345 MPa:

```text
Cm = 0,60 − 0,40 M1/M2, com M1/M2 = +60/120 (reversa) = 0,5  →  Cm = 0,40
N_e,plano = π² E Ix / L² = 20 060 kN     B1 = máx(1 ; 0,40/(1 − 900/20 060)) = máx(1 ; 0,419) = 1,00
Mx,Sd = 120 × 1,00 = 120 kN·m (em x = 0)

Cb (Lb = 4000): Mmax = 120; MA(1000) = 75; MB(2000) = 30; MC(3000) = 15 →
   Cb = 12,5·120/(2,5·120 + 3·75 + 4·30 + 3·15) = 1500/690 = 2,17
FLT: λ = 4000/49,98 = 80,0, λp = 42,4, λr = 279,0, Mcr = 1248,7 kN·m → M_Rd = 346,3 kN·m (FLT governa)
FLM / FLA: compactas → 368,2 kN·m
Mx,Sd/Mx,Rd = 120 / 346,3 = 0,347

Interação: N_Sd/N_Rd = 900/1840,95 = 0,489 ≥ 0,2  →  η = 0,489 + (8/9)(0,347) = 0,797
Cortante: V_Rd = 548,4 kN → η = 45/548,4 = 0,082
```

Resultado do programa: `η = 0,797` (flexo-compressão), tudo atende.

---

## Capítulo 9 — Efeitos de segunda ordem: B1, B2, análise de 2ª ordem e forças nocionais

A NBR 8800 (item 4.9 e Anexo C) permite dois caminhos: análise de **1ª ordem com amplificação** dos esforços (B1 e B2) ou
análise de **2ª ordem** direta. O Structura implementa os dois e escolhe conforme *Parâmetros de dimensionamento*.

### 9.1 Amplificação local B1 (efeito P-δ)

Aplicada em cada seção em que a barra está comprimida (`N < 0`), no plano de cada eixo:

```text
N_e,x = π² E Ix / L²        N_e,y = π² E Iy / L²           (K = 1; comprimento da barra)
B1,x  = máx(1 ; Cm,x / (1 − N_c,Sd / N_e,x))               B1,y análogo
```

`Cm` conforme o Capítulo 1 (carga transversal: 1,0; senão `0,60 − 0,40 M1/M2` com mínimo 0,40). Se `N_c,Sd ≥ N_e`,
`B1 = 10⁶`. Quando a análise de 2ª ordem está ligada, **B1 não é aplicado**.

### 9.2 Amplificação global B2 (efeito P-Δ) — análise de 1ª ordem

O programa identifica os **andares** pelos pilares verticais (barras com `|Δz|/L ≥ 0,9`); os níveis são as cotas distintas dos
extremos dos pilares (tolerância de 1 mm) e cada andar vai de um nível ao seguinte. Para cada combinação e cada andar:

```text
ΣN   = Σ_pilares do andar máx(0 ; −(N_i + N_j)/2)      força axial de compressão (média das extremidades)
ΣH_x = |Σ (Vy_i ŷ_x + Vz_i ẑ_x)|   ΣH_y análoga          cortante do andar, somando as extremidades "i" dos pilares
Δh_x = máx |u_x,topo − u_x,base|   Δh_y análoga          deslocamento relativo (drift) do andar
h    = altura do andar

B2 = 1 / [ 1 − (1/Rs) (Δh/h) (ΣN/ΣH) ]       (Anexo C)       Rs = 0,85 (pórticos; valor padrão do projeto)
```

Regras de proteção: se `ΣH` ou `Δh` são desprezíveis, `B2 = 1`; se o denominador é ≤ 0,05, `B2 = 20` com aviso
("estrutura muito flexível; use análise de 2ª ordem"); se `B2 > 1,4`, aviso de **deslocabilidade alta** (NBR 8800, 4.10.2).
O B2 de cada barra é o **maior entre X e Y** do seu andar (barras que não são pilares pertencem ao andar da sua cota média).
Esse B2 multiplica **todos** os momentos da barra (o programa não separa `M_nt` e `M_lt`), o que é conservador.

### 9.3 Análise de 2ª ordem (P-Δ e P-δ) pelo solver

Com a opção ligada, cada combinação é resolvida com suas cargas ponderadas (não há superposição) iterando
`(K + Kg(N)) u = F` com a matriz de rigidez geométrica consistente. Nas combinações últimas os produtos de rigidez
`EA` e `EI` são multiplicados pelo **fator de redução** (padrão 0,8, NBR 8800, 4.10.5). Os esforços obtidos já contêm
P-Δ e P-δ; **B1 e B2 não se aplicam**, e o programa avisa disso na memória.

### 9.4 Forças nocionais (NBR 8800, 4.9.7.2)

Com a opção ligada, para cada combinação última criam-se quatro variantes (`+NX`, `−NX`, `+NY`, `−NY`) acrescentando
forças horizontais iguais a **0,3 % do peso** (carga gravitacional → massa × g) aplicado em cada nó. Cada caso auxiliar
(`NOC:X:<caso>`, `NOC:Y:<caso>`) é resolvido separadamente, de modo que a combinação continue sendo uma superposição linear.

---

## Capítulo 10 — Estados-limite de serviço

### 10.1 Flechas das barras

Para cada combinação `ELS…` e cada barra **horizontal ou inclinada** (pilares e diagonais íngremes não têm esse limite):

1. A barra é amostrada em 80 intervalos. Para cada abscissa `x`, os esforços de flexão internos fornecem a curvatura
   ```text
   κy(x) = −Mz(x) / (E Iz)         κz(x) = My(x) / (E Iy)
   ```
2. Integra-se **duas vezes** pela regra do trapézio: primeiro a rotação `θ(x) = ∫κ dx`, depois o deslocamento `v(x) = ∫θ dx`.
3. A flecha é medida em relação à **corda** que une os apoios:
   ```text
   barra biapoiada:   δ(x) = v(x) − v(L) · x/L
   balanço:           δ(x) = v(x) − v(xe) − θ(xe)(x − xe)       (xe = extremidade engastada)
   δ_máx = máx_x √(δy² + δz²)
   ```
   Um balanço é a barra com uma extremidade livre (um só elemento conectado, sem apoio).
4. Limite e razão:
   ```text
   n  = parâmetro da barra (padrão automático: 350 se a barra é horizontal [|Δz/L| < 0,02]; 250 se |Δz/L| < 0,5)
   δ_lim = L/n  (vão simples)   ou   2L/n  (balanço)
   η_δ = δ_máx / δ_lim
   ```

A razão exibida é a **pior** entre as combinações de serviço.

### 10.2 Deslocamentos por andar

Com as mesmas combinações `ELS`, o programa calcula para cada andar o drift (`Δh`) e o deslocamento do topo, e compara com
`h/500` por andar e `H/400` no topo (`H/300` em edificação de um pavimento — opção do projeto).

---

## Capítulo 11 — Dimensionamento automático (perfil mais leve)

Dois comandos usam o mesmo motor:

* **Verificar perfis**: valida os perfis adotados e mostra η de cada barra.
* **Dimensionar perfis**: troca perfis até convergir, descrito abaixo.

### 11.1 Série candidata

Para um perfil `P` da família `F`, a série é:

* laminados e tubulares: o **catálogo** da família (`data/catalog_steel.json`), ou séries paramétricas (tubulares quadrado,
  retangular e circular, com espessuras comerciais) quando a família não tem catálogo;
* ordenados por **área crescente** (peso crescente).

### 11.2 Escolha do perfil

```text
para cada candidato C (do mais leve ao mais pesado):
    η = máx over barras do grupo ( verifyMember(esforços fixos, C) )
    se η ≤ η_alvo (padrão 1,0)  e  inércia_máxima(C) ≥ piso de rigidez:  C é escolhido
```

Os esforços são mantidos fixos durante a escolha (não há reanálise dentro do passo).

### 11.3 Laço de convergência

```text
repete até 14 vezes:
    1. reúne esforços (todas as combinações últimas)
    2. para cada grupo (barras com o mesmo perfil, ou da mesma família): chooseBest → troca as barras (undo agrupado)
    3. se nada mudou: verifica as flechas (ELS) — se algum grupo excede L/n, eleva o "piso de inércia"
       do grupo a  Imax,atual · (δ/δ_lim) · 1,02  e repete (até 6 elevações)
       se nada mudou e flechas atendem: CONVERGIU
    4. reanalisa o modelo (os esforços mudam com as rigidezes)
```

O agrupamento por perfil ou por família é escolhido em *Parâmetros* ("mesma família recebe o mesmo perfil"). O perfil
representativo do grupo é o de maior área. O resultado mostra massa de aço antes/depois e o número de trocas.
Barras que nenhum perfil da série atende são reportadas com observação.

---

## Capítulo 12 — Exemplos resolvidos de barras

Os cinco exemplos a seguir foram **calculados pelo programa**; as marchas completas dos Capítulos 4 a 8 já mostram o
roteiro. Aqui estão os demais casos com a mesma sequência.

### 12.1 Pilar circular tubular CHS 114,3 × 4,75, em flexo-compressão

Dados: `D = 114,3 mm`, `t = 4,75 mm`, fy = 345 MPa, L = 3000 mm (`K = 1`), `N_c,Sd = 150 kN`, `Mx` de 4 a 6 kN·m (variação
linear ao longo da barra, sem carga transversal), `Vy = 2 kN`. Propriedades: `A = 1630 mm²`, `I = 2,443·10⁶ mm⁴`,
`W = 42 747 mm³`, `Z = 56 797 mm³`, `r = 38,71 mm`.

```text
Compressão:  N_e = π² E I / L² = 535,8 kN;   λ0 = √(1630 × 345/535 810) = 1,024;   χ = 0,658^(1,024²) = 0,644
             D/t = 24,1 ≤ 0,11 E/fy = 63,8  →  Aef = Ag
             N_c,Rd = 0,644 × 1630 × 345 / 1,10 = 329,5 kN        η = 150/329,5 = 0,455
Esbeltez:    KL/r = 3000/38,71 = 77,5 ≤ 200                        η = 0,387

Flexão:      D/t = 24,1 ≤ 0,07 E/fy = 40,6  →  M_Rd = Mpl/γa1 = 56 797 × 345/1,10 = 17,81 kN·m
B1:          Cm = 0,60 − 0,40 (4/6) = 0,867 (extremidades de mesmo sinal: M1/M2 negativo)
             B1 = 0,867 / (1 − 150/535,8) = 1,204     →   Mx,Sd = 6 × 1,204 = 7,22 kN·m     η_M = 0,405
Interação:   N/N_Rd = 0,455 ≥ 0,2  →  η = 0,455 + (8/9)(0,405) = 0,816
Cortante:    Lv = L/2: Lv/D = 13,1;   τcr = 207 MPa (limitado a 0,6 fy);   V_Rd = 0,5 × 207 × 1630/1,10 = 153,4 kN   η = 0,013
```

### 12.2 Barra tracionada com área líquida reduzida

Ver a marcha do Capítulo 4 (W 200 × 15,0): `N_t,Rd = 495,2 kN`, `η = 0,242`; rege a **ruptura**, por causa de
`Ct = 0,90` e `An/Ag = 0,85`.

### 12.3 Resumo dos exemplos

| Exemplo | Seção | Esforço principal | Estado-limite governante | η |
|---|---|---|---|---|
| Viga biapoiada | W 410 × 53,0 | M = 135 kN·m, V = 90 kN | FLT (Lb = 3000 mm) | 0,479 |
| Pilar | W 310 × 74,0 (H) | N = −900 kN, M = 120 kN·m | flexo-compressão 5.5.1.2-a | 0,797 |
| Tirante | W 200 × 15,0 | N = +120 kN | ruptura da seção líquida | 0,242 |
| Pilar tubular | CHS 114,3 × 4,75 | N = −150 kN, M = 6 kN·m | flexo-compressão 5.5.1.2-a | 0,816 |

---

## Capítulo 13 — Ligações, apoios e base de pilar

Todas as ligações são **verificadas em N e mm** pelo módulo `src/design` (independente de Qt) e orquestradas por
`ConnectionService`. Cada ligação tem **esforços de cálculo** extraídos da análise (envoltória das combinações últimas na
extremidade da barra), **verificações**, **lista de peças** (massas), **memorial** e uma **proposta automática** (menor massa
que atende), exceto a ligação tubular.

Os resultados são sempre uma lista de `Check` (demanda, resistência, razão). Há três tipos:

```text
razão       η = demanda / resistência                   (n > 1 não atende)
mínimo      adotado ≥ mínimo exigido                     (η = mínimo/adotado)
faixa       valor dentro dos limites de validade         (η = 0 se atendida; > 1 se violada)
```

A razão máxima de todas as verificações é o aproveitamento da ligação.

### 13.1 Parafusos (NBR 8800, 6.3)

```text
Ab  = π db²/4          Abe = 0,75 Ab         Ab,ef (tração)
Furo padrão:  dh = db + 2 mm (db ≤ 22 mm)   ou   db + 3 mm (db maior)           (Tabela 14)
Parafusos de alta resistência: A325/F1852 (fub = 830), ISO 8.8, A490, ISO 10.9 …  (Tabela A.3)
```

**Distribuição elástica no grupo** (força de cisalhamento direta + momento no plano): em cada parafuso `i` de coordenadas
`(x_i, y_i)` em relação ao centroide do grupo,

```text
Fx,i = Vx/n − M y_i / Σ r²          Fy,i = Vy/n + M x_i / Σ r²        Fv,Sd = máx_i √(Fx,i² + Fy,i²)
Ft,Sd = N_t / n                      (efeito alavanca não considerado)
```

**Cisalhamento (6.3.3.2):**

```text
Fv,Rd = 0,45 Ab fub / γa2     (plano de corte pela rosca; também em ligações sem protação)
Fv,Rd = 0,56 Ab fub / γa2     (plano fora da rosca, só alta resistência)       por plano de corte
```

**Pressão de contato e rasgamento (6.3.3.3):**

```text
lf = mín( distância do centro do furo extremo à borda − dh/2 ; passo − dh )        (conservador para todos os parafusos)
Fc,Rd = mín( 1,2 lf t fu ; 2,4 db t fu ) / γa2          (deformação no furo limitada; 1,5 / 3,0 se não limitada)
```

**Tração (6.3.3.1) e interação (6.3.3.4):**

```text
Ft,Rd = Abe fub / γa2          (Ft,Sd/Ft,Rd)² + (Fv,Sd/Fv,Rd)² ≤ 1,0
```

**Disposições (conferidas com a NBR 8800:2024):** espaçamento entre centros ≥ 3 db (recomendado; mínimo 2,7 db) e distância livre
entre bordas de furos `p − dh ≥ db` (6.3.9); espaçamento máximo `mín(24 t ; 300 mm)` (6.3.10); distância do centro do furo à borda ≥ Tabela 16
(`db` = 16, 20, 22, 24, 27, 30 mm → 22, 26, 28, 30, 34, 38 mm; 1/2″ → 19, 3/4″ → 25, 1″ → 32, 1 1/4″ → 41; a partir de 36 mm e acima de 1 1/4″: `1,25 db`);
distância máxima `mín(12 t ; 150 mm)` (6.3.12).

**Ligação por atrito (6.3.4)** — opção "Ligação por atrito" nas chapas de cisalhamento, cantoneiras duplas e chapas de nó, com parafusos ASTM F3125
A325/A490 (força de protensão mínima `F_Tb` da Tabela 19, ex.: A325 d20 = 142 kN; A490 d20 = 178 kN):

```text
μ = 0,30 (classe A) | 0,50 (classe B, jateada) | 0,20 (galvanizada);   Ce = 0,85 com duas ou mais chapas de enchimento, senão 1,0
Furos-padrão (deslizamento no ELS, 6.3.4.4):  Ff,Rk = 0,80 μ Ce FTb ns (1 − Ft,Sk/(0,80 FTb))   ≥   70 % × Fv,Sd por parafuso (Ft,Sk = 0,7 Ft,Sd)
Furos alargados/alongados (ELU, 6.3.4.3):   Ff,Rd = 1,13 μ Ce FTb ns (1 − Ft,Sd/(1,13 FTb)) / γe   ≥   Fv,Sd por parafuso,  γe = 1,20 (Tabela 13)
```

A verificação de deslizamento **soma-se** às de contato (6.3.3); a distribuição é uniforme entre os parafusos (sem momento no plano) e, na chapa de nó,
usa a maior força da diagonal. Para a emenda de viga e a ligação por atrito sob momento, a opção não está disponível.

### 13.2 Soldas de filete (NBR 8800, 6.2)

```text
Aw = 0,707 a L                                      (garganta efetiva: 0,707 × perna)
Fw,Rd = 0,60 Aw fw (1 + 0,50 sen^1,5 θ) / γw2       (6.2.5.2-a; θ = ângulo entre a força e o eixo do filete)
Metal-base:  F_Rd = mín(0,6 Agv fy/γa1 ; 0,6 Anv fu/γa2),  Agv = Anv = t L
Perna mínima (Tabela 11):  3 mm (t ≤ 6,3) · 5 mm (≤ 12,5) · 6 mm (≤ 19) · 8 mm (> 19);   t = parte mais fina
Perna máxima ao longo de bordas:  t (se t < 6,3)  ou  t − 1,5 mm
Comprimento mínimo:  máx(4a ; 40 mm)
```

### 13.3 Cisalhamento de bloco (6.5.6) — usado em todas as ligações

```text
Rd = mín[ (0,60 fu Anv + Cts fu Ant) ; (0,60 fy Agv + Cts fu Ant) ] / γa2        Cts = 1,0
Agv = t Lv   Anv = t (Lv − n_furos dh')   Ant = t (Lt − n_furos dh')    dh' = dh + 2 mm
```

### 13.4 Chapa de cisalhamento ("shear tab")

Chapa soldada ao apoio (duas soldas verticais) e parafusada na alma da viga em uma coluna de `n` parafusos.
Dados: `n` (parafusos), passo `p`, distâncias `ev`/`eh`, excentricidade `a` (linha de furos à face do apoio), folga, espessura
da chapa `tp`, solda `a_w`, `V`, `N`.

```text
H = (n − 1) p + 2 ev                    altura da chapa
M = V · a                               momento excêntrico

Parafusos:   cisalhamento direto + torção do grupo (13.1) com Vy = V, Vx = |N|, Mz = M;
             contato na chapa (t = tp) e na alma da viga (t = tw, extremidade = a − folga)
Chapa:       escoamento ao cisalhamento   0,6 Ag fy/γa1,  Ag = tp H
             ruptura ao cisalhamento      0,6 Anv fu/γa2,  Anv = tp (H − n dh')
             rasgamento                   (13.3)
             flexão (excentricidade)      Md = tp H² fy / (4 γa1)  ≥  M = V a
             axial                        N > 0: mín(Ag fy/γa1 ; mín(An; 0,85 Ag) fu/γa2) ;   N < 0: Ag fy/γa1
Alma da viga: rasgamento de bloco
Solda:       método elástico, 2 filetes de comprimento H: 
             fv = V/(2H)    fn = |N|/(2H) + M/(H²/3)    fr = √(fv² + fn²) ≤ 0,6 te fw/γw2,   te = 0,707 a_w
             metal-base: 2 fr ≤ mín(0,6 tp fy/γa1 ; 0,6 tp fu/γa2 ; 0,6 t_ap fy/γa1 ; 0,6 t_ap fu/γa2)  (apoio incluso)
```

**Marcha de cálculo** — chapa 9,5 mm, 3 parafusos A325 d = 20 mm, `p = 70`, `ev = eh = 35`, `a = 60`, folga 12, solda 6 mm,
alma de 7,5 mm (W 410 × 53), apoio com t = 16,3 mm, `V = 110 kN`:

```text
H = 2·70 + 2·35 = 210 mm      M = 110 kN × 60 mm = 6,60 kN·m
Parafuso: Fv,Sd (mais solicitado, com torção) = 59,7 kN   Fv,Rd = 0,45 × 314,2 × 830/1,35 = 86,9 kN     η = 0,687
Contato na chapa:  lf = mín(35 − 11; 70 − 22) = 24 mm;  Fc,Rd = mín(1,2·24·9,5·450 ; 2,4·20·9,5·450)/1,35 = 91,2 kN   η = 0,655
Chapa: Vrd(esc) = 0,6·1995·345/1,10 = 375,4 kN (η 0,293); Vrd(rup) = 262,2 kN (η 0,420); rasgamento 291,3 kN (η 0,378)
       flexão: Md = 9,5·210²·345/(4·1,10) = 32,85 kN·m  →  η = 6,6/32,85 = 0,201
Solda: te = 4,24;  fv = 110 000/420 = 261,9 N/mm; fn = 6,6·10⁶/(210²/3) = 449,0 N/mm; fr = 519,8 N/mm
       0,6 te fw/γw2 = 0,6 × 4,24 × 485/1,35 = 914,4 N/mm    →  η = 0,568
```

(todas as verificações atendem; a governante é o cisalhamento do parafuso, `η = 0,687`).

### 13.5 Cantoneira dupla

Duas cantoneiras iguais, parafusadas na alma da viga (**corte duplo**) e no apoio (corte simples, cada cantoneira recebe `V/2`).
Verifica: parafusos do lado da alma (corte duplo, `Fv,Rd` por plano) e contato na alma e nas cantoneiras (`2 t_c`); parafusos
do lado do apoio por cantoneira (`V/2` e `N/2` de tração, interação 6.3.3.4); cantoneira (escoamento/ruptura ao cisalhamento com
`V/2`, rasgamento da aba da alma, flexão por `M = (V/2) a`, `Md = t L² fy/(4γa1)`); rasgamento da alma da viga. A excentricidade
`a` é a linha de furos do apoio à aresta (`gaugeSupport`).

### 13.6 Chapa de nó (gusset) de treliça e contraventamento

Diagonal (peça simples ou dupla) parafusada em chapa de nó soldada ao banzo ou pilar. Entradas: grupo de parafusos
(`rows × lines`, passo, gabarito, distâncias), diagonal (área bruta `Ag,d`, espessura da aba `t_l`, excentricidade `e_c`),
chapa (largura `lw` na solda, espessura, comprimento livre `L`) e solda. A força é a **máxima tração** `T` ou **compressão** `C`.

```text
Parafusos: cisalhamento (13.1) e contato na chapa (t_g) e na diagonal (n_peças · t_l), força total N = máx(T, C)
Diagonal em tração:
   lc = (rows − 1) p            Ct = 1 − ec/lc  (limitado a ≤ 0,9;  0,5 se lc = 0)
   An = n_peças Ag,d − n_peças · lines · (dh + 2) t_l          Ae = Ct An
   Nt,Rd = mín(Ag fy/γa1 ; Ae fu/γa2)
   Cisalhamento de bloco da diagonal (13.3), Lv = (rows − 1)p + extremidade;  Lt = borda + (lines − 1) g
Chapa — seção de Whitmore (30°):
   lw,Whitmore = (lines − 1) g + 2 L_g tan 30°   (L_g = (rows − 1) p)
   lw = mín(lw,Whitmore ; largura da chapa na solda)
   Tração:  mín(lw t fy/γa1 ; mín(Anw ; 0,85 Agw) fu/γa2)
   Compressão (K = 0,65; r = t/√12; L = comprimento livre): λ0 = (K L/r)/π · √(fy/E);  χ (5.3.3);  Nc,Rd = χ Agw fy/γa1
   Cisalhamento de bloco em dois planos ao longo das linhas extremas
Solda chapa–apoio: filete de comprimento 2 lw (duas faces) para a FORÇA TOTAL da diagonal (conservador)
```

### 13.7 Emenda de viga I/H parafusada

Talas de mesa (externa e opcionalmente internas, quatro) e **duas talas de alma**. Os esforços:

```text
lever = d − tf
Ff,tração   = |M|/lever + N Af/A            Ff,compressão = |M|/lever − N Af/A       (parcela axial proporcional à área da mesa)
Alma: V com excentricidade ao centro do grupo de parafusos:  e = gap/2 + wEdgeH + (wCols − 1) wGauge/2 ;  M = V e
```

Verificações: parafusos das mesas (planos = 1 ou 2 com talas internas), contato na tala e na mesa; talas de mesa em
**tração** (escoamento e ruptura da seção líquida, `Ae = mín(An ; 0,85 Ag)`) e **compressão** (`Ag fy/γa1`, com o trecho livre
`gap + passo ≤ 13 t`); mesa da viga (seção líquida e rasgamento); parafusos da alma com torção (13.1), talas de alma
(cisalhamento `0,6 Agv fy/γa1`, `0,6 Anv fu/γa2`, rasgamento, flexão `Z = 2 t h²/4`).
A alma da viga **não colabora no momento** (hipótese conservadora).

### 13.8 Base de pilar (NBR 8800, 6.7; NBR 6118, 21.2.1)

Base **tipo 1**, perfil I/H com chumbadores fora das mesas. As disposições vêm da Tabela 18 (chumbadores ASTM A36 de
19 a 50 mm: `a1, a2, a3, h1`, arruela, `en`, `fck,mín`, `Nb,mín`).

```text
lx = d + 4 a1               ly = (0,5 nb − 1) a2 + 2 a1          nb = nº de chumbadores (4 a 8)
a  = lx/2 − a1               m = (lx − 0,95 d)/2    n = (ly − 0,80 bf)/2    n0 = √(d bf)/4
σc,Rd = mín[ 0,85 (fck/γc) √(A2/A1) ; 1,7 fck/γc ],   A1 = lx ly,   A2 = Nb × Bb (bloco)
X = [4 d bf/(d + bf)²] · N / (lx ly σc,Rd)        λ = 2√X/(1 + √(1 − X)) ≤ 1  (X ≤ 1)
e = M/N                                                       (excentricidade)
```

**Casos (Figuras 22 e 23):** `C1` (compressão centrada), `C2` (compressão com e ≤ ½(lx − N/(σc,Rd ly)): contato parcial,
`lc = lx − 2e`), `C3` (grande excentricidade: `lc = (lx/2 + a) − √((lx/2 + a)² − 2 N (e + a)/(σc,Rd ly))`), `T1`, `T2` (tração com
momento pequeno) e `T3`.

```text
lmax = máx(m ; n ; λ n0)   se lc ≥ m            ou     máx(√(lc(2m − lc)) ; n)   se lc < m
tp,min = lmax √(2 σc,Sd / (fy/γa1))                (flexão da placa)
Tração nos chumbadores:  Ft,Sd por chumbador (C3: 2(R − N)/nb;  T1: N/nb;  T2: N/nb + M/(a nb);  T3: 2(R + N)/nb)
                         tp,min ≥ √( 2 nb Ft,Sd (m − a1) / (ly fy/γa1) )
db,min = √( 4 Ft,Sd / (0,75 π fub/γa2) )
Cisalhamento (atrito):  V,Rd = mín( μ σc,Sd lc ly/γa2 ; τc,Rd lx ly ),   τc,Rd = mín(0,2 fck/γc ; 4 MPa)
                        se V,Sd > V,Rd → exige placa de cisalhamento / arruelas especiais (6.7.2.3 a 6.7.2.5, não dimensionadas)
Bloco (NBR 6118, 21.2.1):  σ,Rd = mín(fcd √(Ac1/Ac0) ; 3,3 fcd)      Ac0 = lc ly,  Ac1 = Nb Bb
Solda pilar–placa:  duas mesas (força de mesa Ff = M/(d − tf) + N Af/A, ângulo 90°) e alma (cortante V)
Validade:  ly ≥ bf + 25 mm;  fck ≥ fck,mín;  Nb ≥ máx(Nb,mín ; lx + 2 en ; lx + 2(a3 − a1));  4 ≤ nb ≤ 8
```

### 13.9 Ligação tubular soldada (CIDECT Design Guide 1, 2ª ed., 2008)

Banzo circular (CHS) com diagonais circulares ou chapa transversal; tipos T/Y, X, K/N com afastamento e K/N com sobreposição.
Notação: `β = d1/d0`, `γ = d0/(2 t0)`, `n = N0/Npl,0 + M0/Mpl,0` (tração +), `fy0 ≤ 0,8 fu0`, resistências × 0,9 se `fy0 > 355 MPa`.

```text
Qu (eq. 4.4 – X):             2,6 (1 + β)/(1 − 0,7 β) γ^0,15
Qu (eq. 4.5 – T/Y):           2,6 (1 + 6,8 β²) γ^0,2
Qu (eq. 4.6 – K com afastamento): 1,65 (1 + 8 β^1,6) γ^0,3 [1 + 1/(1,2 + (g/t0)^0,8)]
Qu (chapa transversal): X: 2,2 (1 + β)/(1 − 0,7β) γ^0,15 ; T: 2,2 (1 + 6,8 β²) γ^0,2
Qf = (1 − |n|)^C1  (C1 = 0,25 em K com afastamento, 0,45 − 0,25 β nos demais; compressão no banzo)  ou (1 − n)^0,20 (tração)

Plastificação da face do banzo:  N1* = Qu Qf fy0 t0² / sen θ1
Punção (d ≤ d0 − 2 t0):          N1* = 0,58 fy0 π d1 t0 (1 + sen θ)/(2 sen² θ)             (chapa: 1,16 fy0 t0 b1)
Cisalhamento do banzo:  X:  (N0/Npl,0)² + (V/Vpl,0)² ≤ 1,  Vpl,0 = 0,58 fy0 (2 A0/π);  K com afastamento:  idem com N_gap,0 e V_gap
K com sobreposição: escoamento local das diagonais (eqs. 4.8–4.9), eficiência da diagonal coberta, cisalhamento
  das diagonais acima de Ov,lim (60 % ou 80 % com pé oculto soldado), Ov ≥ 25 %
```

Verifica-se também a **faixa de validade** (esbeltez `d0/t0`, `0,2 ≤ β ≤ 1,0`, `θ ≥ 30°`, `g ≥ t1 + t2`, `|e|/d0 ≤ 0,25`,
`fy ≤ 460 MPa`). A ligação **não tem proposta automática**; a solda é suposta de resistência total.

### 13.10 Verificação do perfil de apoio (mesa ou alma do pilar/viga principal)

A chapa de cisalhamento, a cantoneira dupla e a chapa de nó transmitem esforços ao **apoio**. Ao criar a ligação, o programa
identifica o perfil de apoio (outra barra que chega ao nó, em direção diferente), decide **onde a chapa encosta** e preenche
os dados do apoio:

```text
S = direção do nó para dentro da viga;  ay = |S · y'|,  az = |S · z'|   (eixos locais do apoio)
ay ≥ az  →  face da MESA   (afastamento do eixo = d/2;  supT = tf do apoio)
ay < az  →  face da ALMA   (afastamento do eixo = tw/2; supT = tw do apoio)
```

A geometria 3D usa esse afastamento para posicionar a ligação **na face** do perfil (e não no eixo do nó). Se o apoio continua
do outro lado do nó, a ligação é considerada intermediária; senão, está na extremidade do perfil.

Com os dados do apoio (altura `d`, `bf`, `tf`, `tw`, raio, aço, posição), as verificações locais (NBR 8800, 5.7) são
acrescentadas **à lista da ligação**:

```text
Forças normais à face (N):
   chapa de cisalhamento / cantoneira: F_t = máx(N,0) + 1,5 V e/H ;   F_c = máx(−N,0) + 1,5 V e/H
          (e = a ou gaugeWeb; 1,5 V e/H = resultante da zona tracionada/comprimida pelo binário; n = H/2)
   chapa de nó: F_t = máx(T, N,0) ;  F_c = máx(C, −N,0)  (a força da diagonal inteira, conservador; n = lw)

MESA:
   5.7.2 Flexão local da mesa:  Rd = 6,25 tf² fy/γa1      (× 0,5 se a ligação está na extremidade do perfil)
         dispensada se o comprimento transversal da carga (t_chapa + 2 solda) < 0,15 bf  → η = 0
   5.7.3 Escoamento local da alma:  Rd = 1,10 (5k + n) fy tw/γa1   (ou 1,10 (2,5k + n) fy tw/γa1 na extremidade)
   5.7.4 Enrugamento da alma (só compressão):
         intermediária:      Rd = 0,66 tw² [1 + 3 (n/d)(tw/tf)^1,5] √(E fy tf/tw) / γa1
         extremidade:        n/d ≤ 0,2: 0,33 tw² [1 + 3 (n/d)(tw/tf)^1,5] √(E fy tf/tw)/γa1
                             n/d > 0,2: 0,33 tw² [1 + (4 n/d − 0,2)(tw/tf)^1,5] √(E fy tf/tw)/γa1
         k = tf + raio de concordância (ou 1,5 tf)
ALMA (chapa soldada na face da alma, entre as mesas):
   Flexão transversal da alma (linhas de plastificação — estimativa):
         h = d − 2 tf ;  b = n + h ;  Mp = fy tw²/4 (por mm);   Rd = 8 Mp b / (h γa1)
```

> A ligação **não é verificada quando** os dados do apoio (`supType = 0`) não foram informados. As fórmulas das seções
> 5.7.2 a 5.7.4 foram **conferidas** com o texto da NBR 8800:2024 e coincidem com o que está acima.

### 13.11 Proposta automática de ligação

Para chapa de cisalhamento, cantoneira dupla, chapa de nó, emenda e base, o programa faz uma **busca exaustiva** sobre conjuntos
de valores usuais (diâmetros 16–24 mm, 2 a 10 fileiras, espessuras 6,3 a 25 mm, pernas de solda 4 a 12 mm …), respeitando passos
mínimos, distâncias mínimas e a altura disponível na viga, e escolhe a de **menor massa total** (chapas, parafusos, arruelas e
solda) cujo maior η ≤ 1. A verificação do apoio (13.10) **não** orienta a busca: após a proposta, confira também as razões do
apoio.

---

## Capítulo 14 — Vigas mistas e lajes mistas com forma de aço incorporada

Módulo `src/design/CompositeFloor` (NBR 8800:2024, **Anexo L** — vigas mistas — e **Anexo N** — lajes mistas), acessível por
*Dimensionamento › Vigas e lajes mistas*. Vale para viga **biapoiada** de perfil **I duplamente simétrico** com laje maciça ou com
forma de aço (nervuras perpendiculares ou paralelas). Os dados ficam gravados no `.dat` (`[COMPOSITE_FLOOR]`).

### 14.1 Materiais e grandezas auxiliares

```text
E_ci = 5600 √fck  (fck ≤ 50 MPa)        α_i = mín(1 ; 0,8 + 0,2 fck/80)       E_cs = α_i E_ci
η_c  = mín(1 ; (40/fck)^(1/3))           f_cd = fck/γc  (γc = 1,4)             α_E = E_a / E_cs
Largura efetiva (L.2.2.1.1):  b_ef = Σ_lados mín(L/8 ; s_lado/2)       (s = distância ao eixo da viga vizinha; 0 = sem laje)
Peso do concreto fresco:     γ_c,fresco = (massa específica + 100 kg/m³) × 9,80665 / 1000   kN/m³
```

### 14.2 Conector de pino com cabeça (L.4.2)

```text
A_cs = π d²/4
Q_Rd = mín( 0,5 A_cs √(fck E_cs)/γ_cs ;  R_g R_p A_cs f_u / γ_cs )        γ_cs = 1,25
```

| Laje | R_g | R_p |
|---|---|---|
| maciça | 1,00 | 0,75 |
| forma perpendicular, 1 conector/nervura | 1,00 | 0,75 (e_mh ≥ 50 mm) ou 0,60 (e_mh < 50 mm), soldado através da forma |
| forma perpendicular, 2 conectores/nervura | 0,85 | idem |
| forma perpendicular, ≥ 3 conectores/nervura | 0,70 | idem |
| forma paralela | 1,00 se b_F/h_F ≥ 1,5; senão 0,85 | 0,75 |

### 14.3 Momento resistente positivo (L.2.3.1.1) — interação completa e parcial pela mesma formulação

```text
T_a,máx = A_a f_yd                 C_c,máx = 0,85 η_c f_cd b_ef t_c         F_hd = mín(T_a,máx ; C_c,máx)
n_conectores (meio vão) = ⌊ L/(2 s) ⌋ × (linhas)          ΣQ_Rd = n × Q_Rd          η_i = mín(1 ; ΣQ_Rd/F_hd)

C_cd = mín(ΣQ_Rd ; T_a,máx ; C_c,máx)        a = C_cd / (0,85 η_c f_cd b_ef)         (altura do bloco de compressão)
C_ad = ½ (T_a,máx − C_cd)                     (compressão no perfil: zero se η_i = 1 com a linha neutra na laje)
```

Se `C_ad > 0` a linha neutra plástica corta o perfil: `y_p` (profundidade da LNP no perfil, a partir do topo)
é obtida de `C_ad` (primeiro na mesa superior, depois na alma) e as posições `y_c` (centroide da parte comprimida) e `y_t`
(centroide da parte tracionada, medido da base) dão

```text
M_Rd = β_vm [ C_ad (d − y_t − y_c) + C_cd ( t_c − a/2 + h_F + d − y_t ) ]          β_vm = 1,0
```

Dois cálculos: `M_Rd,completa` com `C_cd = F_hd` e `M_Rd` com `C_cd = ΣQ_Rd` (interação real).

### 14.4 Grau mínimo de interação (L.2.3.1.1.2) — perfil duplamente simétrico (ρ = 0)

```text
η0   = 1 − (E/(580 fy)) (0,75 − 0,03 L_e)      L_e em metros (vão da viga biapoiada)
c_r  = máx(0,7 ; M_Sd / M_Rd,completa)           c_p = 1,0 (escorada) ou 0,85 (não escorada)
η_min = mín(1 ; máx( c_p c_r² η0 ; 0,4 c_r ))    →  exige η_i ≥ η_min
```

### 14.5 Propriedades da seção mista e flechas (L.1.2)

Seção homogeneizada ao aço (concreto acima da forma; as nervuras são desprezadas):

```text
A_c = b_ef t_c / α_E       y_c = d + h_F + t_c/2
ȳ = (A_a d/2 + A_c y_c)/(A_a + A_c)
I_tr = I_a + A_a (ȳ − d/2)² + b_ef t_c³/(12 α_E) + A_c (y_c − ȳ)²
I_ef = I_a + √η_i (I_tr − I_a)          W_ef = W_a + √η_i (W_tr,inf − W_a)
```

A flecha de serviço é a soma de três fases:

```text
k  = 5 L⁴ / (384 E)
δ1 = k q_antes / I_a                 (peso do aço + concreto fresco + construção carregam só o perfil; se escorada: zero)
δ2 = k q_longa / I_ef(3 α_E)         q_longa = q_permanente + ψ2 q_variável (+ q_antes, se escorada)   [longa duração: módulo reduzido]
δ3 = k (1 − ψ2) q_variável / I_ef    (curta duração)
δ  = δ1 + δ2 + δ3 − contraflecha     ≤ L/n  (padrão L/350)
```

### 14.6 Verificações

| Verificação | Critério |
|---|---|
| Momento fletor positivo | `M_Sd ≤ M_Rd` (η_i real) |
| Grau de interação mínimo | `η_i ≥ η_min` |
| Força cortante | `V_Sd ≤ V_Rd` (só o perfil de aço; função de cortante da NBR 8800, 5.4.3, ou a de alma `kv = 5` se a seção não é suportada) |
| Fase de construção (não escorada) | `M_Ga,Sd ≤ M_Rd,aço` (flexão do perfil isolado com `Lb` informado ou 0 se travado pela forma); tensões na mesa inferior em serviço `M_Ga,S/W_a + M_L,S/W_ef ≤ fy` (alma compacta, L.2.3.2-b) ou de cálculo (alma semi-esbelta, L.2.3.2-c) |
| Espaçamento dos conectores | `s ≤ mín(8 (h_F + t_c) ; 915 mm [forma perpendicular])` e `s ≥ 6 d` (laje maciça) / `4 d` (forma); `d ≤ 2,5 t_f` |
| Laje com forma (L.2.6) | `h_F ≤ 75 mm`, `b_F ≥ 50 mm`, `t_c ≥ 50 mm`, `d ≤ 19 mm`, projeção do conector acima da forma ≥ 40 mm |
| Flecha | `δ ≤ L/n` |

"Propor espaçamento" varre `s` em múltiplos de 25 mm entre o mínimo e o máximo permitidos e devolve o **maior** passo que
atende o momento e o grau mínimo de interação (menos conectores).

**Marcha de cálculo (resultados do programa)** — viga W 410 × 53,0, L = 9 m, vizinhas a 3 m, laje maciça `t_c = 110 mm`,
`fck = 25 MPa`, pinos `d = 19 mm`, `fu = 415 MPa`, `s = 150 mm`, não escorada; `M_Sd = 330 kN·m`, `V_Sd = 150 kN`:

```text
b_ef = 2 × mín(9000/8 ; 3000/2) = 2 × 1125 = 2250 mm
E_cs = 0,8625 × 5600 √25 = 24 150 MPa ;  α_E = 200000/24150 = 8,28 ; f_cd = 25/1,4 = 17,86 MPa ; η_c = 1,0
A_cs = 283,5 mm² ; Q_Rd = mín(88,1 ; 0,75 × 283,5 × 415/1,25 = 70,6) = 70,6 kN
F_hd = mín(6804 × 313,6 = 2134 kN ; 0,85 × 17,86 × 2250 × 110 = 3757 kN) = 2134 kN
n = ⌊9000/300⌋ = 30  →  ΣQ_Rd = 2118 kN ;  η_i = 0,992
M_Rd = 597,3 kN·m  (completa: 598,1)      η = 330/597,3 = 0,552
η0 = 1 − 200000/(580 × 345) × (0,75 − 0,03 × 9) = 0,520 ; c_r = máx(0,7 ; 330/598,1) = 0,70 ; c_p = 0,85
η_min = máx(0,85 × 0,49 × 0,520 ; 0,28) = 0,28   →   η_i = 0,992 ≥ 0,28   (η de verificação = 0,282)
Construção: M_Ga,Sd = 90 kN·m ≤ M_a,Rd = 327,8 kN·m  (η = 0,275)
Tensão (serviço): 65,0e6/923 295 + 160e6/1 413 305 = 70,4 + 113,2 = 183,6 MPa ≤ 345  (η = 0,532)
Flecha: I_a = 18 604 cm⁴, I_tr(α_E) = 58 083, I_tr(3α_E) = 46 208, I_ef = 57 933 cm⁴ →
        δ = 17,2 + 8,1 + 4,6 = 29,9 mm > L/350 = 25,7 mm   →  η = 1,16 (NÃO atende: contraflechar ou escorar)
```

### 14.7 Laje mista com forma de aço incorporada (Anexo N)

Entradas por metro de largura: geometria (h_F, t_c, b_n, b_b, vão L_F, continuidade), forma (A_F,ef, I, W_ef, W_pl, e, e_p, peso próprio,
`f_yF`), ensaio de cisalhamento (`m`, `k`, `γ_sl`), cargas.

Coeficientes de continuidade (c_M, c_δ, c_V, vão equivalente):

| Continuidade | Momento `M = c_M q L²` | Flecha `δ = c_δ q L⁴/(EI)` | Cortante `V = c_V q L` | Vão equiv. |
|---|---|---|---|---|
| vão simples | 0,125 | 5/384 | 0,500 | 1,0 L |
| dois vãos | 0,125 | 0,00922 | 0,625 | 0,9 L |
| três ou mais vãos | 0,100 | 0,00677 | 0,600 | 0,8 L |

**Fase inicial (N.2)** — a forma resiste ao concreto fresco:

```text
peso do concreto fresco:  q_c = γ_c,fresco (t_c + h_F b_b/b_n)/1000   kN/m²
q_Ed = γg q_forma + γq (q_c + q_construção)      (γg = 1,4; γq = 1,5)
M_Sd = c_M q_Ed L²   ≤   M_Rd = W_ef f_yF/γa1                   η = M_Sd/M_Rd
δ_ini = c_δ (q_forma + q_c) L⁴ / (E I_forma)   ≤  mín(L/180 ; 20 mm)     (aviso se δ > L/250: empoçamento)
```

**Fase final (N.3):**

```text
q_Ed = γg (q_forma + q_c + q_perm) + γq q_var       M_Sd = q_Ed (L_equiv)²/8
d_F = h_t − e ;   N_pa = A_F,ef f_yF/γa1 ;   a = N_pa/(0,85 η_c f_cd b)        (b = 1000 mm)
a ≤ t_c :   M_Rd = N_pa (d_F − a/2)                                         (linha neutra na laje)
a > t_c :   N_cf = 0,85 b t_c η_c f_cd ; M_pa = W_pl f_yF/γa1 ; M_pr = mín(M_pa ; 1,25 M_pa (1 − N_cf/N_pa))
            y = h_t − t_c/2 − e_p + (e_p − e) N_cf/N_pa ;   M_Rd = N_cf y + M_pr
```

**Cisalhamento longitudinal — método m-k (N.3.1.2):**

```text
L_s = L_equiv/4        V_l,Rd = (b d_F/γ_sl) [ m A_F,ef/(b L_s) + k ]          V_Sd = q_Ed L_equiv/2
```

**Cisalhamento vertical (N.3.1.3):**

```text
A_v = t_c b_n + h_F b_b          η = mín(1 ; 0,3 + 0,7 ρ/2400)       f_ctd = η 0,21 fck^(2/3)/γc
τ_Rd = 0,25 f_ctd               V_c,Rd = 1000 τ_Rd (1,0)(1,2) A_v/b_n         V_max = 1000 · 0,285 √fck A_v/b_n
V_v,Rd = mín(V_max ; V_v,F,Rd + V_c,Rd)        V_Sd = c_V q_Ed L
```

**Flecha de serviço (N.3.2.2)** — só a sobrecarga, limite `L/350`, com **inércia média** entre a seção não fissurada e a fissurada:

```text
n = E_a/E_cs        seção por metro: concreto acima da forma + nervuras (largura b_b/b_n) + A_F,ef
I_nf = Σ [...] (seção homogeneizada)       I_cr: concreto só acima da LN (b x³/(3n)) + I_forma + A_F,ef (d_F − x)²,  x de b x²/(2n) = A_F,ef (d_F − x)
I_ef = (I_nf + I_cr)/2          δ = c_δ q_var L⁴ / (E I_ef) ≤ L/350
```

E ainda `t_c ≥ 50 mm` (N.5). **Os dados da forma (A_F,ef, I, W, m, k, V_v,F,Rd) são do fabricante** e devem ser informados.

**Marcha de cálculo (resultados do programa)** — forma de 75 mm, `t_c = 70`, `b_n = 150`, `b_b = 110`, vão 2,5 m (simples),
`A_F,ef = 1100 mm²/m`, `f_yF = 345`, `e = e_p = 40`, `m = 150`, `k = 0,20`; cargas 1,5 + 3,0 kN/m²:

```text
q_c = [(2400 + 100) × 9,80665/1000] × (70 + 75 × 110/150)/1000 = 3,06 kN/m²
Fase inicial: q_Ed = 1,4 × 0,10 + 1,5 (3,06 + 1,0) = 6,24 kN/m² ; M_Sd = 0,125 × 6,24 × 2,5² = 4,87 kN·m/m
              M_Rd = 13 000 × 345/1,10 = 4,08 kN·m/m  →  η = 1,195 (NÃO atende: escorar ou reforçar a forma)
              δ_ini = (5/384) (0,10 + 3,06) 2500⁴/(200000 × 400 000) = 20,1 mm > mín(13,9 ; 20) mm → η = 1,449
Fase final:   q_Ed = 1,4 × 4,66 + 1,5 × 3,0 = 11,03 kN/m² ; M_Sd = 8,62 kN·m/m
              N_pa = 1100 × 345/1,10 = 345 kN/m ; a = 345 000/(0,85 × 17,86 × 1000) = 22,7 mm ≤ 70 ; d_F = 145 − 40 = 105
              M_Rd = 345 000 (105 − 11,4) = 32,3 kN·m/m  →  η = 0,267
              V_l,Rd = (1000 × 105/1,25)[150 × 1100/(1000 × 625) + 0,20] = 39,0 kN/m ; V_Sd = 13,8 kN/m → η = 0,354
```

---

## Capítulo 15 — Hipóteses, limitações e pontos de atenção

**Do modelo e da análise**

1. **Validação:** os testes automáticos (`TestDesign`) conferem fórmulas contra cálculo à mão; **não há validação cruzada** com
   outro programa de dimensionamento.
2. **B2 automático** usa o drift de andar da combinação, aplica B2 a **todos** os momentos da barra (sem separar `M_nt`/`M_lt`) e
   usa o maior entre X e Y — conservador. Para estruturas de deslocabilidade alta prefira a análise de 2ª ordem.
3. **Forças nocionais:** apenas as horizontais proporcionais às cargas gravitacionais (0,3 %); não há imperfeições de barra.
4. **Cargas de vento** (NBR 6123) são geradas por pressões informadas: ver `DIMENSIONAMENTO_NBR.md`.
5. **Ligações e furos** em barras tracionadas entram só pelas razões `An/Ag` e `Ct`.

**Das verificações**

6. **FLT — zona de transição:** `Cb` entra em `Mcr` e `λr` e **não** multiplica o parêntese da interpolação: é exatamente o formato da
   NBR 8800:2024 (D.2.1), conferido contra o texto da norma. Não há ressalva de conservadorismo.
7. **Cortante:** `kv = 5,34` (alma sem enrijecedores transversais). Não há projeto de enrijecedores.
8. **Cantoneira laminada em flexão:** a NBR 8800:2024 não a trata; o programa usa o critério adaptado da AISC 360-22 F10 (Cap. 6.8), sinalizado no memorial.
9. **Torção** em perfis abertos e maciços **não é verificada** (aviso); só tubulares (5.5.2).
10. **Tubular retangular esbelto** (`h/t > λr`, FLA) não é coberto na NBR 8800; use a NBR 14762 para tubulares (opção por projeto).
11. **Alma esbelta** (Anexo E) é tratada para perfis I/H; `ry` é o raio da seção bruta (aproximação).
12. **Mesa em perfil I soldado** (grupo 5 da Tabela 4) está implementada na tabela, mas os perfis do catálogo laminado usam o grupo 4.
13. **Limite de esbeltez de tração** é apenas aviso.

**Das ligações**

14. **Distribuição elástica** de forças em parafusos e soldas; **sem efeito alavanca**.
15. A **verificação do perfil de apoio** (13.10) usa fórmulas simplificadas: binário `1,5 V e/H` com `n = H/2` e, na face da alma, linhas de
    plastificação (estimativa, fora da norma). As expressões 5.7.2, 5.7.3 e 5.7.4 foram **conferidas** com o texto da NBR 8800:2024 (páginas 64–65)
    e coincidem; o limiar de "extremidade" é tratado como binário (ligação no extremo do perfil de apoio ou intermediária).
16. **Chapa de nó:** força uniforme sem decomposição entre banzo e montantes; solda para a força total.
17. **Base de pilar:** tipo 1 apenas; sem placa de cisalhamento e sem armadura do bloco (NBR 6118).
18. **Ligação tubular:** apenas CHS (CIDECT DG 1); sem RHS (DG 3) e sem ligações multiplanares nem momentos nas diagonais.
19. **Proposta automática** minimiza massa de chapas/parafusos/solda por varredura discreta; não otimiza detalhes de fabricação.

**Das vigas e lajes mistas**

20. Sem armadura de costura/cisalhamento longitudinal da laje (L.1.3.4), sem fissuração (L.5), sem cargas concentradas/lineares
    sobre a laje, sem punção e sem viga contínua; perfis duplamente simétricos.

---

## Capítulo 16 — Apêndices

### 16.1 Símbolos

| Símbolo | Significado |
|---|---|
| `A`, `Ag`, `Aef`, `Ae` | área bruta, efetiva (flambagem local), líquida efetiva |
| `E`, `G` | módulos de elasticidade longitudinal e transversal |
| `fy`, `fu` | resistências ao escoamento e à ruptura |
| `Ix`, `Iy` | momentos de inércia em torno de X (horizontal) e Y (vertical) |
| `Zx`, `Zy`, `Wx`, `Wy` | módulos plástico e elástico |
| `J`, `Cw` | constante de torção e de empenamento |
| `r0`, `x0`, `y0` | raio de giração polar e coordenadas do centro de cisalhamento |
| `Ne`, `χ`, `λ0` | carga de flambagem elástica, fator de redução, esbeltez reduzida |
| `Lb`, `Cb` | comprimento destravado e fator de modificação do momento |
| `Mpl`, `Mr`, `Mcr` | momentos de plastificação, de início de escoamento e crítico elástico |
| `B1`, `B2`, `Cm` | amplificações local e global, coeficiente de momento |
| `η` | aproveitamento (demanda/resistência) |

### 16.2 Identificadores das verificações da barra (`CheckResult::id`)

| id | Verificação | Item |
|---|---|---|
| `tension` | tração axial | 5.2 |
| `compression` | compressão axial | 5.3 |
| `slenderness` | limite de esbeltez em compressão | 5.3.7 |
| `flexure_x`, `flexure_y` | flexão em X (maior inércia) e em Y | 5.4.2, Anexos D e E |
| `shear_y`, `shear_z` | cortante na alma e nas mesas | 5.4.3 |
| `interaction` | flexo-compressão / flexo-tração | 5.5.1.2 |
| `torsion` | torção em tubulares (+ interação com V, N, M) | 5.5.2 |

### 16.3 Mapa do código

| Arquivo | Conteúdo |
|---|---|
| `src/design/GrossSection.{h,cpp}` | propriedades geométricas por integração do contorno |
| `src/design/DesignSection.{h,cpp}` | seção de projeto: tipo, norma, `Cw`, centro de cisalhamento, elementos planos |
| `src/design/Nbr8800.{h,cpp}` | tração, compressão, flexão, cortante, torção (laminados, tubulares, maciços) |
| `src/design/BarDesign.{h,cpp}` | orquestração por barra/combinação/seção, Cb, Cm, B1, interações |
| `src/app/services/DesignService.{h,cpp}` | esforços por barra, verificação, escolha do melhor perfil |
| `src/app/services/StructuralChecks.{h,cpp}` | flechas, deslocamentos por andar, B2 |
| `src/app/services/NotionalLoadService.{h,cpp}` | forças nocionais |
| `src/app/services/CombinationGenerator.{h,cpp}` | combinações NBR 8681 |
| `src/design/Connections.*`, `ShearConnections.*`, `TrussConnections.*`, `BasePlate.*`, `TubularJoints.*`, `SupportChecks.*` | ligações |
| `src/design/CompositeFloor.{h,cpp}` | vigas e lajes mistas |
| `src/tests/TestDesign.cpp` | testes numéricos |

### 16.4 Referências normativas

* ABNT NBR 8800:2024 — Projeto de estruturas de aço e de estruturas mistas de aço e concreto de edificações.
* ABNT NBR 8681:2003 — Ações e segurança nas estruturas.
* ABNT NBR 6118:2023 — Projeto de estruturas de concreto (bloco de fundação, 21.2.1).
* CIDECT Design Guide 1 (2ª ed., 2008) — Ligações soldadas de perfis tubulares circulares.
* ABNT NBR 6123:2023 — Forças devidas ao vento em edificações.

> **Aviso:** este documento descreve o comportamento do software para auxiliar na revisão e na compreensão dos cálculos.
> Ele **não substitui** a norma nem o julgamento do engenheiro responsável; confira sempre dados, hipóteses e resultados.

## Apêndice — Conferência com a NBR 8800:2024

Fórmulas conferidas, uma a uma, contra o texto da norma (PDF em `normas/`):

| Item | Norma 2024 | Situação |
|---|---|---|
| χ e λ0 (5.3.3), Tabela 4 e 5, largura efetiva (5.3.4.2) | pág. 45–48 | conforme |
| Cantoneira simples comprimida, `Lx1,eq` (5.3.5.4) | pág. 50 | conforme |
| Limites de esbeltez 300 (tração) e 200 (compressão) (5.2.8, 5.3.7) | pág. 40, 52 | conforme (tração só aviso) |
| Cb e Rm (5.4.2.3) | pág. 54–55 | conforme |
| Cortante, kv, λp, λr (5.4.3) | pág. 56–58 | conforme |
| Interação N + M (5.5.1.2) | pág. 61 | conforme |
| Torção em tubulares (5.5.2) | pág. 61–62 | conforme |
| 5.7.2, 5.7.3, 5.7.4 | pág. 64–65 | conforme |
| Anexo C (B1, B2, Cm, Rs) | pág. 134–135 | conforme |
| Anexo D: FLT/FLM/FLA de I, H e U (D.2.1, D.2.2) | pág. 154–158 | conforme |
| Anexo D: T com alma comprimida (D.2.3) | pág. 159 | **corrigido** nesta revisão |
| Anexo D: FLA do U no eixo de menor inércia (D.2.8-d) | pág. 164 | **corrigido** (Wef) |
| Anexo E: alma esbelta (E.2.1–E.2.4) | pág. 148–150 | conforme |
| Cantoneira simples em flexão | não existe | critério adaptado (AISC F10), sinalizado |

## Adendo — complementos posteriores

**Efeito alavanca (6.3.5)** — na aba tracionada por parafusos (cantoneira dupla, parafusos do apoio): `p_ext = mín(e_ext; 1,75b) + mín(0,5 e_int; 1,75b)`,
`p_int = mín(e_int; 3,5b)`, `δ = 1 − d_f/p`, `β = [(a + 0,5 d_b)/(b − 0,5 d_b)] (F_t,Rd/F_t,0,Sd − 1)` (`β < 0`: trocar o parafuso), `α = 1` se `β ≥ 1`,
senão `α = mín[1; (β/(1−β))/δ]`; espessura mínima `t ≥ √[4 (b − 0,5 d_b) F_t,0,Sd γ_a1 /(p f_u (1 + δ α))]` (placa flexível) e, sem alavanca, `t ≥ √[4 (b − 0,5 d_b) F_t,0,Sd γ_a1/(p f_u)]`.
Exemplo: `b = 40`, `a = 30`, `p = 70`, `d_f = 22 mm`, `F_t,Rd = 100 kN`, `F_t,0,Sd = 50 kN`: `β = 1,33 ≥ 1 → α = 1`; `δ = 0,686`; `t ≥ 11,8 mm`.

**Pega longa e ligação longa (6.3.7, 6.3.8)** — pega `> 5 d_b` (parafuso sem protensão): `F_v,Rd` reduzida em 1 % por 1,5 mm adicionais (pega de 130 mm com `d_b = 20`: fator 0,80);
emenda por contato de barra tracionada com mais de 950 mm: `F_v,Sd` e `F_c,Sd` × 1,20.

**Cortante com enrijecedores (5.4.3.1.1)** — `k_v = 5 + 5/(a/h)²` para `a/h ≤ 3`; sem enrijecedores ou `a/h > 3`, `k_v = 5,34` (parâmetro "espaçamento dos enrijecedores de alma").

**Flambagem lateral da alma no apoio (5.7.5)** — compressão na mesa do apoio com `ℓ` destravado: `F_Rd = C_r t_w³ t_f/(γ_a1 h²) [0,37 ((h/t_w)/(ℓ/b_f))³]` (rotação da mesa não impedida,
`(h/t_w)/(ℓ/b_f) ≤ 1,70`; `C_r = 16E` por prudência). Acima de 1,70 o estado-limite não ocorre.

**Soldas (6.2.5.2-b, -c)** — grupo concêntrico longitudinal + transversal: `F_w,Rd = máx(F_wl + F_wt ; 0,85 F_wl + 1,5 F_wt)`; carga excentrica no plano: método do centro
instantâneo de rotação (`Δ_ui = 1,087 (θ+6)^−0,65 w ≤ 0,17 w`, `Δ_mi = 0,209 (θ+2)^−0,32 w`, `f(p) = [p (1,9 − 0,9 p)]^0,3`), com solução do equilíbrio por varredura e bissecção.

**Flechas (Tabela B.1)** — viga horizontal sobre a qual nasce um pilar (nó livre com pilar acima e nenhum abaixo): `L/500`; demais horizontais `L/350`; inclinadas `L/250`.

**Apoio das ligações** — o usuário escolhe, entre as barras que chegam ao nó (exceto a própria), em qual a ligação se apoia (a barra fica destacada no modelo). A face do apoio é o plano da mesa
(viga chegando pela altura do perfil) ou da alma (pela largura), na posição real do eixo do apoio e da viga (excentricidade e giro da seção). Na verificação local do apoio somam-se os
esforços normais à face de todas as ligações que chegam à mesma barra de apoio no mesmo nó.
