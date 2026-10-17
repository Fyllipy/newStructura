# Dimensionamento de perfis formados a frio

### Como o Structura calcula: método da largura efetiva, flambagem distorcional por faixas finitas e verificações (ABNT NBR 14762:2010)

> Documento técnico-didático do programa **Structura** dedicado às barras de aço **formadas a frio**:
> perfis **U simples**, **U enrijecido (Ue)**, **Z**, **cantoneira (L)**, **cartola** e **tubulares retangulares/quadrados**
> (estes quando o projeto adota a NBR 14762 para tubulares).
> O companheiro é o livro de perfis laminados (`LIVRO_DIMENSIONAMENTO_PERFIS_LAMINADOS.md`, NBR 8800:2024), que contém a
> parte comum a todos os perfis: fluxo do solver ao veredito, combinações, amplificação B1/B2, 2ª ordem, flechas e o
> dimensionamento automático. **Este livro cobre o que é específico dos formados a frio.**
>
> As fórmulas descritas **são as implementadas** (`src/design/Nbr14762.cpp`, `src/design/FiniteStrip.cpp`). Os exemplos numéricos
> foram obtidos executando o próprio módulo de cálculo.

---

## Sumário

1. Escopo, coeficientes e fluxo de cálculo
2. Modelo de parede fina: geometria de linha média, torção e empenamento
3. Relação largura-espessura máxima (NBR 14762, 9.1.2 — Tabela 4)
4. Método da largura efetiva (MLE, 9.2)
5. Barras tracionadas (9.6)
6. Barras comprimidas (9.7), incluindo a flambagem distorcional
7. Barras fletidas (9.8.2): escoamento da seção efetiva, FLT e distorcional
8. Força cortante, momento + cortante e solicitações combinadas (9.8.3, 9.8.4, 9.9)
9. Flambagem elástica por faixas finitas (FSM): formulação e curva de assinatura
10. Dimensionamento automático de perfis formados a frio
11. Exemplos resolvidos
12. Hipóteses, limitações e pontos de atenção
13. Ligações de perfis formados a frio (NBR 14762, Seção 10)
14. Apêndices (tabelas 11 e 14, símbolos, mapa do código)

---

## Capítulo 1 — Escopo, coeficientes e fluxo de cálculo

### 1.1 Perfis cobertos

| Família (catálogo) | Tipo | Parâmetros |
|---|---|---|
| `U Formado` | U simples (sem enrijecedor de borda) | `d` altura, `bf` largura da mesa, `t` espessura |
| `C Formado` | **Ue** — U enrijecido | `d`, `bf`, `c` (enrijecedor), `t` |
| `Z Formado` | Z simples | `d`, `bf`, `t` |
| `L Formado` | cantoneira de abas iguais | `d`, `bf`, `t` |
| `Cartola Formado` | cartola (chapéu) | `d` altura, `bf` largura da base, `c` abas externas, `t` |
| `Tubular Quadrado`, `Tubular Retangular` | tubular fechado | `b`, `h`, `t` (norma conforme opção do projeto) |

A norma de cada tubular retangular/quadrado é uma **opção do projeto** (`NBR 14762` — padrão — ou `NBR 8800`); todos os demais perfis
desta lista são sempre verificados pela NBR 14762.

### 1.2 Coeficientes de ponderação da NBR 14762 usados pelo programa

| Estado-limite | γ |
|---|---|
| Tração: escoamento da seção bruta (9.6.2-a) | 1,10 |
| Tração: ruptura fora da ligação (9.6.2-b) | 1,35 |
| Tração: ruptura na região da ligação (9.6.2-c) | 1,65 |
| Compressão (9.7.2 e 9.7.3) | 1,20 |
| Flexão (9.8.2) e cortante (9.8.3) | 1,10 |

`E = 200 000 MPa`, `G = 77 000 MPa`, coeficiente de Poisson `ν = 0,3` (9.2.2.1).
`fy` e `fu` do aço (biblioteca de materiais: ZAR 230/280/345, ASTM, etc.).

### 1.3 Fluxo (idêntico ao dos laminados, com o ramo `cold`)

O orquestrador `designBar` é o mesmo da NBR 8800 (Capítulo 1 do livro de laminados): 21 seções por barra, todas as combinações últimas,
pior ocorrência por verificação, `η = demanda/resistência`. As diferenças, no ramo formado a frio:

```text
- tração, compressão e flexão vêm do módulo Nbr14762 (e a flexão em X/Y usa MLE e distorcional);
- inclui a verificação "Relação largura-espessura máxima" (9.1.2, Tabela 4), uma vez por barra;
- a flexão composta é LINEAR (9.9): η = N/N_Rd + Mx/Mx_Rd + My/My_Rd;
- inclui "M + V combinados" (9.8.4) no eixo X e no eixo Y;
- B1 (Anexo C) e B2 são aplicados como nos laminados (Cm, N_e do plano e B2 automático).
```

Os identificadores das verificações são: `geometry`, `tension`, `compression`, `slenderness`, `flexure_x`, `flexure_y`,
`shear_y`, `shear_z`, `interaction`, `mv_x`, `mv_y`.

### 1.4 Convenções

* `N > 0` tração; `Mx > 0` comprime a fibra superior (+b); `My > 0` comprime +a.
* **X** é o eixo horizontal (maior inércia nos perfis U/Ue/Z); **Y** o vertical.
* Flexão em torno de X → verifica-se FLT e distorcional; flexão em torno de Y → escoamento da seção efetiva (e distorcional).
* A seção é expressa nos **eixos principais**, que são os eixos locais do solver (a cantoneira L tem eixos principais inclinados; qualquer perfil com giro de posicionamento também é reexpresso).

---

## Capítulo 2 — Modelo de parede fina

Os perfis formados a frio são de **paredes finas**: o programa os calcula por uma **linha média** com espessura constante `t`.
`buildThinWalled` monta, para cada perfil, (a) a lista de **elementos planos** (largura efetiva de cada trecho) e (b) a polilinha
da linha média (seções abertas). Todas as coordenadas têm origem no centroide bruto e eixos principais.

### 2.1 Elementos planos por perfil

Cada elemento tem as extremidades `p1`, `p2` (início e fim da parte **plana**: a ligação dobra→reto é recuada aproximadamente `2t` do
vértice), a espessura, e as marcas: borda livre (`AL`, apoiado em um lado só) ou apoiado nos dois lados (`AA`), e se é uma
mesa com enrijecedor de borda simples (Ue e cartola).

| Perfil | Elementos (comprimento da parte plana) |
|---|---|
| **U simples** | alma (AA): `d − 4t` · mesa superior e inferior (AL): `bf − 2t` |
| **Ue** | alma (AA): `d − 4t` · mesas (AA com enrijecedor): `bf − 4t` · enrijecedores superior e inferior (AL): `c − t` |
| **Z** | alma (AA): `d − 4t` · mesas (AL): `bf − t` |
| **L** | aba vertical (AL): `d − 2t` · aba horizontal (AL): `bf − 2t` |
| **Cartola** | almas (AA): `d − 4t` · mesa inferior (AA): `bf − 4t` · abas superiores (AL): `c − t` |
| **Tubular** | mesas (AA, AA): `b − 5t` · almas (AA, AA): `h − 5t` (linha média `b − t`, recuo de `2t` em cada ponta) |

(Essas larguras planas aproximam a largura reta `b = b_externo − 2(r + t)`; a rotina não usa o raio de dobra: ele é implícito no recuo de `2t`.)

### 2.2 Constante de torção `J` e centro de cisalhamento

```text
seção aberta:   J = L_linha_média t³ / 3                (Σ comprimento da linha média × t³/3)
seção fechada:  J = 4 Am² t / U       com  Am = (b − t)(h − t),  U = 2[(b − t) + (h − t)]       (Bredt)
```

### 2.3 Empenamento `Cw` e centro de cisalhamento de seções abertas

Calculados numericamente sobre a polilinha da linha média, com integração de Gauss de 3 pontos em cada segmento:

```text
1) centroide da linha média (Ac, xc, yc), com t constante
2) coordenada setorial ω em relação ao polo = centroide:  ω_{k+1} = ω_k + (x_k y_{k+1} − x_{k+1} y_k)   (x, y relativos ao centroide)
3) integrais:  Ixx = ∫ y² dA,  Iyy = ∫ x² dA,  Ixy = ∫ x y dA,  Iωx = ∫ ω x dA,  Iωy = ∫ ω y dA
4) deslocamento do polo para o centro de cisalhamento (dx, dy):
        Iωx − dx·Ixy + dy·Iyy = 0       Iωy − dx·Ixx + dy·Ixy = 0
5) ω_S = ω − dx·y + dy·x ;  ω̄ = ∫ ω_S dA / A
6) Cw = ∫ (ω_S − ω̄)² dA
```

O centro de cisalhamento fica em `(x0, y0) = (xc + dx, yc + dy)` relativamente ao centroide. Seções fechadas têm `Cw = 0` e `x0 = y0 = 0`.

### 2.4 Perfis monossimétricos

* U, Ue: simétricos em relação ao eixo **X** (horizontal): o centro de cisalhamento está sobre X, a distância `x0` do centroide
  (para fora da alma). A flexo-torção acopla a flexão em torno de X com a torção.
* Cartola: simétrica em relação a **Y**.
* Z: simétrico em relação ao ponto (ponto-simétrico).
* L: assimétrico; usa-se o cúbico geral de flexo-torção.

---

## Capítulo 3 — Relação largura-espessura máxima (9.1.2, Tabela 4)

A cada barra, antes das demais verificações, o programa calcula para cada elemento `b/t` e compara com o máximo da Tabela 4. O
aproveitamento da verificação `geometry` é o **maior** `(b/t)/(b/t)máx`:

| Elemento | (b/t)máx |
|---|---|
| Elemento com borda livre (AL), p.ex. mesa de U, enrijecedor | 60 |
| AA com enrijecedor de borda e `Is ≥ Ia` (Ue) | 90 |
| AA com enrijecedor de borda e `Is < Ia` | 60 |
| Alma de U simples (sem enrijecedores de borda) | 90 |
| Alma de demais perfis (sem enrijecedores transversais) | 200 |
| Demais elementos AA (mesas de perfis abertos) | 500 |
| Tubular fechado (todos os elementos AA) | 500 |

Com `Is = t d_e³/12` (θ = 90°, `d_e = c − t`) e `Ia` da Seção 4.5. Essa verificação **não** depende dos esforços.

---

## Capítulo 4 — Método da largura efetiva (MLE, 9.2)

A resistência de um perfil de parede fina é governada pela **flambagem local**, que o MLE trata reduzindo a largura que realmente
colabora. Para cada elemento sob tensão normal de compressão `σ` (e uma variação linear dela ao longo do elemento):

### 4.1 Largura efetiva básica (9.2.2.1)

```text
λp = (b/t) / ( 0,95 √(k E / σ) )                 (esbeltez de placa reduzida)
b_ef = b                                  se λp ≤ 0,673
b_ef = b (1 − 0,22/λp) / λp               se λp > 0,673
```

onde `k` é o coeficiente de flambagem local do elemento (4.2 e 4.3) e `σ` a tensão máxima de compressão no elemento.

### 4.2 Elementos apoiados nos dois lados — AA (Tabela 5)

Tensões `σ1` (maior compressão) e `σ2` nas bordas; razão `ψ = σ2/σ1` (tração negativa):

```text
k = 4 + 2 (1 − ψ) + 2 (1 − ψ)³
b_ef,1 = b_ef / (3 − ψ)               b_ef,2 = b_ef − b_ef,1                              (ψ > −0,236)
b_ef,2 = b_ef / 2                      (limitada a b_c − b_ef,1)                          (ψ ≤ −0,236), b_c = b σ1/(σ1 − σ2)
```

A parte **não efetiva** é a faixa entre `b_ef,1` (contada a partir da borda mais comprimida) e `b − b_ef,2`. Elementos totalmente
tracionados (`σ ≤ 0` nos dois lados) são totalmente efetivos.

### 4.3 Elementos com borda livre — AL (Tabela 6)

Com `σ_ap` a tensão na borda apoiada e `σ_li` na borda livre:

```text
σ_ap ≥ σ_li (borda livre menos comprimida):   ψ = σ_li/σ_ap
      ψ = 1:           k = 0,43
      0 ≤ ψ < 1:       k = 0,578 / (ψ + 0,34)
      −1 ≤ ψ < 0:      k = 1,70 − 5 ψ + 17,1 ψ²        (parte da largura fica tracionada: b_c = b σ_ap/(σ_ap − σ_li); b_ef ≤ b_c)
σ_li > σ_ap (borda livre mais comprimida):    ψ = σ_ap/σ_li
      k = 0,57 − 0,21 ψ + 0,07 ψ²
```

A perda fica **do lado livre** do elemento (a parte efetiva está contígua à borda apoiada).

### 4.4 Enrijecedor de borda simples (9.2.3)

Aplica-se às mesas da Ue e da cartola (θ = 90° do enrijecedor). Para a tensão máxima `σ` na mesa:

```text
λp0 = (b/t) / ( 0,623 √(E/σ) )
λp0 ≤ 0,673:   mesa totalmente efetiva (b_ef = b), enrijecedor sem redução (d_s = d_ef)
λp0 > 0,673:
    Is = t d³ / 12                                     (d = c − t, enrijecedor reto)
    Ia = mín[ 399 t⁴ (0,487 λp0 − 0,328)³ ;  t⁴ (56 λp0 + 5) ]
    Is/Ia ≤ 1
    n  = máx(1/3 ; 0,582 − 0,122 λp0)
    d/b ≤ 0,25 :   k = 3,57 (Is/Ia)ⁿ + 0,43  ≤ 4
    d/b > 0,25 :   k = (4,82 − 5 d/b)(Is/Ia)ⁿ + 0,43  ≤ 4          (d/b usa d + t)
    b_ef = largura efetiva básica com esse k (4.1)
    b_ef,1 = (Is/Ia) b_ef / 2               b_ef,2 = b_ef − b_ef,1
```

O enrijecedor é tratado como elemento AL de `k = 0,43` sob `σ`, de largura efetiva `d_ef`; a largura efetiva do enrijecedor é reduzida
pela relação de rigidez: `d_s = mín(d_ef ; (Is/Ia) d_ef)`.
As perdas no programa são: na mesa, a faixa `[b_ef,2 ; b − b_ef,1]`; no enrijecedor, a parte `[d_s ; d]` da extremidade livre.

### 4.5 Propriedades efetivas da seção

A seção efetiva é obtida **subtraindo** as perdas da seção bruta (cada perda é uma tira de comprimento `L_p`, espessura `t`, centro
`(a_i, b_i)` e direção `u`):

```text
A_ef  = A − Σ ΔA_i                               ΔA_i = L_p t
centroide efetivo:  (a_c, b_c) = ( −Σ ΔA_i a_i / A_ef ,  −Σ ΔA_i b_i / A_ef )
Ix,ef = Ix − Σ [ ΔA_i b_i² + t L_p³ u_y²/12 ] − A_ef b_c²                       Iy,ef e Ixy,ef análogos
```

### 4.6 Seção efetiva em flexão (processo iterativo)

Em flexão simples em torno de um eixo com tensão máxima de compressão `σ_máx` na fibra extrema `x_c`:

```text
repetir (até 40 vezes):
    σ(x) = σ_máx (x − x_LN) / (x_c − x_LN)          campo linear de tensões em torno da linha neutra atual x_LN
    calcular as perdas de cada elemento com seu σ(x)  (4.2 a 4.4)
    novo x_LN = centroide da seção efetiva
até |Δ x_LN| < 10⁻⁶ mm
W_c,ef = I_ef / |x_c − x_LN|      W_t,ef = I_ef / |x_LN − x_t|
```

---

## Capítulo 5 — Barras tracionadas (9.6)

```text
N_t,Rd(esc)       = A fy / 1,10                                     (9.6.2-a)
N_t,Rd(rup,longe) = A fu / 1,35                                     (9.6.2-b)
N_t,Rd(rup,lig.)  = Ct An fu / 1,65  = Ct (An/A) A fu / 1,65        (9.6.2-c)
N_t,Rd = mín[ N_t,Rd(esc) ; mín(N_t,Rd(rup,longe) ; N_t,Rd(rup,lig.)) ]
```

`An/A` (por barra) e `Ct` são dados do projeto, com valor padrão 1,0. A esbeltez `L/r_mín ≤ 300` (9.6.3) é só recomendação (aviso).

---

## Capítulo 6 — Barras comprimidas (9.7)

### 6.1 Sequência de cálculo

```text
1) Cargas de flambagem elástica global:  N_ex, N_ey, N_ez  e, nos monossimétricos/assimétricos, a de flexo-torção  →  N_e
2) λ0 = √( A fy / N_e )                  χ = χ(λ0)   (flambagem global)
3) Seção efetiva para σ = χ fy (MLE, Cap. 4)  →  A_ef
4) N_c,Rd(global+local) = χ A_ef fy / γ           γ = 1,20
5) Flambagem distorcional (9.7.3)  →  N_c,Rd(dist)        (quando não dispensada)
6) N_c,Rd = mín[ N_c,Rd(global+local) ; N_c,Rd(dist) ]
7) Limite de esbeltez:  KL/r ≤ 200   (η = KL/r / 200)
```

### 6.2 Flambagem global (9.7.2)

```text
N_ex = π² E Ix / Lx²        N_ey = π² E Iy / Ly²
N_ez = [ π² E Cw / Lz² + G J ] / r0²       r0² = rx² + ry² + x0² + y0²          (Cw e J do modelo de parede fina, Cap. 2)
```

Em seções **fechadas** (tubular): `x0 = y0 = 0`, `Cw = 0`, `N_ez = G J/r0²`.

* **Duplamente simétricas (tubular):** `N_e = mín(N_ex ; N_ey ; N_ez)`.
* **Monossimétricas** (U, Ue em torno do eixo de simetria X; cartola em torno de Y), 9.7.2.2:

```text
ρ    = 1 − (d0/r0)²          d0 = x0 (U, Ue)  ou  y0 (cartola)
N_ft = (N_sim + N_ez) / (2 ρ) · [ 1 − √( 1 − 4 N_sim N_ez ρ / (N_sim + N_ez)² ) ]
N_e  = mín( N_outro ; N_ft )
```

  `N_sim` é a carga de Euler da flexão **em torno do eixo de simetria** (N_ex no U/Ue, N_ey na cartola) e `N_outro` a do eixo perpendicular.
* **Assimétricos** (cantoneira L formada a frio, 9.7.2.3): `N_e` é a menor raiz positiva do cúbico
  `(N_e − N_ex)(N_e − N_ey)(N_e − N_ez) − N_e²(N_e − N_ey)(x0/r0)² − N_e²(N_e − N_ex)(y0/r0)² = 0`.
* **Perfil Z:** sem acoplamento específico: usa-se `mín(N_ex ; N_ey ; N_ez)`.

Fator de redução (9.7.2):

```text
λ0 ≤ 1,5 :  χ = 0,658^(λ0²)           λ0 > 1,5 :  χ = 0,877 / λ0²
```

### 6.3 Flambagem local

Calcula-se a seção efetiva com **σ = χ fy** em todos os elementos (compressão uniforme) pelo MLE (Cap. 4):

```text
Aef = A − Σ perdas
N_c,Rd(global+local) = χ Aef fy / 1,20
```

### 6.4 Flambagem distorcional (9.7.3)

**Dispensa (Tabela 11).** Para o **Ue** (e só para ele), a flambagem distorcional é dispensada quando

```text
D/bw ≥ valor da Tabela 11 interpolado em  (bf/bw , bw/t)         D = c (enrijecedor),  bw = d (altura),  bf = largura da mesa
```

(tabela e interpolação no Apêndice 13.1; `bf/bw` e `bw/t` são limitados à faixa da tabela).

**Cálculo.** Caso não dispensada, obtém-se por **faixas finitas** (Cap. 9) o fator de carga distorcional `μ_dist` sob compressão
uniforme de 1 MPa na seção:

```text
N_dist = μ_dist · A            (carga de flambagem elástica distorcional)
λ_dist = √( A fy / N_dist )
χ_dist = 1                                   se λ_dist ≤ 0,561
χ_dist = (1 − 0,25/λ_dist^1,2) / λ_dist^1,2  se λ_dist > 0,561
N_c,Rd(dist) = χ_dist A fy / 1,20
```

Se a curva de assinatura **não tem mínimo distorcional** (perfis abertos de enrijecimento fraco, seções fechadas), o programa registra
a observação "sem modo distorcional" e considera apenas a flambagem global/local.

### 6.5 Marcha de cálculo — Ue 200 × 75 × 25 × 2,65 em compressão

Aço ZAR 345 (fy = 345 MPa, fu = 450 MPa), L = 3000 mm (Kx = Ky = Kz = 1), `N_c,Sd = 30 kN`. Propriedades do programa:
`A = 1031,7 mm²`, `Ix = 6,320·10⁶`, `Iy = 8,292·10⁵ mm⁴`, `rx = 78,27`, `ry = 28,35 mm`, `J = 2448 mm⁴`, `Cw = 7,429·10⁹ mm⁶`,
`x0 = −71,9 mm` (centro de cisalhamento fora da alma).

```text
N_ex = π² E Ix/L² = 1386,1 kN      N_ey = 181,9 kN
r0² = 78,27² + 28,35² + 71,9² = 12 100 mm² (r0 = 110,0 mm)
N_ez = [π² E Cw/L² + G J]/r0² = [1,629·10⁹ + 1,885·10⁸]/12 100 = 150,2 kN
ρ = 1 − (71,9/110,0)² = 0,573
N_ft = (1386,1 + 150,2)/(2 × 0,573) [1 − √(1 − 4 × 1386,1 × 150,2 × 0,573/(1536,3)²)] = 143,1 kN
N_e = mín(N_ey = 181,9 ; N_ft = 143,1) = 143,1 kN (flexo-torção)

λ0 = √(1031,7 × 345 / 143 130) = 1,577 > 1,5    →  χ = 0,877/1,577² = 0,353
σ = χ fy = 121,7 MPa
Alma (AA, b = 189,4; t = 2,65; b/t = 71,5;  ψ = 1;  k = 4):
   λp = 71,5 / (0,95 √(4·200000/121,7)) = 0,928 > 0,673  →  b_ef = 189,4 (1 − 0,22/0,928)/0,928 = 155,7 mm   (perda 33,7 × 2,65 = 89,3 mm²)
Mesas com enrijecedor (b = 64,4 mm; d = c − t = 22,35 mm):
   λp0 = (64,4/2,65)/(0,623 √(200000/121,7)) = 0,962 > 0,673
   Is = 2,65 × 22,35³/12 = 2465 mm⁴ ; Ia = mín[399 × 2,65⁴ (0,487 × 0,962 − 0,328)³ ; 2,65⁴ (56 × 0,962 + 5)] = 54,6 mm⁴ → Is/Ia = 1
   d/b = 25/64,4 = 0,388 > 0,25 → k = (4,82 − 5 × 0,388) × 1 + 0,43 = 3,31
   λp = 24,3/(0,95 √(3,31 × 200000/121,7)) = 0,347 ≤ 0,673 → b_ef = 64,4 mm (mesa efetiva);  enrijecedor: k = 0,43 → d_ef = d = 22,35 mm
A_ef = 1031,7 − 89,2 = 942,5 mm²
N_c,Rd(global+local) = 0,353 × 942,5 × 345/1,20 = 95,6 kN
Distorcional: D/bw = 25/200 = 0,125 ≥ Tab. 11(bf/bw = 0,375 ; bw/t = 75,5) = 0,060  →  DISPENSADA
N_c,Rd = 95,6 kN          η = 30/95,6 = 0,314
KL/r = 3000/28,35 = 105,8 ≤ 200  →  η = 0,529
```

(resultado do programa: `N_c,Rd = 95,56 kN`, `η = 0,314`, esbeltez `η = 0,529`.)

---

## Capítulo 7 — Barras fletidas (9.8.2)

O momento resistente é o menor entre: **escoamento da seção efetiva** (início de escoamento), **FLT** e **flambagem distorcional**.
Dividido por `γ = 1,10` nas fórmulas abaixo.

### 7.1 Início de escoamento da seção efetiva (9.8.2.1)

```text
1) σ_c = fy na fibra comprimida extrema;  seção efetiva em flexão (4.6)  →  W_c,ef, W_t,ef
2) a fibra tracionada tem σ_t = σ_c · W_c,ef / W_t,ef.  Se σ_t > fy:  σ_c = fy W_t,ef / W_c,ef  e recalcula (até 6 vezes)
3) M_y = mín( W_c,ef σ_c ; W_t,ef fy )
M_Rd(esc) = M_y / 1,10
```

(O programa **não** considera reserva plástica: o momento resistente é o de início de escoamento da seção efetiva.)

### 7.2 Flambagem lateral com torção — FLT (9.8.2.2)

Aplica-se em flexão em torno de **X** com `Lb > 0` e `Ix > Iy`. O momento crítico elástico `Me` depende do perfil
(com `N_ey = π² E Iy/Lb²` e `N_ez = (π² E Cw/Lb² + G J)/r0²`):

| Perfil | Me |
|---|---|
| Seção fechada (tubular) | `Cb √(N_ey G J)` |
| U, Ue (flexão em torno do eixo de simetria) | `Cb r0 √(N_ey N_ez)` |
| Z (ponto-simétrico) | `0,5 Cb r0 √(N_ey N_ez)` |
| Cartola (monossimétrica fletida em torno do eixo perpendicular ao de simetria — Anexo E) | `Cs (N_ex/Cm)[ j + Cs √(j² + r0² N_ez/N_ex) ]`, com `N_ex = π² E Iy/Lb²` (em torno do eixo de simetria), `j = (∫ (b³ + b a²) t ds)/(2 Ix) + y0` e `Cs = ±1` conforme a fibra comprimida esteja do lado do centro de cisalhamento; `Cm = 1` |
| Cantoneira L | não há `Me` na norma para esta seção: FLT **não verificada** (registrada na memória) |

Depois:

```text
λ0 = √( W_c fy / Me )                 (W_c: módulo elástico BRUTO da fibra comprimida)
χ_FLT = 1                                  se λ0 ≤ 0,60
χ_FLT = 1,11 (1 − 0,278 λ0²)               se 0,60 < λ0 < 1,336
χ_FLT = 1 / λ0²                            se λ0 ≥ 1,336

σ = χ_FLT fy → seção efetiva (4.6) → W_c,ef
M_Rd(FLT) = χ_FLT W_c,ef fy / 1,10
```

`Cb` é o do Capítulo 1 do livro de laminados (por trecho destravado, de 1,0 a 3,0).

### 7.3 Flambagem distorcional (9.8.2.3)

**Dispensa (Tabela 14)**: só para Ue fletido em torno de X: dispensada se `D/bw ≥ Tab. 14(bf/bw ; bw/t)`.

**Cálculo**: o fator de carga distorcional `μ_dist` por faixas finitas (Cap. 9), com o campo de tensões **da flexão**
(normalizado de modo que a fibra comprimida extrema valha 1):

```text
M_dist = μ_dist · W_c           (W_c: módulo elástico bruto da fibra comprimida)
λ_dist = √( W_c fy / M_dist )
χ_dist = 1                                  se λ_dist ≤ 0,673
χ_dist = (1 − 0,22/λ_dist) / λ_dist         se λ_dist > 0,673
M_Rd(dist) = χ_dist W_c fy / 1,10
```

Seções fechadas não têm modo distorcional (curva de assinatura sem segundo mínimo).

### 7.4 Momento resistente

```text
M_Rd = mín[ M_Rd(esc) ; M_Rd(FLT) ; M_Rd(dist) ]
```

O estado-limite governante é informado ("escoamento da seção efetiva", "FLT", "flambagem distorcional").

### 7.5 Marcha de cálculo — Ue 200 × 75 × 25 × 2,65 em flexão

Viga biapoiada, L = 3000 mm, carga uniforme de cálculo `w = 4 kN/m`, **dois trechos** destravados (`Lb = 1500 mm`).
`M_Sd = w L²/8 = 4,5 kN·m`, `V_Sd = 6 kN`. Do Cap. 6: `W_c = 63 200 mm³`, `r0 = 110,0 mm`, `Cw = 7,429·10⁹`, `J = 2448`.

```text
Escoamento: σ_c = 345 MPa; mesa superior com enrijecedor: λp0 = 1,609 → Is/Ia = 1, k = 3,31, λp = 0,584 ≤ 0,673 → b_ef = b (mesa efetiva);
            alma (ψ ≈ −1, k = 24): λp = 0,621 ≤ 0,673 → efetiva.  Seção toda efetiva: W_ef = W = 63 200 mm³
            M_Rd(esc) = 63 200 × 345/1,10 = 19,82 kN·m
FLT:        N_ey = π² E Iy/1500² = 727,4 kN ; N_ez = (π² E Cw/1500² + G J)/r0² = (6,517·10⁹ + 1,885·10⁸)/12 100 = 553,8 kN
            Cb = 1,30 ;  Me = 1,30 × 110,0 × √(727,4·10³ × 553,8·10³) = 90,85 kN·m
            λ0 = √(63 200 × 345/90,85·10⁶) = 0,490 ≤ 0,60 → χ_FLT = 1 → M_Rd(FLT) = 19,82 kN·m
Distorcional: Tab. 14 em (bf/bw = 0,375 → 0,40 ; bw/t = 75,5): entre as colunas 100 (0,12) e 50 (0,25): 0,12 + 0,49 (0,25 − 0,12) = 0,184
              D/bw = 0,125 < 0,184  →  NÃO dispensada.  O programa calcula μ_dist por FSM (Cap. 9): M_dist = 45,26 kN·m (meia-onda de 695 mm)
λ_dist = √(63 200 × 345 / 45,26·10⁶) = 0,694 > 0,673  →  χ_dist = (1 − 0,22/0,694)/0,694 = 0,984
M_Rd(dist) = 0,984 × 63 200 × 345/1,10 = 19,51 kN·m
M_Rd = mín(19,82 ; 19,82 ; 19,51) = 19,51 kN·m  (flambagem distorcional)      η = 4,5/19,51 = 0,231
```

---

## Capítulo 8 — Cortante e solicitações combinadas

### 8.1 Força cortante (9.8.3)

Almas sem enrijecedores transversais (`kv = 5,0`). Altura da parte plana `h` e número de almas `n_a`:

| Perfil | h | n_a |
|---|---|---|
| U, Ue, Z | `d − 4t` | 1 |
| Cartola | `d − 4t` | 2 |
| Tubular | altura (ou largura) plana `= d − 4t` (ou `b − 4t`) | 2 |
| L | `d − 2t` | `√2` (duas abas a 45° dos eixos principais) |

Não há cortante `Vz` para U/Ue/Z/cartola (as mesas não são verificadas ao cortante): `covered = false`.

```text
λ  = h/t                  λ1 = 1,08 √(kv E/fy)          λ2 = 1,40 √(kv E/fy)
λ ≤ λ1 :        V_Rd,alma = 0,60 fy h t / 1,10
λ1 < λ ≤ λ2 :   V_Rd,alma = 0,65 t² √(kv fy E) / 1,10
λ > λ2 :        V_Rd,alma = 0,905 E kv t³ / (h · 1,10)
V_Rd = n_a · V_Rd,alma
```

**Exemplo (Ue 200):** `h = 200 − 4 × 2,65 = 189,4`, `h/t = 71,5`; `λ1 = 58,1`; `λ2 = 75,4`:
`V_Rd = 0,65 × 2,65² √(5 × 345 × 200000)/1,10 = 77,08 kN`; `V_Sd = 6 kN` → `η = 0,078`.

### 8.2 Momento fletor e força cortante combinados (9.8.4)

Para almas sem enrijecedores, nas seções com M e V simultâneos (avaliado em cada uma das 21 seções, nos eixos X e Y):

```text
(M_Sd / M_Rd)² + (V_Sd / V_Rd)² ≤ 1,0
```

**Exemplo (Ue 200, x = 1350 mm):** `(0,228)² + (0,008)² = 0,052`.

### 8.3 Solicitações combinadas — normal + flexão (9.9)

Interação **linear**, com `N_Rd` = `N_t,Rd` (se N > 0) ou `N_c,Rd` (se N < 0) e `M_Sd` já amplificado por B1 e B2:

```text
η = N_Sd/N_Rd + Mx,Sd/Mx,Rd + My,Sd/My,Rd  ≤ 1,0
```

(Os momentos de projeto incluem `B1 = máx(1; Cm/(1 − N_c,Sd/N_e,plano))` e `B2` conforme o livro de laminados.)

---

## Capítulo 9 — Flambagem elástica por faixas finitas (FSM)

A NBR 14762 manda obter os modos **distorcionais** por *análise de estabilidade elástica* (9.7.3 e 9.8.2.3). O programa implementa o
**método das faixas finitas semianalítico** (`FiniteStrip.cpp`, com a biblioteca Eigen): a seção é dividida em faixas longitudinais e
a variação ao longo do comprimento é uma **única meia-onda senoidal** de comprimento `a` (extremidades simplesmente apoiadas).

### 9.1 Modelo

* **Nós**: a polilinha da linha média (o mesmo modelo de parede fina do Cap. 2, **incluindo os enrijecedores**). Cada nó tem 4 graus
  de liberdade globais: `U_a` (deslocamento na direção da seção), `V` (longitudinal), `W_b` (transversal ao plano) e `θ` (giro).
* **Faixas**: pares de nós consecutivos, de largura `b` e espessura `t`; o tubular fechado liga o último nó ao primeiro.
* **Tensões**: `σ(ξ)` linear entre os valores nodais (compressão positiva), com `ξ ∈ [0,1]` ao longo da largura da faixa:
  * compressão axial: `σ = 1 MPa` em todos os nós (fator de carga = tensão crítica em MPa);
  * flexão: `σ_i = x_i / x_c` (distância do nó ao eixo neutro dividido pela da fibra extrema comprimida; a fibra extrema vale 1).

### 9.2 Matrizes da faixa (8 graus de liberdade locais: `u1 v1 w1 θ1 u2 v2 w2 θ2`)

Com `k = π/a`, módulos de placa

```text
D_m = E t / (1 − ν²)          G_m = E t / (2 (1 + ν))          D_b = E t³ / (12 (1 − ν²))
```

funções de forma: lineares em `u` e `v` e **hermitianas cúbicas** em `w`; integração de Gauss de 4 pontos na largura.

```text
Rigidez elástica:   K = ∫ [ D_m (ε_x ε_xᵀ + ε_y ε_yᵀ + ν (ε_x ε_yᵀ + ε_y ε_xᵀ)) + G_m γ γᵀ
                          + D_b (κ_x κ_xᵀ + κ_y κ_yᵀ + ν (κ_x κ_yᵀ + κ_y κ_xᵀ) + ½ (1 − ν) κ_xy κ_xyᵀ) ] dA
Rigidez geométrica: G = ∫ k² t σ(ξ) ( N_u N_uᵀ + N_v N_vᵀ + N_w N_wᵀ ) dA
```

Cada faixa é girada para o referencial global da seção (ângulo `α` da faixa: `cos α`, `sen α`) e somada em `K` e `G` globais.

### 9.3 Autoproblema

```text
G d = μ K d          μ_máx > 0           fator de carga λ(a) = 1 / μ_máx
```

A solução usa o autoproblema simétrico generalizado (Eigen). O **maior** `μ` positivo dá o **menor** fator de carga de flambagem.

### 9.4 Curva de assinatura e identificação do modo distorcional

O programa varre o comprimento de meia-onda `a` em **70 valores** igualmente espaçados em logaritmo, de
`a_mín = máx(5 mm ; 3t)` até `a_máx = máx(3000 mm ; 30 × maior dimensão da seção)`, obtendo `λ(a)`. Os **mínimos locais** da curva
(`λ_i < λ_{i−1}` e `λ_i ≤ λ_{i+1}`) correspondem aos modos:

```text
primeiro mínimo   →  modo LOCAL (meia-onda curta)
segundo mínimo    →  modo DISTORCIONAL, se a sua meia-onda é > 2 × a do local
```

O fator distorcional `μ_dist` usado nas verificações é o valor `λ` desse mínimo. Se a curva só tem um mínimo (ou nenhum), o programa
registra "sem modo distorcional" (seções fechadas e perfis abertos sem enrijecimento fraco).

> **Limites desta implementação:** harmônica única e apoios simples nas extremidades (o que é a hipótese da curva de assinatura
> clássica); sem raios de dobra (cantos vivos na linha média); a identificação do mínimo distorcional é heurística (a primeira
> meia-onda maior que o dobro da local). Os testes comparam o método com soluções fechadas de flambagem de placa, mas **não há
> comparação sistemática com outro programa** (p.ex. CUFSM).

---

## Capítulo 10 — Dimensionamento automático de perfis formados a frio

O algoritmo é o do Capítulo 11 do livro de laminados (`chooseBest` do mais leve ao mais pesado, laço com reanálise,
piso de inércia para as flechas). Para os formados a frio, a **série candidata** é:

* o catálogo `data/catalog_steel.json` da família (Ue, U, Z, L formado, cartola, tubulares) — compilado de séries comerciais
  (NBR 6355 / NBR 8261 / NBR 16239); **confirme a série e a espessura com o fabricante**;
* quando a família não tem catálogo, séries paramétricas: Ue/U/Z/cartola (`d × bf × c` de 75×40×15 a 400×100×25, espessuras
  de 1,2 a 6,5 mm), L formado (abas de 25 a 100 mm), tubulares quadrado, retangular e circular com espessuras comerciais.

Cada candidato passa por **todas** as verificações deste livro (inclusive o FSM), de modo que a escolha é consistente com a verificação.

---

## Capítulo 11 — Exemplos resolvidos

### 11.1 Ue 200 × 75 × 25 × 2,65 — compressão e flexão

Marchas completas nos Capítulos 6.5, 7.5 e 8.1/8.2. Resumo dos resultados do programa:

| Verificação | Solicitação | Resistência | η | Observação |
|---|---|---|---|---|
| Compressão (L = 3 m) | 30,0 kN | 95,56 kN | 0,314 | flexo-torção governa `N_e` |
| Esbeltez (KL/r) | 105,8 | 200 | 0,529 | |
| Flexão (L = 3 m, Lb = 1,5 m) | 4,50 kN·m | 19,51 kN·m | 0,231 | flambagem distorcional |
| Cortante | 6,0 kN | 77,08 kN | 0,078 | |
| M + V | — | — | 0,052 | |
| Largura-espessura | — | — | 0,357 | alma: 71,5/200 |

### 11.2 Tubular retangular 100 × 60 × 3,0 — flexo-compressão (NBR 14762)

Aço fy = 345 MPa, L = 2500 mm (`K = 1`), `N_c,Sd = 40 kN`, `Mx` de 2 a 3 kN·m (sem carga transversal), `Vy = 3 kN`.
Propriedades: `A = 900,6 mm²`, `Ix = 5,463·10⁵`, `Iy = 1,205·10⁶ mm⁴`, `W_x = 18 209 mm³`, `J = 1,191·10⁶ mm⁴`.

```text
Compressão:   N_ex = π² E Ix/L² = 172,5 kN (menor);  N_ey = 380,6 kN;  N_ez = G J/r0² = 47 156 kN        r0 = 44,1 mm
              λ0 = √(900,6 × 345/172 530) = 1,342;  χ = 0,658^(1,342²) = 0,471;   σ = 0,471 × 345 = 162,4 MPa
              mesa: b = 100 − 15 = 85 mm, b/t = 28,3: λp = 28,3/(0,95 √(4 × 200000/162,4)) = 0,425 ≤ 0,673 → efetiva; alma idem → A_ef = A
              N_c,Rd = 0,471 × 900,6 × 345/1,20 = 121,8 kN           η_N = 40/121,8 = 0,328
              (seção fechada: sem flambagem distorcional)
Flexão (X):   seção totalmente efetiva (σ = 345: mesa λp = 0,618 ≤ 0,673; alma b/t = 15)  →  W_ef = W = 18 209 mm³
              M_Rd = 18 209 × 345/1,10 = 5,71 kN·m
B1:           Cm = 0,60 − 0,40 × (−2/3) = 0,867  (extremidades de mesmo sinal) ;  B1 = 0,867/(1 − 40/172,5) = 1,128 ;  Mx,Sd = 3,00 × 1,128 = 3,38 kN·m   η_M = 0,593
Interação (9.9): η = 0,328 + 0,593 = 0,921
Cortante:     h = 60 − 4 × 3 = 48 mm, h/t = 16 ≤ λ1 = 58,1 → V_Rd = 2 × 0,60 × 345 × 48 × 3/1,10 = 54,2 kN   η = 0,055
M + V:        (0,593)² + (0,055)² = 0,354
```

Resultado do programa: governa a **flexo-compressão** (`η = 0,921`); atende.
Observação: neste perfil `Ix < Iy`, pois a "altura" (60 mm) é menor que a "largura" (100 mm); a flexão em X é a de **menor** inércia
e por isso não há FLT.

### 11.3 Z 150 × 60 × 2,0 — viga que não atende (FLT)

Viga biapoiada, L = 3000 mm, sem contenção intermediária (`Lb = L`), `w = 2 kN/m`: `M_Sd = 2,25 kN·m`, `V_Sd = 3 kN`.
Propriedades: `A = 534,8 mm²`, `Ix = 1,848·10⁶`, `Iy = 3,027·10⁵ mm⁴`, `Wx = 24 638 mm³`, `J = 709,3 mm⁴`.

```text
Escoamento da seção efetiva (σ = 345 MPa):
   alma (AA): ψ = −0,65, k = 16,36, λp = 0,750  → b_ef = 133,8 mm de 142,0 mm
   mesa comprimida (AL): k = 0,43, λp = 1,923  → b_ef = 26,7 mm de 58,0 mm
   W_ef = 15 496 mm³ (do bruto 24 638)   M_Rd(esc) = 15 496 × 345/1,10 = 4,86 kN·m
FLT (Z ponto-simétrico):  Cb = 1,14 ;  Me = 0,5 Cb r0 √(N_ey N_ez) = 2,52 kN·m
   λ0 = √(24 638 × 345/2,52·10⁶) = 1,837 ≥ 1,336  →  χ_FLT = 1/1,837² = 0,296
   σ = 0,296 × 345 = 102,1 MPa → W_c,ef = 21 271 mm³ ;  M_Rd(FLT) = 0,296 × 21 271 × 345/1,10 = 1,98 kN·m
Distorcional: a curva de assinatura só tem os modos local e global (Z sem enrijecedor) → não há modo distorcional
M_Rd = mín(4,86 ; 1,98) = 1,98 kN·m (FLT)          η = 2,25/1,98 = 1,139   →   NÃO ATENDE
```

(`M + V`: `η = 1,271`, porque o termo `(M/M_Rd)² = 1,127² > 1`.) Para atender: contenção lateral (menor `Lb`), perfil mais alto, ou a série
automática troca por um Z de maior espessura.

---

## Capítulo 12 — Hipóteses, limitações e pontos de atenção

1. **Validação:** os testes automáticos conferem fórmulas e resultados à mão; **não há validação cruzada** com outro programa.
2. **Plastificação parcial não é usada:** o momento resistente é o de **início de escoamento da seção efetiva** (9.8.2.1), conservador.
3. **Larguras planas:** os elementos usam recuo de `2t` nas extremidades; o raio de dobra não entra no cálculo. Isso pode diferir do
   detalhamento exato das larguras planas (`b − 2(r + t)`), para mais ou para menos conforme a relação r/t do perfil.
4. **Flambagem distorcional por FSM** de harmônica única; sem validação sistemática (Cap. 9). Seções fechadas: sem modo distorcional.
5. **Cantoneira L formada a frio:** não há `Me` na norma: **FLT não é verificada**; cortante com as duas abas a 45°.
6. **Cortante:** só almas sem enrijecedores (`kv = 5`); **não há** verificação de **furos na alma** nem de **enrijecedores transversais**.
   **Esmagamento da alma (Anexo D da NBR 14762:2010):** `F_Rd = α t² fy sen θ (1 − αr √(ri/t))(1 + αc √(c/t))(1 − αh √(h/t))/γ`, `γ = 1,35`,
   `θ = 90°`, `c ≥ 20 mm`, `ri = t`, `h = d − 2(ri + t)`; campo de validade `h/t ≤ 200`, `c/t ≤ 210`, `c/h ≤ 2`. Implementado para o **U simples**
   e o **U enrijecido** (Tabela D.2, mesas não conectadas ao apoio, uma mesa carregada): U simples — extremidade `α = 4,6; αr = 0,40; αc = 0,60; αh = 0,03`,
   interna `14,9; 0,32; 0,10; 0,01`; U enrijecido — extremidade `4,3; 0,14; 0,35; 0,02`, interna `15,8; 0,23; 0,14; 0,01`; **Z** (sem enrijecedor, Tabela D.3) —
   mesmos coeficientes do U simples; **cartola** (Tabela D.4, duas almas somadas) — extremidade `4; 0,25; 0,68; 0,04`, interna `19,5; 0,13; 0,13; 0,04`.
   Como o comprimento de apoio `c` não é conhecido pelo módulo, o resultado **não reprova** a barra: aparece como **aviso** com `F_Rd` para um apoio de 50 mm
   (força de extremidade) comparado à reação máxima (cortante nas extremidades). Cantoneira formada a frio não é coberta (sem coeficientes na norma).
7. **Torção:** não é verificada nos formados a frio (nem o empenamento). Há **aviso** se `|T| > 0,1 kN·m`.
8. **Flexão composta linear (9.9)** sem a verificação adicional de excentricidade do centroide efetivo (a seção efetiva muda de centroide
   com N): o programa soma as razões calculadas com as resistências independentes.
9. **Flechas (ELS):** a rigidez à flexão das barras formadas a frio é reduzida pela razão `Ief/I` da **seção efetiva** (NBR 14762, 9.8.5-a, MLE), calculada
   por eixo com a tensão normal de serviço `σn = Mn/Wc`, `Mn` o maior momento da barra nas combinações de serviço (a pior das duas fibras extremas). Os
   **deslocamentos de andar** continuam usando a inércia bruta do solver (a análise estrutural é feita com a seção bruta): podem estar levemente
   subestimados em perfis de paredes esbeltas.
10. **Ligações de perfis formados a frio:** há o tipo "Perfil formado a frio (NBR 14762)" (Capítulo 13). Não há ligação rígida viga–pilar, soldas de
    penetração, soldas em superfícies curvas (10.2.4), ligações de barras compostas nem a verificação por ensaio (Seção 11).
11. **Esbeltez de tração** (9.6.3) é apenas aviso.
12. **Amplificação B1/B2** vale para os formados a frio com `N_e` calculado com as inércias **brutas**.
13. **Catálogo de perfis**: compilado de séries comerciais; confirme dimensões e espessuras com o fabricante.

---

## Capítulo 13 — Ligações de perfis formados a frio (NBR 14762:2010, Seção 10)

Tipo de ligação **"Perfil formado a frio (NBR 14762)"** (grupo "Perfis formados a frio" na escolha da tipologia): o perfil (espessura `t1`) é sobreposto a uma
chapa de ligação (`t2`) por parafusos comuns, solda de filete ou parafusos autoatarraxantes. **Aplicável a `t ≤ 4,75 mm` da parte mais fina** (10.2.1 e 10.3.1);
acima disso a verificação acusa a violação e valem as exigências da NBR 8800. A força de cálculo é a maior entre tração, compressão e cortante da extremidade;
se informada a resistência de cálculo da barra, a ligação é dimensionada **no mínimo para 50 %** dela (10.1-a). A ligação **não tem proposta automática**.

### 13.1 Parafusos comuns (10.3)

```text
Furos (Tab. 15): dh = d + 0,8 mm (d < 12,5)  |  d + 1,5 mm (d ≥ 12,5)
Distância livre entre furos ≥ 2 d;  borda do furo à extremidade ≥ d                                   (10.3.3.1)
Rasgamento entre furos/borda:  F_Rd = t e fu / 1,45                      (por parafuso, para o perfil e para a chapa)   (10.3.4)
Pressão de contato:            F_Rd = αe d t fu / 1,55,   αe = 0,183 t + 1,53  (t em mm, t ≤ 4,75)                        (10.3.5)
Cisalhamento do parafuso:      F_v,Rd = 0,4 Ab fub / 1,35 (rosca no plano) | 0,5 Ab fub / 1,35 (fora da rosca), por plano  (10.3.7)
Tração:                        F_t,Rd = 0,75 Ab fub / 1,35;   interação (Ft,Sd/Ft,Rd)² + (Fv,Sd/Fv,Rd)² ≤ 1       (10.3.6 e 10.3.8-a)
Colapso por rasgamento (≥ 2 linhas): F_Rd = mín[(0,6 fu Anv + fu Ant) ; (0,6 fy Agv + fu Ant)]/1,65, no perfil           (10.4)
```

Os esforços são distribuídos igualmente entre os `n × linhas` parafusos.

### 13.2 Solda de filete em superfícies planas (10.2.3)

Com `t = mín(t1, t2)`, perna `w` (limitada a `t1` em juntas sobrepostas) e `t_ef = 0,7 w`:

```text
Paralela ao eixo da solda:  L/t < 25:  F_Rd = (1 − 0,01 L/t) t L fu / 1,65      |   L/t ≥ 25:  F_Rd = 0,75 t L fu / 2,00
Normal ao eixo da solda:    F_Rd = t L fu / 1,55
Ruptura da solda (t > 2,5 mm), além das anteriores:   F_Rd ≤ 0,75 t_ef L fw / 1,65
```

A resistência é a soma das parcelas longitudinal e transversal; o comprimento total mínimo adotado é `4 w` (boa prática, não fixado pela norma).

### 13.3 Parafusos autoatarraxantes (10.5)

`2,00 ≤ d ≤ 6,35 mm`; centros ≥ `3 d`; centro à borda ≥ `1,5 d`; `γ = 2,00`.

```text
t2/t1 ≤ 1,0:   F_c,Rd = mín[4,2 (t2³ d)^0,5 fu2 ; 2,7 t1 d fu1 ; 2,7 t2 d fu2] / γ
t2/t1 ≥ 2,5:   F_c,Rd = mín[2,7 t1 d fu1 ; 2,7 t2 d fu2] / γ          (1,0 < t2/t1 < 2,5: interpolação linear)
Rasgamento até a borda:  F_c,Rd ≤ t e fu / γ
Cisalhamento do parafuso: F_ss,Rd = F_ss,Rk / 2,00            (F_ss,Rk do fabricante; se não informado, a verificação é omitida e o memorial avisa)
Tração (se informada): F_a,Rd = 0,85 tc d fu2/γ (arrancamento); F_r,Rd = 1,5 t1 dwe fu1/γ (rasgamento do metal-base);  F_ts,Rd = F_ts,Rk/γ
Cisalhamento + rasgamento do metal-base (10.5.4): Fv,Sd/Fc0,Rd + 0,71 Ft,Sd/Fr0,Rd ≤ 1,10   (γ = 1,55; só no campo de validade da norma)
```

### 13.4 Exemplo

Perfil `t1 = 2,0 mm` (`fu = 450 MPa`) com chapa `t2 = 4,75 mm`, dois parafusos `d = 12 mm` comuns (`fub = 400 MPa`, rosca no plano), `e = 30 mm`:
`F_v,Rd = 0,4 × 113,1 × 400/1,35 = 13,4 kN` por parafuso; rasgamento no perfil `2,0 × 30 × 450/1,45 = 18,6 kN`; contato no perfil
`(0,183×2,0 + 1,53) × 12 × 2,0 × 450/1,55 = 13,2 kN`. A ligação resiste a `2 × 13,2 = 26,4 kN` (contato governa).

---

## Capítulo 14 — Apêndices

### 13.1 Tabelas 11 e 14 (dispensa da flambagem distorcional — perfil Ue)

Valores de `D/bw` mínimos (D = largura do enrijecedor; bw = altura da alma; bf = largura da mesa), por `bf/bw` e `bw/t`
(interpolação bilinear; `bf/bw` limitado a 0,4–2,0 e `bw/t` a 50–250):

**Tabela 11 — compressão axial**

| bf/bw \ bw/t | 250 | 200 | 125 | 100 | 50 |
|---|---|---|---|---|---|
| 0,4 | 0,02 | 0,03 | 0,04 | 0,04 | 0,08 |
| 0,6 | 0,03 | 0,04 | 0,06 | 0,06 | 0,15 |
| 0,8 | 0,05 | 0,06 | 0,08 | 0,10 | 0,22 |
| 1,0 | 0,06 | 0,07 | 0,10 | 0,12 | 0,27 |
| 1,2 | 0,06 | 0,07 | 0,12 | 0,15 | 0,27 |
| 1,4 | 0,06 | 0,08 | 0,12 | 0,15 | 0,27 |
| 1,6 | 0,07 | 0,08 | 0,12 | 0,15 | 0,27 |
| 1,8 | 0,07 | 0,08 | 0,12 | 0,15 | 0,27 |
| 2,0 | 0,07 | 0,08 | 0,12 | 0,15 | 0,27 |

**Tabela 14 — flexão (eixo X)**

| bf/bw \ bw/t | 250 | 200 | 125 | 100 | 50 |
|---|---|---|---|---|---|
| 0,4 | 0,05 | 0,06 | 0,10 | 0,12 | 0,25 |
| 0,6 | 0,05 | 0,06 | 0,10 | 0,12 | 0,25 |
| 0,8 | 0,05 | 0,06 | 0,09 | 0,12 | 0,22 |
| 1,0 | 0,05 | 0,06 | 0,09 | 0,11 | 0,22 |
| 1,2 | 0,05 | 0,06 | 0,09 | 0,11 | 0,20 |
| 1,4 | 0,05 | 0,06 | 0,09 | 0,10 | 0,20 |
| 1,6 | 0,05 | 0,06 | 0,09 | 0,10 | 0,20 |
| 1,8 | 0,05 | 0,06 | 0,09 | 0,10 | 0,19 |
| 2,0 | 0,05 | 0,06 | 0,09 | 0,10 | 0,19 |

### 13.2 Resumo das fórmulas

| Item | Fórmula |
|---|---|
| Largura efetiva | `b_ef = b (1 − 0,22/λp)/λp`,  `λp = (b/t)/(0,95 √(kE/σ))` |
| χ global | `0,658^(λ0²)` (λ0 ≤ 1,5);  `0,877/λ0²` |
| χ distorcional (compressão) | `(1 − 0,25/λd^1,2)/λd^1,2` (λd > 0,561) |
| χ distorcional (flexão) | `(1 − 0,22/λd)/λd` (λd > 0,673) |
| χ FLT | `1` (λ0 ≤ 0,6);  `1,11 (1 − 0,278 λ0²)` (λ0 < 1,336);  `1/λ0²` |
| Cortante | `0,6 fy h t` · `0,65 t² √(kv fy E)` · `0,905 E kv t³/h`  (dividir por 1,10) |
| N+M | `N/N_Rd + Mx/Mx_Rd + My/My_Rd ≤ 1` |
| M+V | `(M/M_Rd)² + (V/V_Rd)² ≤ 1` |

### 13.3 Mapa do código

| Arquivo | Conteúdo |
|---|---|
| `src/design/Nbr14762.{h,cpp}` | modelo de parede fina, MLE, tração, compressão, flexão, cortante, limites geométricos |
| `src/design/FiniteStrip.{h,cpp}` | faixas finitas semianalíticas e curva de assinatura |
| `src/design/DesignSection.{h,cpp}` | classificação de famílias formadas a frio e tubulares |
| `src/design/BarDesign.{h,cpp}` | orquestração, interações (9.9, 9.8.4), B1 |
| `src/app/services/DesignService.{h,cpp}` | esforços, verificação e escolha do perfil |
| `data/catalog_steel.json` | catálogo de perfis (Ue, U, Z, L, cartola, tubulares) |
| `src/tests/TestDesign.cpp` | testes numéricos |

### 13.4 Referências normativas

* ABNT NBR 14762:2010 — Dimensionamento de estruturas de aço constituídas por perfis formados a frio.
* ABNT NBR 6355 — Perfis estruturais de aço formados a frio (séries); ABNT NBR 8261 e 16239 — tubulares.
* ABNT NBR 8800:2024 — Anexo C (B1/B2) e 4.9/4.10 (análise de 2ª ordem), usados em comum.

## Adendo — complementos posteriores

**Anexo F — mesa tracionada ligada a painel (terças sob sucção)** — para U enrijecido e Z enrijecido com `b_w ≤ 292 mm`, `60 ≤ b_w/t ≤ 170`, `2,8 ≤ b_w/b_f ≤ 4,5`,
`16 ≤ b/t ≤ 43` e `f_y ≤ 410 MPa`: `M_Rd = R W_ef f_y/1,10`, no lugar da flambagem lateral com torção. `R` (Tabela F.1): biapoiada — `b_w ≤ 165`: 0,70; `165 < b_w ≤ 216`: 0,65;
`216 < b_w ≤ 292`: Z 0,50 e U 0,40; contínua — U 0,60 e Z 0,70. As demais condições (vão `< 10 m`, conectores, painel) ficam com o projetista; a flambagem distorcional continua sendo verificada.

**Anexo B — aumento de `f_y` nas dobras** — `f_ya = C f_yc + (1 − C) f_y ≤ f_u`, `f_yc = B_c f_y/(r_i/t)^m`, `B_c = 3,69 (f_u/f_y) − 0,819 (f_u/f_y)² − 1,79`, `m = 0,192 (f_u/f_y) − 0,068`,
com `f_u/f_y ≥ 1,2`, `r_i/t ≤ 7` (adota-se `r_i = t`) e dobras ≤ 120°. `C` = área das dobras/área da seção (tração e compressão) ou das dobras da mesa comprimida/área da mesa (flexão).
Só vale para seções totalmente efetivas (`A_ef = A` ou `W_ef = W`); caso contrário o cálculo segue com `f_y` e o memorial avisa. É uma opção por barra.

**Apoio das ligações** — vale o descrito no livro de perfis laminados (barra de apoio escolhida pelo usuário, face real e soma dos esforços das ligações da mesma barra).

> **Aviso:** este documento descreve o comportamento do software para auxiliar na revisão e na compreensão dos cálculos.
> Ele **não substitui** a norma nem o julgamento do engenheiro responsável; confira sempre dados, hipóteses e resultados.
