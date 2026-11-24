# Laje sobre vigas e pilares: correção do descarregamento e validação contra o Mecway 28

Data: 2026-11-23. Solver: `rds.dll` candidata de teste (D1/D2 corrigidos), pipeline real do Structura. Referência: Mecway 28 (solver interno, quad4 MITC4 + line2).

## 1. Diagnóstico (o que realmente acontecia)

A laje descarrega nas barras, mas por **três caminhos** com garantias diferentes:

| Modo | Como a laje chega às barras | Estado antes |
|---|---|---|
| T. Quinhões (padrão do projeto) | envoltória: carga distribuída nas vigas **sobre as arestas** da chapa | só carga superficial; **cargas por clique (Fase 4) eram descartadas em silêncio**; laje lisa sobre pilares, temperatura e líquido também; os avisos existiam mas **nunca chegavam à tela**; soma das cargas perdia 0,33% |
| A. Acoplado, apoiada | MEF, só translações ligadas | correto por equilíbrio; aviso de mal condicionamento (efeito da penalidade) |
| M. Acoplado, monolítico | MEF, 6 graus compartilhados | **correto: bate com o Mecway** (seção 3) |

Medido (lv_dump, 5 kN/m²; `resumo_modos.txt`):
- LV5 (mesma laje do LV1, carga por clique) no modo T: **soma das reações = 0**. A carga sumia.
- LV4 (laje lisa) no modo T: reações = 0 (aviso "nenhuma barra sobre as arestas" existia, mas não aparecia).
- LV3 no modo T: 119,6 kN de 120 kN.
- LV6 (vigas não divididas nos nós da malha): flechas 2× maiores (ligação só nas pontas). Isso é o comportamento esperado do modelo (o Mecway dá o mesmo), tratado hoje pelo diálogo "Dividir barras nos nós" antes da análise.

## 2. Correções (código de produção, só aplicativo; DLLs intactas)

1. `ShellTributary`: a integral numérica do quinhão é normalizada para **forca por area x area** (conservação exata, ajuste < 5%).
2. `ShellTributary::tributaryPointForces` + `ShellCouplingService::attachTributary`: cargas por pontos (forca em nós, linear, região) viram forças concentradas e vão para a **viga de aresta apoiada mais próxima** como forças nodais nos dois nós da barra; ponto na linha de simetria divide igualmente entre as vigas.
3. Cargas que o modo de quinhões não leva às barras (temperatura, líquido, objeto que não é chapa plana, laje sem viga nas arestas, pontos sem destino) geram **aviso**; `AnalysisService::BuildResult::warnings` e `executeAnalysis` mostram esses avisos na janela de avisos da análise.
4. Teste novo `tributaryModeConservesTheLoadAndCarriesPointLoadsToTheBars` (área, região por clique, força em nó, simetria, temperatura, laje lisa).

## 3. Validação contra o Mecway (modo M, nó a nó, mesma malha)

Modelo: laje t = 15 cm, E = 30 GPa, ν = 0,2, carga 5 kN/m²; vigas e pilares 30x30 (pilares de 3 m engastados), nós compartilhados. Diferença máxima relativa ao maior valor (`summary_modo_M.json`):

| Caso | uz | rotações x/y | Rz | Mx/My de base |
|---|---|---|---|---|
| LV1 laje, 4 vigas, 4 pilares | 6,8e-5 | 2,7e-4 / 2,5e-4 | 2e-13 | 1,4e-4 / 8e-5 |
| LV2 duas baias, viga central, 6 pilares | 7,2e-5 | 2,0e-4 / 3,4e-4 | 4,8e-6 | 1,3e-4 / 1,0e-4 |
| LV3 laje em 2 vigas, bordas livres | 6,6e-5 | 3,3e-4 / 1,1e-4 | 2e-13 | 3,8e-4 / 4,3e-5 |
| LV4 laje lisa sobre 4 pilares | 2,5e-6 | 2,5e-6 | 1e-13 | 2,8e-5 |
| LV5 carga por clique (= LV1) | idêntico ao LV1 | | | |
| LV6 vigas não divididas | 7,1e-6 | 1,2e-5 | 2e-13 | 9e-5 |

As diferenças de 1e-4 vêm da constante de torção da barra (J do retângulo) e da penalidade de dreno; as rotações em z (≈1e-6 rad) e os deslocamentos no plano (≈1e-6 m) são ruído numérico (relativo alto, absoluto desprezível). O teste de regressão `monolithicSlabOnBeamsAndColumnsMatchesMecway` fixa os valores do Mecway (LV2) com tolerância 5e-4.

Prints (`prints/`): `*_structura.png`, `*_mecway.png`, `*_comparacao.png` de LV1, LV2 e LV4, mesma grandeza (deslocamento total) e mesma deformada.

## 4. Não validado (declarado)

- **Modo A (apoiada)**: sem equivalente simples no Mecway; verificado só por equilíbrio, simetria e por ser mais flexível que o M. O solver avisa "mal condicionado (razão de pivôs 1e16)": vem da penalidade de 1e7 nas ligações; o equilíbrio fecha em 1e-9, mas o aviso aparece em todas as análises desse modo.
- **Modo T**: método aproximado (envoltória). No LV2, reações de pilar da viga central 40,5 kN (T) contra 44,8 kN (M, = Mecway): 10% de diferença por não considerar a viga interna, que só recebe carga se a laje for dividida em painéis cujas arestas coincidam com ela.
- Excentricidade viga-laje (viga abaixo do plano), cargas térmicas, 2ª ordem: não comparados com o Mecway nesta rodada.
- Nenhum botão novo da janela principal foi exercitado manualmente.

## 5. Reprodução

`lv_dump.cpp` (gera casos e resultados), `gen_lv.py` (modelos do Mecway em `modelos_mecway/`), `compare_lv.py`, `resumo_modos.py`, `lv_shots.cpp` e `compose_lv.ps1` (prints). Mecway: `Mecway.exe arq.liml script x.py`, engaste por `faceid 2` (extremidade inicial) do pilar.

`ctest` completo em `build-dev`: 27/27. Hashes comerciais (rds.dll, mefs.dll, .enc, executáveis): inalterados.
