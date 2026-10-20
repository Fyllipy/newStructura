# Catálogo de ligações e emendas

Tipologias do catálogo (ids >= 100, `ConnectionCatalog`), implementadas **a partir das normas** (NBR 8800:2024, NBR 14762:2010, CIDECT DG1), sem uso do código de outros programas.
Cada variante é um `preset` de uma *família* de cálculo; uma família fornece: campos do formulário, valores padrão, verificação (memorial), lista de peças, modelo 3D (`ConnectionSolids`),
cotas/notas do detalhamento. O detalhamento é gerado automaticamente dos sólidos (vistas frontal e superior com linhas ocultas) + cotas da família.

## Famílias

| Família | Estado | Variantes (id) |
|---|---|---|
| Tala de alma (`WebShear`) | **feita** (cálculo, 3D, detalhamento) | 101 viga–viga soldada (tala simples), 102 viga–viga parafusada, 103/104 viga–pilar parafusada (alma/mesa), 105/106 viga–pilar soldada (alma/mesa), 111/112 viga–viga dupla tala (parafusada/soldada), 113/114 viga–pilar dupla tala parafusada (mesa/alma), 115/116 idem soldada |
| Emendas (U, tração, cantoneiras, chapas e tubos, viga com chapa de topo, pilares) | pendente (emenda de viga I parafusada já existe: `kSplice`) | – |
| Bases de pilar rígidas com enrijecedor e engastadas | pendente (base articulada tipo 1 existe: `kBasePlate`) | – |
| Treliça soldada direto no banzo / gousset com solda de topo | pendente (chapa de nó parafusada existe: `kGusset`) | – |
| Ligações rígidas (perfis T e cantoneiras, soldada, simples/dupla tala rígida, flangeada e com talas fora do nó) | pendente (chapa de extremidade rígida existe: `kEndPlate`) | – |
| Ligações mistas (5 variantes) | pendente | – |
| Reticulado tubular (diagonal-diagonal com gap/overlap/knee, diagonal-montante-diagonal) | pendente (CIDECT T/Y/X/K existe: `kTubular`) | – |

## Tala de alma: o que se verifica

Parafusos (cisalhamento com excentricidade, pressão de contato na(s) tala(s) e na alma, espaçamentos e distâncias), tala (escoamento e ruptura ao cisalhamento, rasgamento, flexão pela
excentricidade, axial), alma da viga (rasgamento, cisalhamento na seção da ligação), solda tala–apoio (método elástico e metal-base), solda tala–alma (grupo excêntrico pelo centro instantâneo
de rotação), pernas mínima e máxima, geometria (altura da tala × alma entre mesas, recorte das mesas na viga principal, largura livre da alma do pilar), verificação local do apoio (mesa/alma)
e deslizamento (ligação por atrito) opcional. Dupla tala: corte duplo nos parafusos, cortante e momento divididos entre as talas.

## Validação

* a tala simples parafusada coincide com a chapa de cisalhamento já validada (parafusos, escoamento, flexão e solda: razões idênticas);
* o corte duplo reduz a demanda dos parafusos e da tala a cerca da metade; filete maior reduz a razão do grupo soldado à alma;
* **comparação com o mCalcLIG:** não foi feita (o programa não foi executado nem seus binários analisados). Para validar, forneça casos de entrada e a saída dele (razões, resistências) e
  compare-os com o memorial deste programa.

## Famílias adicionadas na última etapa

| Família | Ids | Base normativa |
|---|---|---|
| Moment (viga–pilar rígida) | 143–146 | mesas soldadas (penetração total/filete) ou perfis T parafusados (efeito de alavanca, alma do T: escoamento, ruptura e rasgamento; mesa da viga); alma direta/tala; efeitos locais e painel de alma (5.7.7.1 completo: NSd > 0,4 Np, kv com enrijecedores, cortante do pilar) |
| FlexEndPlate | 107, 108 | chapa de extremidade flexível (altura parcial): parafusos, chapa (escoamento, ruptura, rasgamento, flexão sob tração), filetes à alma, alma da viga |
| Composite | 161–167 | NBR 8800:2024 Anexo O completo (O.2.2–O.2.5, O.3, O.4 com Tabelas O.1–O.3 interpoladas) e L.5.2.2 (kc); ligação da alma só ao cortante (O.2.5.1) pela tipologia correspondente |
| Bases com nervuras | 172, 173 | painéis entre nervuras por placa de Levy (3 bordas apoiadas, 1 livre; validada contra Timoshenko), balanços, chumbador contra as nervuras, nervura (V, M elástico) e soldas (V + M) |
| Talas fora do nó / cantoneiras viga–viga | 148, 152 | reaproveitam kSplice e kDoubleAngle |

Automático a partir da análise: o cortante do painel de alma (5.7.7.1) e o NSd do pilar vêm do equilíbrio do nó (esforços de extremidade com sinal: mesas das vigas + cortante dos pilares de cima e de baixo; `NodeEquilibrium`), e o M+Rd da viga mista (campo 0 = automático) é calculado pela L.2.3.1 com o perfil, a laje, os conectores do vão e βvm.

Hipóteses declaradas (conservadoras ou dependentes de dado do usuário): momento do nó adotado na emenda 148 sem alívio do cortante; sem análise, o cortante/NSd do pilar vêm dos campos manuais (padrão 0); εsu da armadura e flambagem por distorção (O.3.2-d) informados/verificados pelo projetista; rigidez ki das cantoneiras segue a redação literal de O.2.5.2.3.1.
