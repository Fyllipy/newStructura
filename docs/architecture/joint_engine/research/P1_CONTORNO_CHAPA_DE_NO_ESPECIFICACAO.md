# P1 — Especificação do contorno da chapa de nó (proposta; nada implementado)

Nenhum domínio legado foi alterado; `gussetWidth`/`freeLength` não são migrados automaticamente. Contratos abaixo seguem o estilo de J0/J1 (sem Qt; `Quantity` com origem; ausência explícita).

## 1. Por que é necessário
[CB07] (lido, pp. 255–262) mostra que as verificações de Whitmore e de pseudo-coluna dependem da **geometria**: `L_w`, distâncias `y_b, y_c` (e `y_bf, y_cf`) às bordas e às interfaces, comprimentos `L1, L2, L3`, ângulos das interfaces (`θ_B, θ_b, θ_c, θ_bb, θ_cc`), recortes e bordas não ortogonais. O legado guarda só `gussetWidth` e `freeLength` ([`GussetInput`]), insuficientes para aplicar essas fórmulas ou para a largura ponderada de Thornton & Kane.

## 2. Contrato proposto
```cpp
// src/design/jointengine/GussetOutline.h (futuro)
struct Point2 { double x, y; };                         // coordenadas LOCAIS da chapa (mm)
struct OutlineRing { std::vector<Point2> vertices; };   // polilinha FECHADA; sentido anti-horario; sem auto-interseccao

struct LocalFrame {                                     // orientacao e sistema de coordenadas
  Quantity originInJointFrame[3];                       // origem da chapa no referencial da ligacao
  Quantity axisX[3], axisN[3];                          // eixo x local e normal da chapa (unitarios)
  std::string jointFrameRef;                            // identifica o referencial (ex.: do StructuralJoint)
};

struct Cutout { OutlineRing ring; std::string kind; /*Hole | Notch | Cope*/ std::string note; };

struct EdgeRef {                                        // aresta do contorno externo e seu papel
  int ringEdgeIndex;                                    // aresta i -> i+1
  enum class Role { Free, SupportInterface, BraceSide, Unspecified } role;
  int linkId;                                           // uniao da topologia (J1) que a conecta; 0 se livre
};

struct BraceAttachment {                                // referencia de conexao das barras
  int partId;                                           // PartRef.id (Member) na JointTopology (J1)
  int linkId;                                           // Link (parafusado/soldado) que liga a barra a chapa
  Point2 fastenerGroupFirstRow, fastenerGroupLastRow;   // extremos do grupo ao longo da linha de acao (local)
  Quantity workingPointOffset;                          // excentricidade ate o ponto de trabalho
};

struct GussetOutline {
  int partId;                                           // PartRef.id (PartKind::Gusset) na JointTopology
  OutlineRing outer;                                    // poligono externo
  std::vector<Cutout> cutouts;                          // recortes e furos
  Quantity thickness;                                   // espessura INDEPENDENTE (nao deduzida de outro dado)
  MaterialSpec material;                                // fy, fu com origem
  LocalFrame frame;
  std::vector<EdgeRef> edges;
  std::vector<BraceAttachment> braces;
  Origin dataOrigin;  std::string source;               // proveniencia do desenho (usuario, importacao, derivado)
  std::vector<DataGap> gaps;                            // o que falta
};
std::vector<std::string> validateOutline(const GussetOutline&);   // fechado, nao auto-intersectante, furos dentro, ids da topologia existentes
```
Regras: sem defaults silenciosos (contorno ausente = lacuna, não retângulo presumido); espessura é dado próprio; recortes/furos como anéis; referências a barras usam **ids da `JointTopology`** (J1), e `Link` N-ário representa a união da chapa com várias barras.

## 3. Compatibilidade com a topologia N-ária (J1)
- A chapa é uma `PartRef{kind=Gusset}`; cada barra conectada é outra `PartRef{kind=Member}`; cada união parafusada/soldada é um `Link` (hiperaresta quando >2 partes). `BraceAttachment.partId/linkId` apontam para elas ⇒ nenhuma duplicação da topologia, e `validateTopology` continua válida.
- **Fingerprint (CFP):** o contorno entra na configuração física (polígono ordenado canonicamente a partir do menor vértice, números em 17 dígitos), de modo que alterar um vértice invalida análise e classificação.
- **Legado:** `NodeConnection` não muda; o adaptador pode **derivar um contorno parcial** (retângulo `gussetWidth` × comprimento) **somente rotulado** `Origin::Derived` + gap `OUTLINE_APPROXIMATED_FROM_LEGACY_FIELDS`, **nunca** como dado do usuário, e só mediante autorização de uma fase própria.
- **Persistência:** fora de escopo (fase posterior, com autorização).

## 4. O que a geometria habilitaria (futuro, sem implementar)
Fórmulas de [CB07] para `y_b, y_c, L1–L3` (tabela 1, eqs. 5–26) e largura de Whitmore truncada por bordas; a largura ponderada de Thornton & Kane só com `t F_y` relativos dos elementos do apoio (dados do apoio). Fora do plano e distribuição entre barras **não** são habilitadas por geometria.

## 5. Dados que o usuário/modelo precisaria fornecer
Contorno, recortes, espessura, material da chapa, referencial, ligação de cada barra, posição do grupo de fixadores e ponto de trabalho, e a proveniência de cada um.
