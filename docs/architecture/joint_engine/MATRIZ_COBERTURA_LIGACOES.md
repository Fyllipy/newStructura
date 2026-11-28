# Matriz de cobertura — todas as famílias (D0 revisado)

Substitui, como referência de planejamento, a `MATRIZ_CAPACIDADES_LIGACOES.md` (mantida como histórico). **Nenhuma linha é "suportada" só porque a geometria pode ser representada**: a coluna *Estado atual* separa (G) geometria/parametrização, (N) verificação normativa de resistência, (M) modelo mecânico fundamentado. Hoje **M existe apenas** para: chapa de extremidade e mesas soldadas viga–pilar (EN, `JointComponents`) e T-stub EN (resistência/rigidez, `TStub*`); o resto é G+N. ⚠ = referência **não lida** (a ler no início da fase). "NA" = não auditado.

Famílias reais do `ConnectionCatalog` (ids ≥ 100; lidas em `ConnectionCatalog*.cpp` e `docs/CATALOGO_LIGACOES.md`): WebShear (tala simples/dupla, viga–viga e viga–pilar, soldada/parafusada, cantoneiras), Moment (143–146: mesas soldadas, perfis T), FlexEndPlate (107/108), Composite (161–167), EndPlateSplice, CoverSplice/TubeSplice (emendas), TrussNode, Pin, bases (rígida/engastada com nervuras, 172/173) e os tipos legados (`kGusset`, `kSplice`, `kBasePlate`, `kTubular`, `kColdFormed`, `kEndPlate`). Famílias **adicionais previstas** (não existem hoje): híbridas por compatibilidade, gusset de múltiplos elementos com modelo mecânico, ancoragem com concreto/graute, diafragmas e consolos como componentes, cantoneiras de mesa+alma com rigidez.

## A. Parafusadas
| Tipologia | Componentes físicos | Mecanismos resistentes | Modelos constitutivos necessários | Saída mecânica | Método de solução | Referências | MEF | Exp. | Estado atual | Lacunas | Dependências |
|---|---|---|---|---|---|---|---|---|---|---|---|
| A1 Viga–pilar, chapa de extremidade (estendida/rasa) | chapa, T-stubs equivalentes (mesa/alma do pilar e chapa), parafusos, alma do pilar (cis./compr.), mesa da viga | modos T-stub 1–3, alma do pilar, painel, compressão | T-stub (EN k/F; Piluso F–δ), parafuso (alongamento), alma, painel | M–θ, Sj,ini, Mj,Rd; capacidade de rotação (só com modelo) | S1 (EN); S3 quando houver F–δ não linear | EN 1993-1-8 §6.2/6.3; Piluso 2001; NBR 8800 | calibração | Piluso, Jaspart ⚠ | G+N+**M (EN, 1 tipologia; resposta nos DOIS sentidos desde o Incremento 5)** | sem cap. rotação; sem F–δ integrado; distribuição só EN | J-0..1, C7 final, S1 |
| A2 Chapa de topo / flangeada fora do nó | idem + posição fora do nó | idem | idem | M–θ | S1 | EN; NBR | calib. | ⚠ | G+N (148/flange); **M (EN, composição em série, 147; Incremento 6)**; 148 (talas) sem M | 148: sem regra EN para emenda por talas | A1 |
| A3 Viga–pilar com perfis T | T-stubs de mesa e de alma, parafusos | alavanca, alma do T, mesa | T-stub, parafuso | M–θ | S1/S3 | EN §6.2.4; NBR | calib. | ⚠ | G+N (Moment 143–146); **143: só resistência (nível C, NBR; Incremento 6)**, sem Sj,ini/curva | M não: faltam espalhamento da compressão do T-stub (sem regra na EN), k11/k12 do T com a mesa da viga e benchmark de junta completa | A1 |
| A4 Cantoneiras de alma / mesa+alma | cantoneira(s), parafusos, alma, mesa | flexão da aba, parafusos, rasgamento | rigidez/resistência da cantoneira ⚠ | M–θ ou V–δ | S1 (tabelado) / S3 | ⚠ (EN Anexo? NBR) | recomendável | ⚠ | G+N (WebShear/cantoneiras viga–viga) | **sem modelo de rigidez** | pesquisa de fonte |
| A5 Tala de alma simples/dupla (cisalhamento) | talas, parafusos, alma, solda (se mista) | cis. parafuso, contato, rasgamento, block shear, excentricidade | rotacional (opcional), deslizamento ⚠ | V, rotação; ou só resistência | S2 analítico / só N | NBR 8800 6.3/6.5; EN | opcional | ⚠ | G+N (101–114) | M não; tratada "rotulada" por hipótese | A4 |
| A6 Emenda de viga/pilar (chapas de extremidade, talas) | chapas, parafusos, perfis | idem A1 + escorregamento | T-stub, parafuso, deslizamento | M–θ, N–δ, V–δ | S1/S3 | EN §6.2.7/6.3; NBR | calib. | ⚠ | G+N+**M (EN) para a emenda com duas chapas de extremidade, catálogo 135 (Incremento 5)**; `kSplice` legada e 147 (flangeada fora do nó): sem M | sem cap. rotação; chapas iguais; resposta nos dois sentidos (fileiras acima/abaixo); só reprodução de equações (EN Tabela 6.10, 6.3.3.1(5), 6.2.7.2(10)) + Python independente, sem ensaio/MEF | A1 |
| A7 Emenda axial (cobrejunta, cantoneira, tubo) | cobrejuntas, parafusos | seção líquida, Ct, rasgamento, block shear, escorregamento | rigidez axial por deslizamento/ovalização ⚠ | N–δ | S2 | NBR 8800 5.2/6.3 | opcional | ⚠ | G+N (CoverSplice/TubeSplice) | M não | — |
| A8 Treliça/contraventamento parafusado | diagonal, chapa/cantoneira, parafusos | seção líquida, block shear, Whitmore | N–δ por ovalização ⚠ | N–δ | S2 | NBR 8800 5.2, 6.3 | opcional | ⚠ | G+N (`TrussConnections`) | M não | B-gusset |
| A9 Múltiplas fileiras | fileiras | distribuição não proporcional (EN 6.2.7.2) | idem A1 | M–θ | S1 (EN) | EN | — | — | existe para chapa estendida | outras tipologias | A1 |
| A10 Tração/cisalhamento/interação em parafusos | parafusos | N–V, alavanca, deslizamento (atrito) | parafuso (ε,δ) | F–δ do parafuso | componente | NBR 6.3; EN Tab. 3.4 | — | ⚠ | N (NBR) | M não (k10 EN em T-stub) | — |

## B. Soldadas
| Tipologia | Componentes | Mecanismos | Modelos constitutivos | Saída | Método | Refs | MEF | Exp. | Estado | Lacunas | Dep. |
|---|---|---|---|---|---|---|---|---|---|---|---|
| B1 Viga–pilar mesas soldadas | solda mesa, alma/mesa do pilar, painel | tração/compr. da alma, mesa flexão, painel | alma do pilar EN (k,F); solda **rígida (hipótese rotulada)** | M–θ | S1 | EN 6.2.7.1 | calib. | ⚠ | G+N+**M (EN)** | solda sem lei; sem cap. rotação | — |
| B2 Solda de filete (grupo excêntrico) | filetes | resistência direcional/simplificada | **nenhum fundamentado na base** (norma dá resistência) | só resistência; F–δ **indisponível** até fonte | N | NBR 6.2; EN §4; ⚠ para deformabilidade | recomendável | ⚠ | N | M não | pesquisa |
| B3 Penetração total/parcial | solda, chapas | seção efetiva | idem B2 | resistência | N | NBR/EN | opc. | ⚠ | N | M não | — |
| B4 Emendas/contraventamentos soldados | chapas, soldas | idem | idem | resistência | N | NBR | opc. | ⚠ | G+N | M não | B2 |
| B5 Perfis tubulares (T/Y/X/K, gap/overlap) | banzo, diagonal, soldas | CIDECT: punção, plastificação da face, alma, etc. | rigidez de nó tubular ⚠ | resistência; rigidez ⚠ | S2 | EN §7; CIDECT ⚠; NBR 16239 ⚠ | recomendável | ⚠ | G+N (`TubularJoints`, K/N 15x) | M não | — |
| B6 Enrijecedores/chapas soldadas | enrijecedor, solda, alma | compressão localizada, flambagem | rigidez do enrijecedor ⚠ | resistência/rigidez local | S2 | NBR/EN 6.2.6 | opc. | ⚠ | G+N (`StiffenerDesign`) | M parcial NA | — |

## C. Híbridas
| Tipologia | Componentes | Mecanismos | Modelos | Saída | Método | Refs | MEF | Exp. | Estado | Lacunas | Dep. |
|---|---|---|---|---|---|---|---|---|---|---|---|
| C1 Parafuso+solda compartilhando esforço | ambos | distribuição por rigidez/compat. | rigidez de ambos + compat. ⚠ | N–V–M partilha | S3 | ⚠ | **necessário** (referência) | **necessário** | **inexistente** (verificações em separado) | tudo | B2, A10, S3 |
| C2 Chapa soldada a um elemento e parafusada a outro (ex.: tala 105–114) | tala, solda, parafusos | cada união separada em série pelo caminho de carga | compat. em série da chapa | V–δ, M–θ | S3 | ⚠ | recomend. | ⚠ | G+N (caminhos separados) | M não | C1 |
| C3 Chapa de extremidade + enrijecedores soldados | A1 + B6 | A1 + B6 | A1, B6 | M–θ | S1 com componentes extras | EN 6.2.6 | calib. | ⚠ | N parcial | M para enrijecedor NA | A1, B6 |

## D. Gusset plates / nós
| Tipologia | Componentes | Mecanismos | Modelos | Saída | Método | Refs | MEF | Exp. | Estado | Lacunas | Dep. |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 Contraventamento (1–2 diagonais, parafusada/soldada) | gusset, diagonais, parafusos/soldas, banzos | Whitmore, escoamento, ruptura, block shear, flambagem no plano e fora do plano, excentricidade | gusset (rigidez axial, flambagem) ⚠ | vetor de forças por barra; N–δ | S2 + S3 | NBR 8800; AISC ⚠ (Whitmore/Thornton/Cheng) | **necessário** (fora do plano) | ⚠ | G+N (`TrussConnections`, kGusset, TrussNode) | **M não**; só 1 barra+apoio em `NodeConnection` | topologia N-elementos |
| D2 Nó de treliça (banzo+diagonais+montantes) | gusset ou banzo direto | idem + cisalhamento do banzo | idem | vetor generalizado | S2/S3 | idem | necessário | ⚠ | G+N | M não | D1 |
| D3 Tubulares com gusset | tubo fendido, gusset | punção, plastificação ⚠ | ⚠ | resistência | S2 | CIDECT ⚠ | rec. | ⚠ | parcial NA | M não | B5, D1 |
| D4 Espaciais | gusset 3D/chapas | ⚠ | ⚠ | vetor | S3/MEF | ⚠ | necessário | ⚠ | **inexistente** | tudo | D1 |

## E. Chapas e componentes auxiliares
| Tipologia | Estado atual | Modelo mecânico necessário | Lacuna | Dep. |
|---|---|---|---|---|
| E1 Chapas de extremidade/emenda/alma/mesa | N (NBR 6.5) + M só no T-stub equivalente | flexão/tração por faixa (T-stub) e cisalhamento | M para chapa fora de T-stub | A1 |
| E2 Chapa de base / placa de apoio | N (BasePlate) | flexão + contato | contato/graute | F |
| E3 Enrijecedores | N (`StiffenerDesign`) | rigidez/resistência local | M NA | B6 |
| E4 Diafragmas | **inexistente** como componente | transferência de esforços (placa) | tudo | — |
| E5 Consolos | NA (a verificar no catálogo) | flexão/cisalhamento do consolo | NA | — |
| E6 Cantoneiras/perfis auxiliares/transição | G+N | A4 | M | A4 |

## F. Base e ancoragem
| Tipologia | Componentes | Mecanismos | Modelos | Saída | Método | Refs | MEF | Exp. | Estado | Lacunas | Dep. |
|---|---|---|---|---|---|---|---|---|---|---|---|
| F1 Base articulada tipo 1 / rígida / engastada, nervuras (172/173) | placa, nervuras, chumbadores, soldas, bloco | flexão da placa (Levy nas nervuras), pressão no concreto, tração/cis. dos chumbadores | placa em flexão sobre contato ⚠; chumbador (δ,F) ⚠ | N–M–V → rigidez rotacional da base ⚠ | S2 → S3 | NBR 8800 6.7; EN 1993-1-8 §6.2.5/6.3.3 ⚠; EN 1992-4 ⚠; ACI 318 ⚠ | **necessário** (contato) | ⚠ | G+N | **concreto/graute/ancoragem profunda não existem no programa**; M não | modelo de contato |
| F2 Grupos de chumbadores / interação N–V | chumbadores | tração, cis., arrancamento ⚠, interação | ⚠ | resistência; rigidez ⚠ | S2 | ⚠ | rec. | ⚠ | N parcial | concreto | F1 |

## G. Já no programa, fora da lista do prompt
| Tipologia | Estado | Observação |
|---|---|---|
| Mista aço–concreto (161–167) | G+N (NBR Anexo O) | modelo mecânico M: não; família própria no roadmap |
| Ligação por pino (`Pin`) | G+N | M: não |
| Perfis formados a frio (`kColdFormed`, NBR 14762) | G+N | M: não; outro domínio constitutivo (parafuso em chapa fina) ⚠ |
| Cantoneiras flexíveis viga–viga, talas fora do nó | G+N | via A4/A5 |

## Regra de leitura
Uma linha só passa a "suportada" na família quando houver **modelo (M) com fonte lida, domínio, estados e teste de reprodução + verificação**, e só leva o termo "validada" com evidência experimental/profissional (ver ROADMAP, critérios por família).
