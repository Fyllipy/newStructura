# P0 — F-SOL: soldas (pesquisa documental)

Somente documental: nenhum código, contrato, CMake, teste ou golden foi alterado. Estado do Git confirmado idêntico ao fechamento J0–J3 (`git status` sem diferenças). Sem validação profissional.

## 0. Rastreabilidade das fontes (o que foi realmente lido)
| Código | Fonte | Acesso | Nível de evidência |
|---|---|---|---|
| **[NG04]** | Ng, Deng, Grondin, Driver, "Behavior of Transverse Fillet Welds: Experimental Program", AISC *Engineering Journal*, 2º trim. 2004, pp. 39–… | **PDF lido (pp. 39–42)** | primária para a **revisão de literatura** nela contida; as demais fontes abaixo aparecem **apenas como citadas por [NG04]** |
| [MK89] | Miazga & Kennedy (1989), Can. J. Civ. Eng. | **não acessado**; descrito em [NG04] p. 41 | secundária |
| [LK90] | Lesik & Kennedy (1990), Can. J. Civ. Eng. | **não acessado** (a busca devolveu só o relatório U. Alberta SER 159, 1988, título diferente); descrito em [NG04] | secundária |
| [BK71] | Butler & Kulak (1971) | não acessado; descrito em [NG04] p. 40 | secundária |
| [BPK72] | Butler, Pal & Kulak (1972), ASCE J. Struct. Div. 98 (ST5) | não acessado; citado em resultado de busca (AISC EJ) | secundária |
| [NEIS85] | Neis, V.V., "New Constitutive Law for Equal Leg Fillet Welds", ASCE J. Struct. Eng. 111(8), 1985, p. 1747, DOI 10.1061/(ASCE)0733-9445(1985)111:8(1747) | **só resumo** (página do editor); texto completo **não acessado** | resumo verificado |
| [EN05] | EN 1993-1-8:2005 §4.5.3 | **texto da norma não acessado**; os resultados de busca eram páginas de softwares/cursos | **secundária; qualquer equação abaixo marcada "EN (sec.)" não foi conferida na norma** |
| [NBR24] | NBR 8800:2024 §6.2 | **texto não acessado nesta fase**; usa-se o que o código do Structura declara (`SupportWeld.cpp`), que **não foi reverificado** contra a norma | do código, não da fonte |
| [CBFEM] | IDEA StatiCa (documentação) | página de busca | secundária/comercial |
| [GC04] | Girão Coelho et al., T-stubs soldados (PDF local `docs/references/Coelho2003_WeldedTStubs.pdf`, lido na Fase C6) | **não relido nesta fase** | primária (já lida), a reexaminar em P1 |
Regra aplicada: nada que não esteja em um PDF/página lida é apresentado como equação verificada; os itens de memória do modelo estão marcados "não verificado".

## A. Resistência normativa
| Item | O que a fonte diz | Fonte e locator | Domínio/hipóteses | Dados (Structura) | Estado |
|---|---|---|---|---|---|
| **A1 Filete: dependência do ângulo de carga** | `V_θ/V_0 = 1,0 + 0,50 sen^1,5 θ`, θ = ângulo entre o eixo da solda e a linha de ação da carga; V_0 = resistência de filete longitudinal de mesmo tamanho; equação simplificada de [LK90] que difere ≤ 1,5 % do modelo de [MK89]; **adotada pela CSA S16-01 e pelo AISC LRFD (Apêndice J2.4)**; prevê +50 % de transversal sobre longitudinal | **[NG04] p. 39, eq. (1)** | análise do cisalhamento máximo no plano de ruptura; ensaios de [MK89] só com SMAW/E7014; [NG04] relata que a tenacidade/processo (FCAW) e outras variáveis **não estavam sistematicamente avaliadas** | `SupportWeld.cpp` usa `0,60 te L fw (1,0+0,5 sen^1,5θ)/γw2` com θ = 90° (fator 1,5) e `GussetInput.weldAngleDeg` | **Documentado** (equação) / reprodução já no código; **não** é validação física |
| A2 Faixa experimental da razão transversal/longitudinal | 1,39–2,00 (Pham), 1,57 (Higgins & Preece), 1,59 (Ligtenberg), 1,28–1,60 (Miazga & Kennedy), "30 a 70 %" em geral | [NG04] pp. 40–41 (revisão) | processos e eletrodos diversos | — | **Documentado** (como dispersão publicada) |
| A3 Efeito de tamanho, abertura de raiz, processo | soldas longitudinais maiores perdem resistência unitária (até 25 % em Bowman & Quinn, sem folga); transversais pouco | [NG04] p. 41 | — | **não armazenados:** processo (SMAW/FCAW/SAW), tenacidade do eletrodo, abertura de raiz, número de passes | **Parcialmente documentado**; dados ausentes |
| **A4 Método EN direcional e simplificado** | direcional (§4.5.3.2): tensões na garganta `σ⊥, τ⊥, τ∥`, `√(σ⊥²+3(τ⊥²+τ∥²)) ≤ fu/(βw γM2)` e `σ⊥ ≤ 0,9 fu/γM2`; simplificado (§4.5.3.3): `fvw,d = fu/(√3 βw γM2)`; βw por classe do aço (S235 0,8; S275 0,85; S355 0,9; S420/S460 1,0) | **EN (sec.)** — conferir na norma | a revisão EN 1993-1-8:2024 (§6.5.3.2 na nova numeração) **altera** a tabela de βw e acrescenta fator para metal de adição de alta resistência (informação de página secundária) | `GussetInput.weldFw` e `SupportWeldInput.fw` existem; **βw e a classe do metal-base não** | **Parcialmente documentado** (fonte secundária; edição a fixar) |
| A5 Diferença NBR 8800 × EN 1993-1-8 | NBR (conforme o código): resistência `0,60 te L fw (1+0,5 sen^1,5θ)/γw2`, metal-base `0,60 t fy/γa1` e `0,60 t fu/γa2`; EN: método direcional/simplificado com βw | código do Structura + EN (sec.) | **comparação textual não feita**: faltam os textos de ambas as normas | — | **Não documentado** (comparação) |
| **A6 Grupos de soldas, excentricidade no plano** | o método do centro instantâneo de rotação (ICR) usa curvas carga–deformação do filete por ângulo [BK71]/[BPK72]; [LK90] estendeu [MK89] a cargas **excêntricas no plano por ICR** | **[NG04] p. 41** (descrição de [LK90]); [BPK72] via busca | cada elemento de solda com sua curva; critério de ruptura = elemento de menor Δu/r (AISC, fonte secundária) | `SupportWeld`: grupos **retilíneos simples** (mesas e alma); **sem geometria de grupo genérica** | **Parcialmente documentado** (descrição; equações de ICR não lidas) |
| A7 Excentricidade **fora do plano**/alavanca | não localizado | — | — | — | **Não documentado** |
| **A8 Penetração total/parcial (entalhe)** | `SupportWeld.cpp` trata penetração total como resistência do metal-base (comentário "6.2.4"); penetração parcial: **sem fonte nesta fase** | **do código**, não relido na norma | — | `flangeFullPenetration` (sim/não); espessura efetiva da penetração parcial **não armazenada** | **Não documentado** (fonte) |
| A9 Modos de ruptura | garganta (cisalhamento), metal-base (cisalhamento na face de fusão), perna mínima/máxima (Tabela 11, 6.2.5.2) | código (`SupportWeld.cpp`); fonte primária não relida | — | `minimumFilletLeg`, `maximumFilletLeg` | **Parcialmente documentado** |
| A10 Solicitações combinadas | método direcional (A4) e, no ICR, equilíbrio de forças/momentos | EN (sec.); AISC (sec.) | — | só combinação por componente (mesa: M/(d−tf)+N/2; alma: V) | **Parcialmente documentado** |

## B. Comportamento mecânico
| Item | O que as fontes dizem | Fonte | Estado |
|---|---|---|---|
| **B1 Lei força–deformação do filete (por ângulo)** | existem curvas empíricas carga × deformação de filetes a 0°, 30°, 60°, 90° ([BK71]: 23 ensaios de 6,4 mm; [Clark 1971] curvas "geralmente semelhantes"), usadas como base do ICR; o filete longitudinal tem **muito mais deformação na ruptura** que o transversal ([NG04] p. 40, Clark) | [NG04] p. 40 (descrição) | **Parcialmente documentado**: existência e tendência; **a equação de [BK71] não foi obtida** |
| **B2 Lei constitutiva invariante** | [NEIS85]: nova relação resistência × deslocamento para filetes de pernas iguais, deduzida das tensões médias na solda sob carga plana geral; teoria da plasticidade com endurecimento isotrópico dá resistência última e deslocamento máximo; escrita em forma invariante e adimensional, com resposta anisotrópica; **houve discussão (ASCE 113(9), 1987) e réplica** | resumo no editor | **Parcialmente documentado** (só resumo; equações e hipóteses **não lidas**; discussão indica controvérsia a examinar) |
| B3 Deformação última | segundo resultado de busca sobre o relatório de confiabilidade do AISC, o AWS D1.1 adota as equações de [LK90] com a função do ângulo simplificada e um **limite superior de 0,17 w** para a deformação última; o AISC usa ICR com Δm (deformação na tensão máxima) e Δu (ruptura) | busca (secundária); equações não lidas | **Parcialmente documentado** (secundário) |
| B4 Ductilidade vs. ângulo | [MK89]: "a ductilidade do filete em si é independente do ângulo" (comparação de **deformações específicas**), enquanto [NG04] resume que transversais definem o **limite inferior** de ductilidade da **ligação**; o aparente conflito é entre **deformação do material** e **deslocamento da ligação**: não resolvido sem os textos originais | [NG04] pp. 39, 41 | **Parcialmente documentado** |
| B5 Rigidez inicial axial/transversal/rotacional da solda | **nenhuma formulação de rigidez inicial localizada** nas fontes lidas; a rigidez de um grupo sai das curvas B1/B2 (não é coeficiente tabelado); [CBFEM] usa diagrama elastoplástico com redistribuição de tensões ao longo do comprimento (comercial, secundário) | — | **Não documentado** (rigidez inicial com formulação fundamentada) |
| B6 Momento–rotação de ligação soldada | sem lei publicada obtida; só por integração de B1/B2 no grupo (ICR) ou por MEF | — | **Não documentado** |
| B7 Modelagem por componentes vs. elemento distribuído | ICR = elementos distribuídos ao longo do cordão, cada um com curva própria (descrição); o método dos componentes da EN **não** foi verificado quanto ao tratamento de solda | [NG04]/busca | **Parcialmente documentado** |
| B8 Soldas em T-stub soldado | [GC04] (já lido na C6) trata T-stubs soldados; **não reexaminado** | PDF local | a reexaminar em P1 |

## C. Representação arquitetural (recomendação, sem implementação)
1. **Papel sempre presente:** `IResistanceMechanism` (verificação normativa). Fundamentação: A1, A4 e código existente.
2. **Papel de interface:** a solda aparece na **topologia** como `Link{Welded}` (já existe em J1). Não implica rigidez.
3. **Papel de componente deformável (`IDeformableComponent`):** **só** quando houver lei fundamentada. Candidatas de P1, **condicionadas à leitura dos textos completos**: B1/B2 (curvas de [BK71]/[LK90]/[NEIS85], caso plano) integradas por ICR. Hoje **nenhuma** tem fonte lida suficiente ⇒ **não propor lei**.
4. **Rigidez infinita não é presumida:** o modelo EN atual (`JointComponents`) assume solda de mesa rígida; isso deve continuar **rotulado como hipótese** (já está em `hypotheses`), e a faixa F-SOL deve registrar quando ela for relaxada. Soldas sem lei ficam `NotYetAvailable` para F–δ.
5. **Combinação:** resistência (1) + interface (2) em qualquer nível; deformável (3) apenas em níveis com fonte e domínio.

## D. Classificação do resultado (critérios do prompt)
| Modelo | 1 Resistência reproduzível | 2 Rigidez inicial | 3 Lei F–δ | 4 Lei M–θ | 5 MEF | 6 Ensaio | 7 Dados ausentes no Structura |
|---|---|---|---|---|---|---|---|
| Filete, resistência por ângulo (A1) | **Documentado** | Não documentado | Parcialmente (B1/B2, sem equações) | Não documentado | recomendável p/ conferir ICR | já existe base (ver A2) | processo, tenacidade, folga, passes |
| Filete, método EN (A4) | Parcialmente (fonte sec.) | Não documentado | Não documentado | Não documentado | — | — | βw, classe do metal-base |
| Grupos/excentricidade no plano (ICR, A6/B1) | Parcialmente | Não documentado | Parcialmente | Parcialmente (por integração; sem equações lidas) | **necessário** p/ validar | **necessário** (ensaios de grupos) | geometria genérica do grupo; curvas |
| Excentricidade fora do plano (A7) | Não documentado | Não documentado | Não documentado | Não documentado | **necessário** | necessário | — |
| Penetração total/parcial (A8) | Parcialmente (código; norma não relida) | Não documentado | Não documentado | Não documentado | recomendável | necessário (parcial) | espessura efetiva (parcial) |
| Lei invariante de Neis (B2) | Não aplicável | Não documentado | Parcialmente (só resumo) | Não documentado | recomendável | já publicada (base do autor) | — |

## E. Reaproveitamento de verificações existentes
`SupportWeld.cpp` implementa A1/A9 para **dois** arranjos (mesas e alma). É **verificação de resistência** (nível 1), não modelo mecânico; reaproveitável como `IResistanceMechanism` por wrapper (como em J2). Lacunas conhecidas: grupo genérico, penetração parcial, fora do plano, ICR.

## F. Pendências para P1 (leitura obrigatória, sem substituir por citação secundária)
1. Texto completo de [NEIS85] e réplica/discussão (ASCE 113(9), 1987).
2. [LK90] e [BPK72] / [BK71] (equações de carga–deformação).
3. **EN 1993-1-8** (edição a fixar: 2005 e/ou 2024) §4.5 e **NBR 8800:2024** §6.2 (textos).
4. [GC04] reexame.
