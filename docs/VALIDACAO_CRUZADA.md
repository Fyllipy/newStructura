# Validação cruzada — casos de referência

Casos para conferir o Structura contra outro programa (por exemplo, o CYPE 3D) **com os mesmos dados de entrada**. Os valores abaixo são os calculados hoje pelo Structura e estão protegidos por
`TestDesign::crossValidationReferenceCases` (tolerância de 0,1 %): se uma mudança de cálculo alterar algum deles, o teste reprova e este documento deve ser atualizado conscientemente.

Dados comuns: aço ASTM A572 Gr 50 (`fy` = 345 MPa, `fu` = 450 MPa, `E` = 200 GPa, `G` = 77 GPa) para perfis laminados e soldados; aço ZAR 345 (`fy` = 345 MPa, `fu` = 430 MPa) para os formados a frio;
`γa1` = 1,10 (NBR 8800:2024) e, na NBR 14762:2010, `γ` = 1,20 (compressão) e 1,10 (flexão). Barras biarticuladas, `K` = 1, sem furos (`An/Ag` = 1), sem B1/B2, sem imperfeições.

## Como comparar

1. No outro programa, defina os mesmos perfis (mesmas dimensões nominais), o mesmo aço, `Lb`/`L` iguais e `Cb` indicado. Use **a mesma edição da norma**: o Structura implementa a NBR 8800:2024 e a NBR 14762:2010.
2. Compare a **resistência de cálculo** (não o aproveitamento): `Nc,Rd`, `Mx,Rd`, `Vy,Rd`.
3. Diferenças de até 1–2 % são esperadas por arredondamento das propriedades geométricas (catálogo × geometria calculada). Diferenças maiores merecem investigação: comece pela edição da norma
   (2008 × 2024), pelo coeficiente `Cb`, pelo comprimento destravado e pela seção efetiva/distorcional nos formados a frio.

## Casos

| # | Caso | Resultado do Structura | Base de cálculo |
|---|---|---|---|
| 1 | W 410 × 53,0 (d = 403, bf = 177, tw = 7,5, tf = 10,9 mm), `Nc,Rd`, L = 4 m | 969,26 kN | 5.3.3, 5.3.5.1 (`Ney` governa), Q = 1 |
| 2 | W 410 × 53,0, `Mx,Rd`, Lb = 3 m, Cb = 1,00 | 269,96 kN·m | D.2.1 (FLT), Tabela D.1 |
| 3 | W 410 × 53,0, `Mx,Rd`, Lb = 3 m, Cb = 1,14 | 276,44 kN·m | idem, com Cb |
| 4 | W 410 × 53,0, `Vy,Rd` | 568,78 kN | 5.4.3.1.1: `0,60 d tw fy/γa1` (alma compacta) |
| 5 | Tubular quadrado 100 × 100 × 6,3 (NBR 8800), `Nc,Rd`, L = 2 m | 577,04 kN | 5.3.3, Tabela 4 grupo 1 |
| 6 | I soldado 600 × 250 × 8 × 16, `Mx,Rd`, Lb = 4 m, Cb = 1,00 | 805,40 kN·m | D.2.8-a, FLT; FLM com `kc` (D.2.8-f) |
| 7 | I soldado 600 × 250 × 8 × 16, `Mx,Rd`, Lb = 0 | 935,03 kN·m | `Mpl/γa1` (seção compacta): `Zx fy / 1,10` |
| 8 | Ue 150 × 60 × 20 × 2,65, `Nc,Rd`, L = 2 m | 103,66 kN | NBR 14762, 9.7 (local, global e distorcional) |
| 9 | Ue 150 × 60 × 20 × 2,65, `Mx,Rd`, Lb = 0 | 11,42 kN·m | 9.8.2.1 (seção efetiva) e 9.8.2.3 (distorcional) |
| 10 | Z enrijecido 200 × 75 × 25 × 2,65, `Nc,Rd`, L = 2 m | 152,22 kN | idem, eixos principais da seção |
| 11 | Z enrijecido 200 × 75 × 25 × 2,65, `Mx,Rd`, Lb = 0 | 18,83 kN·m | idem |

Verificação manual dos casos 4 e 7 (independente do código): `0,60 × 403 × 7,5 × 345 / 1,10 = 568,8 kN`; `Zx = 250·16·584 + 8·568²/4 = 2,981·10⁶ mm³`, `Mpl/γa1 = 2,981·10⁶ × 345 / 1,10 = 935,0 kN·m`.
O caso 1 confere com `λ0 = √(A fy/Ney) = 1,368`, `χ = 0,658^λ0² = 0,457` e `0,457 × 6760 × 345 / 1,10 = 969 kN`.

## Validação contra a literatura (livro *Projeto e cálculo de estruturas de aço*, NBR 8800:2008)

Exemplos resolvidos do livro (pasta `livros/`), reproduzidos em `TestDesign::literatureWorkedExamples` e `literatureBoltedConnectionExample`. O livro usa a **edição 2008** e arredonda valores intermediários; a coluna "diferença" explica cada desvio.

| Exemplo do livro | Publicado | Structura | Diferença |
|---|---|---|---|
| 5.4 — W150×18, engastada-livre, K L = 450 cm: `Ney` | 122,82 kN | 122,9 kN | propriedades do catálogo × geometria calculada (< 0,1 %) |
| 5.4 — idem, `λ0` | 2,56 | 2,563 | — |
| 5.4 — idem, `Nc,Rd` | 95,41 kN | 97,97 kN (+2,7 %) | o livro arredonda `χ` para 0,13 (exato 0,1335); recalculando com o `χ` exato, o livro daria 97,9 kN |
| 6.3 — W410×38,8, `λ = Lb/ry` | 106,01 | 106,0 | — |
| 6.3 — idem, `λr` (formato 2008, sem `Cb`) | 120,39 | ≈ 119,5 | `J` do catálogo (11,69 cm⁴) × `J` da geometria com r = 10 mm (9,77 cm⁴) |
| 6.3 — idem, `Mx,Rd` (Lb = 3 m, Cb = 1,43), formato 2008 `Cb[…]/γ` | 224,93 kN·m | ≈ 223 kN·m | idem |
| 6.3 — idem, `Mx,Rd` pelo programa (**NBR 8800:2024**) | — | 174,6 kN·m | **diferença de edição, não erro**: a 2024 põe `Cb` em `λr` e em `Mcr` e não multiplica o parêntese (D.2.1, Tabela D.1); nesta zona de transição o resultado é menos favorável que o do formato 2008 |
| 7.2 — CS450×188, `Ne` (K L = 800 cm) | 10 493,5 kN | 10 493,5 kN | — |
| 7.2 — idem, `Nc,Rd` | 5 313,4 kN | 5 407,8 kN (+1,8 %) | o livro adota `σ = fy` em `bef` (conservador, `Qa = 0,98`); o programa usa `σ = χ fy` (`Qa ≈ 1,0`) |
| 7.2 — idem, `Mx,Rd` (Lb = 4 m, FLM governa) | 1 435,8 kN·m | 1 435,9 kN·m | — |
| 7.2 — idem, interação (N = 300 kN, M = 560 kN·m) | 0,42 | 0,418 | — |
| 9.9.1 — parafuso A325 Ø16, pressão de contato limitada por `2,4 db t fu/γa2` (t = 7,9 mm, fu = 400 MPa) | 89,9 kN | 89,9 kN | — |
| 9.9.1 — parafuso A325 Ø16, cisalhamento com rosca no plano de corte | 49,1 kN | 55,6 kN | **edição**: 2008 usa 0,40 e fub = 825 MPa; a 2024 usa 0,45 e fub = 830 MPa |

Itens do livro **não comparáveis** com a 2024: a base de coluna (9.9.2) segue o Steel Design Guide 1 da AISC, não o método da NBR 8800:2024 (6.7) usado pelo programa; o vento (3.3) segue a NBR 6123:1988, e o programa a edição 2023; o livro do Bellei (*Edifícios industriais em aço*) usa tensões admissíveis e fórmulas em figura, sem texto reutilizável.

### Soluções clássicas do solver (livros de análise estrutural)

`TestProjectIoAndSolver::solverMatchesTextbookFrameSolutions` e os testes já existentes conferem o solver contra soluções fechadas: pórtico de base engastada sob carga horizontal (`Δ = H h³ (2 + 3k)/(12 E Ic (1 + 6k))`), viga biengastada sob carga uniforme (`w L²/12`, flecha `w L⁴/384 E I`), viga contínua de dois vãos iguais (`3wL/8`, `5wL/4`), balanço com torção e empenamento (Vlasov), P‑Δ exato por funções de estabilidade, Euler na flambagem, placa simplesmente apoiada e engastada em `Femdll/tests` (`0,00406 q a⁴/D`), e o balanço com centro de cisalhamento deslocado (`θ = zs P/(GJ)(L − tanh kL/k)`).

## O que este documento não garante

* Coincidência com outro programa — só o próprio cálculo de cada caso foi conferido a mão ou por fórmula fechada. Registre aqui os valores do outro programa quando você os obtiver, com a versão e a norma usadas.
* Combinações com B1/B2, ligações, vigas mistas e vento: estas têm testes próprios em `TestDesign`, `TestConnectionCatalog` e `TestProjectIoAndSolver`.
