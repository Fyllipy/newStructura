# Incremento 5 — Resposta mecânica nos dois sentidos do momento e nova tipologia (emenda de viga)

Data: 2026-11-28. Branch `implantacao_dll` (nada commitado).

## 1. Frente A — dois sentidos do momento

**Convenção (explícita na interface):** sentido POSITIVO = mesa SUPERIOR da viga tracionada (fileiras de parafusos da extensão da chapa acima da mesa); NEGATIVO = mesa INFERIOR tracionada (fileiras abaixo da mesa inferior).

**Como o negativo é montado (não é a curva positiva com sinal trocado):** o cadastro da chapa de extremidade tem `tRows` fileiras acima da mesa (zona tracionada no sentido positivo) e `sRows` abaixo (a "zona comprimida/de cortante"). No sentido negativo essas `sRows` fileiras passam a ser as tracionadas e as `tRows` ficam no lado comprimido (a extensão da chapa acima limita a dispersão `sp`, 6.2.6.2). O modelo físico é remontado com as fileiras que existem naquele lado e as verificações da NBR (nível C) são avaliadas com as zonas trocadas. Mesas soldadas (144–146): a entrada física é idêntica nos dois sentidos (viga e pilar duplamente simétricos) e a simetria é registrada como **demonstrada pela configuração** (cada sentido é calculado). Quando o modelo não permite um sentido, ele fica indisponível, sem números e sem espelhamento.

**Implementação.**
- `JointComponentService::evaluate(c, E, Direction)` (o `evaluate(c, E)` legado continua idêntico; o sentido negativo não aparece no `behavior.negative` legado — `TestJointClassification` segue passando).
- `ConnectionMechanicalResponseService::evaluate(..., Direction)` e `evaluateBoth` → `ConnectionMechanicalBoth` com duas respostas independentes (cada uma com `MechanicalResponse`, proveniência, hipóteses, limitações, componentes, fileiras, níveis de momento, `participants`, fingerprints). CFP igual (a configuração é uma); AFP distinto por sentido (o parâmetro `direction` entra só no AFP do negativo, de modo que o AFP do positivo é idêntico ao do Incremento 3). O aviso legado "sentido inverso não calculado" foi retirado.
- Classificação: `compareRealWithRequired(..., directionLabel)` classifica cada sentido com a rigidez Si (2/3 Mj,Rd) e o Mj,Rd do próprio sentido, pelo mesmo `NbrClassifier`/`JointClassification`; `combineDirections` gera o resultado global.
- **Regra do resultado global (decisão conservadora do programa; a NBR 8800:2024, 6.1.2 não define como reunir classes direcionais e isso é dito na interface):** Atende somente se os dois sentidos atendem; Não atende se algum não atende; Indeterminado se nenhum não atende mas algum é indeterminado/indisponível; Não aplicável se ambos o são. Não há média de rigidezes nem de resistências. Um sentido indisponível impede "Atende" global.
- Interface (aba "Resposta mecânica"): gráfico com a curva positiva (1º quadrante, azul) e a negativa (3º quadrante, laranja), cada uma só se calculada (sentido indisponível mostra a mensagem, sem curva); tabela de indicadores por sentido (resposta, Sj,ini, Mj,Rd, componente de teto, fileiras tracionadas participantes, classe, comparação); clicar numa linha troca o detalhe (níveis de momento, componentes, hipóteses, limitações, classificação) para o sentido; quadro do resultado global com a regra; aba rolável. Coluna "Ligação definida" mostra o global e as classes `[+ / −]`.

## 2. Frente B — nova tipologia com modelo mecânico: emenda de viga com chapas de extremidade (catálogo 135)

**Inspeção objetiva (somente o necessário):** 

| Candidata | Configuração | Verif. NBR | Componentes reutilizáveis | Fundamentação | Decisão |
|---|---|---|---|---|---|
| Emenda de viga com 2 chapas de extremidade (135) | sim (catálogo) | sim | k5, k10, T-stub, distribuição 6.2.7.2, curva 6.3.1 | **completa na EN 1993-1-8**: Tabela 6.10 (conferida na página do PDF local), 6.3.3.1(5), 6.2.7.2(10) | **implementada** |
| Viga–pilar com perfis T (143) | sim | sim | k3–k5, k10 (1 fileira) | faltam: espalhamento de compressão do T-stub (sem regra na EN), k11/k12 do alma-T, benchmark de junta completa (o próprio `docs/METODOLOGIA_COMPORTAMENTO_MECANICO_TSTUB.md` lista fases B+/C/D como PLANEJADO/LACUNA) | não implementada (lacuna específica abaixo) |
| Base de pilar (EN 6.2.8/6.3.4) | sim | sim | parcial | fórmulas na EN, porém depende de N (excentricidade) e o nó de apoio tem uma só barra (o fluxo do Incremento 1 exige par de barras) | não implementada |
| Cantoneiras, talas de cisalhamento, gussets, tubulares | sim | sim | não | sem formulação mecânica fundamentada na base (ver matriz) | não |

**Modelo:** `JointComponents::ModelInput::beamSplice` (padrão falso; ligação viga–pilar inalterada). Rigidez: k1–k4 inexistentes; por fileira, `k5` da chapa 1 e `k5` da chapa 2 (iguais, em série) e `k10` com Lb = 2 tp + arruelas + (cabeça + porca)/2; `keff,r = 1/(2/k5 + 1/k10)`; `keq`, `zeq` por 6.30–6.31; `Sj,ini = E zeq² keq`. Resistência: mesmo método de 6.2.7.2 omitindo o pilar (T-stub da chapa modos 1–3, alma da viga tracionada, distribuição com a regra de 1,9 Ft,Rd) e teto = mesa/alma da viga comprimidas. Curva: eq. 6.28 com ψ = 2,7 (Tabela 6.8, chapas de extremidade). `JointComponentService`: nova família `EndPlateSplice` (só catálogo 135; a variante 147 "flangeada fora do nó" liga a viga ao pilar e não tem modelo). Verificações da NBR da própria emenda (catálogo, sem as de pilar) alimentam a ressalva `nbrBelowCurve`.

**Integração com o fluxo:** a janela do nó lista agora também os pares viga–viga (emenda); o `ConnectionRequirementService` reconhece tipologias viga–viga e escolhe o par viga–viga, de modo que a classificação de rigidez de 6.1.2 resulta em **Não aplicável** (a resposta mecânica existe e é utilizável).

**Verificação numérica e fundamentação (o que é e o que não é):**
- Implementação independente em Python (biblioteca padrão; `docs/validacao_emenda/splice_independent.py`, saída em `saida_splice_independent.txt`) reproduz o C++ para 1, 2 e 3 fileiras: Sj,ini = 87 832,47 / 99 961,84 / 103 662,75 kN·m/rad; Mj,Rd = 45,900 / 84,055 / 110,680 kN·m; zeq, keff,r, k5, k10, resistências individuais e efetivas por fileira (tolerâncias 2e-8 a 2e-5, conforme as casas impressas).
- **Isto é reprodução de equações e verificação numérica. Não é validação experimental nem profissional**: não há ensaio nem MEF desta emenda no projeto, e as duas implementações usam as mesmas fórmulas da EN.

## 3. Testes
- `TestMechanicalDirections` (19): simetria demonstrada × assimetria; fileiras participantes; **identidade de espelho** (positivo de (a,b) = negativo de (b,a), bit a bit); recomputação independente do sentido negativo com `ModelInput` montado à mão; mecanismo por fileira e resistências/rigidezes diferentes entre sentidos; nível NBR do negativo; **regressão** do positivo (bit a bit, AFP/CFP, memória) e do serviço legado; fingerprints por sentido; hipóteses/limitações por sentido; tipologias sem modelo e dados insuficientes sem números; mesas soldadas; classificação por sentido com a rigidez de cada um; tabela de 16 combinações do resultado global; sentido indisponível impede "Atende" global.
- `TestBeamSplice` (12): Python independente; k5/k10 por componente; curva eq. 6.28; dois sentidos e espelho; sensibilidades (espessura, E, parafuso) e limites de série; fingerprints; 147/143/`kSplice` sem modelo; geometria inválida sem números; nível NBR; matriz de capacidade; **Não aplicável** com par viga–viga escolhido.
- `TestRealVsRequired` (+2): janela com os dois sentidos e resultado global; sentido indisponível não é espelhado (colunas "—").
- `TestRequiredBehavior`: contagem de tipologias com curva M–θ passa de 3 para 4 no catálogo (135) — expectativa atualizada, não ocultada.
- Suíte completa (build limpo): ver seção 5.

## 4. Verificações visuais no app real (nó 10, Metalicas Restaurante; `visual/inc5_*`)
| Caminho | Resultado |
|---|---|
| **Declarar sistema e contraventamento** | botão clicado → Parâmetros (Pórtico; Contraventada) → OK → janela reaberta com "sistema/contraventamento: compatível" onde era "indeterminada" (`inc5_declara_e_nao_*`) |
| **Substituição, resposta "Não"** | pergunta exibida; ligação id 1 mantida (mesmo fingerprint) (`inc5_declara_e_nao_*`) |
| **Atende** | modelo com mola rotacional k = 300 kN·m/rad declarada (requerido semirrígido) e chapa 1×1 fileira: Sj,ini = 348,98 kN·m/rad (η = 12,32) nos dois sentidos → semirrígida → **Atende** global; simetria demonstrada (`inc5_atende_*`) |
| **Não aplicável** | emenda 135 criada pelo fluxo no par viga–viga 14–48 → **Não aplicável** (+ e −); curva positiva 6,71 kN·m / negativa 2,82 kN·m (`inc5_emenda_*`) |
| Dois sentidos (chapa viga–pilar) | positivo 741 kN·m/rad, 2,86 kN·m (2 fileiras); negativo 349 kN·m/rad, 1,14 kN·m (1 fileira); global Não atende (`inc5_basico_*`) |
O fluxo usa o driver de desenvolvimento (`STRUCTURA_NODE_FLOW`, agora com `STRUCTURA_FLOW_PLAN/PAIR/TYPE/SET/REPLACE/SYSTEM/BRACING`). O "Atende" exige um modelo com a idealização declarada; o modelo de amostra original não tem (continua com continuidade implícita).

## 5. Build, testes e preservação
- Build limpo (`StructuraCore.lib` removida): sem erros. **ctest Release: 47/47** (45 anteriores + `TestMechanicalDirections` + `TestBeamSplice`).
- `StructuraDemo` compila (0 erros; nenhum membro novo em `MainWindow` exigiu stub).
- Goldens: `numerics.txt` SHA-256 `81f82d53…` (idêntico ao de antes dos Incrementos 3 e 4) e `momento_rotacao.txt` `a17c4238…`; nenhum golden regravado. Os testes de regressão legados (`TestJointClassification`, `TestMomentRotationRegression`, `TestJointEngineRegression`, goldens numéricos) passam com `JointComponents` alterado só por flag.
- `rds.dll`, `mefs.dll` e `.enc`: sem alterações no `git status`.
- Git: alterações não commitadas na branch `implantacao_dll`.

## 6. Arquivos
Novos: `src/tests/TestMechanicalDirections.cpp`, `src/tests/TestBeamSplice.cpp`, `docs/validacao_emenda/splice_independent.py`, `docs/validacao_emenda/saida_splice_independent.txt`, este relatório, imagens `visual/inc5_*` (imagens do Incremento 4 movidas para `visual/inc4_anteriores/`).
Alterados: `src/design/JointComponents.{h,cpp}` (flag `beamSplice`, padrão falso), `src/design/jointengine/RealVsRequired.{h,cpp}` (rótulo de sentido, `combineDirections`, `DirectionalComparison`), `src/app/services/JointComponentService.{h,cpp}` (sentido, família emenda), `ConnectionMechanicalResponseService.{h,cpp}` (sentido, `evaluateBoth`, participantes), `ConnectionRequirementService.{h,cpp}` (dois sentidos; tipologias viga–viga), `src/ui/NodeBehaviorDialog.{h,cpp}` (aba com dois sentidos, curva dupla, indicadores, pares viga–viga, `requestDeclare`), `src/ui/MainWindowJoints.cpp` (laço calcula os dois sentidos; driver), `src/tests/TestRealVsRequired.cpp`, `TestRequiredBehavior.cpp`, `src/tests/CMakeLists.txt`, `docs/architecture/joint_engine/MATRIZ_COBERTURA_LIGACOES.md`.

## 7. Limitações
- Sentido negativo da chapa viga–pilar assume que as fileiras de "cortante" abaixo da mesa inferior podem trabalhar à tração (a ligação não foi dimensionada para isso; o resultado é mecânico, não verificação) — declarado na limitação do sentido.
- Nas 972 geometrias de chapa viga–pilar amostradas o componente de teto (painel/alma comprimida/mesa comprimida) coincidiu nos dois sentidos; o que difere é a limitação por fileira, Sj,ini e Mj,Rd.
- O resultado global é regra conservadora do programa, não prescrição normativa.
- Emenda: chapas iguais; sem capacidade de rotação, escorregamento ou estado individual dos componentes; sem ensaio/MEF; classificação de rigidez não se aplica (viga–viga).
- 143 (perfis T) segue sem modelo. **Lacuna específica:** regra de espalhamento da compressão do T-stub e k11/k12 do T–mesa da viga não estão na EN para esta junta, e não há benchmark de junta completa; sem isso o modelo seria inventado.
- "Atende" no app real só com idealização declarada no modelo (mola); o modelo de amostra tem continuidade implícita.
- Kv/Kp do andar continua indeterminado no modelo de amostra para alguns pares (classe rígida pendente).
