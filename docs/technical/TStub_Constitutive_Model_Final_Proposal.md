# Proposta constitutiva final do T-stub (revisão da proposta da Fase C — C6.6)

Revisa o §6 de `docs/RELATORIO_FASE_C_TSTUB_FORCA_DESLOCAMENTO.md` com as evidências da Fase C6. **Não implementa nada.** Sem validação profissional. API correspondente: `docs/architecture/TStubForceDisplacement_API_Specification.md`.

## 1. Respostas às nove perguntas do prompt
**1. A curva contínua deve ser armazenada como tabela de amostras?** **Sim.** O modelo de 4 pontos de [P1] subestima a força em média 9,3 % (RMS 10,6 %) frente à curva experimental digitalizada; a curva contínua (as *mesmas* equações aplicadas a qualquer `M`) reduz para −3,9 % (RMS 7,5 %). A curva contínua exige a inversão numérica de `M–χ` (única parte numérica do modelo); armazená-la como tabela monótona evita repetir o cálculo para cada consulta. (`docs/validation/TStub_Piluso_Experimental_Validation.md` §4.)

**2. A interpolação linear é suficiente?** **Sim, com ≥ 128 amostras:** erro máximo da interpolação linear sobre a tabela, frente a uma curva densa de 2000 pontos, nos 12 espécimes: 16 amostras: 2,2 %; 32: 0,97 %; 64: 0,51 %; **128: 0,09 %** (média 0,05 %). A amostragem é uniforme em `M` (portanto, mais densa onde a curva é íngreme em `δ`). Resultado do protótipo (`research/tstub_piluso`); não há segundo método de referência.

**3. Os quatro pontos característicos devem ser preservados?** **Sim**, como parte do resultado: são as definições físicas de [P1] (primeiro escoamento, início do endurecimento, `f_u` no ponto crítico, fratura), são os valores publicados (Tabela 3) e servem de benchmark. A tabela contínua é um **complemento**, não substituto.

**4. Como representar os mecanismos 1 e 2?** Pelo critério `β_u` de [P1 (9), (11)], com `Mechanism::{One,Two}` e as forças `(37)` e `(50)`. **Aviso:** o critério falhou em 1 dos 12 espécimes (espécime 8: previu 2, observado 1, e a Tabela 3 de [P2] usa o 1). Os dois espécimes com `β_u` pouco acima do limite (`β_u/β_{u,lim}` = 1,33 no 6 e 1,31 no 8) tiveram resultados opostos (6 acertou, 8 errou), logo **não há base nos dados para uma regra de aviso** além de "o critério foi testado em 12 espécimes e falhou em 1". Proposta: emitir `Warning{CRITERION_UNVALIDATED_NEAR_LIMIT}` e `CalculableWithCaveats` quando `1 < β_u/β_{u,lim} < 1,4` — faixa **escolhida só para cobrir os dois casos observados, [HIPÓTESE NÃO VALIDADA]**, a ser decidida com o responsável metodológico. O mecanismo nunca é alterado silenciosamente.

**5. Como representar o mecanismo 3 indisponível?** `Status::MechanismUnsupported`, `Mechanism::Three`, **sem** `table` nem pontos; `diagnostics` cita: (i) [P1]/[P2] só dão `F_{u,3}=2B_u`; (ii) a flexão dos parafusos não é considerada e compromete a precisão; (iii) modo frágil. Nenhuma curva é inventada (ver `docs/technical/TStub_Failure_Mode_3_Investigation.md`). Uma "rigidez + queda brusca em `2B_u`" seria **hipótese própria** e fica fora.

**6. Como representar a ausência de comportamento pós-pico?** `postPeakModeled=false`; a tabela termina em `(δ_u,F_u)`; o consumidor recebe `Result::ultimateDisplacement` como **limite do domínio**, não como ponto de uma curva com ramo descendente. A "simulation" de [P2] Fig. 8 mostra queda, mas o método não é descrito.

**7. Como identificar a origem dos parâmetros?** `Quantity{value, Origin, source}` por entrada, com `Origin ∈ {Normative, ManufacturerDeclared, Test, Correlation, ModelAssumption}`; os dois últimos forçam `CalculableWithCaveats` e aviso. Os parâmetros críticos (`ε_h, E_h, E_u, ε_u`) **só** podem ser `Test` sem aviso. **`ε_u=ln(A_0/A_f)` (redução de área, 0,49–0,97 nos ensaios de [P2]) não é a deformação de alongamento das normas** (ex.: 0,17–0,25 nos valores de Faralli et al. 2020 para S235–S450, de outra definição): trocá-los altera a ductilidade prevista de forma não controlada. (`docs/technical/TStub_Material_Properties_Alternatives.md`.)

**8. Como distinguir rigidez EN e Piluso?** `StiffnessSource` no resultado; nenhuma função mistura as formulações; qualquer uso da rigidez EN é `UserProvided` com aviso `MIXED_FORMULATIONS`. Diferença medida: EN/Piluso ≈ 1,6–2,2 (com `l_eff=b_eff`) para a flexão da mesa; o MEF elástico (`docs/validation/TStub_FEM_Elastic_Study_Results.md`) não fecha a questão (resultados 3D de uma geometria declarada como hipótese).

**9. Como impedir que resultados de ensaio sejam usados como resistências de cálculo?** `ResultKind::ModelUltimate` único e obrigatório; nomes de campo `ultimate…` (nunca `design…`/`resistance`); o módulo **não inclui** nem retorna `Check`, `CheckStatus` ou coeficientes parciais; documentação e `memory` repetem "estimativa última calibrada em ensaio; não é resistência de cálculo EN/NBR". Os consumidores normativos continuam usando `TStub::resistance` (resistência plástica de cálculo, `γ_M0=1,0`).

## 2. Proposta resumida
- **Modelo:** Piluso 2001, mecanismos 1 e 2 (reproduzidos e comparados em 12 ensaios); 3: indisponível.
- **Dados do material:** curva verdadeira multilinear completa, informada com origem; sem estimativas por padrão.
- **Rigidez:** `UserProvided` até obter Faella et al. (1998); fórmula `0,5 E b_eff t_f³/m'³` implementável como `PilusoFaella1998Formula` **somente depois** de lida a fonte original.
- **Saída:** 4 pontos + tabela de ≥ 128 amostras (interpolação linear) + `Status` + `Diagnostics`.
- **Fora de escopo:** pós-pico, mecanismo 3, cíclico, pré-tensão, T-stubs sem ensaio de cupom.
