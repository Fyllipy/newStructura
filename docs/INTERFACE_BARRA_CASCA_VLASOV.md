# Interface barra com empenamento (Vlasov) × casca — documento técnico (Etapa 3, WP4)

Estado: **proposta, nada implementado**. Este documento fundamenta uma decisão; não altera o solver.
Evidências citadas vêm de execuções registradas em `relatorio_etapa3.md` (Etapa 2 + Etapa 3); itens **[PREVISTO]** não foram executados.

## 1. Problema

A barra do `rds` tem 7 GDL por extremidade quando há empenamento restringido: `u, v, w, θx, θy, θz` e `θ' = dφ/dx` (7º GDL).
Uma casca tem 6 GDL por nó e **não possui GDL de empenamento**: o empenamento é uma consequência da distribuição de `u_x` entre os nós da seção.
Quando uma barra Vlasov termina numa região em casca, o que atravessa a fronteira é:

| Grandeza | Barra | Casca |
|---|---|---|
| Translações e rotações de corpo rígido da seção | 6 GDL do nó | `u_i, θ_i` de cada nó da borda |
| Empenamento | `θ'` (7º GDL) | `u_x(s)` por nó da borda, livre |
| Esforço associado | bimomento `B = −E·Cw·φ''` | par de forças axiais autoequilibradas nas mesas |

Nenhum vínculo atual do solver relaciona `θ'` à distribuição de `u_x` dos nós de borda da casca.

## 2. Cinemática de referência

Para uma seção de perfil de parede fina com contorno `s` e coordenada setorial principal `ω(s)` (origem no centro de cisalhamento):

```
u_x(x, s) = u0(x) − y(s)·θz(x) + z(s)·θy(x) − ω(s)·θ'(x) + ũ(x, s)
```

- Os quatro primeiros termos são a cinemática da barra (Euler/Timoshenko + Vlasov), com `θ' = φ'`.
- `ũ` é o que a barra **não** representa: distorção da seção, *shear lag*, flexão local de mesas.
- O comprimento de transição em que `ũ` não é desprezível é da ordem da maior dimensão da seção (Saint-Venant).

Consequência: a interface correta **impõe** os quatro primeiros termos nos nós da borda da casca e **deixa `ũ` livre**.

## 3. O que foi medido hoje (e por que a interface atual é inadequada)

Experimentos executados (Etapa 2, WP5, e Etapa 3, D1), viga I em balanço com torque na ponta, raiz impedida; barra Vlasov ligada a trecho em casca por `RigidLink` kind 0:

1. Com a penalidade padrão `1e7`, o resultado **antes** da correção D1 era `φ_ponta = −99,4 %` da referência e reação de torção na raiz de **3,9 %** do torque aplicado (o solver "fixava automaticamente" GDL legítimos). Causa: defeito D1 (limiar de "GDL sem rigidez" inflado pelas penalidades), **já corrigido** no fonte (`solver_core.cpp`); a DLL distribuída não foi reconstruída.
2. **Depois** de D1, a reação de torção na raiz fecha em `0,9999` do torque (equilíbrio global correto, estável de `p = 1e5` a `1e8`), porém `φ_ponta` continua **−63 %** da referência de barra Vlasov e `θ'` na barra no lado da interface cai **−52 %**: a interface por corpo rígido **restringe o empenamento** da seção de transição (todos os nós da borda seguem a seção rígida plana, sem o termo `−ω·θ'`). Este é um erro de modelagem, não numérico, e **não depende da penalidade**.
3. Casca pura × Vlasov analítico (sem interface): a casca fica **+5 % a +8 %** do Vlasov de parede fina (efeito de espessura real, cantos e *shear lag* — não é defeito).
4. Nenhum vínculo do solver (diafragma, `RigidLink` 0–3, `LinearConstraint`) preserva a continuidade de empenamento barra↔casca. O diafragma ainda deixa uma rótula de flexão.

## 4. Alternativas

Notação: `N` nós de borda da casca com `u_x,i`; `m` = nó mestre da barra (6 GDL + `θ'`).

### A. Restrição cinemática exata (MPC dura)
Para cada nó `i` da borda: `u_x,i = u0 − y_i θz + z_i θy − ω_i θ'` e, no plano, corpo rígido (`u_y,i`, `u_z,i` com `θx`).
- **Prós:** exata para a cinemática da barra; simples de provar (patch tests por campo imposto).
- **Contras:** impõe também `ũ = 0` na borda ⇒ **enrijecimento local** (a seção da casca fica plana-com-empenamento); superestima rigidez de mesas finas; precisa que o `LinearConstraint` alcance o 7º GDL do nó mestre — **[PREVISTO] hoje não alcança** (os termos de restrição endereçam apenas `dof 0..5`).
- Penalidade: continua `kPenaltyFactor·K_ss`; o limiar físico de D1 evita o falso "sem rigidez".

### B. Restrição ponderada em média (tipo RBE3 / mínimos quadrados)
Os termos de empenamento ficam como **projeção** dos `u_x,i` sobre a base `{1, y, z, ω}` (resíduo `ũ` livre, pesos por área tributária ou largura do contorno):
`[u0, θz, θy, θ'] = argmin Σ w_i (u_x,i − (u0 − y_i θz + z_i θy − ω_i θ'))²`.
- **Prós:** não superrestringe `ũ`; é a formulação natural para "o que a barra enxerga da casca"; reduz o pico de força nos nós da borda (a carga da barra se distribui).
- **Contras:** a barra *recebe* a média, a casca *recebe* a distribuição de forças equivalentes (precisa de transposta consistente para equilibrar bimomento + 5 esforços); exige o mesmo acesso ao 7º GDL; implementação mais invasiva em `constraints.cpp`.

### C. Elemento de interface (transição)
Elemento com 7 GDL de um lado e a borda de casca do outro, rigidez derivada de energia (`½ ∫ (u_x − u_x^barra)ᵀ k (u_x − u_x^barra)` sobre o contorno), com `k` que controla quanto de `ũ` se permite.
- **Prós:** controla rigidez da transição; evita penalidade; generalizável a seções fechadas.
- **Contras:** novo elemento no solver (matriz, esforços, validação); só vale para seções com `ω(s)` conhecida; custo de desenvolvimento alto.

### D. Converter o elemento inteiro (ou até a região de empenamento) em casca
Sem interface Vlasov: toda a barra torcional crítica vira casca (seção completa em chapas) e só as extremidades distantes (≥ 1,5 × maior dimensão da seção) voltam a barras sem empenamento (6 GDL), onde `θ' ≈ 0` por Saint-Venant.
- **Prós:** nenhuma mudança no solver; usa o que está validado (Etapa 2: casca pura +5–8 % do Vlasov).
- **Contras:** malha grande; o usuário precisa definir a região; a barra "sem empenamento" na fronteira introduz o mesmo erro de A (restringe) em escala menor, mas com `θ'` já desprezível.

## 5. Comparação

| Critério | A | B | C | D |
|---|---|---|---|---|
| Exatidão da cinemática barra→casca | exata, `ũ = 0` | média, `ũ` livre | controlável | n/a (sem interface Vlasov) |
| Risco de superrestrição local | **alto** | baixo | controlável | médio (na fronteira de barra simples) |
| Mudança no solver | `LinearConstraint` com 7º GDL | idem + transposta consistente | novo elemento | **nenhuma** |
| Impacto no condicionamento | penalidade adicional (rigidez de mesa × `1e7`) | menor | nenhum | nenhum |
| Fechado/aberto | ambos (com `ω` correto) | ambos | ambos | ambos |
| Custo/risco | médio | alto | muito alto | **baixo** |
| Verificável pelos testes atuais | parcialmente | não | não | sim |

## 6. Recomendação

1. **Curto prazo (sem tocar o solver):** adotar **D** como única opção suportada pela função "converter barra em casca": se a barra tem Cw > 0 (empenamento restringido) e a conversão é parcial, **avisar** que a interface barra→casca não preserva o empenamento (erro medido de −50 % a −60 % em `φ`) e oferecer converter o elemento inteiro; nunca gerar a interface em silêncio.
2. **Médio prazo:** implementar **B** (ou A como etapa intermediária) no `rds`, condicionado a: (i) 7º GDL alcançável pelo vínculo; (ii) testes abaixo aprovados; (iii) mesmo conjunto de testes executado contra a DLL reconstruída.
3. **Não** implementar C antes de A/B.

## 7. Testes obrigatórios para qualquer alternativa (todos no scratchpad isolado antes de entrar no repositório)

| # | Teste | Critério |
|---|---|---|
| T1 | Patch de campo: impor `u_x = −ω θ'` na borda da casca, `θ'` constante | resíduo interno `< 1e-8·‖K‖‖u‖` e `ũ = 0` |
| T2 | Viga I em balanço, torque na ponta, raiz impedida: barra-Vlasov (referência) × barra→casca por interface × casca pura | `φ_ponta`, `θ'(x)`, bimomento: erro ≤ 5 % vs. casca pura (que já difere +5–8 % do Vlasov) |
| T3 | Idem, com empenamento livre na raiz | `φ` coincide com Saint-Venant ± 2 % |
| T4 | Flexão pura, cortante e axial pela interface | continuidade de `u0, θ, V, M` ≤ 1e-6 relativo (a interface não pode alterar o que já funciona) |
| T5 | Seção U (centro de cisalhamento fora do eixo) e caixão (`Cw ≈ 0`) | U: erro de `φ` ≤ 10 %; caixão: interface neutra |
| T6 | Varredura de penalidade `1e5…1e9` e escala m/mm | variação de `φ` < 1 % na faixa `1e6…1e8` |
| T7 | Equilíbrio global de forças e momentos e bimomento (soma das forças nas mesas / altura) | ≤ 1e-6 relativo |
| T8 | Mecanismo proposital (barra sem apoio de torção) | erro explícito, nunca resultado finito |

## 8. Limitações e itens não testáveis hoje

- **Não testável sem alterar produção:** qualquer interface A/B/C (exige o 7º GDL em vínculo) — só D está disponível.
- A referência `ω(s)` para perfis laminados com cantos arredondados vem de `SectionShapes::plates()` (ainda inexistente) e deve ser calculada independentemente (script em Python padrão) antes de qualquer comparação.
- Os números −63 %/−52 % foram medidos com a malha de casca única da Etapa 2 (`wp5_hibrido.csv`) e RigidLink kind 0; a convergência da discretização transversal ({1,2,4,8,16} × {2,…,32}) **não foi varrida** nesta etapa e deve entrar em T2 antes de qualquer decisão numérica final.
