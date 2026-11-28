# Metodologia do comportamento mecânico do T-stub (etapa A3)

Estado de cada item: **[IMPLEMENTADO]**, **[PARCIAL]**, **[PLANEJADO]**, **[HIPÓTESE NÃO VALIDADA]**, **[LACUNA]** (falta fonte). Este documento não declara validação profissional. Nada aqui altera resultados de resistência, a curva momento-rotação, a Camada A, o catálogo padrão ou a regra NÃO VERIFICÁVEL para várias fileiras.

## 1. Diagnóstico da implementação atual

### 1.1 O que o núcleo já calcula (`src/design/TStub.{h,cpp}`, `ColumnFlangeLever.cpp`)
| Item | Situação |
|---|---|
| Unidades | N, mm, MPa; tração positiva; forças de parafuso por parafuso (`Bolts::ftRd`) ou total (`Bolts::total()`). |
| Geometria | `Geometry{m, e, emin, ew}` (EN Fig. 6.2), `n = min(emin ; 1,25 m)`. |
| Material | `Material{fy}`; `EnCoefficients{γM0 = 1,0; γM2 = 1,25; k2 = 0,9}` (resistência; valores recomendados, sem Anexo Nacional). |
| Comprimentos efetivos | `columnFlangeRow`, `columnFlangeGroup` (Tab. 6.4), `endPlateExtensionRow` (Tab. 6.6); `EffectiveLength{circular, nonCircular}`. |
| Resistência | `resistance(...)`: modos 1 (métodos 1 e 2), 2 e 3 (Tab. 6.2, alavanca desenvolvida, EN 6.2.4.1 Nota 1); `Modes{f1,f2,f3,resistance,governing}`. |
| Fileiras e grupos | `columnFlange(...)` (procedimento do SCI P398). |
| Parâmetros de parafuso | `boltTensionResistance` (k2 fub As/γM2), `boltStressArea` (M12–M36), `boltHeadWidth`. |
| Dados ausentes para o comportamento | rigidez (k4, k5, k10), comprimento de alongamento do parafuso Lb (material agarrado, arruelas, altura de cabeça e porca), módulo E, lei força-deformação, deformação disponível por modo: **não existiam no núcleo**. |

Observações da auditoria (sem alteração de código de produção):
1. A rigidez já existe, mas só em `JointComponents.cpp` (k3, k4, k5, k10, `keff`), que **não foi tocado**. O núcleo não a reutiliza (o núcleo é matemática pura sem dependência do módulo de componentes; a migração é etapa posterior).
2. `JointComponents.cpp` usa para `leff` de rigidez apenas a fileira **isolada** (`min(circular, não circular)`). A EN (Tabela 6.11, nota) diz "o menor dos comprimentos efetivos (individualmente ou como parte de um grupo de fileiras)". Para várias fileiras a EN pode dar `leff` menor. **Divergência registrada, não corrigida** (fora do escopo; afeta a ferramenta momento-rotação).
3. `JointComponents.cpp` assume silenciosamente altura de cabeça 0,7·db e de porca 0,8·db quando não informadas. O novo código de rigidez **não** assume nada (NaN = não informado → inválido).
4. A Camada B mostra η = FT,Rd/solicitação; nada nela depende de rigidez.
5. Não há dependência circular: `TStub` e `TStubStiffness` não incluem NBR, `Connections.h` nem `ColumnFlangeLever`.

### 1.2 Divergência de `dw` — resolução
Implementado (`TStub.cpp`): `dw = s · 1,1547005`, onde `s` é a largura entre **faces** da cabeça/porca sextavada: tabelada (série ISO 4014/4017: M12 18, M16 24, M20 30, M22 34, M24 36, M27 41, M30 46, M36 55 mm) ou, fora da tabela, **estimada `s = 1,5·db`** (hipótese empírica). Logo:
- Grandeza implementada: **largura entre vértices** (`2/√3 ≈ 1,1547` × largura entre faces), usada em `ew = dw/4`.
- Fallback efetivo: `dw = 1,5·db·1,1547 = √3·db ≈ 1,732·db`. **`1,5·db` é a estimativa da largura entre FACES, não o `dw`**; o relatório de saneamento descreveu a metade (`1,5·db`), o documento arquitetural descreveu a expressão completa (`1,5·db·1,1547`). O **código e o documento arquitetural estão coerentes entre si; o texto abreviado do relatório de saneamento era ambíguo** (corrigido por errata nele).
- Coerência geométrica: para um hexágono regular, vértice-a-vértice = 2/√3 × face-a-face (confirmado por `Width across flats`, geometria elementar). O fator foi truncado em 1,1547005 (erro relativo 3·10⁻⁸, desprezível; mantido para não alterar resultados).
- Origem do `s`: série ISO de parafusos comuns, **não** de parafusos de alta resistência: a EN 14399-4 (HV) tem `s` maiores (M20 32, M24 41, M30 50 mm, conforme catálogos consultados: [PGB manual](https://pgbeuecommerce.blob.core.windows.net/cmsfiles/downloads/brochures/sb-hr-hv_manual_en.pdf), [BS EN 14399-4](https://pppars.com/wp-content/uploads/2021/01/BS-EN-14399-4-2015.pdf)), e o `dw` da EN 1993-1-8 (Tabela 6.2) é "o diâmetro da arruela **ou** a largura entre vértices da cabeça/porca, conforme o caso" (texto confirmado em cópia da EN 1993-1-8:2005+AC:2009). Quando há arruela, o `dw` da EN é o diâmetro da arruela, que o código **não usa**. **[HIPÓTESE NÃO VALIDADA]**: dw por vértices da série ISO.
- Impacto numérico: **nenhum resultado foi alterado.** Sensibilidade (HEB200, M20, memória da Camada B): `dw` = 34,6 (implementado) → 37,0 (arruela ~37 mm) → 44 mm muda o modo 1 de 649 → 666 → 720 kN; o modo 2 (277 kN, governante no exemplo) **não depende de `ew`**. Só o modo 1 (mesa fina) sente `dw`, e sempre a favor da resistência. Diâmetros fora da tabela (M14, M18, M33): estimativa `1,5·db` para `s` difere de valores comerciais usuais (22, 27, 50 mm — **de memória, não verificados nesta tarefa**) em até ~5 %.
- Testes: `TestTStub::boltHeadWidthDefinitionAndFallback` (grandeza = faces × 2/√3 para todos os tabelados; M24 = 41,57 ≈ 41,6 do benchmark; fallback = √3·db ≠ 1,5·db; a memória da Camada B para M14 contém "VALOR ESTIMADO", a expressão e o valor).

## 2. Fundamentação (referências efetivamente consultadas)
| Fonte | Uso | Situação |
|---|---|---|
| EN 1993-1-8:2005, 6.3.2 e Tabela 6.11; Tabela 6.2 (`dw`) | equações de rigidez e definição de `dw` | texto extraído localmente (OCR com símbolos ilegíveis nas fórmulas: **as equações foram confirmadas pelos números publicados abaixo**, não pelo OCR) |
| SCI P398 | resistência (já usado) | o P398 declara que **a rigidez da ligação não é coberta** pelo guia (arquivo local); não serve de referência para rigidez |
| Exemplo SCIA, EN 1993-1-8 (HEB140 + IPE220) | k3, k4, k5, k10, keff publicados | valores transcritos em `docs/AUDITORIA_LIGACOES.md`; **a fonte original não foi relida nesta tarefa** |
| Exemplo C2 do SCI P398 (New Steel Construction, fev/2020) | k10 = 1,6·353/70,5 = 8,01 mm | idem |
| Piluso, Faella, Rizzano, *Ultimate behavior of bolted T-stubs. I* e *II*, J. Struct. Eng. 127(6), 2001 ([ASCE II](https://ascelibrary.org/doi/10.1061/(ASCE)0733-9445(2001)127:6(694))) | modelo teórico de capacidade de deformação plástica; leis força-deslocamento por modo | **localizado, não lido** (só resumo): equações **não obtidas → [LACUNA]** |
| EN 1993-1-8, 6.4.2 (capacidade de rotação) | condições qualitativas para capacidade de rotação suficiente | cláusula localizada ([cópia](https://www.phd.eng.br/wp-content/uploads/2015/12/en.1993.1.8.2005-1.pdf)); as duas condições do 6.4.2(2) **não foram lidas literalmente nesta tarefa → [LACUNA]**; a literatura nota que são regras grosseiras |
| Jaspart (1991), Weynand (1996), Beg et al. (2004), Latour & Rizzano, Kuhlmann | leis multilineares, ductilidade de T-stubs e ligações | **citados de memória como candidatos; não consultados → [LACUNA]** |

## 3. Rigidez inicial (Fase B)

### 3.1 Equações [IMPLEMENTADO] em `src/design/TStubStiffness.{h,cpp}`
Referência: EN 1993-1-8:2005, 6.3.2, Tabela 6.11. **Específicas da EN.** Os coeficientes ficam em `EnStiffnessCoefficients{0,9; 1,6}`, separados de `EnCoefficients`; não existe equivalente na NBR 8800.

| Componente | Equação | Variáveis (unidades) |
|---|---|---|
| Mesa do pilar em flexão (k4) / chapa em flexão (k5), uma fileira | `k = 0,9 · leff · t³ / m³` | `leff` (mm): o menor dos comprimentos efetivos **da fileira** (Tab. 6.4/6.6); `t` (mm) = tf ou tp; `m` (mm) como na resistência; `k` em mm |
| Parafusos em tração, uma fileira | `k10 = 1,6 · As / Lb` | `As` (mm²) de **um** parafuso; `Lb` (mm) = material agarrado + arruelas + (cabeça + porca)/2; `k10` em mm |
| T-stub de uma fileira | `k_T = 1 / (1/k4 + 1/k10)` (ou `k5`) | em série: a mesma força atravessa mesa/chapa e parafusos |
| Rigidez em força | `K = k · E` | `E` (MPa) → N/mm |

Hipóteses e limites:
- **Uma fileira (2 parafusos).** Várias fileiras (leff de grupo, interação entre fileiras, `keq`, `zeq`): **[PLANEJADO]**, sem implementação.
- A EN dá `ki` por componente sem distinguir casos com e sem alavanca; o núcleo usa a mesma convenção da resistência (alavanca desenvolvida, EN 6.2.4.1 Nota 1). **[HIPÓTESE NÃO VALIDADA]** de coerência; não é prescrição.
- O `leff` é **fornecido** pelo chamador: o núcleo não duplica nem recalcula comprimentos efetivos nesta função. O valor "menor, individual ou de grupo" para várias fileiras não é fornecido pelo núcleo atual.
- Sem pré-carga explícita (a EN dá a mesma equação para parafusos pré-tensionados ou não); sem folgas de furo, sem deslizamento, sem não linearidade; **rigidez inicial elástica apenas**.
- Entradas obrigatórias, sem defaults: `NaN` ou não positivo → `valid = false` com causa. Arruela deve ser informada (0 é válido).
- Não altera `Check`, `Summary`, resistência nem memória das verificações atuais (o módulo não é chamado por `ColumnFlangeLever`).

### 3.2 Exemplo numérico (SCIA, entradas publicadas)
`k4 = 0,9 × 107,55 × 12³ / 26,9³ = 8,593 mm`; `Lb = 24 + 3,3 + (10 + 13)/2 = 38,8 mm`; `k10 = 1,6 × 157 / 38,8 = 6,474 mm`; `k_T = 1/(1/8,593 + 1/6,474) = 3,69 mm` → `K = 3,69 × 210 000 = 7,7·10⁵ N/mm`.

### 3.3 Validação (Fase B)
| Referência | Grandeza | Publicado | Calculado | Erro | Tolerância e justificativa |
|---|---|---|---|---|---|
| SCIA | k4 fil. 1 / 2 | 8,59 / 8,39 | 8,593 / 8,389 | 0,03 % / 0,01 % | 0,2 %: valores publicados com 2 decimais (±0,06 %) e `leff`, `m` publicados arredondados |
| SCIA | k5 fil. 1 / 2 | 7,55 / 7,57 | 7,547 / 7,565 | 0,04 % / 0,07 % | 0,5 %: `mx` recalculado de valores arredondados |
| SCIA | k10, Lb | 6,47; 38,8 | 6,474; 38,8 | 0,06 %; 0 | 0,2 % |
| SCIA | keff (série com k3 publicado) | 1,73 / 1,71 | 1,730 / 1,710 | < 0,02 % | 1 % |
| SCI P398 C2 | k10 = 1,6·353/70,5 | 8,01 | 8,011 | 0,02 % | 0,2 % (só aritmética: Lb composto não foi publicado) |
Testes: `TestTStubStiffness` (benchmark publicado; leis de escala t³, leff, m⁻³, As, Lb⁻¹; monotonicidade em t; limite de mesa rígida → k10; série com componente infinito; dados ausentes/inválidos). Conferência independente em Python: `docs/validacao_alavanca/validate_stiffness.py` (`saida_rigidez.txt`).

**Limites da validação:** (i) as referências são cálculos **publicados** com as mesmas equações da EN: validam a **aritmética e a transcrição**, não o comportamento físico; (ii) **não há** MEF independente nem resultados experimentais de rigidez de T-stub obtidos; (iii) os testes de escala e monotonicidade são propriedades das equações, não validação; (iv) a mesma entrada (`leff`, `m`) é a publicada, não calculada pelo núcleo.
Proposta para fechar a lacuna: MEF sólido elástico de um T-stub (o projeto tem fluxo com o Mecway, ver `docs/validacao_mecway/`) comparando a rigidez de flexão da mesa com `0,9 leff t³/m³` para 3 espessuras, e buscar nos artigos de Piluso et al. (2001) e de Bursi & Jaspart os valores medidos/FE de rigidez inicial.

## 4. Lei força-deslocamento (Fase C) — alternativas e estado
**[LACUNA]: não implementada.** A EN não define leis força-deslocamento de **componentes**; a EN 6.3.1 (ψ = 2,7) é lei **da ligação** (já usada na ferramenta de momento-rotação). As alternativas abaixo só podem ser implementadas depois de obtida a fonte:
| Alternativa | O que representa | Fonte necessária | Situação |
|---|---|---|---|
| Bilinear: K elástica até F_y, patamar até F_u | trecho elástico, plastificação | definir F_y (EN usa 2/3 da resistência na curva da ligação, nível junta, não componente) | não adotar só por simplicidade; **[HIPÓTESE NÃO VALIDADA]** |
| Multilinear por modo (Jaspart; Weynand, método dos componentes) | elástico, pós-escoamento, endurecimento | texto original (Jaspart 1991, SERICON) | **[LACUNA]** |
| Analítica por modo (Piluso–Faella–Rizzano 2001) | deformação plástica disponível dos modos 1, 2 e 3, ruptura de parafusos | artigo (Parte I e II) | **[LACUNA]**; só o resumo foi lido |
| MEF independente | referência de validação | Mecway/modelo sólido | proposta |
Modos de falha: qualitativamente (EN 6.2.4 e literatura) o modo 1 é dúctil (plastificação completa da mesa), o modo 3 é frágil (ruptura de parafuso) e o modo 2 é intermediário. **A deformação disponível de cada modo não pode ser quantificada sem as fontes acima**; nenhuma expressão própria será criada. Condição de entrada para a Fase C: obter e ler as equações originais, registrar equação/hipótese/limite de cada uma e definir benchmarks de curva (ensaios publicados de Piluso et al. e Beg et al.).

## 5. Integração entre componentes (Fase D) — estado
**[PLANEJADO]**: associação em série/paralelo, compatibilidade de deslocamentos, equilíbrio e `keq`/`zeq` (EN 6.3.3.1) já existem na ferramenta de momento-rotação e **não foram tocados nem duplicados**. Não se presume soma de resistências: a distribuição entre fileiras exige o procedimento de grupos (já no núcleo) combinado a rigidez e limites de ductilidade que ainda não existem.

## 6. Arquitetura
| Elemento | Decisão | Motivo |
|---|---|---|
| Geometria, Material, Bolts, EffectiveLength, Resistência | **inalterados**, estruturas simples com funções livres | funções puras, testadas, fora do escopo de mudança |
| Rigidez | funções livres em arquivo **separado** (`TStubStiffness.{h,cpp}`) com estruturas de dados e `EnStiffnessCoefficients` próprios | não toca `TStub.cpp`; coeficientes de rigidez e de resistência não se misturam; sem dependência da NBR nem de `Check` |
| Lei força-deslocamento (Fase C) | **estrutura de dados + função por modelo escolhido**, a definir após as fontes | evitar hierarquia de classes antes de saber se haverá mais de um modelo |
| Capacidade de deformação | idem, parte da lei | depende de quais fontes forem obtidas |
| Critério normativo | quem consome (Camada A/B, momento-rotação) escolhe coeficientes; o núcleo recebe `En…Coefficients` explícitos | separação normativa preservada |

## 7. Plano e benchmarks
| Fase | Entrega | Benchmarks | Estado |
|---|---|---|---|
| A | este documento; dw; auditoria | — | concluída |
| B | rigidez inicial (1 fileira) | SCIA, P398 C2 (publicados); escala; limites | **[IMPLEMENTADO]**, validação parcial (seção 3.3) |
| B+ | várias fileiras (leff de grupo, `keq`) | exemplo multi-fileira publicado + MEF | [PLANEJADO] |
| C | lei força-deslocamento e ductilidade | ensaios de T-stub publicados; MEF | [LACUNA] bibliografia |
| D | combinação de componentes | exemplos completos de junta publicados | [PLANEJADO] |
