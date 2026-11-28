# Relatório da Fase C — comportamento força-deslocamento do T-stub

Investigação, reprodução e comparação de um **modelo publicado** (Piluso, Faella, Rizzano, 2001). **Nada foi integrado ao Structura**: nenhum arquivo de produção foi alterado (C++, `JointComponents.cpp`, M–θ, Camada A/B, goldens). Este relatório **não declara validação profissional** e não usa "validado" sem dizer o quê e com que evidência.

## 1. Entregáveis
| Entregável | Arquivo | Estado |
|---|---|---|
| Reconstrução matemática | `docs/technical/TStub_Piluso_Model_Reconstruction.md` | **CONCLUÍDA** (mecanismos 1 e 2; mecanismo 3 sem fonte) |
| Compatibilidade de rigidez | `docs/technical/TStub_Stiffness_Compatibility.md` | **PARCIALMENTE CONCLUÍDA** (comparação entre formulações; sem rigidez experimental) |
| Validação experimental (12 espécimes) | `docs/validation/TStub_Piluso_Experimental_Validation.md` | **PARCIALMENTE CONCLUÍDA** |
| Estudo MEF elástico | `docs/validation/TStub_FEM_Elastic_Study.md` | **NÃO CONCLUÍDA** (motivos e protocolo registrados) |
| Protótipo Python independente | `research/tstub_piluso/` (`piluso_tstub.py`, `specimens.py`, `validate_published.py`, `validate_experimental.py`, `stiffness_compare.py`, `test_piluso.py`, `digit/`) | feito |
| Gráficos | `docs/validation/figures/tstub_piluso_comparison.svg/.png` | feito |
| Proposta de lei constitutiva (C5) | §6 deste relatório | proposta, **não implementada** |

Arquivos criados: os acima e `research/tstub_piluso/saida_*.txt`, `digit/exp_curve_01..12.csv`. Alterados: nenhum arquivo existente do projeto. **Observação:** `docs/references/` contém os PDFs da ASCE que você forneceu (material protegido por direitos autorais); recomendo **não** versioná-los. Os rasters de figura extraídos para a digitalização foram removidos (regenerar com `pdfimages -f 6 -l 7 ASCE-2001-Model-Validation.pdf fig` dentro de `digit/`).

## 2. Fontes
[P1] e [P2] lidas integralmente (texto e imagens das páginas). A revisão da MDPI e demais fontes complementares **não foram obtidas** (HTTP 403 nas tentativas); Faella et al. (1998), origem de `b_eff`, `K`, `m'`, **não consultada**. EN 1993-1-8, 6.4.2: não usada (critério global de rotação, fora do escopo de lei local, conforme sua instrução).

## 3. Testes executados
- `research/tstub_piluso/test_piluso.py`: **14/14** (reprodução matemática: exemplo trabalhado, Tabela 3, formas fechadas × integração numérica; propriedades: continuidade nos limites dos casos, monotonicidade, escala dimensional por semelhança geométrica, transição de mecanismo, entradas inválidas, dependência da espessura; comparação experimental: estatísticas publicadas, falha do critério no espécime 8).
- `validate_published.py`: exemplo trabalhado 35/35 dentro da tolerância; Tabela 3: 11/12 espécimes reproduzidos a ≲ 0,01 % em `β_u`, `δ_u`, `F_u` (espécime 8 com mecanismo 1 forçado, como em [P2]; espécime 11 com `β_u` impresso divergente em +2,7 %).
- `ctest -C Release` do Structura: **35/35** (nada do C++ foi alterado; biblioteca reconstruída anteriormente, conferida).
Separação: os testes de **reprodução** não são validação física; a **comparação** com `F_u,exp`/`δ_u,exp` tabulados e com a curva digitalizada é o único elo experimental, e só vale para os 12 espécimes de [P2].

## 4. Principais resultados
1. O modelo (mecanismos 1 e 2) foi **reproduzido**: exemplo trabalhado a ≤ 0,06 %, Tabela 3 a ≲ 0,01 % (exceto espécimes 8 e 11), e as fórmulas fechadas `D,F,G` conferem com integração numérica independente a ≤ 3,4·10⁻⁷.
2. **Correspondência com o experimento (12 espécimes):** `F_u,th/F_u,exp` média 0,977 (dp 0,068; 0,87–1,11); `δ_u,th/δ_u,exp` média 0,952 (dp 0,086; 0,83–1,09) — iguais às estatísticas de [P2]. Curva multilinear de 4 pontos: viés médio −9,3 %, RMS 10,6 %; curva contínua: viés −3,9 %, RMS 7,5 %.
3. **Erros e ambiguidades na publicação** (documentados, sem alterar equações): `F_u` trocados na Tabela 3 (6 e 7); "My=1.126.283" (p. 702); `β_u` do espécime 11; inconsistência da área do parafuso (459 mm² × 572,56 mm²); mecanismo do espécime 8 (a tabela usa o 1, o critério dá o 2).
4. **O critério de mecanismo** acertou 11/12 (falha no espécime 8). **Mecanismo 3: sem equações → não reproduzido.**
5. **Rigidez:** Piluso/Faella e EN (Fase B) diferem ≈ 1,6–2,2× (com `l_eff=b_eff`) para a flexão da mesa; com `l_eff` da EN o resultado é inaplicável a estes espécimes (`l_eff` > largura). A rigidez inicial pouco afeta `δ_u` (parte elástica 4–10 %), mas muda `δ_y` em ≈ 50 %.

## 5. Respostas objetivas (§12 do prompt)
1. **O modelo foi integralmente compreendido?** Para os mecanismos 1 e 2: **sim, com ressalvas** (equações transcritas e reproduzidas). Mecanismo 3 e a origem de `b_eff`/`K`/`m'` (Faella 1998): **não** — **PARCIALMENTE CONCLUÍDA**.
2. **Todas as equações necessárias foram identificadas?** Não: faltam as do mecanismo 3, a dedução de `K`, `b_eff`, `ζ` e a forma do trecho pós-pico da "simulação".
3. **Todos os mecanismos foram reproduzidos?** Tipos 1 e 2 sim; tipo 3 não.
4. **Os 12 espécimes foram avaliados?** Sim (resistência e deslocamento tabulados; curva digitalizada), com o espécime 8 em duas variantes de mecanismo.
5. **Concordância satisfatória?** Em `F_u` e `δ_u`: dentro das estatísticas publicadas (faixa −13 %/+11 % em `F_u`; −17 %/+9 % em `δ_u`). Na curva: subestima em média 4 % (contínua) a 9 % (multilinear). "Satisfatória" depende do uso; para **capacidade de deformação** é o que [P2] declara; para **curva completa** é aproximada.
6. **Rigidez original compatível com a EN da Fase B?** **Não demonstrado e provavelmente não**: diferem ≈ 2× e dependem de `l_eff` vs `b_eff`; as curvas digitalizadas não discriminam.
7. **MEF?** **Não executado** — sem evidência.
8. **Limitações:** mecanismo 3; parafusos pré-tensionados; só 12 espécimes (M20–M27, Fe430, monotônico); exige curva completa do material (`ε_h, E_h, E_u, ε_u`, de ensaio de cupom); rigidez inicial sem validação; curvas digitalizadas; sem pós-pico; fontes de origem não consultadas.
9. **Formulação a adotar?** Ver §6: o modelo de Piluso para **capacidade de deformação** (mecanismos 1 e 2) é a única formulação **publicada, reproduzida e comparada** disponível; a adoção **depende** da decisão sobre os dados do material (§6) e da rigidez (alternativa C).
10. **O que impede a implementação definitiva?** (a) Dados do aço exigidos pelo modelo normalmente **inexistentes em projeto** (sem defaults silenciosos); (b) mecanismo 3 sem fonte; (c) rigidez inicial não validada (MEF/ensaio); (d) obter Faella et al. (1998) e Faella et al. (1999); (e) decisão sobre a Tabela 3 (espécime 8) e a área do parafuso.

## 6. Proposta de lei constitutiva (C5) — NÃO IMPLEMENTADA
- **Representação:** conjunto de pontos `(F_y,δ_y),(F_h,δ_h),(F_m,δ_m),(F_u,δ_u)` **mais** a tabela densa `F(M),δ(M)` (`M∈[M_y,M_u]`, p. ex. 64 amostras, monótona), pois o multilinear de 4 pontos subestima em média 9 % entre pontos (viés da curva contínua é −3,9 %). Interpolação linear sobre a tabela densa.
- **Mecanismos:** tipo 1 e 2 conforme o critério `β_u` (documentar que o critério falhou em 1 de 12 espécimes); **tipo 3: estado "SEM LEI DISPONÍVEL — frágil"**, sem inventar curva (a fonte diz apenas `F_u=2B_u` e que é frágil e a evitar).
- **Encerramento:** em `(δ_u,F_u)` (fratura do material por `ε_u`); pós-pico não modelado (a "simulation" de [P2] mostra queda, mas o método não é descrito).
- **Dados:** `t_f, m, n, r, b, d_h, d_w, t_h, t_n, d_b, A_b, f_ub, E, f_y, f_u, ε_h, E_h, E_u, ε_u` obrigatórios; ausentes ou inconsistentes → estado explícito "NÃO CALCULÁVEL" (sem defaults). **Bloqueio prático:** `ε_h, E_h, E_u, ε_u` só vêm de ensaio de cupom; a proposta é aceitar valores informados pelo usuário com rastreio de origem, nunca valores padrão.
- **Rigidez:** `K_i` do próprio modelo (alternativa C); não misturar com a EN até haver validação.
- **Arquitetura:** funções puras em arquivo separado (`TStubForceDisplacement.{h,cpp}`, sem NBR nem `Check`), sem tocar `TStub.cpp` (resistência de cálculo EN) nem `JointComponents.cpp`. **Atenção semântica:** as forças de Piluso são **resistências últimas calibradas em ensaio** (com `M_u≈3,0–3,7 M_y`), não resistências de cálculo; precisam ser rotuladas assim e não entram em verificações NBR/EN.
- **Testes:** os 14 do protótipo, mais a Tabela 3 como benchmark publicado (tolerâncias: 0,01–0,2 %), mais comparação experimental com as estatísticas de [P2].
- **Requer autorização** antes de implementar em C++ e decisão sobre a origem dos dados do material.

## 7. Limitações e pendências
Pendências: obter [P1]-citadas (Faella 1998/1999); revisar a MDPI quando acessível; decidir sobre o mecanismo 3; MEF (protocolo no estudo); digitalização de maior resolução do trecho inicial ou dados brutos dos ensaios para validar a rigidez; confirmar com os autores/errata a Tabela 3 (espécimes 6, 7, 8, 11). Limitações gerais: os resultados valem para os 12 espécimes e para o modelo como publicado; nenhum resultado é validação profissional.

**Próxima etapa recomendada:** (1) obter Faella et al. (1998) e (1999); (2) executar o MEF do Modelo A (§2 do estudo MEF) com geometria declarada; (3) só então decidir a implementação da Fase C em C++, começando por um módulo isolado e os testes de benchmark acima.
