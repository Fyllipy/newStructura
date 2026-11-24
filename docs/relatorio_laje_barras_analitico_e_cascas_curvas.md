# Laje sobre estrutura reticulada contra formulações analíticas, e benchmarks de casca curva (pinched cylinder, hemisfério com furo)

Data: 2026-11-24. Sem Mecway nesta rodada. Só aplicativo e harnesses; DLLs, `.enc` e executáveis comerciais intactos (hashes `f40c09a9d032`, `09ffc08a6001`, `c3a9b8b2c3b6`, `ff6c9550e2d2`). Nenhum conserto de código foi necessário; só harnesses e scripts novos.

## 1. Laje (placa MEF) sobre vigas e pilares (barras) x formulação analítica

Pipeline real: `Shell::Model` + barras → `ShellCouplingService` → `rds.dll`. Vigas e pilares 30x30, E = 30 GPa, laje E = 30 GPa, q = 5 kN/m2, pilares de 3 m engastados na base. Script: `docs/validacao_mecway/laje_vigas/check_analitico.py`; saídas `resultado_analitico.json`, `resumo_analitico.txt`. Esforços lidos das forças de extremidade das barras do solver (`s_bars.csv`), sem Mecway.

### 1.1 Referência fechada

LV3: laje 6x4 m armada numa direção, apoiada em duas vigas (y = 0 e 4) sobre 4 pilares. Cada viga é um pórtico plano de base engastada com w = q·ly/2 = 10 kN/m: θ = (wL²/12) / (EI (4/h + 2/L)), M_apoio = wL²/12 − 2EIθ/L, M_meio = wL²/8 − M_apoio, M_base = 2EIθ/h, M_topo = 4EIθ/h, H = (M_base + M_topo)/h, N_pilar = wL/2, V_apoio = wL/2. Referência: M_apoio 24,0 kN·m, M_meio 21,0, M_base 12,0, M_topo 24,0, H 12,0, N 30,0.

### 1.2 Modo T (quinhões de carga, barras com carga distribuída)

| grandeza | analítico | Structura | erro |
|---|---|---|---|
| M_apoio | 24 000 N·m | 23 964 | −0,15 % |
| M_meio | 21 000 | 21 036 | +0,17 % |
| V_apoio | 30 000 N | 30 000 | 0,00 % |
| N_pilar | 30 000 N | 30 000 | 0,00 % |
| N na viga (H) | 12 000 N | 11 952 | −0,40 % |
| M_base | 12 000 N·m | 11 892 | −0,90 % |
| M_topo | 24 000 N·m | 23 964 | −0,15 % |

O resíduo (≤ 0,9 %) é a deformação axial e a flexibilidade do nó, que a fórmula de Euler com barras inextensíveis ignora.

LV1 (laje 6x4 nos dois sentidos, 4 vigas de borda, 4 pilares), quinhões a 45°: viga longa trapézio, curta triângulo. Pela simetria, V_apoio = metade da carga da viga e N_pilar = qA/4, independentes da rigidez; M_meio − M_apoio vem do equilíbrio do semivão.
- N_pilar = 30 000 N nos quatro pilares (0,00 %); V_apoio = 20 000 N (longa) e 10 000 N (curta), 0,00 %.
- M_meio − M_apoio: 37 500 N·m (longa) e 12 500 N·m (curta). Contra o quinhão ideal (38 333 e 13 333) a diferença é 2,2 % e 6,3 %; contra o quinhão médio por elemento de 1 m, **que é como o solver recebe a carga, a diferença é 0,00 %** (as rampas discretizadas explicam todo o desvio).

LV4 (laje lisa 6x6 m sobre 4 pilares, sem vigas), modos A e M: N_pilar = 45 000 N nos quatro (= qA/4), 0,00 %.

### 1.3 Modos acoplados (A: laje apoiada, só translações; M: monolítico) — o que a hipótese de quinhão não captura

Com a laje de 150 mm o modo A fica 25 % abaixo do pórtico em M_apoio, M_meio e V_apoio (M: 59 %, 37 % e 75 %). A causa é física, não defeito: a laje é mais rígida que a viga (EI da faixa de 6 m: 5,1e7 contra 2,0e7 N·m² da viga) e transmite carga direto aos nós sobre os pilares. N_pilar continua exato (0,00 %), pois depende só do equilíbrio.

Para provar a causa, varri a espessura (laje 6x4 m, vigas 30x30) e comparei com o pórtico sob as **cargas nodais equivalentes** que o MEF entrega (10 kN por nó de 1 m, nós de ponta direto ao apoio: M_apoio 23,3, M_meio 21,7 kN·m):

| laje | A: M_apoio | A: M_meio | A: M_base | A: V_apoio |
|---|---|---|---|---|
| 150 mm, ν = 0,2 | −22,6 % | −29,0 % | −22,9 % | 22,0 kN |
| 100 mm | −11,8 % | −13,2 % | −12,2 % | 23,1 |
| 50 mm | −7,0 % | −6,3 % | −7,5 % | 23,1 |
| 20 mm | −6,3 % | −5,4 % | −6,9 % | 23,0 |
| **20 mm, ν = 0** | **−0,2 %** | **+0,1 %** | **−0,8 %** | **25,0 (ref. 25,0)** |

Leitura: o resíduo de 6 % que sobra quando a laje fica fina é a **força de canto de placa com bordas livres** (efeito de Poisson); com ν = 0 ele desaparece e o modo A coincide com o pórtico (M_apoio −0,2 %, M_meio +0,1 %, V_apoio 25 003 contra 25 000 N). A esforço normal da viga fica −10 % mesmo assim: a laje fina, ligada por translação, absorve parte do empuxo H pela rigidez no plano (esperado; sem fórmula fechada simples). O modo M (6 GDL compartilhados) tem ação mista de viga T; sem fórmula fechada (largura efetiva e *shear lag*), o resultado fica sem referência analítica nesta rodada (o equilíbrio global e o N_pilar fecham a 0,00 %; contra o Mecway já estava validado em 5e-5).

Gráfico: `docs/validacao_mecway/laje_vigas/prints/diagrama_momento_lv3_analitico.png` (diagrama de momento: analítico x Structura, modos T e A, três lajes).

## 2. Cascas curvas (MacNeal-Harder), revolução parcial

Harness `docs/validacao_mecway/curvas/pc_dump.cpp` (`pc_shots.cpp` para os prints), resultados em `resultado_pc_hs.csv`. Malhas sem aviso ou erro de qualidade.

**Pinched cylinder** (1/8; R = 300, L = 600, t = 3, E = 3e6, ν = 0,3; carga 0,25 no ponto sobre dois planos de simetria; diafragmas uy = uz = 0). Referência 1,8248e-5 (série de Flügge; Dvorkin e Bathe 1984; **conferida na web**: [SolidWorks](https://help.solidworks.com/2025/English/simconntutorial/Test_of_a_Pinched_Cylinder_with_Diaphragm.htm), [Abaqus](https://abaqus-docs.mit.edu/2017/English/SIMACAEBMKRefMap/simabmk-c-pinchcyl.htm)):

| elementos (lado) | nós | u sob a carga | u / ref. |
|---|---|---|---|
| 75 | 45 | 1,277e-5 | 0,700 |
| 37,5 | 135 | 1,622e-5 | 0,889 |
| 18,75 | 459 | 1,774e-5 | 0,972 |
| 9,375 | 1 749 | 1,827e-5 | **1,001** |

**Hemisfério com furo de 18°** (1/4; R = 10, t = 0,04, E = 6,825e7, ν = 0,3; cargas de 1,0 nos dois nós do equador sobre planos de simetria; uz preso num ponto). Referência 0,0924:

| elementos no meridiano | nós | u sob a carga | u / 0,0924 |
|---|---|---|---|
| 4 | 35 | 0,0246 | 0,266 |
| 8 | 99 | 0,0715 | 0,774 |
| 16 | 357 | 0,0913 | 0,988 |
| 32 | 1 353 | 0,0933 | **1,009** |

Pontos de atenção:
- Com cargas de 0,5 (F = 1 no modelo inteiro) o resultado convergia a 0,0467, metade da referência. Com a convenção do modelo de 1/4 (1,0 por nó, F = 2 no modelo inteiro) converge a 0,0933. A convenção foi confirmada só **indiretamente**: [SkyCiv](https://skyciv.com/docs/tech-notes/s3d-plate-verification-models/hemispherical-shell-under-concentrated-loads/) usa 1,0 nos nós do modelo de 1/4 e dá 0,0948 na casca sem furo (referência 0,094); [ENERCALC](https://media.enercalc.com/ec3d_verification/d-05-(hemispherical-shell-with.htm) cita 0,0927 a 0,0935 em malhas finas do caso com furo. **O valor 0,0924 continua de memória**: não achei fonte que o dê explicitamente.
- A faixa de 1 % em malha de 32 elementos fica dentro da dispersão de outros programas, mas a malha 4 e 8 estão longe (membrana e flexão acopladas; elemento só converge com malha fina).
- Não comparei com o Mecway (o usuário dispensou nesta rodada); diferença para o Scordelis-Lo medida antes (até 11 % em malha grossa) segue sem causa isolada por teste.
- Legenda dos prints diz "mm" mas o modelo é adimensional (a vista multiplica por 1000); os valores da tabela acima estão em unidades do modelo.

Prints: `docs/validacao_mecway/curvas/prints/pinched_cylinder_{16,32}_structura.png`, `hemisferio_furo18_{16,32}_structura.png`.

## 3. Não feito / limites

- O modo M não tem comparação analítica (ação mista sem fórmula fechada); LV2 (duas baías) só foi conferido por equilíbrio.
- Esforço cortante e axial das barras foram comparados só no pórtico do LV3 (T) e por simetria no LV1.
- Os benchmarks de casca curva rodam no harness, **não entram no `ctest`** (precisam da `mefs.dll` com chave de teste): se quiser regressão automática, adiciono ao `TestShell` com tolerância de 3 %.
- Referência do hemisfério é de memória, apoiada em evidência indireta.

## 4. Investigação da divergência dos modos acoplados (adendo)

Pergunta: a divergência do modo A em relação ao pórtico é defeito do acoplamento? **Não: é a resposta física do modelo; a hipótese de quinhão (apoios rígidos) é que não vale para laje mais rígida que a viga.** Evidências (mesmo caso LV3, viga y = 0, modo A, esforços em N·m e N; referência do pórtico sob as cargas nodais da laje: M_apoio 23 333, M_meio 21 667, M_base 11 667, H 11 667):

| caso | M_apoio | M_meio | M_base | N na viga | V_apoio |
|---|---|---|---|---|---|
| laje 150 mm, malha 1 m | 18 058 | 15 388 | 8 999 | 5 255 | 22 047 |
| laje 150 mm, malha 0,5 m | 18 648 | 14 407 | 9 291 | 6 745 | 27 377 |
| laje 150 mm, malha 0,25 m | 18 791 | 14 399 | 9 361 | 7 724 | 31 123 |
| laje 20 mm, ν = 0 | 23 291 | 21 681 | 11 572 | 10 466 | 25 003 |
| laje 5 mm, ν = 0 | 23 300 | 21 700 | 11 567 | 11 287 | 25 000 |

- **Independência de malha:** com a laje de 150 mm, M_apoio, M_meio e M_base convergem (18,8 / 14,4 / 9,4 kN·m). A diferença para o pórtico (24 / 21 / 12) não é erro de discretização: a laje (EI da faixa de 6 m = 5,1e7 N·m², contra 2,0e7 da viga) divide a carga com os pilares por flexão e torção.
- **Limite correto:** laje fina (5 a 20 mm) e ν = 0 reproduz o pórtico (M_apoio −0,2 %, M_meio ±0,2 %, V_apoio 25 000 N) e a normal na viga tende ao valor analítico quando a espessura cai (−10 % com 20 mm, −3 % com 5 mm), o que confirma que a laje absorve parte do empuxo pela rigidez no plano.
- **Único valor que não converge:** o cortante na primeira barra junto ao pilar (22,0 / 27,4 / 31,1 kN com a malha refinando): é a reação concentrada no nó de canto de uma placa com bordas livres, singular. Para dimensionar a viga junto ao apoio use o cortante a uma distância da face do pilar, não o do primeiro elemento. Os momentos de vão e a normal do pilar não têm esse problema.
- Conclusão: nenhum conserto de código foi indicado pelos dados; o modo T continua valendo só para laje flexível em relação às vigas.
