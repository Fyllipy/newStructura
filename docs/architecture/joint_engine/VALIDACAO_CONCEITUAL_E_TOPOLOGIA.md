# Validação conceitual da arquitetura e estrutura de topologia (D1/D2 aprovadas)

Nada aqui é implementado. Objetivo: verificar se o domínio e os contratos representam **sete configurações de famílias distintas**, sem que nenhuma determine a arquitetura. Onde a arquitetura **não** consegue representar algo, está dito.

## 1. Estrutura de referência de topologia (D2)
Separada de `NodeConnection.values`; preserva compatibilidade: ausente ⇒ o adaptador **deriva** a topologia da ligação legada (mesmo id, mesmo `type`), sem gravar nada.
```cpp
// src/design/jointengine/JointTopology.h  (proposto; sem Qt)
struct PartRef   { int id; PartKind kind; /*Member, Plate, Angle, TStubEq, Stiffener, Diaphragm, Gusset, BasePlate, Tube, Concrete, Grout, Anchor*/
                   int memberId /*0 se nao e barra*/; EndSide end; std::string role; };
enum class LinkKind { Bolted, Welded, Contact, Bearing, Pin, Embedded, Rigid /*hipotese declarada*/ };
struct Link      { int id; LinkKind kind; std::vector<int> partIds /*N >= 2: hiperaresta*/;
                   std::vector<int> fastenerIds, weldIds; std::string note; };
struct LoadPath  { int id; std::vector<int> linkIds; std::string description; };   // caminhos de carga declarados (serie/paralelo vem daqui)
struct JointTopology { int configId /*= NodeConnection.id ou 0*/; std::vector<PartRef> parts; std::vector<Link> links; std::vector<LoadPath> paths; };
```
`Link` é hiperaresta (N partes): um gusset ligado a 3 barras é um `Link` ou três, conforme o adaptador. Série/paralelo **nunca** é inferido: vem de `LoadPath`. Persistência futura em seção própria (ex.: `[JOINT_TOPOLOGY]`, chaveada por `NodeConnection.id`), versionada; seção ausente = legado. Fica fora das fases iniciais (§ROADMAP, J1).

## 2. Sete casos conceituais
Legenda dos 7 itens: (1) geometria, (2) componentes, (3) interações, (4) transmissão, (5) modelos necessários, (6) resposta, (7) limitações.

### Caso 1 — Viga–pilar parafusada, chapa de extremidade estendida
1. `parts`: viga, pilar, chapa (`Plate`), 2×2 parafusos (`fasteners`), solda viga–chapa; `Link{Bolted: chapa↔mesa pilar}`, `Link{Welded: viga↔chapa}`. Fileiras = `Fastener` com cota `h`.
2. Por fileira: T-stub da mesa do pilar, T-stub da chapa, parafusos; globais: alma do pilar (compr./cis.), painel, mesa da viga.
3. Interações: distribuição por fileira (EN 6.2.7.2); contato da mesa comprimida.
4. Tração via fileiras → mesa do pilar; compressão pela mesa comprimida → alma; cortante aos parafusos inferiores/solda.
5. T-stub (EN ou Piluso, **um por análise**), parafuso, alma, painel.
6. `MomentRotation` (+ resistências por componente).
7. Cap. de rotação só com modelo F–δ completo; Piluso: mec. 3 indisponível.
**Representável:** sim.

### Caso 2 — Viga–pilar soldada (mesas + alma)
1. `Link{Welded}` mesas–pilar (penetração/filete), alma–pilar.
2. Alma do pilar tração/compressão/painel; mesa do pilar; solda como **mecanismo resistente** (norma) com rigidez `Rigid` **hipótese rotulada**.
3. Interação mesa↔alma do pilar.
4. Tração/compressão das mesas → alma do pilar; cortante pela alma da viga.
5. Alma do pilar EN; solda sem lei constitutiva.
6. `MomentRotation` com limitação "solda rígida por hipótese" **ou** `ResistanceOnly` se o usuário rejeitar a hipótese.
7. Ductilidade da solda não modelada; sem cap. de rotação.
**Representável:** sim; a limitação está *dentro* do resultado.

### Caso 3 — Híbrida: tala soldada à viga e parafusada ao pilar (ex.: 105–114)
1. `Link{Welded: tala↔viga}`, `Link{Bolted: tala↔pilar}`, `LoadPath` em série pela tala.
2. Solda, tala (flexão por excentricidade), parafusos, alma/mesa do pilar.
3. Compatibilidade em série (mesma força nos dois elos) **declarada** pelo `LoadPath`; **sem repartição presumida**.
4. Cortante: solda → tala → parafusos → pilar; momento da excentricidade.
5. Resistência de cada elo (norma) + rigidez parafuso (⚠) + solda (**sem lei**): o modelo de rigidez da hibridização **falta**.
6. `ResistanceOnly` (já N hoje) ou `ForceDisplacement` só se ambas as rigidezes tiverem fonte.
7. Sem rigidezes fundamentadas ⇒ `InsufficientData`, nunca partilha.
**Representável:** a *topologia* sim; a *resposta de rigidez* não até haver modelos (declarado).

### Caso 4 — Contraventamento com gusset
1. `parts`: gusset (`Gusset`, contorno), 1–2 diagonais, banzo, pilar/viga; `Link{Bolted|Welded: diagonal↔gusset}` (N hiperaresta), `Link{Welded: gusset↔apoio}`; excentricidade por `Eccentricity`.
2. Mecanismos: Whitmore (distribuição de tensões simplificada, domínio angular), escoamento/ruptura, block shear, flambagem (no plano e **fora do plano**), solda do gusset.
3. Interação das diagonais (tração × compressão) e com o banzo.
4. Força axial da diagonal → linhas de parafusos/solda → gusset (largura de Whitmore) → apoio (cisalhamento+tração+momento por excentricidade).
5. Hoje N em `TrussConnections`; M do gusset (rigidez/estabilidade) ⚠ sem fonte; MEF recomendável (fora do plano).
6. `GeneralizedVector` (forças nas barras × deslocamentos) + `ResistanceOnly`; **não** M–θ.
7. Segunda ordem e excentricidade exigem estudo; `NodeConnection` legado tem 1 barra ⇒ topologia nova necessária.
**Representável:** sim, e é o caso que **exige** `Link` N-ário e saída vetorial.

### Caso 5 — Nó de treliça com múltiplos elementos
1. Banzo + 2 diagonais + montante, gusset ou solda direta (`LinkKind::Welded` entre tubos/perfis).
2. Resistência por modo (CIDECT/NBR) ou gusset (caso 4).
3. Equilíbrio do nó (já há `NodeEquilibrium` no app) como restrição entre forças das barras.
4. Forças axiais das barras convergentes → nó.
5. N existente (`TrussNode`, K/N); M ⚠.
6. `GeneralizedVector`/`ResistanceOnly`.
7. Nó tubular: rigidez do nó ⚠ sem fonte.
**Representável:** sim.

### Caso 6 — Emenda (viga com chapas de extremidade, ou cobrejunta axial)
1. Duas peças `Member` ligadas por `Plate` e parafusos (ou cobrejuntas, `Bearing`+`Bolted`); `LoadPath` simétrico.
2. T-stubs/parafusos (chapa) ou parafusos em corte/seção líquida (cobrejunta); deslizamento.
3. Interação N–V–M entre metades.
4. Esforços da seção cortada atravessam a ligação (sem pilar).
5. EN (chapa de extremidade, via A1 adaptado — **a confirmar** que o modelo cobre emenda), deslizamento ⚠.
6. `MomentRotation` e/ou `ForceDisplacement` axial.
7. Cobrejunta: N–δ por deslizamento sem fonte lida.
**Representável:** sim (topologia sem "elemento principal").

### Caso 7 — Base com chumbadores
1. `parts`: pilar, placa de base (`BasePlate`), chumbadores (`Anchor`), graute (`Grout`), concreto (`Concrete`); `Link{Welded: pilar↔placa}`, `Link{Contact: placa↔graute↔concreto}`, `Link{Embedded: chumbador↔concreto}`.
2. Flexão da placa, chumbadores (tração/cis.), pressão de contato, arrancamento do concreto ⚠.
3. Contato unilateral (interface), interação N–V nos chumbadores, alavanca.
4. N/M do pilar → solda → placa → contato (compressão) e chumbadores (tração).
5. N em `BasePlate` (NBR 6.7); M de contato/ancoragem ⚠ **inexistente**; concreto e graute **não existem no programa**.
6. `GeneralizedVector` (N–M–V × deslocamentos/rotação da base) com `InterfaceModel`; rigidez rotacional da base (⚠ fonte).
7. Depende de modelos de contato e de ancoragem sem fonte lida.
**Representável:** a topologia sim; `IInterfaceModel` é exigido (já previsto); a análise é **declarada indisponível** até haver modelo.

## 3. Resultado da verificação
| Verificação | Resultado |
|---|---|
| Nenhum caso depende de "viga/pilar principal/secundário" | atendido (`Member` é papel; gusset e base não têm "principal") |
| Série/paralelo inferido | não ocorre; `LoadPath` explícito |
| Saída não M–θ | casos 4, 5, 7 exigem `GeneralizedVector`/`ResistanceOnly` — previstos |
| Interface (contato) | caso 7 e mesas comprimidas — previsto |
| Hiperaresta | caso 4/5 — exige `Link` N-ário (acrescentado) |
| Ajustes encontrados | (a) `Link` N-ário; (b) `LoadPath`; (c) `PartKind` com `Concrete/Grout/Anchor`; (d) `ResistanceOnly` como resposta de 1ª classe; (e) **estado "topologia representável, resposta indisponível"** distinto de "não suportado" |
| Lacuna não resolvida pela arquitetura | rigidez de solda, de cantoneira, de nó tubular, de chumbador/contato: dependem de **fontes** (não de arquitetura) |
