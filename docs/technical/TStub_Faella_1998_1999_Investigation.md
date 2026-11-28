# Investigação de Faella, Piluso e Rizzano (1998) e (1999) — Fase C6.1

**Estado: NÃO CONCLUÍDA.** As duas publicações **não foram obtidas**; portanto nada do que segue substitui a leitura delas. O documento registra as referências completas, as tentativas de acesso, **somente o que Piluso et al. (2001) afirmam** sobre essas fontes (identificado como informação *secundária*), o que foi verificado de forma independente e as lacunas. Sem validação profissional.

## 1. Referências completas (como impressas em [P1]/[P2])
- **Faella, C.; Piluso, V.; Rizzano, G. (1998).** *Experimental analysis of bolted connections: snug versus preloaded bolts.* Journal of Structural Engineering, ASCE, 124(7), 765–774. DOI 10.1061/(ASCE)0733-9445(1998)124:7(765).
- **Faella, C.; Piluso, V.; Rizzano, G. (1999).** *Structural steel semirigid connections.* CRC Press, Boca Raton, Fla. (livro; sem DOI em [P1]/[P2]).
- Também citadas por [P2] e relacionadas à capacidade de deformação do T-stub: Faella et al. (1996a), *Costruzioni Metalliche* 4, 15–31; (1996b), IABSE Colloquium Istanbul, 339–348; (1997a) *Plastic deformation capacity of bolted T-stubs*, STESSA'97, 572–581; (1997b) *Prediction of bolted connection ductility*, STESSA'97, 582–591; (1997c) *A new method to design extended end plate connections…*, J. Constr. Steel Res. 41(1), 61–91. **Nenhuma foi obtida.**

## 2. Disponibilidade e tentativas
| Fonte | Situação | Evidência |
|---|---|---|
| Faella et al. (1998), J. Struct. Eng. 124(7) | **acesso fechado**; sem cópia aberta em repositório | consulta ao OpenAlex (registro completo): `is_oa=false`, `oa_status=closed`, `oa_url=null`, `any_repository_has_fulltext=false` |
| Faella et al. (1999), CRC | **não encontrado** como obra aberta | busca no OpenAlex por título/autores não devolveu o livro |
| 1996–1997 | não pesquisadas individualmente | — |
Tentativas adicionais de acesso a fontes com restrição (HTTP 403) **foram bloqueadas pela política de permissões da sessão** e não foram insistidas. Nenhuma citação secundária foi usada no lugar das fontes originais (as expressões abaixo vêm de [P1]/[P2], não de Faella).

## 3. O que se sabe, por Piluso et al. (2001) — **informação secundária**
| Item | Como aparece | Fonte |
|---|---|---|
| Rigidez do elemento T, parafusos *snug* | `K = 0,5 E b_eff t_f³ / m³`, "stiffness K for snug-tightened bolts is given by (Faella et al. 1998)" | [P1 (38)] |
| `b_eff` | "effective width for stiffness calculation (Faella et al. 1998)"; no apêndice: `b_eff = min[d_h + 2m ; b]` | [P1] p. 691; [P2] p. 702 |
| `m'` e `ζ` | "Computation of the stiffness of the single T-element (Faella et al. 1998): `ζ = 0,16 (m+0,8r)/t_f − 0,08`, `m' = m + 0,8r − ζ r`", e `K = 0,5 E b_eff t_f³/m'³` | [P2] p. 702 |
| Largura efetiva na resistência | `M_y = b_eff t_f² f_y/6` com `b_eff` de Faella 1998 | [P1 (4)]; [P2] |
| Mecanismo 3 | "procedimento similar (Faella et al. 1999)" — **sem equações** | [P1] p. 693 |
Não consta em [P1]/[P2]: a dedução de `K`, a faixa de validade de `ζ`, a calibração com os ensaios *snug* × *preloaded*, a definição precisa de `r` em seções laminadas × soldadas, e as condições de contorno e de aperto da campanha de 1998.

## 4. Verificações independentes realizadas (sem a fonte)
1. **Reprodução numérica do exemplo trabalhado** de [P2] (espécime 7), onde as fórmulas de Faella são aplicadas: `ζ=0,7724`, `m'=53,1952 mm`, `K=230 046 N/mm`, `δ_y=1,0285 mm`, `K_i=111 567 N/mm` reproduzidos (≤ 0,004 %; `research/tstub_piluso/saida_validate_published.txt`). Isto confirma a **aritmética** do exemplo, **não** a formulação original.
2. **Coerência dimensional e leis de escala** (testes `test_propriedades`): `K ∝ t_f³`, `∝ b_eff`, `∝ m'⁻³` (por construção da fórmula).
3. **Comparação com um MEF elástico 3D independente** (`docs/validation/TStub_FEM_Elastic_Study_Results.md`): geometria **assumida** (placa, sem filete, `t_w` assumido). O resultado não valida nem refuta `K` do modo como foi publicado por Faella (geometria e domínio diferentes).
4. **Plausibilidade física de `ζ`:** `ζ` cresce com `(m+0,8r)/t_f` (efeito do filete); não foi possível verificar o sentido ou a faixa porque a fonte não foi lida.

## 5. Comparação com Piluso et al. (2001)
Não é possível afirmar se Piluso reproduziu **integralmente** as formulações de Faella (1998) ou se introduziu modificações: isso exige a fonte original. Observa-se apenas que [P1] atribui `K` e `b_eff` a Faella (1998) e que o apêndice de [P2] os aplica **exatamente** como impressos. A divergência entre "área líquida" (texto) e área bruta (cálculo) para `A_b` em [P2] **também não pode ser esclarecida** sem a fonte (se vem de Faella 1998 ou é de Piluso).

## 6. Implicações para o Structura
- A rigidez inicial de Piluso **depende de Faella (1998)** (`K`, `b_eff`, `m'`) e, enquanto essa fonte não for lida, **não deve ser implementada como fórmula "de Faella"**: apenas como entrada `UserProvided`, ou como fórmula rotulada "transcrita de [P1]/[P2], fonte original não verificada".
- A mistura com a rigidez EN permanece indefinida (ver `TStub_Stiffness_Compatibility.md`).
- A origem de `b_eff` (`d_h+2m` vs `b`) é geometria de dispersão da força do parafuso, mas sua validade para T-stubs com mais de 2 parafusos ou chapas de extremidade **não está demonstrada**.

## 7. Lacunas e como resolvê-las
1. Obter Faella et al. (1998) por via legítima (assinatura/biblioteca ASCE; pedido ao autor; empréstimo entre bibliotecas) e o capítulo correspondente do livro de 1999.
2. Ler: definição de `K`, `b_eff`, `ζ`, `m'`, faixa de validade, tipo de aperto, geometria ensaiada, dados de rigidez medida (**essenciais** para decidir entre Piluso/Faella e EN).
3. Com os dados de rigidez medida de 1998, reconstruir um benchmark independente de rigidez inicial.
Até lá, **C6.1 permanece NÃO CONCLUÍDA**.
