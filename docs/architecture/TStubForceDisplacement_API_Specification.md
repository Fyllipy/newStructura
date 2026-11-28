# Especificação da futura API `TStubForceDisplacement` (Fase C6.5)

**Documento de especificação. Nada foi programado.** Nenhum arquivo C++ foi criado ou alterado. Escopo: um módulo **isolado** (matemática pura, sem Qt, sem interface, sem NBR/EN, sem `Check`, sem relatórios, sem M–θ), a ser implementado **somente** depois da decisão do §10 do relatório da Fase C6. Sem validação profissional.

## 1. Princípios
1. **Modelo explícito:** toda chamada identifica a formulação (`ModelId`); não há seleção silenciosa nem substituição de um modelo por outro.
2. **Sem defaults:** dado ausente = estado de erro com a causa (nunca `0`, `NaN` propagado ou valor padrão).
3. **Origem rastreável:** cada entrada carrega `Origin` e `source`; resultados repetem as origens e as referências.
4. **Resistências do modelo de ensaio ≠ resistências de cálculo:** os resultados de Piluso são *estimativas últimas calibradas em ensaio* (`M_u≈3,0–3,7 M_y`, `F_u≈2,0–2,5` vezes a resistência plástica) e são rotuladas `ResultKind::ModelUltimate`; nunca alimentam verificações normativas.
5. **Independência:** o módulo não inclui `TStub.h` (resistência EN), `TStubStiffness.h` (rigidez EN), `Connections.h` nem `JointComponents.h`. Integrações futuras usam adaptadores fora do módulo.
6. **Unidades:** N, mm, MPa (como `TStub`); deformações **adimensionais** (fração, nunca %); ângulos em rad.

## 2. Identificação do modelo
```cpp
enum class ModelId { Piluso2001, Unavailable };          // futuros modelos entram aqui, nunca por substituição silenciosa
struct ModelInfo { ModelId id; std::string name; std::vector<std::string> references; std::string version; };
ModelInfo describe(ModelId);                              // para Piluso2001: [P1], [P2] com DOI; "mecanismos 1 e 2"
```
`Unavailable` existe para representar explicitamente "sem lei" (por exemplo, mecanismo 3).

## 3. Entradas
Cada campo: **nome, unidade, tipo, obrigatoriedade, validação, origem, justificativa.** `Origin = { Normative, ManufacturerDeclared, Test, Correlation, ModelAssumption }`; todo campo é `Quantity{double value; Origin origin; std::string source;}`; `Correlation` e `ModelAssumption` geram `Warning` obrigatório e `status = CalculableWithCaveats`.

### 3.1 Geometria (`PilusoGeometry`)
| Campo | Unidade | Obrig. | Validação | Justificativa |
|---|---|---|---|---|
| `tf` | mm | sim | finito, > 0 | espessura da mesa (`M_y`, `K`, `δ_p`) |
| `m` | mm | sim | > 0 | distância entre eixo do parafuso e a seção da rótula (já com `0,8r`) |
| `n` | mm | sim | > 0 | distância eixo–borda onde atua a alavanca; `λ=n/m` |
| `b` | mm | sim | > 0 | largura do T-stub |
| `dh` | mm | sim | > 0 | diâmetro da cabeça (`b_eff=min(dh+2m;b)`) |
| `dw` | mm | sim | > 0 | arruela/porca/cabeça; na EN é o diâmetro da arruela **ou** a largura entre vértices: o chamador informa e documenta qual |
| `r` | mm | sim (≥ 0) | ≥ 0 | raio de concordância (perfil laminado) ou garganta da solda; entra em `m'` |
| `th`, `tn` | mm | sim | > 0 | alturas de cabeça e porca (`L_b`) |
| `d_b` | mm | sim | > 0 | diâmetro nominal do parafuso |
| `grip` | mm | sim | > 0 | espessura total das chapas agarradas (no ensaio: `2t_f`) |
Se `b_eff` ou `m'` forem fornecidos por outra regra, `BeffRule`/`MPrimeRule` **explícitos** (`enum`), nunca implícitos.

### 3.2 Parafuso (`PilusoBolt`)
| Campo | Unidade | Obrig. | Validação | Observação |
|---|---|---|---|---|
| `fub` | N/mm² | sim | > 0 | resistência última do parafuso |
| `areaResistance` | mm² | sim | > 0 | área usada em `B_u = A f_ub`; **a Tabela 3 de [P2] reproduz-se com a área bruta** (ver reconstrução §9): o campo existe separado e documentado |
| `areaElongation` | mm² | sim | > 0 | área usada em `δ_b`; separada de `areaResistance` para não repetir a ambiguidade de [P2] |

### 3.3 Material (`PilusoMaterial`) — curva verdadeira multilinear
| Campo | Unidade | Obrig. | Validação | Origem típica |
|---|---|---|---|---|
| `E` | N/mm² | sim | > 0 | normativa ou ensaio |
| `fy`, `fu` | N/mm² | sim | `fu>fy>0` | normativa/declarada/ensaio (verdadeiras, não nominais: conversão `σ=σ_n(1+ε_n)` registrada em `source`) |
| `epsH`, `Eh` | −, N/mm² | sim | `ε_y<ε_h`; `Eh>0` | **ensaio de cupom** (ou `Correlation`/`ModelAssumption` com aviso) |
| `Eu` | N/mm² | sim | `0<Eu<Eh` | idem |
| `epsU` | − | sim | `ε_m<ε_u` (`ε_m=ε_h+(fu−fy)/Eh`) | ensaio de cupom (`ln(A0/Af)`); **não** equivale à deformação de alongamento da norma do aço |
Validação cruzada: `ε_y<ε_h<ε_m<ε_u`; falha → `InsufficientData`/`MissingMaterialProperties` (nunca ajusta).

### 3.4 Rigidez (`StiffnessSpec`)
`enum StiffnessSource { PilusoFaella1998Formula, UserProvided }`. A fórmula `K=0,5E b_eff t_f³/m'³` só entra como `PilusoFaella1998Formula` **após** a obtenção e leitura de Faella et al. (1998) (lacuna atual) e com aviso de *snug*. `UserProvided`: `Ki` (N/mm) com `Origin` obrigatória (p. ex. `Test`, ou `ModelAssumption`). **A rigidez EN (Fase B) não é aceita como fonte implícita**: se for usada, vem como `UserProvided` com `source="EN 1993-1-8 Tab. 6.11 / Fase B"` e aviso de mistura de formulações (§8).

### 3.5 Carregamento e modo
Só tração monotônica (`LoadingKind::MonotonicTension`; qualquer outro → `OutOfValidityDomain`). O mecanismo é **calculado** por `β_u`; o chamador pode pedir `expectedMechanism` apenas para verificação (diferença gera `Warning`, não substitui).

## 4. Saídas
```cpp
enum class ResultKind { ModelUltimate };                 // explicito: NAO e resistencia de calculo
struct CharacteristicPoint { std::string label /*y,h,m,u*/; double force /*N*/; double displacement /*mm*/; };
struct Sample { double displacement; double force; };    // monotono em ambos
struct Result {
  ModelInfo model; Status status; std::vector<Diagnostic> diagnostics;   // causas/avisos/limitacoes
  Mechanism mechanism;               // One, Two, Three(sem lei), Undetermined
  double betaU, betaULimit, psi;
  std::optional<double> initialStiffness; StiffnessSource stiffnessSource;
  std::array<CharacteristicPoint,4> points;  // so se calculavel
  std::vector<Sample> table;         // curva densa (>= 128 amostras; ver proposta final)
  ResultKind kind;                   // ModelUltimate
  double ultimateForce, ultimateDisplacement;
  bool postPeakModeled;              // sempre false no Piluso 2001
  std::vector<std::string> memory;   // equacao + valores substituidos, com a numeracao de [P1]/[P2]
};
```
`std::optional` para o que não existe; **nunca `0`** para "ausente". O resultado repete as `Origin` das entradas relevantes.

## 5. Estados explícitos
| `Status` | Quando |
|---|---|
| `InsufficientData` | campo obrigatório ausente (nomeado em `diagnostics`) |
| `InvalidGeometry` | `m≤0`, `dw` incompatível com `n`, `8mn−(m+n)dw ≤ 0`, etc. |
| `MissingMaterialProperties` | `epsH, Eh, Eu, epsU` ausentes ou fora de ordem |
| `MechanismUnsupported` | `β_u>2` (mecanismo 3): `mechanism=Three`, sem curva, `diagnostics` cita a lacuna e o caráter frágil |
| `OutOfValidityDomain` | carregamento não monotônico; parafuso pré-tensionado; ou fora do domínio testado de forma grosseira |
| `Calculable` | todos os dados de `Test`/`Normative`/`ManufacturerDeclared`, dentro do domínio |
| `CalculableWithCaveats` | há `Correlation`/`ModelAssumption`; geometria fora da faixa dos 12 ensaios (`m/t_f` 2,7–4,4; M20–M27; Fe430); `K` fornecido pelo usuário; critério de mecanismo conhecido por falhar (§ proposta) |

## 6. Operações (funções puras)
`Result evaluate(ModelId, const PilusoInput&)`; `double forceAt(const Result&, double displacement)` (interpolação linear sobre `table`; fora do intervalo → `std::nullopt`); `std::optional<double> displacementAt(const Result&, double force)`. Sem estado global, sem alocação oculta de defaults, `noexcept` para consultas.

## 7. Compatibilidade futura (sem acoplamento)
Consumidores (M–θ, séries/paralelos de componentes) usam só `Result` (`table`, `initialStiffness`, `points`, `kind`, `status`). Nenhuma dependência de Qt/UI/relatórios/aprovação. A rigidez e a lei ficam desacopladas: o consumidor decide se aceita `stiffnessSource`.

## 8. Rigidez EN × Piluso (como distinguir)
`stiffnessSource` é parte do resultado e do relatório; `memory` cita a equação; qualquer `UserProvided` baseado na EN traz `Warning{code="MIXED_FORMULATIONS"}` e `status=CalculableWithCaveats`. Não existe função que "combine" as duas.

## 9. Testes exigidos da futura implementação
Benchmarks publicados (exemplo trabalhado e Tabela 3 de [P2], 11/12 + o caso do espécime 8 com `mechanism=Two` e aviso); dados ausentes para **cada** campo; ordem das deformações; mecanismo 3 sem curva; monotonicidade; escala dimensional; ausência de `0` como ausência. Referência: `research/tstub_piluso/test_piluso.py`.
