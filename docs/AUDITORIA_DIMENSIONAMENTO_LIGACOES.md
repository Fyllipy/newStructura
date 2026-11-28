# Auditoria técnica do módulo de dimensionamento de ligações — Structura::Metálicas

**Natureza deste documento:** relatório diagnóstico. **Nenhum código do projeto foi modificado.** Os cálculos de conferência foram feitos fora do repositório (um executável de auditoria que chama o núcleo existente e uma reimplementação independente das equações a partir do texto da norma).

**Fontes normativas consultadas:** NBR 8800:2024 (exemplar da pasta `normas`, páginas conferidas em imagem quando o texto extraído perdia as equações); EN 1993-1-8:2005 (cópia pública, só para as comparações com o método dos componentes); exemplos publicados citados na seção 6.

---

## 0. Escopo e honestidade sobre a cobertura

O núcleo de cálculo tem cerca de 7 100 linhas em `src/design/` (17 arquivos). **Não foi possível auditar todas as linhas com a mesma profundidade.** O quadro abaixo diz, sem ambiguidade, o que foi feito em cada arquivo.

| Arquivo(s) | Profundidade |
|---|---|
| `Connections.cpp/.h` (parafusos, atrito, filetes, centro instantâneo de rotação, efeito de alavanca) | **Leitura integral** e comparação equação a equação com a NBR 8800:2024 (6.2.5, 6.3.2 a 6.3.5, 6.3.9 a 6.3.12, Tabelas 9, 11, 13, 14, 16, 19, A.3, A.4) |
| `SupportChecks.cpp` (5.7.2 a 5.7.7: flexão local da mesa, escoamento local, enrugamento, flambagem lateral e instabilidade da alma, painel de alma) | **Leitura integral** e conferência com o texto da norma (5.7) |
| `EndPlateConnection.cpp` (ligação rígida por chapa de extremidade) | **Leitura integral** |
| `ShearConnections.cpp` (chapa de cisalhamento, cantoneira dupla) | Leitura da chapa de cisalhamento; cantoneiras por inventário de verificações e amostragem |
| `MomentConnection.cpp`, `WebShearConnection.cpp`, `FlexEndPlate.cpp`, `SupportWeld.cpp`, `StiffenerDesign.cpp` | **Inventário de verificações e cláusulas citadas e amostragem**; equações não conferidas uma a uma |
| `BasePlate`, `TrussConnections`, `TrussNodeConnection`, `TubularJoints` (CIDECT), `ColdFormedConnections` (NBR 14762), `CompositeConnection` (Anexo O), `CoverSplice`, `PinConnection` | **Não auditados em profundidade**: apenas inventário. Nenhuma conclusão de confiabilidade é emitida para eles |
| `ConnectionService.cpp` (despacho, defaults) | Leitura dirigida (entradas, defaults e agregação de razões) |

Consequência: as conclusões da seção 11 valem para **parafusos, soldas de filete, efeito de alavanca, forças localizadas e painel de alma do pilar, e para a ligação por chapa de extremidade**. Para o resto, o relatório apenas mapeia e declara "não auditado".

---

## 1. Resumo executivo

* **O núcleo de verificação de parafusos, soldas de filete, atrito, efeito de alavanca, forças localizadas (5.7) e painel de alma reproduz fielmente as equações da NBR 8800:2024** em todos os casos conferidos. Nenhum erro de formulação foi encontrado nesses blocos.
* **Os problemas encontrados estão, principalmente, em (a) o que não é verificado e (b) como os dados de entrada são tratados**, não nas equações que existem. Os achados principais:
  1. **Valores padrão silenciosos para a geometria do pilar** (260 × 146 × 12,7 × 7,6 mm): se a ligação não tem barra de apoio vinculada, as verificações são feitas contra um pilar fictício, sem erro (Alto; confirmado por leitura do código).
  2. **Efeito de alavanca (6.3.5) não verificado na mesa do pilar** na ligação por chapa de extremidade: a mesa do pilar também é uma "placa" tracionada por parafusos; só a chapa de extremidade tem a verificação 6.3.5.4 (Alto; suspeita fundamentada que precisa de validação — com uma fileira, a capacidade pela mecânica de T-stub é ~27% menor que a usada pelo programa).
  3. **Enrijecedores do pilar** na ligação de extremidade: a flambagem do enrijecedor e a solda são deixadas "a conferir no detalhamento" no próprio texto do resultado (Alto/Médio; funcionalidade ausente no fluxo, embora exista `StiffenerDesign`).
  4. **Rasgamento de bloco com `Cts = 1,0` fixo** (Médio; suspeita).
  5. **Funções que devolvem lista vazia** com dados zerados (painel de alma e forças localizadas): o resultado é "sem verificação", não "reprovado" (Médio).
* **A validação independente é limitada**: não há, em fonte aberta, um exemplo publicado completo de ligação por chapa de extremidade pela NBR 8800:2024. As comparações feitas são (i) reimplementação independente das equações a partir do texto da norma (concordância até a 6ª casa) e (ii) valores publicados de parafusos em uma dissertação de 2025. **A validação do conjunto da ligação contra um exemplo publicado completo continua pendente.**
* **Testes:** a maioria reproduz a própria equação implementada; há um único teste com exemplo de literatura, com tolerância de 15% e de outra edição da norma. Não há testes de rejeição de entradas inválidas.
* **Integração com a ferramenta momento-rotação:** usam o mesmo cadastro (`NodeConnection`) e as mesmas dimensões, mas **resistências diferentes** (NBR no dimensionamento; EN no comportamento mecânico). Na ligação padrão, os dois momentos resistentes diferem 27% (66,3 × 84,0 kN·m). A diferença é explicável, mas **não está documentada na interface do dimensionamento**.

**Conclusão em uma frase:** as equações existentes que foram auditadas são corretas; o módulo **não deve ser tratado como validado** para uso profissional sem tratamento dos itens 1 a 3 acima e sem validação contra exemplo publicado completo; usos de **pré-dimensionamento e conferência de ordem de grandeza** são justificáveis.

---

## 2. Mapa da arquitetura e do fluxo de cálculo

```
Interface (NodeConnectionsDialog, ConnectionDialog, MainWindowConnections)
   │ NodeConnection { tipo, grade, values (QMap chave->double), memberId, atEnd, nodeId }
   ▼
ConnectionService (app/services)
   ├─ defaults e campos por tipologia (makeDefault, fields)
   ├─ conversores  NodeConnection -> *Input   (toEndPlate, toShearTab, ... ; ConnectionCatalog para variantes)
   ├─ esforços por combinação (ForceCase: V, N, M, FSd do painel) e mínimo de 45 kN (6.1.5.2)
   └─ evaluate()  -> chama o núcleo e agrega  maxRatio = worstRatio(checks)
   ▼
Núcleo de cálculo (design/, sem dependência de interface)
   Connections.cpp      parafusos, atrito, filetes, CIR, alavanca
   SupportChecks.cpp    5.7.2..5.7.7 (forças localizadas, painel)
   EndPlateConnection / MomentConnection / WebShearConnection / ShearConnections / FlexEndPlate / CoverSplice / PinConnection / BasePlate /
   TrussConnections / TubularJoints / ColdFormedConnections / CompositeConnection / StiffenerDesign / SupportWeld
   ▼
Check { nome, cláusula, demanda, resistência, unidade, regra mínima/faixa, memória }   ratio() = demanda/resistência
```

* **Interface × núcleo:** separação limpa; o núcleo não depende de Qt nem do solver.
* **Integração com o solver:** o dimensionamento recebe esforços das combinações (`ForceCase`) calculados a partir dos resultados da análise (`MainWindowConnections`, `ConnectionForces.h`); não devolve rigidez ao solver (isso é feito pela ferramenta momento-rotação, seção 8).
* **Unidades:** N, N·mm, mm, MPa no núcleo; a conversão da unidade do modelo é feita no serviço. Verificado: as unidades das verificações de parafusos/soldas conferem.
* **Erros e dados inválidos:** não há camada de validação; os `Check` com resistência zero resultam em razão 1e9 (reprovado), mas algumas funções devolvem lista vazia (seção 9, item M2).

---

## 3. Inventário de tipologias

| Tipo (`NodeConnection`) | Descrição |
|---|---|
| `kShearTab` (0) | Chapa de cisalhamento soldada ao apoio, parafusada na alma da viga |
| `kDoubleAngle` (1) | Cantoneira dupla (viga-apoio) |
| `kGusset` (2) | Chapa de nó de treliça/contraventamento |
| `kSplice` (3) | Emenda de viga parafusada |
| `kBasePlate` (4) | Base de pilar com chumbadores (NBR 8800, 6.7) |
| `kTubular` (5) | Treliça tubular soldada (CIDECT) |
| `kColdFormed` (6) | Perfil formado a frio (NBR 14762, seção 10) |
| `kEndPlate` (7) | Ligação rígida viga-pilar por chapa de extremidade |
| Catálogo (id ≥ 100) | Famílias: tala de alma, ligação rígida viga-pilar (mesas soldadas / perfis T), emenda por cobrejunta, emenda de tubo, emenda por chapa de extremidade, nó de treliça, chapa de extremidade flexível, ligação mista (Anexo O), chapa tracionada com pino |

---

## 4. Matriz de verificações normativas (blocos auditados)

Legenda: **C** = equação conferida com a norma e correta; **S** = simplificação declarada; **—** = ausente.

### 4.1 Parafusos (`Connections.cpp`)

| Fenômeno | Equação implementada | Cláusula | Estado |
|---|---|---|---|
| Tração | `Ft,Rd = Abe·fub/γa2`, `Abe = 0,75·Ab` | 6.3.2.2 e 6.3.3.1 | **C** |
| Cisalhamento | `Fv,Rd = 0,45·Ab·fub/γa2` (rosca no plano) ou `0,56·Ab·fub/γa2` (fora da rosca, alta resistência), por plano | 6.3.3.2 | **C** |
| Pressão de contato e rasgamento | `Fc,Rd = min(1,2·lf·t·fu ; 2,4·db·t·fu)/γa2` (ou 1,5/3,0 sem limitação de deformação) | 6.3.3.3 | **C**; **S**: `lf` conservador aplicado a todos os parafusos |
| Tração + cisalhamento | `(Ft,Sd/Ft,Rd)² + (Fv,Sd/Fv,Rd)² ≤ 1,0` | 6.3.3.4 (verificado na imagem da página 85) | **C**; a alternativa da Tabela 12 não é oferecida |
| Atrito, ELS (furo-padrão) | `Ff,Rk = 0,80·μ·Ce·FTb·ns·(1 − Ft,Sk/(0,80·FTb))`, 70% simplificado | 6.3.4.4 | **C** |
| Atrito, ELU (furo alargado) | `Ff,Rd = 1,13·μ·Ce·FTb·ns·(1 − Ft,Sd/(1,13·FTb))/γe`, γe = 1,20 | 6.3.4.3, Tabela 13 | **C**; **—**: γe = 1,40 (furo muito alongado) e combinação excepcional; μ = 0,30 de classe C (galvanizado com escova) |
| Efeito de alavanca (placa flexível) | `t ≥ √[4(b−0,5db)·Ft,0,Sd·γa1 / (p·fu·(1+δα))]`, β, α, δ conforme 6.3.5.2 | 6.3.5.2 a 6.3.5.4 | **C**; **S**: `p` é um dado de entrada, não calculado por `pext/pint` da norma (`checkEndPlate` usa `min(largura da chapa/2 ; gabarito)`) |
| Pega longa e ligação longa | redução de 1% por 1,5 mm além de 5 db; ×1,20 acima de 950 mm | 6.3.7, 6.3.8 | **C** |
| Distribuição de esforços entre parafusos | elástica (cortante direto + torção em torno do centroide) | — (norma não prescreve) | **S**, usual |
| Ruptura por seção líquida e rasgamento de bloco | só nas chapas de ligação (cisalhamento, cantoneira) | 6.5.5, 6.5.6 | ver 4.4 |
| Espaçamentos | `≥ 3·db`; máximo `min(24·t ; 300)`; distância a borda: Tabela 16; máxima `min(12·t ; 150)` | 6.3.9 a 6.3.12 | **C** |

Propriedades dos parafusos (Tabela A.3): ASTM A325 (635/830 MPa), A490 (895/1040), ISO 8.8 (640/800), ISO 10.9 (900/1000), 4.6 (235/400), A307 (415): **conferidas**. `FTb` da Tabela 19 (M16: 91 kN, M20: 142 kN, M22: 176 kN): **conferido**.

### 4.2 Soldas de filete (`Connections.cpp`)

| Fenômeno | Equação | Cláusula | Estado |
|---|---|---|---|
| Metal da solda | `Fw,Rd = 0,6·Aw·fw·(1 + 0,5·sen^1,5 θ)/γw2`, `Aw = 0,707·a·L` | 6.2.5.2-a, Tabela 9 | **C** |
| Grupo longitudinal + transversal | `max(Fwl+Fwt ; 0,85·Fwl+1,5·Fwt)` | 6.2.5.2-c | **C** |
| Excentricidade, centro instantâneo de rotação | Δu = 1,087(θ+6)^−0,65·w ≤ 0,17w; Δm = 0,209(θ+2)^−0,32·w; f(p) = [p(1,9−0,9p)]^0,3 | 6.2.5.2-b | **C** nas equações; a resolução numérica foi conferida só no caso limite (seção 6) |
| Metal-base | `min(0,6·A·fy/γa1 ; 0,6·A·fu/γa2)` | 6.5 | **C** |
| Perna mínima/máxima, comprimento mínimo | Tabela 11 (3/5/6/8 mm), `t−1,5`, `max(4a ; 40)` | 6.2.6.2 | **C** |
| Solda de penetração total | resistência da mesa da viga | Tabela 9 | **C** (em `MomentConnection`, por amostragem) |
| Interação de solicitações em grupo de soldas | método elástico (chapa de cisalhamento) | — | **S** |

### 4.3 Forças localizadas e painel de alma (`SupportChecks.cpp`)

| Verificação | Equação | Cláusula | Estado |
|---|---|---|---|
| Flexão local da mesa | `Rd = 6,25·tf²·fy/γa1` (× 0,5 a menos de 10 tf da extremidade; dispensada se a atuação < 0,15 bf) | 5.7.2 | **C** |
| Escoamento local da alma | `1,10(5k+n)·fy·tw/γa1` (ou `2,5k+n` na extremidade) | 5.7.3 | **C** |
| Enrugamento | `0,66·tw²[1+3(n/d)(tw/tf)^1,5]·√(E·fy·tf/tw)/γa1`, e as variantes de extremidade | 5.7.4 | **C** |
| Flambagem lateral da alma | `Cr·tw³·tf/(γa1·h²)·0,37·[(h/tw)/(ℓ/bf)]³`, Cr = 16E (conservador) | 5.7.5 | **C**; **S**: só o caso "rotação da mesa não impedida" |
| Instabilidade por compressão | `24·tw³·√(E·fy)/(γa1·h)`, × 0,5 na extremidade | 5.7.6 | **C** |
| Painel de alma | `VRd` de 5.4.3 com `Aw = dc·tw`, `FRd = VRd` ou `VRd(1,4−NSd/Np)`; chapa de reforço e enrijecedores entram em `tw` e `kv` | 5.7.7.1 | **C** |
| Flexão transversal da alma (ligação na alma) | linhas de plastificação, "estimativa" | — (não é da NBR) | **S** |

### 4.4 Chapa de cisalhamento (`ShearConnections.cpp`)

| Verificação | Cláusula | Estado |
|---|---|---|
| Parafusos (cortante + excentricidade `V·a`, torção elástica), contato na chapa e na alma | 6.3 | **C** (via `checkBolted`) |
| Chapa: escoamento e ruptura por cisalhamento | 6.5.5 | **C** |
| Rasgamento de bloco | 6.5.6 | **C** nas equações; **Suspeita:** `Cts = 1,0` fixo (a norma exige 0,5 quando a tensão de tração não é uniforme) |
| Chapa: flexão da excentricidade | `MRd = t·H²·fy/(4γa1)` | **C** |
| Solda de filete (método elástico) | 6.2.5 | **C**; **S** (método elástico, conservador) |

### 4.5 Ligação por chapa de extremidade (`EndPlateConnection.cpp`)

| Verificação | Estado |
|---|---|
| Binário das mesas `Ft,Fc = M/(d−tf) ± N/2`; tração dividida **igualmente** entre as fileiras | **S** (seção 9, M1) |
| Parafusos tracionados `0,75·Ab·fub/γa2` | **C** |
| Efeito de alavanca na **chapa de extremidade** (6.3.5.4) | **C** |
| Efeito de alavanca na **mesa do pilar** | **— AUSENTE** (seção 9, A2) |
| Parafusos de cisalhamento na zona comprimida e contato | **C** |
| Soldas das mesas e da alma da viga à chapa | **C** |
| Mesa/alma do pilar: 5.7.2 a 5.7.5, 5.7.6, painel 5.7.7.1 | **C** (equações), **S** na aplicabilidade de 5.7.2 a parafusos |
| Com enrijecedores horizontais: 5.7.9 | **parcial:** resistência à tração do enrijecedor; **— ausente:** flambagem do enrijecedor e solda (o próprio resultado diz "a conferir") |
| Modos de ruptura de T-stub (EN, modos 1 a 3), alma da viga tracionada | **— ausentes** (não pertencem à NBR) |

---

## 5. Auditoria por tipologia: configuração, componentes e modos de falha

Apenas as tipologias auditadas em profundidade são detalhadas.

### 5.1 Chapa de extremidade (`kEndPlate`)

* **Geometria no modelo:** chapa soldada à viga, parafusada na mesa do pilar; **todas as fileiras tracionadas ficam na extensão** acima da mesa superior da viga (campo `firstRow` + `pitch`); fileiras de cisalhamento na zona comprimida.
* **Componentes considerados:** parafusos (tração, cisalhamento, contato), chapa (alavanca), soldas, mesa e alma do pilar (forças localizadas), painel de alma, enrijecedores (parcial).
* **Modos de falha calculados:** ruptura do parafuso, escoamento da chapa por flexão com alavanca, solda, flexão local da mesa do pilar (5.7.2), escoamento/enrugamento da alma, painel.
* **Modos não contemplados:** alavanca na mesa do pilar; tração da alma da viga junto à chapa; flambagem do enrijecedor; interação entre fileiras (grupo).
* **Consistência mecânica:** o equilíbrio global está garantido (`Ft = M/z`, `Fc = M/z`); a **distribuição** entre fileiras é uniforme e `z = d − tf` independe da posição das fileiras. Para fileiras na extensão, o braço real (`h = d − tf/2 + x`) é maior que `z`: a força total é superestimada (conservador) e a distribuição uniforme subestima a fileira externa em ~9% (compensada). Para fileiras entre as mesas (não representadas), o efeito seria contrário.

### 5.2 Chapa de cisalhamento (`kShearTab`)

Equilíbrio: cortante `V` e momento de excentricidade `V·a` nos parafusos (torção elástica) e na solda (método elástico). Modos: cisalhamento dos parafusos, contato, escoamento/ruptura/rasgamento da chapa, flexão da chapa, rasgamento da alma da viga, solda e metal-base. Consistente.

---

## 6. Validação numérica independente

### 6.1 Método

Um executável de auditoria (fora do repositório) chamou as funções existentes com entradas conhecidas; cada resultado foi comparado com o **cálculo manual a partir do texto da norma**. Registra-se que a reimplementação foi feita por quem escreveu este relatório; ela é independente do código do Structura, mas **não é uma validação por terceiros**.

### 6.2 Resultados (entrada → referência → Structura)

| Caso | Entrada | Referência (manual, NBR 8800:2024) | Structura | Erro |
|---|---|---|---|---|
| Fv,Rd, rosca no plano | M20 A325, fub = 830 | `0,45·314,159·830/1,35 = 86,917 kN` | 86,917 kN | < 1e-6 |
| Fv,Rd, fora da rosca | idem | `0,56·…/1,35 = 108,164 kN` | 108,164 kN | < 1e-6 |
| Ft,Rd | idem | `0,75·314,159·830/1,35 = 144,862 kN` | 144,862 kN | < 1e-6 |
| Fc,Rd | t = 12,5; fu = 400; le = 40 → lf = 29 | `1,2·29·12,5·400/1,35 = 128,889 kN` | 128,889 kN | < 1e-6 |
| Interação | Ft,Sd = 30; Fv,Sd = 50 | `(30/144,86)² + (50/86,92)² = 0,3738` | 0,3738 | < 1e-4 |
| Filete θ = 0 | a = 6; L = 200; fw = 485 | `0,6·0,707·6·200·485/1,35 = 182,877 kN` | 182,877 kN | < 1e-6 |
| Filete θ = 90° | idem | `182,877 × 1,5 = 274,316 kN` | 274,316 kN | < 1e-6 |
| Metal-base | t = 10; L = 200 | `0,6·2000·min(250/1,1 ; 400/1,35) = 272,727 kN` | 272,727 kN | < 1e-6 |
| Alavanca (placa flexível) | b = 40; a = 35; p = 70; df = 22; db = 20; Ft,0,Sd = 70; Ft,Rd = 144,86; fu = 400 | β = 1,604 → α = 1; δ = 0,686; `t = √[4·30·70000·1,1/(70·400·1,686)] = 13,99 mm` | 13,99 mm | < 1e-4 |
| Atrito ELS | M20 A325; 4 parafusos; classe A; V = 160; Nt = 40 | `0,8·0,3·142·(1 − 7/113,6) = 31,98 kN` | 31,98 kN | < 1e-4 |
| Atrito ELU | idem, furo alargado | `1,13·0,3·142·(1 − 10/160,5)/1,2 = 37,62 kN` | 37,62 kN | < 1e-3 |
| 5.7.2 | HEB200-like: tf = 15; fy = 250 | `6,25·15²·250/1,1 = 319,60 kN` | 319,60 kN | < 1e-6 |
| 5.7.3 | k = 33; n = 100; tw = 9 | `1,10·(165+100)·250·9/1,1 = 596,25 kN` | 596,25 kN | < 1e-6 |
| 5.7.4 | n/d = 0,5; tw/tf = 0,6 | `0,66·81·1,697·9128,7/1,1 = 752,94 kN` | 752,94 kN | < 1e-4 |
| 5.7.6 | h = 134 | `24·729·7071,07/(1,1·134) = 839,32 kN` | 839,32 kN | < 1e-4 |
| Painel 5.7.7.1 | d = 200; tw = 9; h = 134 | `Vpl = 0,6·200·9·250 = 270 kN → 245,45 kN` | 245,45 kN | < 1e-6 |
| Painel com N > 0,4 Np | N = 1500 kN | `245,45 × (1,4 − 1500/1952) = 155,0 kN` | 155,02 kN | < 1e-3 |
| CIR, caso limite | linha transversal de 200 mm, perna 8, carga no centroide | `0,6·485/1,35·5,657·200·1,5 = 365,83 kN` | 365,83 kN | < 1e-4 |

**Fontes publicadas:**

* Dissertação UFJF (2025), "ligação viga-pilar rígida, NBR 8800:2024" (parafuso M16 A325, fub = 825 MPa): `Fv,Rd = 55,292 kN` e `Ft,Rd = 92,153 kN`. Aplicando as fórmulas do Structura com fub = 825: `0,45·201,06·825/1,35 = 55,29 kN` e `0,75·201,06·825/1,35 = 92,15 kN` — **coincidem**. (O Structura usa fub = 830 MPa da Tabela A.3 de 2024; a dissertação usa 825.) O restante do exemplo da dissertação (linha neutra elástica, posições h1, h2, h3) usa um método de distribuição diferente e não foi comparado.
* Trabalho acadêmico UFMG "Aço II": **descartado como referência** — usa fórmulas da NBR 8800:2008 (0,40 em vez de 0,45; fator 0,67 na tração) e contém inconsistências.
* O teste existente `literatureBoltedConnectionExample` (seção 9.9.1 de um livro, edição de 2008) compara com tolerância de 15%: não serve como validação independente da edição de 2024.

**Validação não realizada (pendente):**

* Resistência do **conjunto** de uma ligação por chapa de extremidade contra um exemplo publicado completo pela NBR 8800:2024: **não encontrei fonte aberta com todos os dados**.
* Centro instantâneo de rotação com carga excêntrica contra valores tabelados (Tabela 8-4 do AISC): **não disponível**; o caso excêntrico do Structura (carga a 150 mm do centroide de uma linha de 200 mm: 102,9 kN) é fisicamente plausível (>2× o limite elástico de ~53 kN), mas **não está validado**.
* Soldas de penetração, T-stub parafusado (`MomentConnection`), chapa de extremidade flexível, cantoneiras: **sem validação independente**.

---

## 7. Auditoria dos testes existentes

| Aspecto | Achado |
|---|---|
| O que é testado | Equações isoladas de parafusos e soldas (`connectionsBoltsAndWelds`), atrito, alavanca com pega longa, ligação por chapa de extremidade (`endPlateMomentConnectionMatchesHandCalculation`), painel de alma, enrijecedores (5.7.9), integração do nó (`nodeEquilibriumPanelShear`), catálogo (todas as variantes avaliam e geram sólidos) |
| Testes que apenas reproduzem a equação implementada | **A maioria.** O "cálculo à mão" é a mesma fórmula reescrita no teste (verifica transcrição, não correção) |
| Casos com resultado externo conhecido | Poucos: coeficientes de Levy (Timoshenko), um exemplo de livro (tolerância 15%, edição 2008) |
| Casos-limite | Escassos (pega longa, ligação longa, γ excepcional não) |
| Rejeição de entradas inválidas | **Não há.** O teste da chapa de extremidade valida `Ft = M/290` (distribuição uniforme e `z = d − tf`), isto é, **fixa como correta** a simplificação do item M1 |
| Conversão de unidades | Coberta indiretamente; não há teste de invariância mm × cm × m do serviço |
| Lacunas | alavanca na mesa do pilar; 5.7.2 com parafusos; Cts; verificação do pilar sem barra de apoio (defaults); lista vazia de verificações |

**Testes propostos (não implementados):**

1. Comparação do `Mj,Rd` da chapa de extremidade com exemplos publicados completos (EN: SCIA/SCI P398; NBR: quando houver fonte) para validar o conjunto.
2. Teste de rejeição: `NodeConnection` sem barra de apoio **deve** produzir erro/aviso explícito (não valores padrão).
3. Teste de verificações vazias: dados zerados do pilar devem reprovar, não omitir.
4. Alavanca na mesa do pilar contra a mecânica de T-stub, para uma fileira e para duas.
5. Rasgamento de bloco com `Cts = 0,5` (viga recortada com mais de uma linha de parafusos).
6. Invariância de unidades (mm × m) e de ordem de parafusos.
7. Centro instantâneo de rotação contra a tabela publicada do AISC (casos θ = 0°, 90°, k = 0, a variando).
8. Testes de regressão **numéricos fixos** (valores calculados por terceiros) em vez de reescrever a fórmula.

---

## 8. Integração com a ferramenta momento-rotação

| Pergunta | Resposta |
|---|---|
| Usam os mesmos parâmetros geométricos? | **Sim**: ambos partem de `NodeConnection` → `ConnectionService::endPlateInput` / `ConnectionCatalog::momentInput` |
| Formulações mecânicas compatíveis? | **Não são as mesmas.** Dimensionamento: NBR 8800 (forças localizadas 5.7, alavanca 6.3.5, `Ft = M/z`, distribuição uniforme). Comportamento mecânico: EN 1993-1-8 (T-stubs, `h_r`, distribuição por rigidez, rigidez `ki`) |
| Componentes com definições consistentes? | **Divergem**: o dimensionamento usa `p`, `b` (alavanca) e 5.7.2; o modelo mecânico usa `leff`, `m`, `n` (T-stubs). A posição das fileiras é a mesma (`firstRow`, `pitch`), o braço não (`z = d − tf` × `h_r`) |
| Rigidez e resistência com os mesmos parâmetros? | Dentro da ferramenta momento-rotação, sim (um só modelo físico). Entre as duas ferramentas, não |
| Uma ligação dimensionada pode ser recalculada na ferramenta? | **Sim** (tipologias chapa de extremidade e mesas soldadas) |
| Os resultados são comparáveis? | Com ressalvas: na ligação padrão de exemplo, `Mj,Rd` = 83,97 kN·m (EN, componentes) × 66,28 kN·m (NBR, governada por "flexão local da mesa do pilar" 5.7.2). A diferença de 27% decorre de: flexão local da mesa (NBR) × T-stub por fileira (EN); braço `z` × `h_r`; distribuição. **A interface do dimensionamento não informa isso.** Como a ferramenta momento-rotação já sinaliza quando a verificação da NBR é menor que a da curva (`nbrBelowCurve`), a inconsistência está **tratada na direção da segurança**, mas apenas na ferramenta momento-rotação |

Incompatibilidades a tratar antes de qualquer integração: (i) o dimensionamento não tem T-stub da mesa do pilar nem da chapa por fileira (item A2); (ii) o braço e a distribuição são diferentes; (iii) fatores parciais diferentes (a ferramenta momento-rotação usa γ da NBR nas fórmulas da EN).

---

## 9. Problemas encontrados, classificados

Classificação de **severidade** (conforme solicitado) e de **natureza**: **EC** erro confirmado; **SV** suspeita que precisa de validação; **LC** limitação conhecida; **FA** funcionalidade ausente.

### Crítico

Nenhum **erro de formulação** classificado como crítico nos blocos auditados. O item A1 abaixo tem potencial crítico (aprovação sobre geometria fictícia); está classificado como Alto por depender de a interface sinalizar ou não a situação (não verificado em execução).

### Alto

| Id | Natureza | Achado | Evidência |
|---|---|---|---|
| **A1** | **EC** (comportamento confirmado por leitura) | **Geometria do pilar substituída silenciosamente por valores padrão.** `toEndPlate` usa `positive("supD", 260)`, `positive("supBf", 146)`, `positive("supTf", 12,7)`, `positive("supTw", 7,6)`, `supR` = 10: se os campos do apoio são zero (ligação sem barra de apoio vinculada), a verificação é feita contra um perfil fictício, sem erro. Aprovação indevida possível se o pilar real for mais fraco | `ConnectionService.cpp`, `toEndPlate`; `makeDefault` grava `supD = supBf = supTf = supTw = 0`; `chooseSupport` admite "fica sem barra de apoio (preencha o apoio à mão)" |
| **A2** | **SV fundamentada** | **Efeito de alavanca (6.3.5) não verificado na mesa do pilar.** A mesa do pilar é a segunda "placa" do T tracionado pelos parafusos; só a chapa tem `checkPrying`. O programa usa 5.7.2 (`6,25·tf²·fy/γa1`, equação de força localizada) contra a tração **total** `T`. Para uma fileira de dois parafusos M20 A325 em HEB200 (tf = 15, g = 90, fy = 250), a mecânica de T-stub (modo 2, com alavanca) dá ~232 kN por fileira, contra 319,6 kN de 5.7.2 e 289,7 kN de dois parafusos: o programa aceitaria forças até ~290 kN, ~25% acima da capacidade por T-stub. Com duas fileiras o programa é conservador (compara `T` total com uma única fórmula) | Cálculo manual (EN, apenas indicativo; a NBR não define T-stub, mas 6.3.5.1 exige considerar a alavanca "produzida pelas deformações das partes ligadas") |
| **A3** | **FA** | **Enrijecedores do pilar:** só a resistência à tração/compressão do enrijecedor (`A·fy/γa1`) é verificada; flambagem do enrijecedor (5.7.9.4) e solda enrijecedor-mesa/alma ficam "a conferir no detalhamento" (texto do resultado). Existe `StiffenerDesign` com essas verificações, **sem uso no fluxo da ligação** (nenhuma chamada a `Stiffeners::` em `EndPlateConnection`, `MomentConnection` ou nos serviços) | grep; `EndPlateConnection.cpp` linhas ~170-185 |

### Médio

| Id | Natureza | Achado |
|---|---|---|
| **M1** | **LC** | **Distribuição de forças entre fileiras e braço:** tração dividida igualmente e `z = d − tf` (6.1.1.3-d exige distribuição compatível com a rigidez). Para fileiras na extensão é conservador no total, mas **fixado por teste** como a única referência |
| **M2** | **EC** | **Verificações omitidas com dados zerados:** `checkColumnPanel` (`d ≤ 0` ou `tw ≤ 0`) e `checkSupportMember` (`face = None`, `tf`, `tw`, `d ≤ 0`) devolvem **lista vazia**: não há reprovação, a verificação some (`maxRatio` não é afetado). Executado: `tw = 0` → 0 verificações |
| **M3** | **SV** | **Rasgamento de bloco com `Cts = 1,0` fixo** (6.5.6 pede 0,5 quando a tração na área líquida não é uniforme, caso típico de viga recortada com mais de uma linha de parafusos) |
| **M4** | **LC** | **5.7.2 usado para forças introduzidas por parafusos:** a equação vale para força localizada de largura `lengthTransverse`; a dispensa por "< 0,15 bf" usa a largura da **mesa da viga**, não o espalhamento das fileiras (a verificação não é dispensada, a favor da segurança) |
| **M5** | **LC** | **Largura tributária `p` do efeito de alavanca:** a norma define `pext = min(eext ; 1,75b) + min(0,5·eint ; 1,75b)`; `checkEndPlate` usa `min(largura da chapa/2 ; gabarito)`: pode diferir da definição da norma e alterar a espessura exigida (diferença não quantificada) |
| **M6** | **LC** | **Atrito:** faltam γe = 1,40 (furo muito alongado), combinação excepcional e μ da classe C |
| **M7** | **LC** | **Tabela 12** (alternativa à interação quadrática) não oferecida; a interação quadrática é correta |
| **M8** | **LC** | **Flambagem lateral da alma (5.7.5):** só a variante "rotação da mesa não impedida", com `Cr = 16E` (conservador) |
| **M9** | **FA** | **Sem camada de validação de entradas** (negativos, NaN, relações geométricas impossíveis): depende de cada função |

### Baixo

| Id | Achado |
|---|---|
| B1 | Pressão de contato usa `lf` mínimo (borda ou passo) para todos os parafusos: conservador |
| B2 | Semântica invertida nas regras de valor mínimo: em `minimumRule`, o campo `resistance` guarda o mínimo exigido e `demand` o valor adotado (funciona, mas confunde a leitura e os relatórios) |
| B3 | A memória de cálculo da interação cita 6.3.3.4 mas não oferece a Tabela 12 |
| B4 | A documentação do módulo não registra a diferença NBR × EN entre dimensionamento e momento-rotação |

### Funcionalidades ausentes (relevantes)

Alavanca na mesa do pilar; T-stub por fileira (EN, se for desejado como método complementar); efeito de grupo de fileiras; verificação de enrijecedores do pilar no fluxo; verificações de ligação com fileiras entre as mesas; ligação rígida com perfis T e chapa flexível sem validação independente; ruptura de bloco com `Cts` variável; tração + cisalhamento pela Tabela 12.

---

## 10. Plano recomendado de correção e validação

Em ordem de prioridade (nenhum item foi executado):

1. **A1:** impedir a verificação sem geometria do pilar: erro explícito ("selecione/defina a barra de apoio") em vez de valores padrão; exibir no resultado a seção de apoio usada.
2. **M2:** funções que hoje devolvem lista vazia devem devolver uma verificação reprovada ("dados insuficientes"), nunca omitir.
3. **A2:** implementar verificação da mesa do pilar à tração com alavanca (6.3.5 aplicado à mesa, com `b`, `a`, `p` do pilar) e/ou T-stub por fileira como verificação complementar rotulada como "EN 1993-1-8 (complementar)". Validar contra exemplos publicados (SCIA/SCI P398) **antes** de usar como critério.
4. **A3:** acoplar `StiffenerDesign` ao fluxo da ligação (flambagem e solda do enrijecedor).
5. **M3:** parametrizar `Cts` (0,5 quando houver mais de uma linha de parafusos em recorte).
6. **M5:** calcular `pext/pint` como na norma.
7. **Validação:** obter exemplos publicados completos (NBR 8800:2024 e EN) para chapa de extremidade, mesas soldadas e chapa de cisalhamento; substituir testes "reescrita da fórmula" por valores externos; criar a suíte de rejeição de entradas inválidas.
8. **Integração:** documentar na interface do dimensionamento a diferença NBR × EN e, quando as duas ferramentas forem aproximadas, adotar um único modelo de componentes com os dois critérios aplicados (sem substituir a NBR no dimensionamento).

---

## 11. Conclusão sobre a confiabilidade atual

* **Equações auditadas** (parafusos, soldas de filete, atrito, alavanca da chapa, 5.7.2 a 5.7.7, painel): **corretas** em relação ao texto da NBR 8800:2024 nos casos conferidos (concordância até a 6ª casa decimal com o cálculo manual independente). Isso **não** é uma validação do módulo: é a conferência de que as equações foram transcritas corretamente.
* **Lacunas que afetam a segurança:** A1 (geometria padrão silenciosa), A2 (alavanca da mesa do pilar), A3 (enrijecedores incompletos) e M2 (verificações omitidas). Enquanto essas lacunas não forem tratadas, **o módulo pode aprovar ligações por chapa de extremidade que não resistem** em casos específicos.
* **Validação independente do conjunto:** **não realizada**, por falta de exemplo publicado completo pela NBR 8800:2024.

**Usos tecnicamente justificáveis hoje:**

| Uso | Justificável? |
|---|---|
| Pré-dimensionamento, ordem de grandeza, comparação entre alternativas | **Sim** |
| Verificação de parafusos e soldas de filete com forças conhecidas (blocos auditados) | **Sim**, por revisão do projetista |
| Dimensionamento final de ligação rígida por chapa de extremidade com pilar de seção real | **Não**, sem tratar A1, A2, A3 e conferência independente |
| Uso das tipologias não auditadas (base, treliça tubular, formado a frio, mista, emendas, pino) | **Sem conclusão**: não foram auditadas |
| Documento de projeto sem revisão de engenheiro responsável | **Não** |

O módulo **não deve ser declarado validado**: compila, executa e produz resultados plausíveis, mas a conclusão acima se apoia nas verificações normativas conferidas, nas lacunas de verificação identificadas e na ausência de validação independente do conjunto.
