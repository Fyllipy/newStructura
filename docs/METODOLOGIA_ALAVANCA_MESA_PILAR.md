# Efeito de alavanca na mesa do pilar — definição matemática e validação (A2)

Etapa de **definição e validação**. Nenhum código do programa foi alterado. Scripts, saídas e referências estão em `docs/validacao_alavanca/` (LEIAME lá). Este documento **não declara validação profissional** do módulo.

## 0. Conclusões em uma página

1. **Camada B (EN 1993-1-8, T-stub) — viável e bem validada.** A especificação reproduz, sem nenhum ajuste, 4 resistências de mesa de pilar e 1 de chapa do SCI P398 (diferença ≤ 0,3 %) e 26 amostras do método dos componentes publicadas na verificação de T-stub da IDEA StatiCa (razão 0,998–1,019). Passa pelas transições de modo 1→2→3.
2. **Camada A (NBR 8800:2024, 6.3.5.4 na mesa do pilar) — mecanicamente coerente, mas a validade NÃO está plenamente demonstrada.** O 6.3.5.4 é o mesmo modelo de duas rótulas de um T-stub e o texto da norma não o restringe à chapa. Porém: (i) a norma não diz como tratar o raio de concordância; (ii) a distribuição da tração entre fileiras não é definida; (iii) a interação entre fileiras só entra pela largura tributária, e isso dá resistência **7 % a 25 % acima do grupo da EN** na geometria publicada de 3 fileiras e **+24 %/+12 %** (b = m / b literal) no HEB200 de 2 fileiras; (iv) frente ao único MEF sólido publicado (T-stub), a razão NBR/MEF varia de 0,45 a 1,18. Isto exige decisões suas antes de implementar (seção 8).
3. **Correção do relatório anterior:** os valores de T-stub citados lá (236/428 kN) usavam o modo 1 pelo método 1 e γM0 = 1,10 da NBR. Com a especificação correta (método 2, γM0 = 1,0 e γM2 = 1,25 da EN) o HEB200 dá **240 kN (1 fileira)** e **430 kN (2 fileiras, grupo)**. A conclusão qualitativa se mantém.
4. A ferramenta momento-rotação já tem um T-stub próprio (`JointComponents.cpp`, `tStub`) com **método 1 e γ da NBR**. Isso precisa convergir para o módulo compartilhado (seção 5), com aprovação, porque altera resultados daquela ferramenta.
5. **Não foi feita comparação com MEF próprio de casca** (e há razão: o ELU do T-stub exige não linearidade de material, contato e parafuso, o que o solver linear de cascas do Structura e o Mecway linear não fazem; além disso o Structura não pode ser a referência). Usei o MEF sólido não linear **publicado**. Um MEF independente do grupo de fileiras continua pendente (seção 7).

## 1. Decisões adotadas
- **Camada A** (NBR): decide a aprovação. Coeficientes e critérios exclusivamente da NBR.
- **Camada B** (EN, T-stub): **informativa**, rotulada "método complementar". Não entra em `Summary`/aprovação global. Coeficientes parciais da própria EN.
- Nenhum coeficiente é trocado entre as camadas.

## 2. Camada A — NBR 8800:2024, 6.3.5

### 2.1 O que o texto da norma diz (página 88–90 da NBR, lida em imagem)
- 6.3.5.1: considerar o efeito de alavanca "produzido pelos deslocamentos e deformações das **partes ligadas**"; 6.3.5.3 (placa rígida) e 6.3.5.4 (placa flexível) atendem a 6.1.1.3.
- 6.3.5.2: `p_ext = min(e_ext; 1,75 b) + min(0,5 e_int; 1,75 b)`; `p_int = min(e_int; 3,5 b)`; `δ = 1 − d_f/p`; `β = [(a + 0,5 d_b)/(b − 0,5 d_b)] (F_t,Rd/F_t,0,Sd − 1)`; `α = 1` se β ≥ 1, `min[1; (1/δ)(β/(1−β))]` se 0 < β < 1; β < 0 ⇒ trocar o parafuso; `a ≤ 1,25 b`.
- 6.3.5.4: `t ≥ √[ 4 (b − 0,5 d_b) F_t,0,Sd γ_a1 / ( p f_u (1 + δ α) ) ]`. 6.3.5.3 é o mesmo sem o fator `(1 + δα)`.
- A Figura 15 mostra um perfil T/L com fileiras de parafusos **ao longo da aba**, parafusos dos dois lados da alma (corte A–A) e `e_ext`, `e_int` medidos ao longo do comprimento. **É exatamente a topologia da mesa de um pilar I/H tracionada por fileiras de parafusos dos dois lados da alma.**

### 2.2 Adequação mecânica (análise)
| Aspecto | Análise |
|---|---|
| Apoio da mesa na alma | O modelo assume linha de apoio na alma onde se forma a rótula 1. A alma real é flexível (componente "alma tracionada", 5.7.3/EN 6.2.6.3), então o apoio não é rígido; isso afeta a rigidez e é um estado-limite à parte, não a alavanca. |
| Raio de concordância | A norma mede `b` da linha do parafuso à face da alma e não trata filete. O raio enrijece a região: a EN usa `m = (g − tw)/2 − 0,8 r`. **Hipótese H1** (complementar): `b = m` (com 0,8 r). Sensibilidade no HEB200: 274 kN (b = m) × 256 kN (b literal), 1 fileira. |
| Distância `b`, `a` | Mesmos significados do T-stub: `b` = m, `a` = e (borda livre da mesa, ≤ 1,25 b). |
| Largura tributária | Medida **ao longo do eixo do pilar** para cada parafuso (cada parafuso do par tem seu `p`). O cap `1,75 b` por lado reproduz o alcance das linhas de plastificação. Uma fileira isolada resulta em `p = 3,5 b`. **H2**: usar 6.3.5.2 literalmente; borda `e_ext` = distância à extremidade do pilar/enrijecedor; fileira de cortante vizinha conta como parafuso vizinho. |
| Deformação / alavanca | A alavanca existe quando a mesa é flexível; 6.3.5.4 a contém implicitamente via `(1 + δα)`; `β ≥ 1` (α = 1) corresponde ao modo 1; `β < 0` ao modo 3. |
| Interação entre fileiras | **Só pela largura tributária.** Não há mecanismo de grupo. Nos testes abaixo isso dá resistências acima da do grupo EN. |
| Distribuição entre fileiras | A norma não define. O programa hoje usa partes iguais (T/n). **H3**: demanda por parafuso `F_t,0,Sd,i ∝` braço da fileira em relação ao centro de compressão (elástico), e usar a mais desfavorável entre elástico e igual. Com a distribuição igual, a fileira inferior pode ficar insegura (HEB200: fileira 1 até 240 kN, fileira 2 até 190 kN pelo T-stub). |

### 2.3 Formulação proposta (Camada A)
Para cada fileira `i` tracionada e para o parafuso mais carregado dela:
1. `F_t,Rd` = resistência do parafuso à tração (6.3.3.1, com interação de cortante se houver).
2. `F_t,0,Sd,i` por H3.
3. `b = m = (g − t_w)/2 − 0,8 r` (H1), `a = min(e, 1,25 b)`, `e = (b_f − g)/2`, `p_i` por 6.3.5.2 (H2), `d_f` = furo, `f_u` e `γ_a1` do pilar.
4. `δ, β, α` como em 6.3.5.2; `t_req` por 6.3.5.4; razão = `t_req / t_f`. `β < 0` ⇒ verificação reprovada ("parafuso inadequado").
5. Para uma fileira: o mesmo com `p = 3,5 b`. Para várias: `p_i` com vizinhas (soma das larguras = espaçamento quando `0,5 e_int < 1,75 b`).
6. Resultado listado por fileira, com os valores de b, a, p, δ, β, α e `t_req`.

**Classificação**: normativo = fórmulas e `p_ext/p_int` (6.3.5.2–6.3.5.4) e a obrigação de considerar a alavanca nas partes ligadas (6.3.5.1). **Complementar** = H1 (b = m), H2 (bordas/vizinhas no pilar), H3 (distribuição), e a **extensão** do 6.3.5.4 a uma mesa laminada com alma e filete.

### 2.4 Limitações da Camada A (registradas, não ocultadas)
- Frente ao grupo EN, a soma por fileiras (largura tributária) deu **+7 % a +25 %** (varredura da seção 4.3). A EN é conservadora frente ao MEF sólido para flanges finas, o que atenua, mas **não há MEF de grupo** que prove segurança.
- Frente ao MEF sólido do T-stub, NBR/MEF = 0,45–1,18 (seção 4.3): conservadora para mesas finas (o MEF captura efeito de membrana), acima do MEF em 18 % na faixa de transição (tf = 25–35 mm do T-stub publicado). Resistências nominais com γ = 1,0; a comparação é de mecânica, não de confiabilidade.
- Sem verificação específica de punção/arrancamento, alma flexível, nem mesa enrijecida.

## 3. Camada B — T-stub EN 1993-1-8 (método complementar)

Notação: `tf`, `fy` da mesa (ou chapa); `m`, `e`, `n = min(e_min; 1,25 m)`; `leff,1`, `leff,2`; `ΣFt,Rd` do T-stub; `e_w = d_w/4`; coeficientes **EN**: `γM0 = 1,0`, `γM2 = 1,25` (valores recomendados; o Anexo Nacional pode alterá-los, e o SCI P398 UK NA usa 1,1 em um dos casos), `k2 = 0,9`.

### 3.1 Resistência do parafuso
`F_t,Rd = k2 f_ub A_s / γM2`. Para parafusos ASTM (A325/A490) a EN não tem classe: o usuário informa `f_ub` e a ligação do método EN a esse parafuso fica **sob responsabilidade do projetista** (rótulo explícito).

### 3.2 Modos (Tabela 6.2; EN 6.2.4.1(6) e Nota 1: em ligações viga-pilar assumir que a alavanca se desenvolve)
`M_pl,1,Rd = 0,25 leff,1 tf² fy / γM0`; `M_pl,2,Rd = 0,25 leff,2 tf² fy / γM0`
- Modo 1, método 1: `F_T,1,Rd = 4 M_pl,1,Rd / m`
- Modo 1, método 2 (sem chapa de reforço): `F_T,1,Rd = (8n − 2e_w) M_pl,1,Rd / [2 m n − e_w (m + n)]`
- Modo 2: `F_T,2,Rd = (2 M_pl,2,Rd + n ΣF_t,Rd) / (m + n)`
- Modo 3: `F_T,3,Rd = ΣF_t,Rd`
- `F_T,Rd = min(F_T,1,Rd; F_T,2,Rd; F_T,3,Rd)`; o modo governante é reportado.
**Escolha:** método 2 (é o que o SCI P398 e a IDEA usam e reproduz os exemplos). O método 1 é mais conservador no modo 1 (por exemplo, `tf = 10` no P398: 167 × 214 kN).

### 3.3 Comprimentos efetivos
- Mesa de pilar não enrijecida (Tabela 6.4), fileira isolada: `leff,cp = 2π m`; `leff,nc = 4 m + 1,25 e`.
- Como fileira de um grupo: extremidade `cp = π m + p`, `nc = 2 m + 0,625 e + 0,5 p`; interna `cp = 2 p`, `nc = p` (com `p` = média dos dois espaçamentos). Soma sobre as fileiras do grupo.
- Modo 1: `leff,1 = min(Σleff,cp; Σleff,nc)`; modo 2: `leff,2 = Σleff,nc`.
- Chapa de extremidade (Tabela 6.6): fileira na extensão `cp = min(2π m_x; π m_x + w; π m_x + 2e)`, `nc = min(4 m_x + 1,25 e_x; e + 2 m_x + 0,625 e_x; 0,5 b_p; 0,5 w + 2 m_x + 0,625 e_x)`; primeira fileira abaixo da mesa: `cp = 2π m`, `nc = α m` (α pelo ábaco/Apêndice G). Outras fileiras: Tabela 6.6.
- Mesa enrijecida (Tabela 6.5): fora do escopo da v1.

### 3.4 Fileiras isoladas e grupos; distribuição (EN 6.2.4.2 e 6.2.7.2)
Condições: (a) a força em cada fileira não excede a resistência da fileira isolada; (b) o total de cada grupo de fileiras adjacentes não excede a resistência do grupo. Procedimento (como no P398): a resistência efetiva da fileira `r` é `min(isolada_r ; R_grupo(1..r) − Σ efetivas anteriores ; …)`, partindo da fileira mais afastada do centro de compressão. Resultado = resistência total à tração `T_Rd` do conjunto, comparável a `T_Ed = M/z`.

### 3.5 Interação com a chapa de extremidade
Para cada fileira: componentes `mesa do pilar` e `chapa de extremidade` (T-stub com Tabela 6.6, **sem grupo para a chapa**, como no P398). Resistência da fileira = menor dos componentes incluídos. **Fora do v1:** alma do pilar tracionada (6.2.6.3), alma da viga tracionada (6.2.6.8), cisalhamento do painel e compressão (a NBR 5.7 já cobre alma e painel no programa). Isso é listado como limitação no resultado.

## 4. Validação independente

### 4.1 Referências verificadas (baixadas e lidas)
| Referência | Conteúdo usado |
|---|---|
| SCI P398, Apêndice C, Exemplo C.1 (UKC 254×254×107, S275, M24 8.8, γM0 = 1,0; γM2 = 1,25) | Mesa do pilar: fileira 1; grupos 1-2, 1-2-3, 2-3; chapa: fileiras 1 e 2 |
| Gödrich, *Verification of T-stub* (IDEA StatiCa, 2015) | 26 amostras do método dos componentes (EN) e 14 resultados de MEF sólido hexaédrico validado em ensaios (RM) |
| EN 1993-1-8:2005 | Tabela 6.2, 6.4, 6.6, 6.2.4.2 (texto extraído tem ruído de OCR; conferir contra a norma oficial) |
| NBR 8800:2024, 6.3.5 | Equações (lidas na imagem da página) |

### 4.2 Camada B contra as referências
**SCI P398 C.1 (mesa do pilar; kN):**

| Caso | Publicado | Especificação | Diferença |
|---|---|---|---|
| Fileira 1 isolada (modo 2) | 398 | 398 | 0,1 % |
| Fileiras 1-2 (grupo) | 697 | 698 | 0,2 % |
| Fileiras 1-2-3 (grupo) | 988 | 991 | 0,3 % |
| Fileiras 2-3 (grupo) | 691 | 691 | 0,0 % |
| Chapa, fileira 1 (modos 901/377/406) | 377 | 377 (900/377/407) | 0 % |
Comprimentos efetivos reproduzidos (210/233; grupo 410/333). Tabelas de referência: nenhuma constante foi ajustada.

**IDEA, método dos componentes:** 10 espessuras (tf = 10…50 mm, modos 1, 2 e 3), 8 parafusos (M16…M27, classes 4.8…10.9), 4 distâncias `w` e 4 larguras `b`: razão EN/MC entre **0,998 e 1,019**.

### 4.3 Camada A (NBR 6.3.5.4) contra as mesmas referências
Todos com γ = 1,0 e a resistência do parafuso da referência (apenas para comparar a mecânica).
- **T-stub IDEA (p = 100 mm por parafuso):** NBR/MC = 1,00–1,46; NBR/MEF sólido = 0,55–1,18 (b = m) e 0,45–1,12 (b literal). Valores: tf = 20 → MC 174, MEF 269, NBR 255 (b = m); tf = 30 → 305, 329, 389; tf = 40 → 371, 371, 407.
- **Fileira isolada, geometria P398 (varredura de tf):** NBR/EN = 0,93–1,15 (tf = 8 → 0,93; tf = 14 → 1,15; tf = 20,5 → 1,02).
- **Grupo de 3 fileiras, geometria P398 (varredura):** NBR/EN = 1,07–1,25 (tf = 20,5 → 1,22).
- A comparação com o MEF **não é uma validação de segurança**: o MEF publicado tem efeito de membrana e critério de deformação limite, e a NBR tem outra calibração.

### 4.4 Exemplo HEB200 (investigação anterior, refeito)
HEB200: bf = 200; tf = 15; tw = 9; r = 18; g = 90; pitch = 70; M20; `m = 26,1`, `e = 55`.

| kN | 1 fileira | 2 fileiras |
|---|---|---|
| Parafusos (Σ) | EN 282 (M20 8.8) / NBR 289,7 | EN 564 / NBR 579 |
| **Camada B** EN (fy = 250; γM0 = 1,0, γM2 = 1,25), modos 1/2/3 | 463 / **240** / 282 | grupo 686 / **430** / 564 (isoladas somadas 479) |
| Idem fy = 355 | 275 (modo 2) | 479 (grupo) |
| **Camada A** NBR, b = m (H1), fu = 400, γa1 = 1,10 | 274 | 533 (p_i = 80,7) |
| **Camada A** NBR, b literal | 256 | 481 |
| Structura hoje (5.7.2 contra T e parafusos) | 289,7 | 319,6 |

Leitura: com 1 fileira o programa aceita 20 % acima do T-stub EN (e 6 % acima da NBR/H1): o A2 existe pelas duas camadas. Com 2 fileiras o programa é conservador frente às duas, mas a NBR/H1 (533) fica **24 % acima do grupo EN** (430): é a divergência que a seção 2.4 registra.

## 5. Arquitetura proposta (sem implementar)
```
design/
  TStub.{h,cpp}            -> núcleo puro: TStubGeometry {m, e, n, ew, leff1, leff2, tf, fy, Σ Ft,Rd, método} ; modos 1/2/3, governante, linhas de memória
  TStubEffectiveLengths    -> Tabelas 6.4/6.5/6.6 (fileira isolada, grupo), com a origem de cada termo
  TStubRowDistribution     -> EN 6.2.4.2/6.2.7.2 (isolada x grupo, distribuição por fileira)
  CoefficientSet           -> {EN recomendado (γM0=1,0; γM2=1,25; k2=0,9), NA configurável}; NÃO contém coeficientes NBR
  PryingNbr.{h,cpp}        -> Camada A: tributary widths (6.3.5.2), H1–H3, reutiliza o checkPrying existente
consumidores:
  EndPlateConnection  -> Camada A (decide) + Camada B (informativa)
  JointComponents     -> k4/k5 e resistência usando o MESMO TStub (hoje tem cópia própria: método 1 e γ NBR)
  JointMemory/relatório -> linhas geradas pelo próprio TStub
```
Regras: um único cálculo de `m`, `e`, `leff` e de modos; a Camada A não importa `CoefficientSet`; a Camada B não importa coeficientes NBR; `JointComponents` só migra com aprovação (muda seus resultados).

## 6. Interface e resultados
Na tabela de verificações, após "Parafusos tracionados":
- **Efeito de alavanca — mesa do pilar (NBR 8800:2024, 6.3.5.4)** — uma linha por fileira, razão `t_req/t_f`, estado APROVADO/REPROVADO, participa da aprovação. Linhas de memória: `b, a, p, δ, β, α, t_req`, hipóteses H1–H3 numeradas, e a limitação da seção 2.4.
- **T-stub EN 1993-1-8 — MÉTODO COMPLEMENTAR (informativo)** — um bloco com: resistência `T_Rd,EN` do conjunto, modo governante por fileira, componente governante (mesa do pilar/chapa), razão `T_Ed/T_Rd,EN`, coeficientes usados ("EN: γM0 = 1,0; γM2 = 1,25; k2 = 0,9"), hipóteses, **limitações** (sem alma tracionada nem mesa enrijecida; bolt ASTM sob responsabilidade do projetista) e o texto fixo: "Não participa da aprovação global".
- Novo estado `CheckStatus::Complementary` (ou equivalente): fora de `executed/failed/unverifiable`; `Summary` e `maxRatio` o ignoram; o relatório e o diálogo mostram o valor em cinza com o rótulo.
- **Aviso de divergência (decisão sua):** se a Camada A for ≥ X % menos conservadora que a B, mostrar uma advertência (sem alterar aprovação).

## 7. Plano de testes
1. **Regressão contra referências:** os 5 valores do P398 e as 26 amostras da IDEA (tolerância 2 %); transições de modo; método 1 × 2.
2. **Propriedades:** monotonicidade em `tf`; `F_T,Rd ≤ ΣF_t,Rd`; grupo ≤ soma das isoladas; invariância à troca de fileiras; limites (`n ≤ 1,25 m`; `β < 0` ⇒ reprovado).
3. **Camada A:** fórmulas independentes (reimplementação em Python do 6.3.5.2–6.3.5.4, já em `tstub_spec.py`) para HEB200, UKC 254×254×107 e casos de borda (`a > 1,25 b`, δ ≤ 0, fileira única/múltipla).
4. **Validação de entradas** (`InputValidation`): `tf`, `r`, `g`, `p` inválidos ⇒ NÃO VERIFICÁVEL.
5. **Golden** das demais verificações inalterado.
6. **Pendente — MEF independente:** modelo não linear com contato e parafuso de **um grupo de 2–3 fileiras** em MEF sólido/casca externo (CalculiX/Abaqus/Mecway com plasticidade e contato), para verificar a Camada A multi-fileira e H1. Sem ele, a divergência de 7–25 % não pode ser classificada como segura.
7. **Pendente — exemplos publicados para 6.3.5.3/6.3.5.4:** não encontrei exemplo resolvido da NBR 8800:2024 para este item; deve-se buscar na bibliografia brasileira (por exemplo, exemplos de ligações do CBCA) e conferir. Enquanto isso a única verificação independente do 6.3.5.4 é mecânica (seção 4.3).

## 8. Conclusão e decisões necessárias

**Viabilidade:** Camada B — pode ser implementada, com validação já feita. Camada A — pode ser implementada como **verificação condicionada**, mas só depois das decisões abaixo; a norma permite a aplicação à mesa do pilar, porém não define H1–H3 e a interação de fileiras.

Decisões que preciso de você:
1. **H1:** adotar `b = m` (com 0,8 r) como hipótese complementar, ou `b` literal (mais conservador, menor risco)?
2. **H3:** distribuição por fileira — elástica (mais rigorosa), igual (atual) ou a mais desfavorável das duas?
3. **Multi-fileira:** aceitar a soma por largura tributária (NBR literal) com a divergência registrada, ou exigir antes o MEF independente do item 7.6? Minha recomendação: **só habilitar a Camada A como critério de aprovação para 1 fileira**, mostrar a multi-fileira como "NÃO VERIFICÁVEL — validação pendente" **até o MEF independente**, e enquanto isso usar a Camada B para sinalizar.
4. **Aviso de divergência** entre camadas (sim/não, limiar).
5. **Migração** da ferramenta momento-rotação para o `TStub` compartilhado (muda resultados: método 1 → 2, γ).
6. **Método 1 ou 2** para a camada B (recomendo 2, validado).

Nada será implementado até a sua aprovação.
