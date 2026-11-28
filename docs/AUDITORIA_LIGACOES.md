# Auditoria do modelo mecânico de ligações

Este documento registra a auditoria crítica do módulo de ligações e da ferramenta "Gráfico momento-rotação" (NBR 8800:2024, 6.1.2), os defeitos encontrados e corrigidos, a arquitetura resultante, a integração com o solver global, a validação contra exemplos publicados e as limitações que permanecem. Todos os números citados foram obtidos por execução (testes automatizados em `src/tests/TestJointClassification.cpp`) ou por conferência com os textos das normas e dos exemplos publicados.

> **Princípio da auditoria:** build e testes passando não bastam. A implementação anterior passava em todos os testes e continha um erro de rigidez que só apareceu ao comparar com exemplos publicados (seção 3).

---

## 1. Resumo

| | Estado |
|---|---|
| Modelo físico (rigidez + resistência + curva) em um só módulo | feito: `design/JointComponents` |
| Idealização estrutural derivada do modelo físico, sem alterá-lo | feito: `deriveEquivalent`, `JointAnalysisService::idealize` |
| Resistência por componentes (EN 1993-1-8, 6.2) usando as mesmas fileiras da rigidez | feito |
| Níveis A (comportamento), B (classificação/idealização) e C (dimensionamento) separados; análise **sem exigir ligação dimensionada** (modelo preliminar) | feito (seções 1.1 e 1.2) |
| Validação contra exemplos publicados | feito para chapa de extremidade (SCIA/EN, SCI P398); erros < 1% |
| Validação da idealização no solver (mola de extremidade) contra solução analítica | feito |
| Controle de consistência (recálculo/invalidação quando a ligação muda) | feito para a ferramenta; veja limites na seção 8 |
| Tipologias com modelo mecânico | 2 (mesas soldadas; chapa de extremidade). As demais permanecem só com verificação de resistência |
| Efeito de grupo de fileiras, excentricidades, painel de alma com vigas opostas de momentos diferentes | **não** tratados (limitações declaradas e avisadas na janela) |

---

## 1.1 Escopo correto: três níveis independentes, mas integrados

O objetivo da ferramenta **não** é dimensionar uma ligação já escolhida. É permitir que o projetista investigue o comportamento rotacional de uma conexão, conheça rigidez e classificação e, só depois, decida a idealização na análise global.

| Nível | Conteúdo | Depende de ligação dimensionada? |
|---|---|---|
| **A — Comportamento mecânico** | curva momento-rotação, rigidez inicial, secante, tangente, `Mj,Rd` do modelo | **não**: pode vir de modelo preliminar, de ligação cadastrada, de curva informada |
| **B — Classificação e idealização** | classificação automática (NBR 8800, 6.1.2) e decisão posterior (rotulada, semirrígida linear, semirrígida não linear, rígida) | não |
| **C — Dimensionamento construtivo** | verificação de parafusos, soldas, chapas, enrijecedores, estabilidade (NBR 8800) | sim; **etapa posterior**, no módulo de ligações |

### Auditoria antes da correção: o que já respeitava e o que não

| Aspecto | Antes | Situação |
|---|---|---|
| Classificação calculada, não informada | respeitava | mantido |
| Idealização como decisão posterior, que não altera os resultados, com aviso de divergência | respeitava | mantido |
| Identificação da origem do resultado (calculado × informado) | respeitava parcialmente | ampliado (natureza "PRELIMINAR") |
| Curva só com dados reais, sem curva fictícia | respeitava | mantido |
| **Exigir uma ligação dimensionada cadastrada para obter a curva** | **violava**: sem `NodeConnection` utilizável, a janela só dizia "dados insuficientes" | corrigido: **modelo preliminar** (seção 1.2) |
| **Botões "Criar chapa de extremidade/mesas soldadas" gravavam `NodeConnection` padrão no módulo de ligações** (uma ligação aparentemente dimensionada surgia como efeito de uma análise) | **violava** o nível C como etapa posterior | removidos; a análise nunca cria ligação |
| **`Mj,Rd` da curva limitado pelas verificações da NBR da ligação cadastrada** (mistura nível C no nível A) | **violava** a independência | corrigido: a curva usa o modelo mecânico (EN); a verificação da NBR é nível C e apenas **sinaliza** quando a ligação cadastrada resiste menos que a curva |
| Interface organizada em torno de "ligação construtiva do modelo" | induzia o fluxo errado | reorganizada: fonte A1 (cadastrada, opcional), fonte A2 (preliminar), níveis A/B/C no painel |

### 1.2 Modelo preliminar (nível A sem dimensionamento)

`JointComponentService::evaluatePreliminary` estima o comportamento a partir **apenas dos perfis I/H das duas barras** e de uma **configuração de referência** com hipóteses declaradas:

* **Mesas soldadas (referência):** penetração total, alma sem enrijecedores, β = 1 (ou 0, vigas dos dois lados). Não adota nenhum componente construtivo: depende só da seção da viga e do pilar.
* **Chapa de extremidade estendida (referência):** configuração **adotada por regras** e declarada (parafusos M20 classe 8.8; chapa de espessura igual ao diâmetro, largura bf + 20 mm; gabarito 0,6·bf; 1 fileira na extensão a 40 mm da mesa; solda de perna ≈ 0,75·tf). O usuário pode alterar espessura, diâmetro e número de fileiras.
* **Faixa preliminar:** a janela mostra as duas referências lado a lado (Si, η e classe), para apoiar a decisão de modelagem.
* **Identificação:** toda a interface, a legenda do gráfico, o painel e a memória marcam "PRELIMINAR / NÃO dimensionada". A memória lista as hipóteses e o domínio de validade (efeito de grupo não calculado; só um sentido de momento para chapa; `Mj,Rd` do modelo de referência; sem verificação construtiva).
* **Sem curva fictícia:** o modelo preliminar é a mesma mecânica de componentes validada na seção 9, aplicada a uma configuração declarada; perfis que não são I/H (ou sem dimensões) não geram resultado.
* **Limite:** o modelo preliminar **não** substitui o dimensionamento: a rigidez adotada na análise global só é realizável se a ligação final for projetada para ela (aviso na aba de idealização).

---

## 2. Auditoria do fluxo atual (14 pontos)

| # | Pergunta | Onde e como é feito |
|---|---|---|
| 1 | Como a ligação é recuperada do modelo | `MainWindowJoints.cpp` (`onJointMomentRotation`): procura em `m_extras.connections` (`NodeConnection`, cadastro único do módulo de ligações) as ligações com `nodeId` igual ao do nó e `memberId` em uma das duas barras selecionadas, preferindo a viga. Não há cadastro paralelo. |
| 2 | Como são identificadas as demais barras do nó | `JointAnalysisService::analyze`: todas as barras de `ModelData.members` que chegam ao nó; papel por inclinação (`roleOfDirection`: viga ≤ 10°, pilar ≥ 80°, diagonal entre eles); pares formam ligações distintas (`PairInfo`). |
| 3 | Como a orientação é determinada | Plano de análise = plano das duas barras (normal `n = d_A × d_B`); se colineares, plano vertical que as contém (declarado). Inércia da viga em torno de `n` com os eixos locais de `BarFrame` girados pela seção; grau de liberdade da mola = rotação local em torno do eixo mais alinhado a `n` (`rotationDof`, 5/4 em I; 11/10 em J). |
| 4 | Como os componentes são extraídos | `JointComponentService::evaluate`: `NodeConnection` → `ConnectionService::endPlateInput` / `ConnectionCatalog::momentInput` (os mesmos dados da verificação de resistência) → `JointComponents::ModelInput`. |
| 5 | Como os modelos são montados | `JointComponents::computeStiffness` e `computeResistance`: fileiras (`deriveExtensionRows`), coeficientes `ki`, T-stubs, distribuição por fileira. |
| 6 | Como a rigidez Si é calculada | `Sj,ini = E·z² / Σ(1/ki)` (EN 1993-1-8, 6.3.1), `ki` da Tabela 6.11; com várias fileiras, `keq` e `zeq` (6.3.3.1). Si da norma = secante a 2/3 de `Mj,Rd` lida da curva (`computeStiffness` em `JointClassification`); coincide com `Sj,ini` porque a curva da EN é linear até 2/3. |
| 7 | Como a curva é construída | `momentRotationCurve`: `θ = M/Sj,ini` até 2/3 de `Mj,Rd`; `θ = M/Sj,ini · (1,5 M/Mj,Rd)^ψ` acima, ψ = 2,7 (Tabela 6.8). **É uma lei fechada da EN aplicada à rigidez e à resistência calculadas**, não uma resolução numérica do equilíbrio de cada componente (ver 6). |
| 8 | Como o momento resistente é determinado | Nível A: componentes da EN (6.2): T-stubs modos 1-3, almas, painel, compressão; distribuição por fileira (6.2.7.2); `Mj,Rd = Σ Ftr,Rd·hr`. Nível C (só com ligação cadastrada): as verificações da NBR 8800 (bissecção no momento) **não alteram a curva**; sinalizam `nbrBelowCurve` quando resistem menos. |
| 9 | Como são tratadas as excentricidades | **Não são.** O modelo mede o momento no nó. A janela avisa quando a viga tem offset de eixo ou zona rígida no nó (`Bar::hasEccentricity`). |
| 10 | Interação entre componentes | Em série (`1/ki`) dentro da fileira; em paralelo entre fileiras (`keq`, `zeq`); limite global do somatório de forças (painel, compressão da alma e da viga). Efeito de grupo de fileiras **não** é calculado (avisado). |
| 11 | Ligações com múltiplos elementos | Cada par é uma ligação distinta; vigas opostas só entram por `β` (0 ou 1, cadastro). Momentos desiguais dos dois lados não são tratados (aviso na janela). |
| 12 | Como os resultados vão ao modelo global | `JointAnalysisService::idealize` grava em `MemberConnectionSpec` (`springK[dof]` e/ou `hinges`), que o `AnalysisService` converte em `EndSpringData`/`PlasticHingeData` para o solver. |
| 13 | Como a idealização é armazenada | Nas ligações de extremidade da barra (`m_extras.memberConnections`), persistidas com o projeto. Os resultados mecânicos não são armazenados: são recalculados da ligação cadastrada (por isso não podem ficar inconsistentes). |
| 14 | Consistência entre o modelo físico e o equivalente | A idealização é função pura do modelo físico (`deriveEquivalent`); a janela detecta idealização **desatualizada** (compara a mola gravada com a Si atual) e recalcula tudo ao voltar do editor de ligações (impressão digital da ligação, `fingerprint`). |

---

## 3. Defeitos encontrados e corrigidos

Cada item foi encontrado pela auditoria; os erros de rigidez foram detectados **comparando com exemplos publicados**, não pelos testes anteriores.

| Id | Defeito | Como foi encontrado | Correção |
|---|---|---|---|
| D1 | `k10` multiplicado por 2 (dois parafusos por fileira). A EN dá `k10 = 1,6·As/Lb` por fileira. | SCI P398 C2: `1,6·353/70,5 = 8,01` publicado; SCIA: `1,6·157/38,8 = 6,47`, e `keff,1 = 1,73` só fecha com `k10 = 6,47`. | `k10` sem fator 2. Superestimava a rigidez dos parafusos e, portanto, `Sj,ini`. |
| D2 | `beff,c,wc` da chapa de extremidade sem o termo `2√2·ap` da solda mesa-chapa (EN 6.2.6.2, eq. 6.11). | Leitura do texto da EN e do exemplo SCIA (`9,2 + 2√2·5 + 5(12+12) + sp = 163,27 mm`). | Termo incluído, com `ap = perna/√2` e `sp` limitado pelo comprimento da chapa abaixo da mesa. |
| D3 | `Mj,Rd` vinha só das verificações da NBR com **tração distribuída igualmente** entre as fileiras e `z = d − tf`, enquanto a rigidez usava `zeq` e rigidez por fileira: dois modelos diferentes para a mesma ligação. | Auditoria do fluxo (item 8). | Resistência por componentes da EN com as mesmas fileiras da rigidez (nível A). A verificação da NBR passou a ser nível C: sinaliza, não altera a curva. |
| D4 | Comprimento efetivo circular da fileira da extensão da chapa usava `min(2π·mx; π·mx + w; π·mx + 2e)` (fórmulas da mesa do pilar). A Tabela 6.6 dá `2π·mx`. | Leitura da Tabela 6.6 e do exemplo (`2·π·33,66 = 211,49`). | Fórmula exata da Tabela 6.6. |
| D5 | Curva calculada pelo método dos componentes aparecia na legenda como "curva informada". | Revisão da interface (item 11 do pedido). | Legenda e painel identificam a origem e a natureza: "CALCULADA" × "INFORMADA/importada". |
| D6 | A idealização só oferecia liberação ou mola linear; a perda de rigidez a partir de 2/3 de `Mj,Rd` era descartada sem aviso. | Auditoria do item 12. | Nova opção "semirrígida não linear": rótula bilinear derivada da curva (`deriveEquivalent`). Mola linear continua como a simplificação permitida pela NBR (6.1.2.1). |
| D7 | Nenhum controle de consistência: alterar a ligação deixava curva e idealização antigas. | Auditoria do item 14. | `fingerprint` da ligação; a janela reavalia ao voltar a ela; idealização defasada é sinalizada. |
| D8 | Excentricidades, plano não alinhado a eixo principal e vigas opostas eram ignorados em silêncio. | Auditoria dos itens 9 e 11. | A janela passa a avisar cada situação (`JointAnalysisService::analyze`, notas do contexto e memória de cálculo). |

---

## 4. Arquitetura: modelo físico × modelo estrutural equivalente

```
 NodeConnection (cadastro único do módulo de ligações: tipo, geometria, aços)
        │
        ▼
 ConnectionService::endPlateInput / ConnectionCatalog::momentInput   ← mesmos dados da verificação NBR
        │
        ▼
 JointComponents::ModelInput ──► computeStiffness ──► ki, keq, zeq, Sj,ini         ┐
                             └─► computeResistance ─► T-stubs, almas, painel, Mj,Rd ├ MODELO FÍSICO
 (+ verificações NBR, bissecção) ─► NÍVEL C: só sinaliza se NBR < EN                │ (curva M-θ, métricas)
                                                                                    ┘
        │ curva M-θ (6.3.1), métricas (secante, tangente, 2/3 Mj,Rd, Sj,ini/2)
        ▼
 JointClassification (NBR 8800 6.1.2: Si, limites, Kv/Kp)  ──► classificação (RESULTADO)

        │ decisão posterior do usuário (idealização)
        ▼
 JointComponents::deriveEquivalent(kind, Si, Mj,Rd) ─► EquivalentModel (derivado, nunca editável)
        ▼
 JointAnalysisService::idealize ─► MemberConnectionSpec (springK / hinges) ─► AnalysisService ─► solver
```

* **Cadastro único:** a ferramenta nunca grava dados de ligação próprios; só idealizações nas ligações de extremidade da barra.
* **Interfaces comuns:** a rigidez e a resistência de qualquer tipologia futura entram por `ModelInput` e `BoltRow`/`Component` (coeficiente, resistência, memória). Hoje `ModelInput` ainda tem um `kind` com dois valores e funções únicas; para acrescentar uma tipologia nova é preciso estender `computeStiffness`/`computeResistance`. **Isso é uma limitação arquitetural real:** o passo seguinte é extrair uma interface (`IJointModel` com `components()`, `resistance()`, `curve()`) e registrar cada tipologia, o que o formato atual de dados permite sem reescrever o núcleo.
* **Separação garantida por testes:** `idealizationIsALaterModelingDecisionAndKeepsTheMechanicalResults` e `idealizationDerivesFromThePhysicalModelAndWritesAHinge`.

---

## 5. Matriz de cobertura das tipologias

Legenda: **R** = verificação de resistência pela NBR existe no módulo; **K** = rigidez por componentes; **Mj** = `Mj,Rd` por componentes; **C** = curva M-θ calculada; **I** = idealização derivada.

| Tipologia | R | K | Mj | C | I | Observação |
|---|---|---|---|---|---|---|
| Viga-pilar, mesas soldadas | sim | sim | sim | sim | sim | simétrica; solda de penetração total: `ap = 0` (conservador) |
| Viga-pilar, chapa de extremidade (fileiras na extensão) | sim | sim | sim | sim | sim | só o sentido que traciona as fileiras; sem efeito de grupo |
| Viga-pilar com perfis T parafusados | sim | não | não | não | não | exige T-stub de mesa da viga e do pilar |
| Chapa de extremidade rente (flush) | parcial | não | não | não | não | geometria com fileiras entre as mesas não representada em `deriveExtensionRows` |
| Chapa de cisalhamento / cantoneira dupla (cortante) | sim | não | não | não | não | normalmente tratadas como rotuladas (NBR 6.1.2.3); falta K de cantoneiras (EN 6.3.2, `k6`) |
| Ligação com assento | sim | não | não | não | não | |
| Emenda de viga (chapa de extremidade, cobrejunta) | sim | não | não | não | não | EN 6.3.3.1(5): `keq` com `k5` e `k10` |
| Viga-viga | sim | não | não | não | não | o critério 6.1.2 não é aplicado a viga-viga |
| Nó de treliça (chapa de nó, tubular) | sim | não | não | não | não | tratado como rotulado na prática |
| Base de pilar | sim | não | não | não | não | EN 6.3.4 |
| Ligações com enrijecedores | sim | sim (`k2 = k3 = ∞`) | sim | sim | sim | só para as duas tipologias com modelo |

Para as tipologias sem modelo, a ferramenta continua aceitando a curva, `Si` e `Mj,Rd` informados (origem identificada como não calculada).

---

## 6. Sobre a curva momento-rotação: o que é e o que não é

A resposta objetiva à pergunta "a curva é resolvida ou é uma expressão simplificada?": **é a expressão da EN 1993-1-8 (6.3.1, eq. 6.28)**, alimentada por `Sj,ini` e `Mj,Rd` calculados dos componentes. Não há solução incremental do equilíbrio de cada componente com leis constitutivas não lineares individuais.

* **Equações:** `θ = M/Sj,ini` para `M ≤ 2/3·Mj,Rd`; `θ = M/Sj,ini · (1,5·M/Mj,Rd)^ψ` até `Mj,Rd`; ψ = 2,7.
* **Hipóteses:** componentes elásticos lineares até 2/3 de `Mj,Rd`; perda de rigidez agregada pelo fator μ; força axial na viga ≤ 5% de `Npl,Rd`.
* **Domínio de validade:** `0 ≤ M ≤ Mj,Rd`. A curva **termina** em `Mj,Rd`; não há trecho de patamar nem capacidade de rotação (não calculada). A janela, a memória e o guia dizem isso.
* **Métricas disponíveis** (`curveMetrics`, mostradas no painel e na memória): rigidez inicial, secante e tangente em qualquer M, início da perda de rigidez (2/3 de `Mj,Rd`), momento em que a secante cai à metade, rotação em `Mj,Rd`.
* **Procedimento numérico:** o único procedimento iterativo é a bissecção do momento nas verificações da NBR (60 iterações, tolerância de precisão de máquina).
* **Evolução natural:** resolver o equilíbrio por componentes com leis força-deslocamento não lineares próprias (por exemplo, a lei de Jaspart para cada componente) e obter a curva por incremento de carga. A estrutura `Component` já guarda `k` e `resistance` por componente; falta a lei constitutiva e o laço incremental.

---

## 7. Integração com o solver global

### 7.1 O que o solver faz com uma mola de extremidade

Em `Structuradll/src/member.cpp` (`Member::getLocalStiffnessMatrix`, `condenseFixedEndForces`), os graus de liberdade com mola são **condensados em série**: `S = Krr + Ks`, `Kc = Kaa − Kar·S⁻¹·Kra`. Isso é exatamente o comportamento mecânico de uma mola em série entre o nó e a extremidade da barra: a rotação do **nó** menos a rotação da **extremidade da barra** é `M/k`. Foi validado numericamente contra a solução analítica (seção 9, teste do balanço com mola).

### 7.2 Quando a mola de extremidade representa a ligação, e quando não

| Aspecto | Situação |
|---|---|
| Rotação relativa viga-pilar | representada: a rotação do nó é a do pilar (barras contínuas pelo nó) e a mola dá a diferença para a viga |
| Painel de alma | já incluído em `Sj,ini` (`k1`): não é modelado à parte; correto para viga de um lado |
| Vigas opostas, momentos diferentes | **não representado**: o painel acopla as duas ligações; a mola de cada lado usa o mesmo `β` fixo |
| Excentricidades de eixo | **não**: o modelo mede o momento no nó; a janela avisa |
| Rigidez dos pilares | representada pelo solver (barra contínua) |
| Flexibilidade dos elementos conectados | só a do painel e dos componentes locais (dentro de `Sj,ini`); a da viga e do pilar vêm do solver |
| Plano de análise não alinhado a um eixo principal da seção | **aproximado**: a mola age em torno do eixo local mais próximo; a janela avisa abaixo de 0,98 de alinhamento |
| Duas extremidades da viga | cada nó é analisado e idealizado separadamente |
| Elementos de ligação não lineares entre nós (`LinkElement` com lei multilinear) | **não usados** pela idealização; a rótula bilinear (`PlasticHingeData`) cobre a não linearidade da extremidade, mas só em análise não linear |

### 7.3 Extensões propostas ao solver (não implementadas)

1. **Zona de painel** como elemento de dois nós com mola rotacional ao cisalhamento e braços rígidos, para acoplar vigas opostas com momentos diferentes.
2. **Mola multilinear** por `LinkElement` a partir da curva completa (mais exata que a bilinear).
3. **Excentricidades**: braço rígido entre o nó e o eixo da viga com mola na posição física da ligação.

---

## 8. Controle de consistência

* A ligação do projeto é a única fonte: curva, rigidez, `Mj,Rd`, classificação, memória e idealização derivam dela.
* `JointComponentService::fingerprint` (tipo, extremidade, parafuso, todos os valores e `E`) identifica o estado. A janela chama `refreshFromModel` ao voltar a ela (ativação da janela): se a impressão digital mudou, **tudo é recalculado** e a idealização aplicada é sinalizada como desatualizada.
* A idealização gravada nas ligações de extremidade é comparada com a `Si` atual: mola ou rótula que diferem em mais de 2% aparecem como "DESATUALIZADA" na aba de idealização.
* **Limites:** a invalidação ocorre quando a janela da ferramenta está aberta ou é reaberta; alterações feitas com a janela fechada são detectadas ao abri-la, não propagadas automaticamente para o modelo global (a idealização é decisão do usuário e só muda quando ele a reaplica).

---

## 9. Validação mecânica

### 9.1 Exemplo SCIA (EN 1993-1-8): HEB140 + IPE220, chapa 140×12, 6 M16 8.8, viga de um lado

Os comprimentos efetivos e os `m` de cada fileira foram os publicados (o cálculo de `leff` de grupo não faz parte desta validação). Teste: `validationScia_stiffnessComponentsAndSjIni` e `validationScia_resistanceAndMomentResistance`.

| Grandeza | Publicado | Programa | Erro relativo |
|---|---|---|---|
| `k3` fileira 1 / 2 (mm) | 5,73 / 5,59 | 5,728 / 5,592 | < 0,1% |
| `k4` fileira 1 / 2 (mm) | 8,59 / 8,39 | 8,593 / 8,389 | < 0,1% |
| `k5` fileira 1 / 2 (mm) | 7,55 / 7,57 | 7,547 / 7,565 | < 0,1% |
| `k10` (mm) | 6,47 | 6,474 | 0,06% |
| `keff` fileira 1 / 2 (mm) | 1,73 / 1,71 | 1,730 / 1,710 | < 0,1% |
| `zeq` (mm) | 216,4 | 216,42 | 0,01% |
| `keq` (mm) | 3,35 | 3,348 | 0,06% |
| `k2` (mm), `beff,c,wc = 163,27` | 8,70 | 8,696 | 0,05% |
| **`Sj,ini` (kN·m/rad)** | **11 596** | **11 602** | **0,05%** |
| `Vwp,Rd` (kN) | 160,2 | 160,21 | 0,01% |
| `Ft` fileira 1 (kN) | 97,31 | 97,06 (alma da viga) ou 97,31 (chapa) | 0,26% |
| Fileira 2 (kN), limitada pelo painel | 62,9 | 62,9 | < 0,1% |
| **`Mj,Rd` (kN·m)** | **34,91** | **34,91** | **< 0,5%** |

Observação: o exemplo publicado lista a fileira 1 como "ruptura da chapa" (97,31 kN); pela EN 6.2.6.8 a alma da viga tracionada (`70·5,9·235 = 97,06 kN`) fica 0,26% abaixo. A diferença é irrelevante para `Mj,Rd`, que a fileira 2 e o painel definem.

### 9.2 SCI P398, exemplo C2 (New Steel Construction, fev/2020)

`k10 = 1,6·353/70,5 = 8,01 mm`, `k1 = 0,38·3810/498 = 2,91 mm`, `Sj = 210·498²/(1/2,91 + 1/6,01) = 102 MN·m/rad` (com `k2 = ∞` por enrijecedor). Conferidos aritmeticamente (`validationP398_k10AndK1ReproduceThePublishedValues`); as entradas completas do exemplo não estão disponíveis, então este caso valida a interpretação das fórmulas, não a geometria.

### 9.3 Leis físicas do modelo (propriedades)

`physicalLawsOfTheComponentAssembly`, `curveMetricsMatchTheAnalyticalDerivatives`, `bilinearEquivalentReproducesTheCurveAtTheKnees`:

* `1/Sj,ini = Σ(1/ki)/(E·z²)` (associação em série), exata em 1e-12;
* `Sj,ini` é menor que a rigidez de qualquer componente isolado;
* `Sj,ini` escala linearmente com `E`; `Mj,Rd` não depende de `E`;
* chapa mais espessa: mais rígida e nunca menos resistente; enrijecedores nunca reduzem `Mj,Rd`; `γ` maior reduz `Mj,Rd`;
* equilíbrio: `Σ Ftr,Rd ≤` limite do painel/compressão e `Mj,Rd = Σ Ftr,Rd·hr`;
* tangente analítica confere com diferença finita da curva (< 1%); a secante a 2/3 de `Mj,Rd` é `Sj,ini`;
* a rótula bilinear passa pelos dois pontos (`2/3·Mj,Rd` e `Mj,Rd`) da curva.

### 9.4 Solver: a mola de extremidade representa a rotação relativa

Teste `endSpringInTheSolverGivesTheAnalyticalCantileverWithRotationalSupport`: balanço com mola rotacional `k` no apoio; flecha da ponta `P·L³/(3EI) + P·L²/k`. O resultado do solver coincide com a solução analítica (veja o resultado nos testes).

### 9.5 O que **não** foi validado

* Ligação soldada contra exemplo publicado (só contra cálculo à mão e leis físicas).
* Efeito de grupo de fileiras e vigas opostas (não implementados).
* Comportamento não linear contra ensaios (a curva é a da EN; a rótula bilinear não foi comparada com análise não linear de pórtico).
* Resistência com mais de duas fileiras e geometrias diferentes da do exemplo SCIA (as fileiras do programa ficam todas na extensão da chapa).

---

## 10. Limitações restantes (declaradas ao usuário)

1. Só duas tipologias com modelo mecânico; interface comum a extrair (seção 4).
2. Curva = lei da EN, não solução incremental; sem capacidade de rotação.
3. Sem efeito de grupo de fileiras (rigidez possivelmente superestimada e resistência possivelmente não conservadora; mitigada pelo mínimo com a NBR).
4. Excentricidades e vigas opostas de momentos diferentes não modeladas.
5. Chapa de extremidade: só o sentido que traciona as fileiras.
6. Idealização por mola/rótula de extremidade; elementos de ligação não lineares entre nós não usados.
7. Verificações da EN usam os fatores parciais da NBR (`γa1` para escoamento; resistência de parafuso da NBR 6.3.3.1), não os da EN.
8. A norma EN 1993-1-8 consultada foi uma cópia pública em PDF (BS EN 1993-1-8:2005); o texto da NBR 8800:2024 foi conferido no exemplar da pasta `normas`.

## 11. Referências

* ABNT NBR 8800:2024, itens 6.1.2 e 6.1.1 a 6.1.5.
* EN 1993-1-8:2005: 5.3, 6.2.4 (T-stubs, Tabela 6.2), 6.2.6 (componentes), 6.2.7 (momento resistente), 6.3 (rigidez, Tabelas 6.8 e 6.11).
* SCIA Engineer, *Manual calculation — Design example of a joint with extended end plate* (HEB140/IPE220).
* New Steel Construction, fev/2020, artigo técnico sobre rigidez de ligações (exemplo C2 do SCI P398).
