# Qualidade da malha de cascas (Q4) — limites e fundamentação (Etapa 3, WP3)

Implementação: `src/shell/ShellMeshQuality.{h,cpp}`; chamada em `ShellDialog::runAnalysis` **antes** de resolver (erro bloqueia a análise, aviso aparece no status).
Testes: `src/tests/TestShellMeshQuality.cpp`. Varreduras executadas (scratchpad, `mefval_fix.exe malha|domo`): `results/wp3_sensibilidade_malha.csv`, `results/wp3_domo_polo.csv`.

## 1. Métricas por elemento
Plano médio pela normal das diagonais; todas as medidas no plano médio.

| Métrica | Definição |
|---|---|
| Ângulo interno mín./máx. | ângulos dos 4 cantos; canto reentrante conta > 180° |
| Jacobiano normalizado | `(e_próx × e_ant)·n / (‖e_próx‖‖e_ant‖)` por canto (= sen do ângulo com sinal; ≤ 0 ⇒ não convexo/invertido) |
| Razão de aspecto | maior aresta / menor aresta |
| Empenamento | maior distância dos 4 nós ao plano médio (`δ`), reportada como `δ/h` (`h = √área`) e `δ/t` |
| Área | `½‖d1 × d2‖`; degenerado se `< 1e-10 · (maior aresta)²` |
| Continuidade de normais | por aresta compartilhada (2 elementos do **mesmo objeto**): sentido de percurso igual ⇒ normais opostas (ERRO); ângulo entre normais (mesmo objeto e zona) > 45° ⇒ aviso |

## 2. Limites e dados que os sustentam (todos medidos, placa simplesmente apoiada 1×1, pressão uniforme)

### Razão de aspecto (malha 4 × 4·AR; erro da flecha central vs. malha 40×40 regular)
| AR | t/a = 0,01 | t/a = 0,1 |
|---|---|---|
| 1 | −2,57 % | −6,08 % |
| 4 | −1,64 % | −3,70 % |
| 16 | −1,44 % | −3,18 % |
| 32 | −1,33 % | −3,15 % |

O erro **não piora** com a razão de aspecto até 32 (domina o erro de discretização na direção curta). ⇒ aceitável até 32; aviso acima (sem dado); erro > 1000 (quase colapsado).

### Distorção angular — losango (malha 8×8, erro vs. mesma geometria em 32×32)
| Skew | Ângulos | t/a = 0,01 | t/a = 0,1 |
|---|---|---|---|
| 0° | 90 | −0,69 % | −2,78 % |
| 30° | 60..120 | −2,71 % | −3,49 % |
| 45° | 45..135 | −5,38 % | −3,65 % |
| 60° | 30..150 | −3,58 % | −1,98 % |
| 70° | 20..160 | −3,82 % | −6,27 % |

Afilamento (trapézio, aspecto até 7,2, ângulos 67..113): degradação ≤ 0,6 ponto percentual. Nó interno deslocado até 0,4 h (Etapa 2): −0,44 % → −2,68 %.
⇒ **aceitável** em [45°, 135°] (piora ≤ ~5 pontos percentuais da flecha); **aviso** fora disso.

### Elementos do polo de domos ("pizza", ângulo ≈ 173°)
Hemisfério sob pressão interna (malha do próprio app), esforço de membrana vs. `p R / 2`:
| h (m) | elementos do polo: erro de `n` (médio) | demais elementos |
|---|---|---|
| 0,50 | 4,46 % | 0,66 % |
| 0,25 | 5,39 % | 0,33 % |
| 0,20 | 5,52 % | 0,28 % |
| 0,175 | 5,57 % | 0,27 % |

O erro local de **4–6 % não diminui com o refino**. ⇒ esses elementos são **aviso** (não erro, não aceitável). **ERRO** só para elemento matematicamente degenerado/invertido: jacobiano ≤ 0 ou ângulo < 1° ou > 179° (o jacobiano se anula em 180°). **Entre 173° e 179° não há dado medido**; a faixa é aviso.

> **Anomalia aberta (não resolvida):** nas malhas `h ≤ 0,15` m (3 318 e 4 108 elementos; o gerador muda de regime entre 1 404 e 3 318 elementos) o deslocamento radial do equador sai **−33 %** da solução de membrana, embora os esforços de membrana dos demais elementos tenham erro médio de 0,5 % e o equilíbrio vertical feche (ΣRz = −1,2559e6 vs. `p π R²` = 1,2566e6). Causa **não determinada** (malha do gerador, formulação facetada ou o ensaio). Registrada como pendência; **cascas curvas finas não são aprovadas para deslocamentos** até a investigação.

### Empenamento (placa SS 8×8, t/a = 0,01, nós alternados ±δ; Etapa 2)
| δ | δ/h | δ/t | erro da flecha |
|---|---|---|---|
| 1,25e-4 | 0,001 | 0,0125 | −0,47 % (≈ base) |
| 1,25e-3 | 0,010 | 0,125 | −3,45 % |
| 6,25e-3 | 0,05 | 0,625 | −41,8 % |
| 1,25e-2 | 0,10 | 1,25 | −69,4 % |

A rigidez do elemento descarta o desvio fora do plano; o erro é rápido. ⇒ aviso: `δ/h > 0,005` ou `δ/t > 0,05`; erro: `δ/h > 0,03` ou `δ/t > 0,25` (entre os pontos medidos 3 % e 42 %; conservador). Os limites foram interpolados entre 4 pontos, numa única placa fina.

## 3. Limites do método
- As varreduras medem **deslocamento central** (e membrana no domo). Sensibilidade de **momentos e cortantes** à distorção **não foi medida**.
- Dados só para o Q4 desta DLL (MITC4 + Wilson + drilling), material isotrópico e malha 8×8/4×4AR.
- O erro de empenamento depende de `t`; o critério `δ/t` vale para cascas finas, `δ/h` para grossas.
- Um elemento "aceitável" aqui **não** garante resultado correto: indica apenas que a distorção não degrada a flecha acima do erro de discretização medido.
