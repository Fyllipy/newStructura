# O método dos componentes — guia para quem está começando

Este texto explica, do zero, como se calcula a **rigidez** de uma ligação entre uma viga e um pilar de aço, e como o Structura::Metálicas usa essa rigidez para desenhar o diagrama **momento-rotação** e **classificar** a ligação (rotulada, semirrígida ou rígida), conforme a ABNT NBR 8800:2024, item 6.1.2.

Você não precisa conhecer o método antes de ler. Precisa saber o básico de resistência dos materiais: tensão, deformação, módulo de elasticidade, momento fletor, rigidez de mola (`F = k · δ`) e a rotação de uma seção.

---

## 1. O problema: a ligação não é nem articulação perfeita, nem engaste perfeito

Quando você modela um pórtico no programa, é comum escolher entre dois extremos para o encontro de uma viga com um pilar:

* **Articulação (rótula):** a viga gira livremente em relação ao pilar. Nenhum momento passa de um para o outro.
* **Engaste (nó rígido):** a viga e o pilar giram juntos, como se fossem uma peça só. O ângulo entre eles nunca muda.

Na obra, a ligação real é um conjunto de chapas, parafusos e soldas. Ela deforma: o parafuso alonga, a chapa flexiona, a alma do pilar amassa. Por isso, quando a viga recebe momento, o ângulo entre viga e pilar **muda um pouco**. Essa mudança de ângulo é a **rotação relativa da ligação**, representada por **θ** (teta), em radianos.

> **Não confunda:** a rotação *do nó* (o nó inteiro girando junto com o pilar) é outra coisa. A rotação da ligação, θ, é a diferença entre a rotação da viga e a do pilar no encontro.

A relação entre o momento **M** transmitido pela ligação e a rotação **θ** é o que chamamos de **curva momento-rotação** da ligação.

```
M (kN·m)
  ^
  |                  ___________  Mj,Rd  (momento resistente)
  |              .-'
  |           .-'
  |        .-'            <- trecho não linear
  |     .-'
  |   .'    <- trecho inicial, quase uma reta (rigidez inicial)
  | .'
  +-------------------------------> θ (rad)
```

Três grandezas importam:

| Símbolo | Nome | O que é |
|---|---|---|
| **Mj,Rd** | momento resistente de cálculo da ligação | o maior momento que a ligação suporta |
| **Sj,ini** | rigidez inicial | inclinação do trecho inicial da curva (kN·m/rad). A NBR 8800 chama de **Si** |
| **θ** | rotação da ligação | quanto a viga gira em relação ao pilar |

A rigidez rotacional é como a rigidez de uma mola, mas de giro: `M = S · θ`. Uma mola mais dura (S maior) gira menos para o mesmo momento.

---

## 2. Como a NBR 8800:2024 classifica a ligação (item 6.1.2)

O texto da norma (item 6.1.2.1) diz que, na análise elástica, uma ligação viga-pilar é:

* **rotulada** se `Si ≤ 0,5 · E · Iv / Lv`;
* **rígida** se `Si ≥ 25 · E · Iv / Lv`;
* **semirrígida** entre os dois limites.

Aqui:

* `Si` é a rigidez da ligação **correspondente a 2/3 do momento resistente de cálculo**, que a norma chama de "rigidez inicial";
* `E` é o módulo de elasticidade do aço;
* `Iv` é o momento de inércia da seção da viga no plano da estrutura;
* `Lv` é o comprimento (vão) da viga.

Por que dividir por `E·Iv/Lv`? Esse número é a **rigidez da própria viga** à rotação. Comparar `Si` com ele diz se a ligação é mole ou dura *em relação à viga que ela liga*. Uma ligação com `Si` igual a 25 vezes a rigidez da viga gira tão pouco que a viga se comporta como engastada. Com `Si` menor que metade da rigidez da viga, quase não transmite momento.

Chamamos a razão `η = Si / (E·Iv/Lv)` de **rigidez relativa**.

Item 6.1.2.2: o limite de 25 só vale se, em cada andar, **`Kv/Kp ≥ 0,1`**, em que `Kv` é o valor médio de `Iv/Lv` das vigas do topo do andar e `Kp` o valor médio de `Ip/Lp` dos pilares do andar. Se o pilar for muito mais rígido que as vigas isso falha, e a ligação, mesmo com `Si ≥ 25·E·Iv/Lv`, deve ser considerada **semirrígida**.

Item 6.1.2.1 também diz como obter `Si`: "na ausência de Norma Brasileira aplicável, de acordo com a **EN 1993-1-8**, com base em resultados experimentais ou em análises numéricas". A EN 1993-1-8 (Eurocode 3, parte 1-8) traz o **método dos componentes**, que é o assunto deste guia.

---

## 3. A ideia central do método dos componentes

Imagine a ligação como um **conjunto de molas**, cada uma representando uma parte da ligação que se deforma:

* a alma do pilar sendo cisalhada;
* a alma do pilar sendo comprimida pela mesa da viga;
* a mesa do pilar flexionando, puxada pelos parafusos;
* a chapa de extremidade flexionando;
* os parafusos alongando.

Cada "mola" é um **componente**. O método tem três passos:

1. **Identificar os componentes** da ligação e calcular a rigidez de cada um (um coeficiente `ki`).
2. **Combinar as molas** em série e em paralelo para chegar à rigidez de uma única mola equivalente, a rigidez da ligação.
3. **Calcular a resistência** de cada componente, a menor resistência define o momento resistente `Mj,Rd`.

### 3.1 Molas em série e em paralelo (revisão)

* **Em série** (uma depois da outra, a mesma força passa por todas): as **flexibilidades** somam.

  ```
  1/k_total = 1/k1 + 1/k2 + 1/k3
  ```

  A mola equivalente é sempre **mais mole** que a mais mole das molas. Um elo fraco domina.

* **Em paralelo** (a força se divide): as **rigidezes** somam.

  ```
  k_total = k1 + k2 + k3
  ```

### 3.2 Como o momento vira força

Uma viga transmite momento ao pilar como um **binário**: uma força de tração na mesa superior, uma força de compressão na mesa inferior, separadas por um braço de alavanca `z`.

```
        F ↑ (tração)     ←  fileira de parafusos / solda da mesa tracionada
   ─────────┐
            │  viga
   ─────────┘
        F ↓ (compressão) ←  mesa comprimida apoiada no pilar

   M = F · z
```

A força `F` passa pelos componentes: cada um se deforma `δi = F / ki'`. A soma das deformações, dividida pelo braço `z`, dá a rotação θ. Escrevendo isso com as flexibilidades, chega-se à fórmula principal do método:

```
            E · z²
Sj,ini = ───────────────
           Σ (1 / ki)
```

Em palavras: **a rigidez inicial da ligação é E·z² dividido pela soma das flexibilidades dos componentes** (cada flexibilidade é `1/ki`). Cada `ki` tem unidade de comprimento (mm), de forma que `E[N/mm²] · z²[mm²] / (1/ki [1/mm])` resulta em N·mm/rad, uma rigidez rotacional.

Antes de seguir, veja por que a fórmula faz sentido: se um componente for infinitamente rígido (`ki → ∞`), sua flexibilidade `1/ki` é zero e ele deixa de limitar a ligação. Se um componente for muito mole, `1/ki` é grande, e ele domina a soma: **a ligação é tão mole quanto o seu componente mais mole**.

---

## 4. Os componentes e suas rigidezes (EN 1993-1-8, item 6.3)

Os coeficientes abaixo são os da Tabela 6.11 da EN 1993-1-8. Todos os `ki` têm unidade de comprimento (mm).

### 4.1 Painel de alma do pilar ao cisalhamento — `k1`

A parte da alma do pilar entre as mesas da viga fica sob cisalhamento, porque as forças de tração e compressão da viga atuam em lados opostos.

```
k1 = 0,38 · Avc / (β · z)
```

* `Avc` é a área de cisalhamento do pilar. Para um perfil I laminado: `Avc = A − 2·b·tf + (tw + 2r)·tf`.
* `β` é o **parâmetro de transformação**, que mede o quanto o painel é solicitado:
  * `β = 1`: viga só de um lado do pilar;
  * `β = 0`: vigas dos dois lados com momentos iguais (o cisalhamento se cancela; `k1 → ∞`);
  * `β = 2`: vigas dos dois lados com momentos opostos (o cisalhamento é o dobro).

### 4.2 Alma do pilar comprimida — `k2`

A mesa comprimida da viga empurra a alma do pilar, que "amassa".

```
k2 = 0,7 · beff,c,wc · twc / dc
```

* `beff,c,wc` é a **largura efetiva** da alma que resiste. A força da mesa da viga se espalha pela espessura da mesa, pela solda, pela mesa do pilar e pela raiz (raio) do perfil. Para ligação soldada: `beff,c,wc = tfb + 2√2·ap + 5·(tfc + s)`; para chapa de extremidade soma-se ainda a dispersão `sp` através da chapa.
* `twc` é a espessura da alma do pilar;
* `dc` é a altura livre da alma: `dc = hc − 2·tfc − 2·rc`.

### 4.3 Alma do pilar tracionada — `k3`

O mesmo raciocínio, do lado tracionado:

```
k3 = 0,7 · beff,t,wc · twc / dc
```

Nas ligações parafusadas, `beff,t,wc` é o comprimento efetivo `leff` da região da mesa do pilar em torno da fileira de parafusos.

### 4.4 Mesa do pilar em flexão — `k4` (só ligações parafusadas)

Os parafusos tracionados puxam a mesa do pilar para fora, como se fosse uma chapa engastada nas almas.

```
k4 = 0,9 · leff · tfc³ / m³
```

* `leff` é o comprimento efetivo da mesa que trabalha em flexão;
* `m` é a distância do eixo do parafuso até a face da alma (descontando o raio da raiz).

Note o expoente 3: a rigidez de flexão de uma chapa cresce com o **cubo da espessura**. Dobrar a espessura da mesa torna o componente **oito vezes** mais rígido.

### 4.5 Chapa de extremidade em flexão — `k5`

A chapa de extremidade, puxada pelos parafusos de um lado e pela solda da mesa da viga do outro, também flexiona:

```
k5 = 0,9 · leff · tp³ / mx³
```

### 4.6 Parafusos em tração — `k10`

```
k10 = 1,6 · As / Lb
```

* `As` é a área resistente à tração do parafuso;
* `Lb` é o comprimento de alongamento: a espessura das chapas agarradas, mais as arruelas, mais metade da cabeça e metade da porca.

Atenção: a EN 1993-1-8 dá `k10` **por fileira de parafusos**, com `As` de um parafuso (conferido com o exemplo C2 do SCI P398: `1,6 × 353 / 70,5 = 8,01 mm`). Uma versão anterior do programa multiplicava por dois; o erro foi encontrado ao comparar com exemplos publicados e corrigido.

### 4.7 Cada fileira de parafusos é uma série de molas

Os componentes `k3`, `k4`, `k5` e `k10` de uma **mesma fileira** estão em série (a mesma força passa por eles), então:

```
keff,r = 1 / (1/k3 + 1/k4 + 1/k5 + 1/k10)
```

Quando há mais de uma fileira de parafusos tracionados, as fileiras trabalham em **paralelo** e cada uma tem um braço `hr` até o centro de compressão. Calcula-se então uma mola equivalente `keq` e um braço equivalente `zeq`:

```
          Σ keff,r · hr²                     Σ keff,r · hr
zeq  =  ─────────────────        keq  =  ─────────────────
           Σ keff,r · hr                         zeq
```

Fileiras mais afastadas do centro de compressão têm braço maior e, com a mesma rigidez, pesam mais no resultado.

### 4.8 Juntando tudo

Para uma ligação soldada, a ligação é a série `k1 – k2 – k3`. Para uma ligação por chapa de extremidade, é a série `k1 – k2 – keq`, com `z = zeq`.

---

## 5. Exemplo numérico completo: ligação soldada

Dados: viga **IPE 300** ligada a um pilar **HEB 200**; mesas da viga soldadas na mesa do pilar com solda de penetração total; sem enrijecedores; viga só de um lado (`β = 1`). Aço com `E = 210 000 MPa`.

| Dado | Valor |
|---|---|
| altura da viga `d` | 300 mm |
| espessura da mesa da viga `tfb` | 10,7 mm |
| pilar: `hc`, `bfc`, `tfc`, `twc`, `rc` | 200; 200; 15; 9; 18 mm |

**Passo 1 — geometria auxiliar**

```
dc  = hc − 2·tfc − 2·rc = 200 − 30 − 36 = 134 mm
A   = 2·200·15 + (200−30)·9 + (4−π)·18² = 7 808 mm²
Avc = A − 2·bfc·tfc + (twc + 2rc)·tfc
    = 7 808 − 6 000 + (9+36)·15 = 2 483 mm²
z   = d − tfb = 300 − 10,7 = 289,3 mm
```

**Passo 2 — rigidez de cada componente**

```
k1 = 0,38 · 2 483 / (1 · 289,3) = 3,26 mm

beff = tfb + 5(tfc + s) = 10,7 + 5·(15+18) = 175,7 mm      (solda de penetração: ap = 0)
k2 = 0,7 · 175,7 · 9 / 134 = 8,26 mm
k3 = k2 = 8,26 mm      (mesma largura efetiva, do lado tracionado)
```

**Passo 3 — rigidez da ligação**

```
Σ (1/ki) = 1/3,26 + 1/8,26 + 1/8,26 = 0,3066 + 0,1210 + 0,1210 = 0,5486 1/mm

Sj,ini = E · z² / Σ(1/ki) = 210 000 · 289,3² / 0,5486
       = 3,203 × 10¹⁰ N·mm/rad  =  32 031 kN·m/rad
```

**Passo 4 — comparação com a viga (classificação)**

Para uma viga IPE 300 (`Iv = 8 356 cm⁴`) com `Lv = 6 m`:

```
E·Iv/Lv = 210 000 · 8 356×10⁴ / 6 000 = 2,925 × 10⁹ N·mm/rad = 2 925 kN·m/rad
η = Si / (E·Iv/Lv) = 32 031 / 2 925 ≈ 10,95
```

Como `0,5 < η < 25`, a ligação é **semirrígida** (mesmo sendo soldada!). Para ser rígida, teria de ter `η ≥ 25`; um enrijecedor na alma do pilar tornaria `k2` e `k3` infinitos e aumentaria muito `Sj,ini`.

Observe quem domina: `k1` (cisalhamento do painel) é o componente mais mole, e responde por 56% da flexibilidade total (0,3066 de 0,5486). Em ligações soldadas sem reforço, é comum o painel de alma ser o ponto fraco.

---

## 6. Exemplo numérico: uma fileira de parafusos (chapa de extremidade)

Mesma viga e pilar do exemplo anterior, agora com chapa de extremidade `160 × 20 mm`, parafusos M20 com gabarito `w = 90 mm`, uma fileira tracionada a 40 mm da face externa da mesa da viga, borda da chapa a 35 mm da fileira, solda de filete de perna 8 mm (garganta `ap = 8/√2 = 5,66 mm`).

```
mc  = (w − twc − 1,6·rc)/2 = (90 − 9 − 28,8)/2 = 26,1 mm
ec  = (bfc − w)/2 = 55 mm
leff,mesa = min(2π·mc ; 4·mc + 1,25·ec) = min(164,0 ; 173,3) = 164,0 mm

mx  = 40 − 0,8·8 = 33,6 mm                       (da chapa)
leff,chapa = min(2π·mx = 211,1 ; não circular = 80,0) = 80,0 mm     (0,5·bp = 80 governa)

k3  = 0,7 · 164,0 · 9 / 134            =  7,71 mm
k4  = 0,9 · 164,0 · 15³ / 26,1³        = 28,02 mm
k5  = 0,9 · 80,0 · 20³ / 33,6³         = 15,18 mm
Lb  = 20 + 15 + 8 + (0,7+0,8)·20/2     = 58,0 mm
k10 = 1,6 · 245 / 58,0                 =  6,76 mm

keff = 1 / (1/7,71 + 1/28,02 + 1/15,18 + 1/6,76) = 2,64 mm
```

Os componentes mais moles da fileira são o parafuso (`k10`) e a alma do pilar tracionada (`k3`). Como só há uma fileira, `keq = keff` e o braço é o da própria fileira: `h = d − tfb/2 + 40 = 334,65 mm`. A alma comprimida, com a largura efetiva `beff,c,wc = tfb + 2√2·ap + 5(tfc + s) + sp = 10,7 + 16,0 + 165 + 40 = 231,7 mm` (a dispersão `sp` pela chapa vale `2·tp = 40 mm`), dá `k2 = 10,89 mm`; o painel, com o braço da fileira, `k1 = 0,38·2 483/334,65 = 2,82 mm`:

```
Sj,ini = 210 000 · 334,65² / (1/2,82 + 1/10,89 + 1/2,64) = 28 484 kN·m/rad
```

Um valor da mesma ordem do exemplo soldado, porque o painel de alma continua dominando.

---

## 7. A curva momento-rotação completa

Até aqui só calculamos a **rigidez inicial**. O trecho inicial da curva é uma reta, mas a ligação real amolece à medida que se aproxima do momento resistente. A EN 1993-1-8 (item 6.3.1) propõe:

```
θ = M / Sj,ini                              para M ≤ 2/3 · Mj,Rd
θ = (M / Sj,ini) · (1,5 · M / Mj,Rd)^ψ      para 2/3 · Mj,Rd < M ≤ Mj,Rd
```

O expoente **ψ** vale 2,7 para ligações soldadas e para chapa de extremidade parafusada (Tabela 6.8). A partir de 2/3 de `Mj,Rd` a curva "desencosta" da reta inicial, e o fator `(1,5·M/Mj,Rd)^ψ` é igual a 1 em `M = 2/3 Mj,Rd` (continuidade).

Exemplo com `Sj,ini = 32 031 kN·m/rad` e `Mj,Rd = 120 kN·m`:

| M (kN·m) | M/Mj,Rd | fator (1,5 M/Mj,Rd)^2,7 | θ (rad) |
|---|---|---|---|
| 40 | 0,33 | 1 (trecho linear) | 0,00125 |
| 80 | 0,67 | 1 | 0,00250 |
| 100 | 0,83 | 1,83 | 0,00570 |
| 120 | 1,00 | 2,99 | 0,01120 |

Repare em dois pontos: a rigidez **secante** em `Mj,Rd` vale só `1/2,99` (≈ 33%) da inicial, e **a norma usa o nível de 2/3 de Mj,Rd**, onde ainda vale a rigidez inicial. É por isso que `Si = (2/3·Mj,Rd) / θ(2/3·Mj,Rd)` coincide com `Sj,ini` na curva da EN.

> **Cuidado com três "rigidezes" diferentes:** a *tangente* (inclinação local da curva), a *inicial* (inclinação em M→0) e a *secante* (M/θ num ponto qualquer). A NBR 8800 usa a secante no nível de 2/3 de `Mj,Rd`. Para a curva da EN até 2/3, as três coincidem, mas depois se separam.

A **capacidade de rotação** (até onde a curva pode seguir depois de `Mj,Rd` antes da ruptura) **não** é calculada. A curva do programa termina em `Mj,Rd`.

---

## 8. E o momento resistente `Mj,Rd`?

Na EN 1993-1-8 (item 6.2), `Mj,Rd` vem da **resistência de cada componente**, combinada fileira por fileira. O programa implementa esse procedimento com **o mesmo modelo físico** que calcula a rigidez (mesma geometria, mesmas fileiras, mesmos materiais):

1. **Cada fileira de parafusos**, como fileira isolada, tem a menor entre as resistências de: mesa do pilar em flexão (T-stub), chapa de extremidade em flexão (T-stub), alma do pilar tracionada e alma da viga tracionada.
   * O **T-stub** (tê equivalente) tem três modos de ruptura: modo 1, escoamento completo da mesa (`4·Mpl,1/m`); modo 2, ruptura do parafuso com escoamento da mesa (`(2·Mpl,2 + n·ΣFt,Rd)/(m + n)`); modo 3, ruptura dos parafusos (`ΣFt,Rd`). A resistência é a menor das três.
2. **Limites do somatório** das forças de tração: painel de alma ao cisalhamento (`Vwp,Rd/β`), alma do pilar comprimida (`Fc,wc,Rd`) e mesa e alma da viga comprimidas (`Fc,fb,Rd`).
3. **Distribuição**: da fileira mais afastada do centro de compressão para a mais próxima, cada fileira recebe o menor entre a sua resistência isolada e o que sobra do limite do somatório; a regra de 1,9·Ft,Rd (item 6.2.7.2(9)) impõe distribuição triangular quando a fileira é muito mais resistente que um parafuso.
4. `Mj,Rd = Σ Ftr,Rd · hr`.

Esse é o **momento resistente do modelo mecânico** (nível A): ele define a curva e não depende de a ligação estar dimensionada. Quando existe uma ligação **cadastrada** no módulo de ligações, o programa também mostra, como **nível C**, o momento resistente pelas verificações da **NBR 8800:2024** (soldas, parafusos com efeito alavanca, mesa e alma do pilar, enrijecedores). Essa verificação **não altera a curva**: se ela resiste menos que a curva, o programa avisa que a ligação cadastrada não sustenta o comportamento calculado e deve ser redimensionada.

**Limitações do `Mj,Rd`:** o efeito de grupo de fileiras (item 6.2.7.2(8)) não é calculado (o resultado da EN pode ser não conservador com fileiras muito próximas, por isso a verificação construtiva posterior, de nível C, é indispensável); o método não vale com esforço normal da viga acima de 5% de `Npl,Rd` (6.2.7.1(2)).

---

## 9. O que o programa implementa

| Tipologia da ligação | Rigidez (componentes) | Mj,Rd | Observação |
|---|---|---|---|
| Mesas da viga soldadas ao pilar | k1, k2, k3 | componentes EN (6.2) e verificações da NBR 8800: o menor | simétrica: a curva vale para os dois sentidos do momento |
| Chapa de extremidade parafusada (fileiras na extensão tracionada) | k1, k2, k3, k4, k5, k10 por fileira | componentes EN (6.2) e verificações da NBR 8800: o menor | só o sentido que traciona as fileiras; o inverso não é calculado |
| Ligação rígida com perfis T parafusados | não implementado | — | exige o modelo de tê equivalente com alavanca |
| Ligações de cortante (chapa de cisalhamento, cantoneiras) | não implementado | — | normalmente tratadas como rotuladas |

O resultado aparece na janela "Análise de Ligação" (botão *Gráfico momento-rotação*, com duas barras selecionadas): o programa procura a ligação definida no nó (pelo botão *Ligações*) e usa a geometria dela. A aba "Memória de Cálculo" mostra todas as etapas, componente por componente.

### 9.1 Simplificações adotadas

Estas simplificações são deliberadas e devem ser conhecidas por quem usa o resultado:

* **Comprimentos efetivos (`leff`)**: as fileiras são tratadas individualmente (Tabela 6.4: fileira interna da mesa do pilar; Tabela 6.6: fileira na extensão da chapa). A EN manda usar, na rigidez, o **menor** comprimento efetivo entre o da fileira isolada e o da fileira como parte de um grupo; como o efeito de grupo não é calculado, a rigidez pode ser levemente superestimada (a diferença aparece no exemplo da seção 14).
* **Enrijecedores horizontais** (alinhados com as mesas da viga): fazem `k2 = k3 = ∞`; `k1` continua calculado e a mesa do pilar em flexão (`k4`) também (conservador).
* **Chapa de reforço da alma** (*doubler*): aumenta `Avc` pela espessura multiplicada pela altura livre da alma.
* **Vigas dos dois lados** (`β = 0`): considera momentos iguais e de mesmo sentido (cargas de gravidade). Para vento, com momentos opostos, `β = 2` seria o caso desfavorável. O programa usa `β = 1` ou `β = 0`.
* **Arruelas e cabeça do parafuso**: espessura de arruela de 4 mm sob cabeça e porca; cabeça `0,7·db` e porca `0,8·db`.
* **Componentes de tê equivalente de perfis T** e de **ligações de cantoneira de alma**: não implementados.
* **Esforço axial no pilar**: a redução por tensão de compressão alta (`kwc`) não é aplicada à rigidez.
* **Rotação relativa pequena**: a rigidez é calculada para a primeira ordem, como na EN.

---

## 10. Passo a passo para usar no programa

1. Modele o pórtico e defina as ligações (aba Cálculo → *Ligações*) escolhendo uma tipologia rígida viga-pilar com geometria definida (mesas soldadas ou chapa de extremidade).
2. Selecione duas barras que compartilham o nó: a viga e o pilar.
3. Clique em *Gráfico Momento-Rotação*. Se a ligação do nó for de uma tipologia suportada, a janela já abre com a curva do método dos componentes.
4. Veja as abas: **Momento-Rotação** (a curva, `Mj,Rd`, o nível de 2/3 e a reta secante da rigidez), **Classificação da Rigidez** (onde a ligação cai entre rotulada, semirrígida e rígida), **Memória de Cálculo** (todas as contas).
5. Se não houver ligação definida, ou a tipologia não for suportada, informe a curva ou `Si` e `Mj,Rd` na aba *Dados da ligação*. O programa nunca inventa uma curva.

---

## 11. Perguntas para fixar o conteúdo

1. Uma ligação tem `Sj,ini = 4 000 kN·m/rad`. A viga tem `E·Iv/Lv = 3 000 kN·m/rad`. Como é classificada? *(Resposta: `η = 1,33`, semirrígida.)*
2. Por que colocar um enrijecedor na alma do pilar aumenta a rigidez? *(Porque `k2` e `k3` passam a ser infinitos, saem da soma das flexibilidades.)*
3. Se dobrarmos a espessura da chapa de extremidade, o que acontece com `k5`? *(Fica 8 vezes maior, pois `k5 ∝ tp³`. Mas a ligação só ficará 8 vezes mais rígida se `k5` era o único componente.)*
4. Duas molas de rigidez 10 e 10 em série dão que rigidez? E em paralelo? *(Série: 5; paralelo: 20.)*
5. Por que a rigidez inicial é avaliada em 2/3 de `Mj,Rd`, e não perto de `Mj,Rd`? *(Porque a curva já amoleceu perto de `Mj,Rd`; em serviço os momentos ficam em torno dessa fração.)*
6. Num pórtico, as ligações viga-pilar são todas semirrígidas, mas na análise foram modeladas como engastes. O que isso faz com os momentos nos pilares e vigas? *(Superestima momentos nos apoios das vigas e subestima os do vão e os deslocamentos laterais.)*

---

## 12. Glossário

* **Ligação:** conjunto de elementos (chapas, parafusos, soldas) que une duas barras.
* **Componente:** parte da ligação com comportamento próprio (alma do pilar comprimida, parafuso tracionado...).
* **Rigidez rotacional `S`:** momento necessário para girar a ligação de 1 rad (kN·m/rad).
* **Rigidez inicial `Sj,ini` ou `Si`:** rigidez do trecho inicial da curva.
* **`Mj,Rd`:** momento resistente de cálculo da ligação.
* **`η`:** rigidez relativa, `Si / (E·Iv/Lv)`.
* **Tê equivalente (T-stub):** modelo de uma mesa ou chapa flexionada por parafusos, usado para obter comprimentos efetivos.
* **Painel de alma:** parte da alma do pilar entre as mesas da viga.
* **β:** parâmetro de transformação que depende dos momentos nos dois lados do pilar.

---

## 13. Referências

* ABNT NBR 8800:2024, Projeto de estruturas de aço e de estruturas mistas de aço e concreto de edificações — item 6.1.2 (rigidez das ligações entre viga e pilar) e itens 6.1.1 a 6.1.5.
* EN 1993-1-8:2005, Eurocode 3: Design of steel structures — Part 1-8: Design of joints — itens 5.1, 5.2, 6.2 e 6.3, Tabelas 6.8 e 6.11.
* Jaspart, J.-P.; Weynand, K. *Design of Joints in Steel and Composite Structures*. ECCS, 2016.
* Silva, L. S.; Simões, R.; Gervásio, H. *Design of Steel Structures*. ECCS, 2010.

> **Aviso sobre a implementação:** as fórmulas de rigidez foram implementadas conforme a EN 1993-1-8 e conferidas com os exemplos numéricos deste documento; os comprimentos efetivos usam simplificações declaradas na seção 9.1. Os resultados devem ser analisados pelo responsável técnico, sobretudo em ligações com mais de duas fileiras de parafusos ou com vigas dos dois lados do pilar.

---

## 14. Validação do programa contra exemplos publicados

O modelo foi comparado com um exemplo de cálculo manual publicado pela SCIA (ligação de extremidade estendida HEB140/IPE220, EN 1993-1-8) e com valores do exemplo C2 do SCI P398. Resultados completos e diferenças relativas estão em `docs/AUDITORIA_LIGACOES.md`. Resumo: `k3`, `k4`, `k5`, `k10`, `keff`, `zeq`, `keq`, `k2` e `Sj,ini = 11 596 kN·m/rad` reproduzidos com erro menor que 1%; `Ft = 97,3 kN`, `Vwp,Rd = 160,2 kN` e `Mj,Rd = 34,9 kN·m` reproduzidos com erro menor que 0,5%.
