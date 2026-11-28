# P0 — F-GUS: gussets, nós de treliça e ligações tubulares (pesquisa documental)

Somente documental. Nenhum código, contrato, CMake, teste ou golden alterado. Sem validação profissional.

## 0. Rastreabilidade das fontes
| Código | Fonte | Acesso | Evidência |
|---|---|---|---|
| **[CB07]** | Chambers & Bartley, "Geometric Formulas for Gusset Plate Design", AISC *Engineering Journal*, 3º trim. 2007, pp. 255–… | **PDF lido (pp. 255–258)** | primária para as **fórmulas geométricas e para a revisão** nela contida |
| [WHIT52] | Whitmore (1952), "Experimental Investigation of Stresses in Gusset Plates", Univ. of Tennessee Engineering Experiment Station, Bulletin No. 16 (titulo corrigido na P1; o titulo anterior deste documento estava errado) | **não acessado**; citado por artigos do AISC (busca) | secundária |
| [THO84] | Thornton (1984) | **não acessado**; descrito em [CB07] p. 255 | secundária |
| [WR86] | Williams & Richard (1986) | não acessado; usado em [CB07] eq. (4) | secundária |
| [CG99] | Cheng & Grondin (1999), ângulo de 45° | só trecho de busca | secundária |
| [DG29] | AISC Design Guide 29, *Vertical Bracing Connections* (Appendix C: método da coluna equivalente, Muir & Thornton 2014) | só menção em busca | secundária; **não lido** |
| [CIDECT1] | CIDECT Design Guide 1, 2ª ed. (2008), perfis circulares | **não acessado** (só cópias em sites de terceiros nos resultados) | secundária |
| [EN05-7] | EN 1993-1-8:2005 §7 (ligações de perfis tubulares) | **texto não acessado** | — |
| [TUNI] | Garifullin et al. e afins (Tampere, repositório), rigidez de nós RHS | só trechos de busca | secundária |
| [NBR24] | NBR 8800:2024 | texto não acessado; usa-se o que o código declara | do código |
Itens "do código" descrevem o que `TrussConnections.cpp`/`TubularJoints.h` implementam; **não foram reverificados** contra as normas.

## A. Gusset plates
| Item | O que a fonte diz | Fonte e locator | Domínio/hipóteses | Dados Structura | Estado |
|---|---|---|---|---|---|
| **A1 Largura de Whitmore** | `L_w = 2 L tan 30° + s` (L = comprimento do grupo de parafusos ao longo da força; s = distância entre linhas extremas; ângulo de 30°) | **[CB07] Tabela 1 e Fig. 1** | 30° a partir da primeira linha de parafusos; a "área de Whitmore" = `L_w × t_p`; **é um método simplificado de distribuição de tensões** | código: `lw = (linhas−1) g + 2 L tan 30°`, **limitado** à largura da chapa (`gussetWidth`) | **Documentado** (equação); validade física dos 30° **parcial** (ver A2) |
| A2 Hipóteses e conservadorismo do ângulo | 30° é conservador; redistribuição após plastificação alarga a seção (45° proposto por [CG99], só trecho de busca); [CB07] cita a base como trajetórias elásticas observadas (busca de 2019) | [CB07] p. 255; busca | — | — | **Parcialmente documentado** |
| **A3 Whitmore além das bordas da chapa** | a parte que sai da borda livre é **deduzida**; se entra na viga/pilar, usa-se largura ponderada por `t F_y` relativos (Thornton & Kane, via [CB07]); o cisalhamento de bloco pode governar antes do escoamento de Whitmore (eq. (4): `t[2(L+L_e)0,6F_y + sF_y] ≈ t[2L tan30 F_y + sF_y] = t L_w F_y`, [WR86]) | **[CB07] p. 256, eq. (4)** | geometria com bordas simétricas (as fórmulas do artigo valem para **bordas simétricas e perpendiculares à linha de centro do grupo**) | **só `gussetWidth` e `freeLength`**; o **contorno** da chapa não é armazenado | **Documentado** (descrição e eq. 4) |
| **A4 Flambagem: coluna de Whitmore (pseudo-coluna/Thornton)** | usa fórmulas globais de coluna, área de Whitmore e comprimentos de pseudo-coluna `L1, L2, L3` e fatores `K` "de **0,5 a 1,2**"; a precisão **é variável** frente a ensaios, **mas consistentemente conservadora**, razão da adoção pela indústria | **[CB07] p. 255** (texto e Figs. 2–3, 7) | — | código usa `K = 0,65` com comprimento `freeLength` (comentário do código); **escolha dentro de uma faixa de incerteza declarada pela fonte** | **Parcialmente documentado**: método e sua variabilidade; **a escolha de K e do comprimento não é fundamentada por fonte lida** |
| A5 Geometria para obter `y_c, y_b, L1–L3` | fórmulas de [CB07] (Tabela 1, eqs. 5–16) para barras de contraventamento ligadas a vigas e pilares ortogonais e não ortogonais | [CB07] pp. 256–258 | gusset com cantos simétricos | polígono da chapa **ausente** | **Documentado** (geometria); **inaplicável hoje** (falta o contorno) |
| A6 Ruptura da seção líquida / block shear | block shear na chapa/diagonal: verificação NBR via `blockShearRd` (`ConnectionHelpers.h`); fonte primária **não relida**; [CB07] só cita a relação com Whitmore | código | — | existe | **Parcialmente documentado** |
| A7 Flambagem **fora do plano** | não localizada em fonte lida | — | — | — | **Não documentado** |
| A8 Excentricidade, segunda ordem, distribuição entre elementos | [DG29] (Apêndice C, coluna equivalente; método da força uniforme conhecido pelo título do guia, **não lido**) | busca | — | `barEccentricity` existe (excentricidade da diagonal) | **Não documentado** (equações e domínio não lidos) |

## B. Nós de treliça
Nenhuma das fontes lidas trata distribuição de esforços entre múltiplas barras, excentricidades do nó ou a diferença análise global × comportamento local.
| Item | Estado | Observação |
|---|---|---|
| B1 Distribuição entre barras (equilíbrio do nó) | **Não documentado** (fonte); equilíbrio de nó já existe no app (`NodeEquilibrium`) como cálculo estático, não como modelo de ligação | o método de [DG29] é a pista, **não lida** |
| B2 Geometria e excentricidade do nó | **Não documentado** | `barEccentricity` cobre só a excentricidade da diagonal |
| B3 Ligações parafusadas/soldadas e transferência barra→chapa | verificações NBR existentes (código); **sem modelo mecânico** | resistência (parafusos, solda de apoio pelo método da força uniforme "conservador" segundo o próprio código) |
| B4 Análise global × local | **Não documentado** | |

## C. Ligações tubulares
| Item | O que a fonte diz | Fonte | Estado |
|---|---|---|---|
| C1 Existência/escopo | CIDECT DG1, 2ª ed. 2008, substitui a 1ª ed. (1991); o código declara basear-se em suas Tabelas 4.1, 4.3, 6.1 e 7.1 | busca; código | **Parcialmente documentado** (equações **não lidas**) |
| C2 Modos de falha | plastificação da face do banzo, falha da parede lateral, combinação, **punção** (punching shear) | trabalhos acadêmicos citados em busca | **Parcialmente documentado** (lista; fórmulas e faixas **não lidas**) |
| C3 Critério de deformação | limite de **3 % do diâmetro do banzo** (CHS, "CIDECT DG1" segundo trabalho acadêmico) e **3 % b₀** (RHS, Lu et al./Zhao, válido para β ≤ 0,85); critério de 1 % d de chapa-CHS para serviço (outro critério) | busca (secundária) | **Parcialmente documentado** — critério de **resistência por deformação**, não lei F–δ |
| C4 Base normativa | o EN 1993-1-8 §7 adotou recomendações do IIW 2ª ed. (1989), enquanto o CIDECT DG1 2008 segue de perto a IIW 3ª ed.; as equações **podem divergir** (trecho garbling na busca, **tratar como pista**) | busca | **Parcialmente documentado**; reconferir nas normas |
| C5 Limites geométricos (β, γ, τ, ângulos, gap/overlap) | **não obtidos** | — | **Não documentado** (precisam do texto) |
| **C6 Rigidez de nós** | para RHS T sob carga axial a rigidez inicial pela abordagem de componentes (flexão da face do banzo + parede lateral); fontes **discordam** sobre o viés (superestima em axial; subestima 0,30–0,45 em rotação) e a **força axial no banzo reduz a rigidez inicial em até 50 %** (estudo numérico de SHS) | repositório Tampere (busca) | **Parcialmente documentado**; **sem formulação fundamentada para CHS** |
| C7 Flambagem local e outros | não localizado | — | **Não documentado** |

## D. Reaproveitamento das verificações existentes
- `TrussConnections.cpp` (`checkGusset`, `whitmoreWidth`, `gussetLength`, `blockShearRd`) e `TubularJoints.h`: **verificações de resistência por modos**; reaproveitáveis como `IResistanceMechanism` por wrappers (padrão J2). **Não** são modelo mecânico: não dão rigidez, distribuição entre barras ou resposta vetorial.
- A **largura de Whitmore** é método de distribuição simplificado com hipóteses de [CB07]; o código a limita à largura da chapa, o que é compatível com A3 apenas na parte "deduzir o excedente de borda livre"; não implementa a largura ponderada de Thornton & Kane quando a seção entra no apoio.
- Pontos de atenção documentados: `K = 0,65` e comprimento da coluna (A4); solda ao apoio "metodo da força uniforme conservador, sem decomposição" (comentário do código).

## E. Classificação do resultado
| Modelo | 1 Resistência reproduzível | 2 Rigidez inicial | 3 Lei F–δ | 4 Lei M–θ | 5 MEF | 6 Ensaio | 7 Dados ausentes no Structura |
|---|---|---|---|---|---|---|---|
| Whitmore (tração/escoamento) | **Documentado** (eq. de [CB07]) | Não documentado | Não documentado | Não aplicável | recomendável | já há ensaios históricos (não lidos) | contorno da chapa; ângulo das barras |
| Flambagem no plano (pseudo-coluna) | Parcialmente (K variável 0,5–1,2) | Não aplicável | Não documentado | Não aplicável | **necessário** p/ calibrar K | necessário | contorno, K justificado |
| Flambagem fora do plano | Não documentado | — | — | — | **necessário** | necessário | espessura/vinculação lateral |
| Block shear (chapa) | Parcialmente (código; NBR não relida) | Não documentado | Não documentado | Não aplicável | — | — | furos/bordas (existem) |
| Distribuição/excentricidade/2ª ordem | Não documentado | Não documentado | Não documentado | Não documentado | **necessário** | necessário | topologia N-ária; excentricidades |
| Nó tubular CHS (resistência por modos) | Parcialmente (CIDECT2008/EN §7 não lidos) | Parcialmente (só RHS, discordante) | Não documentado | Não documentado | recomendável | já há base (não lida) | limites β,γ,τ no cadastro |
| Nó de treliça (modelo mecânico do nó) | Não documentado | Não documentado | Não documentado | Não documentado | **necessário** | necessário | ver B |

## F. Pendências para P1
1. Obter e ler **[DG29]**, **[CIDECT1]**, **EN 1993-1-8 §7**, **NBR 8800:2024 6.5 e 6.2** (textos) e [WHIT52]/[THO84] originais.
2. Verificar `K = 0,65` e os comprimentos de pseudo-coluna do código contra [CB07]/[THO84]/[DG29].
3. Definir (com o usuário) se o **contorno da chapa** passa a ser dado do domínio (hoje só `gussetWidth`, `freeLength`).
