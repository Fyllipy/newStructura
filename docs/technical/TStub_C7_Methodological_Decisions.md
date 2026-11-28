# Fase C7.0 — decisões metodológicas pendentes para a implementação do `TStubForceDisplacement`

**Documento de decisão. Nenhum código C++ foi criado ou alterado.** A implementação (C7.1 em diante) só começa após a sua autorização das decisões D1–D6 abaixo. Dados: `research/tstub_piluso/c7_area_study.py` (`saida_c7_area.txt`) e `c7_mechanism_study.py` (`saida_c7_mecanismo.txt`), reproduzíveis. Sem validação profissional.

## 0. Resumo executivo
1. **A decisão da área do parafuso é menor do que parecia:** com a rigidez `K_i` **fornecida pelo usuário** (escopo da C7), `δ_y=F_y/K_i` e `δ_e=F/K_i`; as equações (38)–(40)/(51), que usam a área de alongamento, **não são implementadas**. Resta **uma** área: a da **resistência** `B_u = A·f_ub`. Ela decide o mecanismo e `F_u` (tipo 2). Só a área **bruta** reproduz a Tabela 3 e as estatísticas publicadas; com a área resistente ISO o modelo erra `δ_u` em ≈ +20 % (dp 30 %).
2. **Achado novo que exige decisão (D2):** o espécime 8 não é apenas "critério errado". Para `β_{u,lim} < β_u < 2λ/(1+2λ)` o critério (11) manda usar o mecanismo 2, mas (49) dá **ψ > 1** (o momento no eixo do parafuso excederia `M_u`): **a premissa do mecanismo 2 é violada** e o protótipo Python hoje calcula esse caso **fora do domínio, silenciosamente** (inversão `M–χ` truncada em `χ_u`). O espécime 8 (ψ=1,177) é o único dos 12 nessa faixa.
3. Divergência do Python com a documentação (e com a especificação da API): ver §6 — **a implementação da parte afetada fica interrompida até decisão**, conforme a sua regra.

## 1. Decisão D1 — área do parafuso
### 1.1 Onde a área aparece
| Equação | Área no protótipo | Fonte | Significado físico |
|---|---|---|---|
| `β_u` [P1 (9)], [P2 (10)], [P2 (13)] | `B_u = s.As · f_ub` (os scripts de Tabela 3 passam a área **bruta** `πd_b²/4`; o default de `specimens.py` é a área resistente ISO) | [P1] define `B_u` como "ultimate axial resistance of the single bolt" (**sem dizer a área**); [P2 (13)] usa "`d_b` = net diameter"; exemplo trabalhado: **459 mm²** ("Bolt resistant area") | resistência última à tração do parafuso: convencionalmente a **área resistente à tração** (seção roscada) × `f_ub`; a área bruta (haste lisa) é maior |
| `F_{u,2}` [P1 (12)], `F_{u,3}` [P1 (13)] | idem `B_u` | idem | idem |
| `δ_{b,y}` [P1 (40)], [P1 (51)] (e então `δ_y`, `K_i`) | `As_stiffness`, bruta (`π d_b²/4`) | [P1] chama `A_b` de "bolt net area"; o exemplo trabalhado usa **572,5553 mm²** (bruta) | rigidez axial da haste (alongamento) |
| `K` [P1 (38)], `ζ`, `m'` | sem área de parafuso | Faella 1998 (não lida) | — |
| `M_y, M_u` | sem área de parafuso | — | — |
**Inconsistência interna de [P2]:** o exemplo trabalhado (cálculo manual) usa 459 mm² em `β_u=0,47`; a Tabela 3 (programa dos autores) para o mesmo espécime dá `β_u=0,3794`, que só se obtém com **572,56 mm²**. Os dois não podem estar corretos para a mesma grandeza.

### 1.2 Comparação nos 12 espécimes (`saida_c7_area.txt`; mecanismo pelo critério `β_u`, sem forçar)
| Área na resistência | `F_u,th/F_u,exp` (média, dp, faixa) | `δ_u,th/δ_u,exp` (média, dp, faixa) | mecanismo previsto ≠ observado |
|---|---|---|---|
| **Bruta** `πd_b²/4` | 0,961; 0,057; 0,870–1,082 | **0,945; 0,089; 0,834–1,091** | #8 |
| **Resistente ISO** (M20 245; M24 353; M27 459) | 0,896; 0,062; 0,779–1,015 | **1,198; 0,303; 0,857–1,771** | #8 |
| [P2] publicou | 0,98; 0,07; 0,87–1,11 | 0,95; 0,09; 0,84–1,09 | — |
Com a área ISO, nos 5 espécimes de mecanismo 2 (1, 2, 3, 6, 8) `δ_u` sai ≈ 2× o experimental (ex.: espécime 1: 38,8 × 21,9 mm). O espécime 11 passa a `β_u/β_{u,lim}=0,947` e o 5 a 0,966 (quase na transição); com a bruta eles ficam em 0,738 e 0,753.
**Mudança de `K_i` ao trocar a área de alongamento** (informativo; não usada na C7): −0,6 a −2,4 %.

### 1.3 Opções
| Opção | Descrição | Consequência |
|---|---|---|
| **R-A** | `areaResistance` = bruta, fixa | reproduz Tabela 3/estatísticas; **fisicamente discutível** (haste lisa ≠ seção roscada); decisão embutida no código |
| **R-B** | `areaResistance` = área resistente ISO | convencional/segura em `F_u`, mas **o modelo deixa de reproduzir seus próprios ensaios** (δ_u +20 %, dp 30 %); a precisão publicada não vale |
| **R-C (recomendada)** | `areaResistance` **obrigatória, explícita, sem default**, com `areaKind ∈ {Gross, TensileStress, Other}` + `source`; o módulo **não escolhe**. `Gross` gera `Warning` ("reproduz [P2] Tab. 3; difere da área resistente convencional"); `TensileStress` gera `Warning` ("a precisão publicada de [P2] foi obtida com a área bruta") | a decisão fica visível e rastreável em cada resultado; os **testes de reprodução** usam `Gross` (única combinação que reproduz os valores publicados), documentado |
**Decisão pedida (D1):** aprovar R-C e confirmar que os testes de reprodução usam `Gross`; ou escolher R-A/R-B.
**A área de alongamento deixa de existir na API** (decisão D4 abaixo).

## 2. Decisão D2 — critério de identificação dos mecanismos
### 2.1 Expressões (inalteradas) — [P1 (9)–(13)], (49)
`β_u = 2M_u/(B_u m)`; `β_{u,lim} = [2λ/(1+2λ)]·[1 − (1+λ)d_w/(8n)]`, `λ=n/m`. Tipo 1: `β_u ≤ β_{u,lim}`; tipo 2: `β_{u,lim} < β_u ≤ 2`; tipo 3: `β_u > 2`. Para o tipo 2, `ψ = (2−β_u)λ/[β_u(1+λ)]` e o momento no eixo do parafuso é `ψM_u`.

### 2.2 Achado: a faixa em que o tipo 2 é **internamente inconsistente**
`ψ ≤ 1 ⇔ β_u ≥ β_ψ := 2λ/(1+2λ)`. Como `β_{u,lim} = β_ψ·[1 − (1+λ)d_w/(8n)] < β_ψ` quando `d_w>0`, existe a faixa `β_{u,lim} < β_u < β_ψ` em que (11) indica tipo 2 e (49) dá `ψ>1` (a rótula do eixo do parafuso excederia `M_u`; as rótulas (60)–(62) só são definidas para `ξ=ψ≤1`). Das 12 amostras, **só o espécime 8 cai nessa faixa** (`β_u=0,4053`; `β_{u,lim}=0,3100`; `β_ψ=0,4606`; `ψ=1,177`); observado e Tabela 3: **tipo 1**. A faixa é determinada **só pelas equações** (sem número arbitrário).
| # | λ | `β_u,lim` | `β_ψ` | `β_u` | `β_u/β_{u,lim}` | ψ | mec. critério | observado |
|---|---|---|---|---|---|---|---|---|
| 1 | 1,038 | 0,519 | 0,675 | 0,713 | 1,374 | 0,919 | 2 | 2 |
| 3 | 0,525 | 0,349 | 0,512 | 0,597 | 1,708 | 0,809 | 2 | 2 |
| 6 | 0,990 | 0,504 | 0,665 | 0,672 | 1,335 | 0,983 | 2 | 2 |
| 7 | 1,367 | 0,563 | 0,732 | 0,379 | 0,674 | — | 1 | 1 |
| **8** | 0,427 | 0,310 | 0,461 | 0,405 | 1,308 | **1,177** | 2 | **1** |
| 5 | 1,506 | 0,613 | 0,751 | 0,462 | 0,753 | — | 1 | 1 |
(12 linhas em `saida_c7_mecanismo.txt`.)
**O que o protótipo faz hoje no espécime 8:** a inversão numérica de `M–χ` para `ξ=1,177` fica **truncada em `χ_u`** e o mecanismo 2 produz `F_u=319,4 kN`, `δ_u=41,24 mm` **sem fundamento no artigo** (domínio violado). Com o mecanismo 1: `385,99 kN` e `45,19 mm` = **exatamente a Tabela 3**; experimental `348,75 kN` e `47,61 mm`.

### 2.3 Cobertura experimental (dado, não regra)
`β_u/β_{u,lim}`: tipo 1 (observado) entre **0,581 e 0,753** (#9–#5); tipo 2 entre **1,335 e 1,708** (#6–#3); #8 em 1,308. **Nenhum ensaio tem `β_u/β_{u,lim}` entre 0,753 e 1,308**: a região não tem cobertura experimental. Para trocar o mecanismo, `β_u` teria de variar +33 % a +72 % (tipo 1) ou −25 % a −41 % (tipo 2): erros dessa ordem em `f_y`, `f_ub`, área ou `m` seriam necessários.

### 2.4 Consequência de uma classificação errada (mecanismo alternativo forçado; `saida_c7_mecanismo.txt` D)
Tipo 2 → tipo 1 (válido para as fórmulas): `F_u` **+25 % a +45 %** (espécimes 1, 2, 3, 6) e **+20,9 %** (8); `δ_u` de −20 % a +10 %. Tipo 1 → tipo 2 resulta sempre em `ψ>1` (fora do domínio): **não é uma alternativa calculável** (o tipo 2 só existe para `β_u ≥ β_ψ`). A diferença em `F_u` é maior que o espalhamento publicado do modelo (dp 7 %): **a classificação importa**. Continuidade: `F_u` é contínuo na transição (por construção: (11) vem de `F_{u,1}=F_{u,2}`), mas `δ_u` **salta** (+21 % ao passar do tipo 2 para o 1 no espécime 7 em varredura da área).

### 2.5 Proposta de estados (sem criar janela arbitrária; **não altera o critério**)
| Estado | Condição (determinada pelas equações e pelos dados) | Ação |
|---|---|---|
| **MechanismIdentified** | tipo 1: `β_u ≤ β_{u,lim}`; tipo 2: `β_ψ ≤ β_u ≤ 2` (ψ ≤ 1) | curva calculada |
| **MechanismNearTransition** | `β_{u,lim} < β_u < β_ψ` (critério diz 2, mas **ψ>1**; premissa violada) | **não calcula curva** (ver opções) |
| **MechanismNotIdentifiable** | entradas inválidas/inconsistentes, `M_u≤0`, `B_u≤0`, `8mn−(m+n)d_w≤0`, valores não finitos | sem curva; causa nomeada |
| **MechanismUnsupported** | `β_u > 2` (tipo 3) | sem curva (`FailureMode3Unavailable`) |
**Aviso adicional, apenas informativo e baseado em dados (não é estado):** `Warning{OUTSIDE_EXPERIMENTAL_COVERAGE}` quando `β_u/β_{u,lim}` ∈ (0,753; 1,308) (faixa sem ensaio) ou fora de [0,581; 1,708] (extrapolação). Os limites vêm **dos 12 espécimes** e são recalculados se a base de ensaios mudar.
**Opções para `MechanismNearTransition`:**
- **N-A (recomendada, estrita):** sem curva; o espécime 8 do artigo não é reproduzível pela API pública (o teste usa a função interna do §4/D5).
- **N-B (requer sua autorização expressa; altera o critério):** tratar `ψ>1` como **tipo 1** (a rótula do eixo do parafuso se forma antes) — é o que a Tabela 3 e o observado indicam para o espécime 8. **Não é o critério original de [P1].**
- N-C (rejeitada): calcular o tipo 2 com `ψ` truncado (comportamento atual do Python; sem fundamento).
**Decisão pedida (D2):** aprovar N-A (ou N-B), e os quatro estados acima.

## 3. Decisão D3 — conjunto mínimo de entradas
Com `K_i` fornecido e a geometria `b_eff` informada, o modelo precisa só de: `t_f, m, n, d_w, b_eff`; `f_ub`, `areaResistance`, `areaKind`; `E, f_y, f_u, ε_h, E_h, E_u, ε_u`; `K_i`. **Não são necessários** `d_h, t_h, t_n, d_b, r, grip, b` (só entravam em `b_eff`, `K`, `m'`, `L_b`, `δ_b`). Recomenda-se **não** pedir esses campos (instrução: "não acrescente parâmetros desnecessários"). `b_eff` deve ser **entrada explícita com origem** (regra do artigo `min[d_h+2m; b]` é de Faella 1998, **não verificada**); uma função auxiliar opcional `effectiveWidthFromP2(d_h,m,b)` pode existir, rotulada "transcrita de [P2], fonte original não verificada", sem ser chamada implicitamente.
**Decisão pedida (D3):** aprovar este conjunto; confirmar `b_eff` explícito.

## 4. Decisões técnicas D4–D6 (menor impacto)
- **D4 — Especificação da API (C6) a revisar:** (i) remover `areaElongation` e `StiffnessSource::PilusoFaella1998Formula`; (ii) `K_i` obrigatória (`UserProvided` com origem/unidade); (iii) `areaKind` novo; (iv) estados do §2.5. **Pedido: autorizar atualizar o documento de especificação.**
- **D5 — Reprodução do espécime 8:** expor uma função **interna** (`detail::`) `evaluateForcedMechanism(...)` usada **só pelos testes** para reproduzir a Tabela 3 (mecanismo 1) — a API pública nunca aceita mecanismo imposto. **Pedido: autorizar** (alternativa: não reproduzir o espécime 8).
- **D6 — Tabela de amostras:** "128 intervalos" = **129 amostras**; o protótipo gera `(0,0), (δ_y,F_y)` e `N` passos uniformes em `M`; os pontos `h` e `m` **não** coincidem com amostras. Proposta: gerar a malha uniforme em `M` e **inserir os quatro pontos característicos exatos**, mantendo ordenação e ≥ 128 intervalos. Interpolação linear, sem extrapolação, `δ>δ_u` → `PostPeakUnavailable`. Tolerância C++ × Python: erro relativo ≤ 1·10⁻⁹ por amostra (ambos em dupla precisão; a inversão numérica tem tolerância própria a fixar, 1·10⁻¹²). **Pedido: autorizar.**

## 5. Outras observações
- `K_i` fornecida: toda curva é `δ = F/K_i + 2δ_p`; a monotonicidade fica garantida para qualquer `K_i>0` (`δ_h > δ_y` sempre). `K_i` muito diferente da rigidez de Piluso (≈ 2× da EN) **altera δ_y e δ_h em ≈ 50 %** mas `δ_u` só 2–5 % (Fase C): isso será dito no resultado.
- Domínio testado dos 12 espécimes (aviso, não bloqueio): `m/t_f` 2,7–4,4; M20–M27; Fe430; monotônico; parafusos *snug*; `β_u/β_{u,lim}` conforme §2.3.
- O protótipo valida entradas (`ValueError`); a C++ devolverá estados.

## 6. Divergências encontradas (Python × documentos) — ação
| # | Divergência | Parte afetada | Ação |
|---|---|---|---|
| 1 | O Python calcula o mecanismo 2 com `ψ>1` sem avisar (espécime 8). Os documentos da Fase C descrevem o 8 apenas como "critério errado" | mecanismo 2 em `β_{u,lim}<β_u<β_ψ` | **implementação desta parte interrompida** até D2 |
| 2 | Especificação da API (C6) exige `areaElongation` e prevê `K` de Faella; o escopo C7 pede só `K_i` do usuário | entradas e rigidez | D4 |
| 3 | O relatório da Fase C cita "Fu média 0,98" (espécime 8 forçado no mecanismo 1); pelo critério dá 0,961 | estatística | informado aqui; os documentos serão corrigidos se aprovado |
| 4 | A tabela do protótipo não contém os pontos `h` e `m` exatos | tabela | D6 |
Nada foi alterado no Python nem nos documentos da Fase C6 nesta etapa; apenas os dois scripts de análise e este documento foram criados.

## 7. Decisões solicitadas (resumo)
| ID | Pergunta | Recomendação |
|---|---|---|
| D1 | Área do parafuso na resistência | **R-C**: obrigatória, explícita, `areaKind`; testes de reprodução com `Gross` |
| D2 | Estados e tratamento de `β_{u,lim}<β_u<β_ψ` | estados do §2.5; **N-A** (sem curva) |
| D3 | Conjunto mínimo de entradas; `b_eff` explícito | aprovar |
| D4 | Atualizar a especificação da API | aprovar |
| D5 | Função interna de mecanismo imposto, só para testes | aprovar |
| D6 | Tabela com pontos exatos inseridos; tolerâncias | aprovar |
