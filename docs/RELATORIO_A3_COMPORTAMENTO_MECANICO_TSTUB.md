# Relatório A3 — comportamento mecânico do núcleo T-stub (Fases A e B)

Este relatório **não declara validação profissional**. Estado: Fase A concluída; Fase B (rigidez inicial, uma fileira) implementada com validação **parcial**; Fases C (lei força-deslocamento) e D (integração entre componentes) **não implementadas**, por lacuna bibliográfica (item 10). Metodologia completa: `docs/METODOLOGIA_COMPORTAMENTO_MECANICO_TSTUB.md`.

## 1. Auditoria do código original
Situação inicial: `ctest` 34/34 ao fim do saneamento pós-A2, com reconstrução completa. Inspecionados: `TStub.{h,cpp}`, `ColumnFlangeLever.{h,cpp}`, `Connections.{h,cpp}`, `JointComponentService.cpp`, `TestTStub.cpp` e `JointComponents.cpp` (só leitura).
- **Calculado:** geometria (m, e, emin, ew), material (fy), parafusos (Ft,Rd), comprimentos efetivos (Tab. 6.4 e 6.6), modos 1/2/3 (Tab. 6.2, método 1 ou 2, alavanca desenvolvida), fileiras e grupos (procedimento do SCI P398).
- **Convenções:** N, mm, MPa; tração positiva; coeficientes EN `γM0 = 1,0; γM2 = 1,25; k2 = 0,9` em `EnCoefficients`; a NBR não entra em `TStub`.
- **Ausente para o comportamento:** rigidez, `Lb`, `E`, lei força-deslocamento, deformação por modo.
- **Achados (não corrigidos):** (a) a rigidez só existe em `JointComponents.cpp`; (b) esse arquivo usa `leff` de rigidez da fileira **isolada**, enquanto a EN (Tab. 6.11) diz "o menor, individual ou de grupo"; (c) esse arquivo assume silenciosamente cabeça 0,7·db e porca 0,8·db; o código novo não assume nada.

## 2. Divergência de `dw` e resolução
Implementado: `dw = s·1,1547005`, `s` = largura entre faces (tabela ISO 4014: M12 18 … M36 55 mm; fora da tabela estimada `1,5·db`). Logo, a grandeza é a **largura entre vértices** e o fallback é `dw = 1,5·db·1,1547 = √3·db ≈ 1,732·db`. Código e documento arquitetural estavam coerentes; o relatório de saneamento descrevia só `1,5·db` (a estimativa de **faces**): **errata registrada** nele e no relatório de auditoria. Verificado: o fator `2/√3` é a geometria do hexágono regular; os `s` tabelados são da série ISO, **não** EN 14399-4 (HV: M20 32, M24 41, M30 50 mm segundo catálogos); a EN define `dw` como diâmetro da arruela **ou** largura entre vértices — o código nunca usa arruela. **Nenhum resultado numérico mudou**; só o modo 1 depende de `dw` (M20/HEB200: 649 → 666 → 720 kN para `dw` 34,6 → 37 → 44 mm, mas o modo 2 de 277 kN governa e não depende de `ew`). O valor estimado continua rotulado "VALOR ESTIMADO (hipótese empírica…)" na memória da Camada B. Alteração em produção: apenas comentário em `TStub.h`.

## 3. Referências técnicas utilizadas
EN 1993-1-8:2005 (texto local, equações das fórmulas ilegíveis no OCR; confirmadas pelos números publicados; definição de `dw` conferida por busca); SCI P398 (declara não cobrir rigidez); exemplo SCIA (valores via `AUDITORIA_LIGACOES.md`); exemplo C2 do P398/New Steel Construction (k10 = 8,01 mm); catálogos EN 14399-4 (largura entre faces). **Localizadas e não lidas (lacuna):** Piluso–Faella–Rizzano 2001 (partes I e II), EN 6.4.2(2) literal, Jaspart, Weynand, Beg et al.

## 4. Metodologia
Rigidez inicial por componente, equações da EN 6.3.2/Tab. 6.11, uma fileira, série mesa/chapa + parafusos; rigidez em força = k·E. Lei força-deslocamento: alternativas avaliadas (bilinear, multilinear por modo, analítica por modo, MEF), **nenhuma implementada** por falta das fontes; nenhuma expressão própria criada.

## 5. Equações implementadas (`src/design/TStubStiffness.{h,cpp}`)
`k4 = k5 = 0,9·leff·t³/m³`; `k10 = 1,6·As/Lb` com `Lb = grip + arruelas + (cabeça + porca)/2`; `k_T = 1/(1/k4 + 1/k10)`; `K = k·E`. Todas **específicas da EN**, em `EnStiffnessCoefficients`, separadas de `EnCoefficients`; sem equivalente NBR.

## 6. Arquitetura desenvolvida
Funções livres e estruturas simples em arquivo separado, sem tocar `TStub.cpp`; sem dependência de NBR, `Check` ou `ColumnFlangeLever` (sem ciclos); entradas obrigatórias (NaN = não informado → `valid = false` com causa); memória de cálculo em `lines`. O módulo **não é chamado** pelo dimensionamento nem pela ferramenta momento-rotação. Classes/hierarquias foram descartadas até que se saiba quantos modelos constitutivos existirão.

## 7. Testes criados
`TestTStubStiffness` (7 testes): benchmark publicado; série com ki publicados; T-stub = série; leis de escala (t³, leff, m⁻³, As, Lb⁻¹); monotonicidade e limite (mesa rígida → k10); série com componente infinito e inválido; dados ausentes/inválidos para cada campo (arruela não informada não vira 0); coeficientes nomeados pela tabela. `TestTStub::boltHeadWidthDefinitionAndFallback` (dw). Conferência independente em Python: `docs/validacao_alavanca/validate_stiffness.py` / `saida_rigidez.txt`.

## 8. Resultados dos benchmarks
Publicado × calculado: k4 8,59/8,39 → 8,593/8,389 (0,03 %/0,01 %); k5 7,55/7,57 → 7,547/7,565 (0,04 %/0,07 %); k10 6,47 → 6,474 (0,06 %); Lb 38,8 → 38,8; keff 1,73/1,71 → 1,730/1,710; k10 do P398 C2 8,01 → 8,011 (0,02 %). Tolerâncias justificadas pelo arredondamento dos valores publicados (0,2 % a 1 %). **Todas dentro da tolerância.** Natureza da validação: confere aritmética e transcrição de equações publicadas; **não** validação física (sem MEF, sem ensaio).

## 9. Regressões executadas
Reconstrução completa com remoção do `StructuraCore.lib` antes dos builds relevantes (data do `.lib` conferida posterior às fontes). `ctest -C Release`: **35/35** (34 anteriores + `TestTStubStiffness`). Inalterados e aprovados: `TestTStub` (SCI P398, 26 amostras IDEA, Camada A, estados), `TestMomentRotationRegression` (14 casos, texto idêntico), `TestGoldenNumerics` (sem regravar), `TestJointClassification`, `TestPostA2Sanitation`, `TestDocxEncoding`. Resistência última, curvas, Camada A, catálogo padrão e NÃO VERIFICÁVEL de várias fileiras não foram tocados. `JointComponents.cpp` não foi modificado.

## 10. Limitações conhecidas
Rigidez só para uma fileira; `leff` fornecido pelo chamador; sem pré-carga, folgas ou não linearidade; convenção "alavanca desenvolvida" para `ki` é hipótese de coerência **[HIPÓTESE NÃO VALIDADA]**; `dw` por vértices da série ISO (não arruela, não HV) **[HIPÓTESE NÃO VALIDADA]**; fallback de `s` não conferido com catálogo; a fonte original dos exemplos SCIA e C2 não foi relida; a lei força-deslocamento e a capacidade de deformação por modo (dúctil × frágil) **não podem ser quantificadas sem as fontes**.

## 11. Itens pendentes
1. Obter e ler Piluso–Faella–Rizzano (2001, I e II), EN 6.4.2(2), Jaspart (1991), Weynand (1996), Beg et al. (2004); registrar equações/hipóteses/limites.
2. MEF elástico independente de T-stub (Mecway) para validar k4/k5 fisicamente; buscar rigidez medida/FE publicada.
3. Rigidez de várias fileiras (leff de grupo, `keq`).
4. Decidir, com autorização metodológica, o tratamento da divergência de `leff` de rigidez em `JointComponents.cpp`.
5. Conferir `s` de parafusos com catálogo (série ISO × EN 14399-4) e decidir o uso de arruela em `dw`.

## 12. Próxima etapa recomendada
Fechar a lacuna bibliográfica (pendente 1) e o MEF elástico (pendente 2) **antes** da Fase C; só então implementar a lei força-deslocamento por modo, com ensaios publicados como benchmark. Não migrar o T-stub de `JointComponents.cpp` até haver lei validada e comparação quantitativa das curvas.
