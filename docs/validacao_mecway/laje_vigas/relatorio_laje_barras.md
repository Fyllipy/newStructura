# Acoplamento laje (casca/MEF) x barras (rigidez direta): nós, apoios, quinhões e esforços contra o Mecway 28

Data: 2026-11-23. Barras: `rds.dll` (solver de rigidez direta, perfil 30x30, E = 30 GPa). Laje: `mefs`/Q4 só em casca, 15 cm, carga 5 kN/m². Mecway 28: laje em quad4 e barras em line2 (1D), nos mesmos nós. Complementa `relatorio_laje_vigas.md`.

## 1. Nós que a laje cria nas barras (e apoios)

Verificação independente em Python (posições dos nós da malha sobre o eixo de cada barra) contra `ShellCouplingService::barCuts` (o que "Dividir barras nos nós das cascas" aplica):

| Caso | cortes esperados | cortes do serviço | nós de malha sobre barras | todos com nó de barra após dividir | ligações modo apoiada (translação, mesmo ponto) |
|---|---|---|---|---|---|
| LV1 laje 6x4, 4 vigas | 16 | 16, iguais | 20 | sim | 20 de 20 |
| LV2 8x4, viga central | 23 | 23, iguais | 27 | sim | 27 de 27 |
| LV3 2 vigas | 10 | 10, iguais | 14 | sim | 14 de 14 |
| LV5 carga por clique | 16 | 16, iguais | 20 | sim | 20 de 20 |
| LV6 vigas não divididas | 16 | 16, iguais | 20 | **não (só 4)** | 4 de 4 |

- Modo monolítico: nós fundidos = nós de malha sobre barras (20, 27, 14, 20). Modo apoiada: uma ligação de translação por nó, no mesmo ponto, sem rotação.
- LV6 mostra o que acontece sem dividir: 16 nós da laje caem sobre barras sem ligação (o solver avisa e o diálogo de divisão resolve). O Mecway reproduz a mesma desconexão e dá o mesmo resultado (diferença 7e-6).
- Apoios: reações verticais dos pilares iguais ao Mecway (diferença 2e-13 a 5e-6 do maior) e soma igual à carga em todos os casos.

## 2. Esforços das barras (laje acoplada, modo monolítico) contra o Mecway

Momento médio por elemento de viga: Structura = esforços de extremidade do solver de barras; Mecway = EI·Δθ/L a partir das rotações nodais (o Mecway não liberou o momento de barra por nome de variável; para vigas Timoshenko M = EI·dθ/dx é exato). Diferença máxima relativa ao maior momento:

| Caso | elementos | erro máx |
|---|---|---|
| LV1 | 20 | 7,6e-5 |
| LV2 | 28 | 8,2e-5 |
| LV3 | 12 | 2,1e-5 |
| LV6 | 4 | 6,1e-5 |

Momento no meio de cada viga (N·m), Structura / Mecway: LV1 viga longa 12 258,8 / 12 258,4; viga curta 6 257,3 / 6 257,8; LV2 viga central 8 759,6 / 8 758,9; LV3 12 537,7 / 12 537,6 (lista completa em `resumo_barras.txt`; gráfico em `prints/barras_momento_meio_vigas.png`).

## 3. Quinhões (modo sem MEF) contra a envoltória analítica e contra o Mecway 1D

Carga distribuída que a laje aplica em cada barra, contra a regra dos 45° calculada fora do Structura (`check_quinhoes.py`):

| Caso | barras | erro máx / pico | total aplicado |
|---|---|---|---|
| LV1 | 20 | 5e-8 | 120 000 N = q·A |
| LV2 (laje única, viga central fora das arestas) | 28 | 7e-8 | 160 000 N |
| LV3 | 12 | 0 | 120 000 N |
| LV6 (barras não divididas) | 4 | 1e-7 | 120 000 N |
| LV5 (carga por clique, força nodal por nó) | | soma 120 000 N; viga longa +2,1%, curta +8,3% (degrau do nó mais próximo) | |

**Defeito achado e corrigido:** o último trecho de cada viga recebia 1,7% a menos (a marcha de profundidade partia do vértice e saía do polígono; a normalização global escondia o erro nos demais). `ShellTributary.cpp` agora amostra um fio para dentro; teste novo exige q uniforme em todas as barras.

Barras 1D do Mecway com **as mesmas cargas** que o Structura aplicou (distribuída por barra via densidade, forças nodais por clique): deslocamentos, rotações, reações e momentos de base **iguais a 1e-14** (LV1, LV3, LV5), isto é, o solver de barras e o do Mecway coincidem; `resultado_barras_modoT.json`.

## 4. Prints (`prints/`)

`nos_apoios_planta_LV1.png`, `nos_apoios_planta_LV2.png` (nós criados e apoios), `quinhoes_carga_por_barra.png`, `barras_momento_meio_vigas.png`, e os de campo de deslocamento `*_structura.png`, `*_mecway.png`, `*_comparacao.png` (LV1, LV2, LV4).

## 5. Não validado

- Modo apoiada (translações ligadas, laje articulada): sem equivalente no Mecway; só equilíbrio, ligações nos pontos certos e simetria.
- Quinhão nodal de carga por clique: aproximação em degraus (2% a 8% por viga), total exato.
- Esforço cortante e axial por barra no Mecway: não extraídos (variáveis de barra não habilitadas); momentos sim.
- O botão "Dividir barras nos nós das cascas" na janela principal não foi exercitado; validei a função que ele usa (`barCuts`) e o resultado da divisão.
- Barra abaixo do plano da laje (excentricidade) e 2ª ordem: não comparados.

Reprodução: `lv_dump.cpp`, `gen_lv.py`/`gen_lb.py`, `compare_lv.py`, `check_barras.py`, `check_quinhoes.py`, `charts_lv.ps1`, `lv_shots.cpp`, `compose_lv.ps1`. Teste de regressão: `TestShellCoupling` (uniformidade dos quinhões, conservação, carga por clique, laje x Mecway). `ctest` 27/27; hashes comerciais inalterados.
