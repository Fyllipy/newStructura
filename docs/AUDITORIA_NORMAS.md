# Auditoria do código contra as normas (NBR 8800:2024 e NBR 14762:2010)

Conferência feita lendo o texto das normas em `normas/` (páginas renderizadas, pois a extração de texto dos PDFs corrompe as fórmulas) e
comparando com o código de `src/design/`. Estado: **todas as divergências encontradas foram corrigidas**; o que ainda **não está
implementado** está listado na seção 3.

## 1. Conforme (conferido, sem alteração)

**NBR 8800:2024** — χ e λ0 (5.3.3); Tabelas 4 e 5 e largura efetiva (5.3.4); `Ne` de seções simétricas, monossimétricas e assimétricas e cantoneira
simples (5.3.5.1 a 5.3.5.4); limites de esbeltez 300/200 (5.2.8, 5.3.7); Cb e Rm (5.4.2.3); cortante `kv`, λp, λr (5.4.3); interação N+M (5.5.1.2);
torção em tubulares (5.5.2); 5.7.2, 5.7.3 e 5.7.4; Anexo C (B1, B2, Cm, Rs); Anexo D — FLT, FLM e FLA de I/H/U (D.2.1, D.2.2); Anexo E (alma esbelta);
Tabela 14 (furos-padrão), Tabelas 9, 10, 11 (soldas); 6.3.3.1 a 6.3.3.4 (parafusos); 6.2.5.2-a (filete com ângulo); 6.5 (metal-base); 6.5.6 (rasgamento).

**NBR 14762:2010** — χ (Tabela 8); χdist em compressão (9.7.3) e Tabela 11; χdist em flexão (9.8.2.3) e Tabela 14; χFLT (9.8.2.2) e Cb; cortante
(9.8.3, kv = 5); interação M+V (9.8.4); flexão composta linear (9.9); `Me` das seções (9.8.2.2 e Anexo E); largura efetiva (9.2).

## 2. Divergências encontradas e corrigidas

| Item | Antes | Agora |
|---|---|---|
| T com alma comprimida, FLT (D.2.3) | `mín(Mcr ; fy W)` | `(1,92 − 1,17√r) fy W ≤ 1,5 fy W` (r ≤ 1) ou `(0,92 − 0,17/r) Mcr` (r > 1) |
| U, eixo menor, alma comprimida (D.2.8-d) | `Mr = fy W`, sem ramo `λ > λr` | largura efetiva da alma, `Mr = fy Wef`, `Mcr = Wef² fy/W` |
| Cantoneira laminada em flexão | sem verificação | critério adaptado da AISC 360-22 F10 (a NBR 8800:2024 não a trata), sinalizado no memorial |
| Distância mínima furo–borda (Tabela 16) | tabela arredondada para cima (ex.: 1/2″ → 22; 36 mm → 46) | valores exatos da Tabela 16 |
| Espaçamento entre furos (6.3.9, 6.3.10) | só `p ≥ 3 db` | acrescentados `p − dh ≥ db` e `p ≤ mín(24 t ; 300)` |
| Furos na mesa tracionada (5.4.2.6) | não verificado | `M_Rd ≤ fu (Afn/Afg) Wt/γa1` quando `fu Afn < Yt fy Afg` (usa a razão `An/Ag` informada) |
| Flechas de perfis formados a frio | inércia bruta | inércia efetiva `Ief` para a tensão de serviço (14762, 9.8.5-a) |

## 3. Implementado nesta rodada (antes ausente)

* **Ligação por atrito (NBR 8800, 6.3.4)** em chapa de cisalhamento, cantoneira dupla e chapa de nó (Tabela 19 de protensão).
* **Ligações de perfis formados a frio (NBR 14762, Seção 10)**: parafusos comuns, solda de filete em superfícies planas e parafusos
  autoatarraxantes, com o mínimo de 50 % da resistência da barra (10.1-a), colapso por rasgamento (10.4) e 10.5.4.
* **Esmagamento da alma (NBR 14762, Anexo D)**: U simples, U enrijecido, Z e cartola (aviso, apoio de 50 mm).

## 3b. Implementado na rodada seguinte (antes ausente)

| Item | Onde |
|---|---|
| Efeito alavanca, placa rígida e flexível (NBR 8800, 6.3.5.2 a 6.3.5.4) | `checkPrying`; aplicado à aba do apoio da cantoneira dupla tracionada |
| Pega longa (6.3.7) e ligação de grande comprimento (6.3.8) | `BoltedInput::grip`, `pretensioned`, `connectionLength` |
| `kv = 5 + 5/(a/h)²` com enrijecedores transversais (5.4.3.1.1) | parâmetro de barra "espaçamento dos enrijecedores de alma (a)" |
| Flambagem lateral da alma (5.7.5) no perfil de apoio | campo `supLb` da ligação (0 = mesas contidas) |
| Filetes longitudinais + transversais concêntricos (6.2.5.2-c) e centro instantâneo de rotação (6.2.5.2-b) | `WeldInput::lengthTransverse`, `weldGroupIcr` |
| Tabela B.1: viga que suporta pilares, `L/500` | detecção automática em `StructuralChecks::deflections` (pilar que nasce na viga) |
| Ligação por atrito na emenda de viga (6.3.4) | talas de mesa e de alma |
| NBR 14762, Anexo F (mesa tracionada ligada a painel) | parâmetro de barra; `R` da Tabela F.1 |
| NBR 14762, Anexo B (aumento de `fy` nas dobras) | parâmetro de barra; só seções totalmente efetivas |
| Barra de apoio escolhida pelo usuário, com face geométrica real (excentricidades e giro) | `ConnectionPlacement.h`, `chooseSupport` |
| Esforços de todas as ligações da mesma barra de apoio somados na verificação local do apoio | `Forces::hostTension/hostCompression` |
| Barra que se apoia termina na face do apoio (3D e pranchas), não no nó | `MainWindow::connectionPlacements`, `SheetBuilder::Bar::trimI/trimJ` |
| NBR 14762, 10.2.4 (filete em superfície curva) | meio de ligação "Filete em superfície curva" da ligação de formado a frio |
| NBR 14762, Anexo D: esmagamento da alma como verificação efetiva | parâmetro de barra "comprimento de apoio" (0 = só aviso) |
| NBR 8800, 5.4.3.1.3: requisitos dos enrijecedores transversais | linhas do memorial do cortante com enrijecedores (`j`, `I ≥ a tw³ j`, `b/t`) |
| NBR 8800, Anexo H (fadiga): categorias A a G, `σSR`, `σTH`, dispensas H.2.5/H.2.6 | parâmetros de barra "categoria do detalhe" e "ciclos N"; faixa de tensão pelas combinações selecionadas |
| NBR 8800, Anexo F (aberturas circulares sem reforço) | parâmetros de barra (diâmetro, posição, excentricidade, abertura adjacente); verifica as condições de F.3 |
| NBR 8800, 4.12 (contenções laterais) | memorial "Contenções laterais": `F_br` e `S_br` de pilares (4.12.2.2) e vigas (4.12.3.3) quando há travamentos |
| Ligação rígida viga–pilar por chapa de extremidade (nova tipologia) | `EndPlateConnection`: parafusos tracionados, efeito alavanca, cortante, soldas, mesa e alma do pilar (5.7.2 a 5.7.5), enrijecedores (5.7.9), painel de alma (5.7.7), chapa de reforço |

Observações: o efeito alavanca só é verificado onde há parafusos tracionados por aba flexível (cantoneira dupla); a verificação por centro
instantâneo de rotação e o grupo combinado existem como funções de projeto, ainda sem tipologia de ligação que as use (a chapa de cisalhamento
é verificada pelo método elástico, conservador). O Anexo B usa `ri = t` e `fyf = fy` (sem ensaios).

## 4. Ainda não implementado (diferenças conhecidas em relação às normas)

**NBR 8800:2024**
* Projeto/verificação do enrijecedor propriamente dito (5.7.9); 5.4.3.1.3 só emite os requisitos no memorial.
* 5.7.6 (instabilidade da alma por par de forças, vigas dos dois lados do pilar) e 5.7.8 (apoios sem restrição à rotação); 5.7.7.3 (enrijecedores diagonais).
* Anexo G (barras de seção variável), contenção de torção (4.12.3.6 em diante) e análise plástica (4.10.2.2-b). Anexo H: só categorias de tensões normais e
  de cisalhamento na garganta (F), sem os casos H.4-c (soldas de penetração parcial C', C'').
* Tabela B.1: vigas de rolamento, travessas/pilares de fechamento e limites horizontais por pavimento continuam por informação do usuário
  (campo "denominador de flecha"; deslocamentos de andar na verificação de estabilidade global).
* Chapa de extremidade: distribuição igual da tração entre as fileiras, sem método das linhas de plastificação nem fileiras entre as mesas; ligação por atrito sob momento.

**NBR 14762:2010**
* Anexo G (vigas mistas), 9.5 (enrijecedores), 9.8.6 (barras compostas fletidas), Seção 11 (ensaios) e ligações rígidas viga–pilar;
  interação entre esmagamento da alma e momento (não localizada no texto do Anexo D).
* Esmagamento da alma sem o comprimento de apoio real (aviso, não verificação) e sem a interação com o momento fletor.
* Ligações: não há verificação do perfil de apoio (flexão local) nem da tração na seção líquida do perfil na ligação (use a razão `An/Ag` nos
  parâmetros da barra).

## 5. Como foi verificado

Cada fórmula corrigida ou acrescentada tem teste com cálculo à mão em `src/tests/TestDesign.cpp`
(`norm2024FlexureCorrections`, `coldFormedWebCripplingAndServiceInertia`, `coldFormedConnectionsMatchHandCalculation`,
`slipCriticalBoltsFollowNorm2024`). A suíte completa (`ctest -C Release`) passa. Não há validação cruzada com outro programa.
