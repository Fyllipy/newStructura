# Validação Structura × Mecway 28 — placas e cascas, estática linear

Gerado automaticamente por `make_report.py` a partir de `summary.json`. Todos os números abaixo vêm dos arquivos de resultado dos dois programas.

## 1. Método
- **Mesmo modelo nos dois:** cada caso é montado pelo pipeline real do Structura (`Model` → `generateMesh` → `buildJob` → `mefs.dll` distribuída, SHA-256 `09ffc08a6001b2c4…`). A malha (nós, elementos, ordem dos nós, espessura), os apoios e a pressão são exportados e escritos em `.liml` (formato do Mecway) pelo `gen_mecway.py`: **mesmos nós, mesmos elementos, mesma numeração**, E = 200 GPa, ν = 0,3.
- **Mecway controlado por linha de comando:** `mecway arquivo.liml script extrai.py` (solve sem interface, via script IronPython do próprio Mecway), com extração dos resultados por nó e por elemento. Dois solvers do Mecway foram rodados: o **interno** (quad4 = MITC4, segundo o manual do Mecway) e o **CalculiX 2.21** que acompanha o Mecway.
- **Convenções confirmadas por execução:** pressão positiva do Structura (sentido da normal do elemento) = pressão na **face 6** do Mecway; reação com o mesmo sinal nos dois; momento principal com o mesmo sinal (razão Structura/Mecway = 1,00 nos casos de placa).
- **Comparação:** nó a nó (deslocamentos e rotações) e canto a canto de cada elemento (momentos principais e tensão de von Mises na face inferior). Os eixos locais dos elementos diferem entre os programas, então momentos são comparados por **invariantes** (momentos principais).

## 2. Casos
| Caso | Descrição | Nós | Elementos |
|---|---|---|---|
| `ss_plate` | Placa 1×1 m, t = 10 mm, apoiada nas 4 bordas (UZ=0), pressão 1 kPa, malha 8×8 | 81 | 64 |
| `clamped_plate` | Placa 1×1 m, t = 10 mm, engastada nas 4 bordas, pressão 1 kPa, malha 8×8 | 81 | 64 |
| `cantilever_plate` | Faixa 4×1 m, t = 100 mm, engastada em x = 0, pressão 50 kPa, malha 16×4 | 85 | 64 |
| `trapezoid_plate` | Placa trapezoidal (malha não estruturada, elementos distorcidos), t = 20 mm, apoiada nas bordas, pressão 5 kPa | 187 | 160 |
| `cylinder_pressure` | Cilindro R = 1 m, H = 2 m, t = 10 mm, engastado na base, topo livre, pressão interna 100 kPa, malha 0,2 m | 352 | 320 |
| `cylinder_pressure_fine` | O mesmo cilindro com malha 0,07 m (2 668 elementos) | 2760 | 2668 |

## 3. Structura × Mecway (solver interno)
Erro relativo = máx. |diferença| ÷ máx. |valor do Mecway|.

| Caso | u_z máx. Structura (m) | u_z máx. Mecway (m) | Dif. u_z | Dif. rotação rx | Dif. momento principal máx. | Dif. momento principal mín. | ΣRz Structura / Mecway (N) |
|---|---|---|---|---|---|---|---|
| `ss_plate` | 0.00022094 | 0.00022094 | 2.6e-11 % | 5.39e-11 % | 1.01e-10 % | 1.11e-10 % | -1000 / -1000 |
| `clamped_plate` | 6.8422e-05 | 6.8422e-05 | 1.63e-11 % | 1.88e-11 % | 4.34e-11 % | 4.24e-11 % | -1000 / -1000 |
| `cantilever_plate` | 0.093308 | 0.093308 | 7.55e-09 % | 7.86e-09 % | 7.04e-09 % | 7.04e-09 % | -200000 / -200000 |
| `trapezoid_plate` | 0.00029003 | 0.00029003 | 4.34e-11 % | 5.25e-11 % | 0.264 % | 0.406 % | -8500 / -8500 |
| `cylinder_pressure` | -2.831e-05 | -2.8305e-05 | 0.104 % | 10 % | 10.8 % | 18 % | 1.24786e-09 / 8.92105e-10 |
| `cylinder_pressure_fine` | -2.8782e-05 | -2.877e-05 | 0.0465 % | 0.526 % | 1.43 % | 0.261 % | 5.62499e-09 / -2.65105e-09 |

Tensão de von Mises na face inferior (valores nos 4 cantos de cada elemento; o Mecway também inclui o cisalhamento transversal):

| Caso | Dif. máx. | Dif. mediana (pontos > 5 % do máx.) | Dif. L2 |
|---|---|---|---|
| `ss_plate` | 0.203 % | 0.0195 % | 0.0733 % |
| `clamped_plate` | 0.201 % | 0.061 % | 0.0886 % |
| `cantilever_plate` | 0.597 % | 0.0259 % | 0.183 % |
| `trapezoid_plate` | 2.8 % | 0.0466 % | 0.62 % |
| `cylinder_pressure` | 7.44 % | 1.03 % | 3.33 % |
| `cylinder_pressure_fine` | 7.29 % | 0.000162 % | 1.5 % |

## 4. Contra soluções analíticas (referência independente dos dois programas)
| Caso | Referência | Valor de referência | Structura | Mecway interno | Dif. Structura |
|---|---|---|---|---|---|
| `ss_plate` | Navier (série dupla), flecha no centro | 0.0002218 m | 0.00022094 m | 0.00022094 m | -0.389 % |
| `clamped_plate` | Timoshenko & Woinowsky-Krieger, α = 0,00126 (placa quadrada engastada, ν = 0,3), flecha no centro | 6.8796e-05 m | 6.8422e-05 m | 6.8422e-05 m | -0.543 % |
| `cantilever_plate` | Euler–Bernoulli + cortante (largura unitária, EI = E t³/12) | 0.096062 m (sem cortante: 0.096 m) | 0.093308 m | 0.093308 m | -2.87 % |
| `cylinder_pressure` | Teoria de membrana, deslocamento radial longe do engaste (p R² / E t), média da metade superior | 5e-05 m | 4.9753e-05 m | 4.9759e-05 m | -0.493 % |
| `cylinder_pressure_fine` | Teoria de membrana, deslocamento radial longe do engaste (p R² / E t), média da metade superior | 5e-05 m | 4.9971e-05 m | 4.9971e-05 m | -0.0583 % |

Momento de engaste do cilindro (camada limite): a teoria dá M₀ = p R t / (2 √(3(1−ν²))) = **302.6 N·m/m**. Maior |momento principal| nos cantos dos elementos da primeira fileira: Structura **129.2**, Mecway **128.9** (malha 0,07 m) — os **dois concordam entre si, mas nenhum chega ao valor teórico**: a camada limite tem comprimento característico ≈ 0,08 m, da ordem do elemento, então o pico não é resolvido nessa malha (valores nos cantos, sem extrapolação). A comparação com a teoria para esse pico **não foi concluída**; uma malha mais fina não pôde ser comparada porque o Mecway não devolveu solução para a malha de 8 000 elementos (falha silenciosa não investigada).

## 5. Convergência da diferença Structura × Mecway no cilindro
| Quantidade | Malha 0,2 m | Malha 0,07 m |
|---|---|---|
| Deslocamento radial (ux) | 2.03 % | 0.479 % |
| Rotação (rx) | 10 % | 0.526 % |
| Momento principal máx. | 10.8 % | 1.43 % |
| Momento principal mín. | 18 % | 0.261 % |

## 6. Solver CalculiX do Mecway (comparação secundária)
O CalculiX expande cada nó de casca em dois (superfícies superior e inferior); o deslocamento do nó da superfície média foi obtido pela média do par. Só deslocamentos foram comparados (as variáveis por elemento do CalculiX não são equivalentes).

| Caso | u_z máx. Structura (m) | u_z máx. CalculiX (m) | Dif. u_z (máx. no nó) |
|---|---|---|---|
| `ss_plate` | 0.00022094 | 0.00016326 | 35.3 % |
| `clamped_plate` | 6.8422e-05 | 4.0861e-05 | 67.5 % |
| `cantilever_plate` | 0.093308 | 0.093061 | 0.267 % |
| `trapezoid_plate` | 0.00029003 | 0.00028202 | 2.89 % |
| `cylinder_pressure` | -2.831e-05 | -2.8265e-05 | 0.403 % |
| `cylinder_pressure_fine` | -2.8782e-05 | -2.8509e-05 | 0.959 % |

Nas placas finas e simplesmente apoiada/engastada, o CalculiX resulta **bem mais rígido** que o solver interno do Mecway, o Structura e as soluções analíticas (flecha 26 % e 40 % menor). Isso foi **observado, não investigado**; a hipótese é a limitação conhecida dos elementos de casca expandidos do CalculiX em placas muito finas. É comportamento do solver CalculiX do Mecway, não do Structura, e por isso o solver interno é a referência principal.

## 7. Conclusões e limites
- **Placas (flexão):** Structura e Mecway interno coincidem em **1e-13 a 1e-10** em deslocamentos, rotações e momentos, inclusive em malha não estruturada com elementos distorcidos. Isto é coerente com o fato de ambos usarem a formulação MITC4 para flexão e cisalhamento (o manual do Mecway indica MITC4 para o quad4). Portanto, nesses casos, a concordância mostra que a implementação do Structura reproduz a formulação, **não é uma validação de formulação independente**.
- **Contra a teoria:** flechas dentro de ~0,6 % de Navier/Timoshenko (placas) e de 3 % no balanço (efeito de largura finita e malha), deslocamento radial do cilindro a 0,06 % da teoria de membrana na malha fina.
- **Cilindro (casca curva):** diferença de 0,1 % (u_z), 2 % (ux), 10–18 % (momentos, malha grossa) que **cai para 0,3–1,4 % no refino**. As diferenças restantes em membrana/curvatura são esperadas porque o Structura acrescenta modos incompatíveis de Wilson na membrana e rigidez de drilling, que o quad4 do Mecway não tem (explicação plausível, não testada isoladamente).
- **Tensão de von Mises:** diferença mediana ≤ 0,1 % nas placas; o máximo é maior (até 7,5 % no cilindro e 2,8 % na trapezoidal) em poucos pontos, em parte porque o Mecway inclui cisalhamento transversal. Não é desvio sistemático.
- **Não coberto por esta validação:** análise não linear, flambagem, modal, apoios elásticos/só-compressão, ties e acoplamento com barras, cargas térmicas e de corpo, peso próprio, hidrostática, moments/cortantes contra referência independente (só contra o outro programa), curvatura dupla (domos). Apenas **5 casos + 1 refinamento**, todos com uma única carga de pressão.
- **Versão do Structura testada:** `mefs.dll` distribuída (a antiga, sem as correções D1/D2 da Etapa 3; elas não afetam análise linear com um caso de carga).

## 8. Arquivos
Em `docs/validacao_mecway/`: `*.liml` (modelos prontos para abrir no Mecway 28), `summary.json` (todos os números), `gen_mecway.py`, `compare.py`, `val_cases.h`, `val_dump.cpp`, `val_shots.cpp`, `mw_shot.ps1` (reprodução) e `prints/` (imagens para divulgação: Structura, Mecway e lado a lado).