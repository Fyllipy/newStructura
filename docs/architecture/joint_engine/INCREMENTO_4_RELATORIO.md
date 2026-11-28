# Incremento 4 — Classificação normativa da ligação configurada e comparação com o comportamento requerido

Data: 2026-11-28. Branch `implantacao_dll` (nada commitado).

## 1. O que foi feito

**Classificação normativa (sem classificador novo).** A classe da ligação real vem de `JointClass::analyzeJoint`/`classify` (NBR 8800:2024, 6.1.2.1 e 6.1.2.2) pelo `NbrClassifier` (J2) já existente, alimentado com a resposta M–θ do Incremento 3 e com os **mesmos** limites (EIv/Lv, 0,5 e 25 EIv/Lv) e Kv/Kp que o Incremento 1 usou para o requerido (agora guardados no `RequiredBehaviorItem`). Si = rigidez a 2/3 de Mj,Rd lida da curva; confere com o Sj,ini do Incremento 3 (divergência > 1e-6 gera ressalva).

**Comparação.** `design/jointengine/RealVsRequired.{h,cpp}` (`compareRealWithRequired`) só interpreta duas classes já estabelecidas: requerido {rotulado, semirrígido, rígido, indeterminado} × classe real {rotulada, semirrígida, rígida, rígida pendente de Kv/Kp}. Quatro resultados distintos:

| Resultado | Quando |
|---|---|
| Atende | classe real = requerida (rotulado: Si ≤ 0,5 EIv/Lv; semirrígido: entre os limites, ou ≥ 25 EIv/Lv com Kv/Kp < 0,1; rígido: Si ≥ 25 EIv/Lv e Kv/Kp ≥ 0,1) |
| Não atende | classe real firmemente diferente da requerida (ex.: rotulado requerido com Si > 0,5 EIv/Lv; rígido requerido com Si < 25 EIv/Lv ou Kv/Kp < 0,1) |
| Indeterminado | dado ausente/pendente: sem resposta M–θ ou sem Mj,Rd; E/Iv/Lv ausentes; requerido indeterminado; norma sem critério; Kv/Kp do andar não verificável quando Si ≥ 25 EIv/Lv e a classe depende dele; par ambíguo |
| Não aplicável | par que não é viga–pilar (6.1.2 não se aplica) |

Regras de cuidado: com Kv/Kp pendente, a classe "rígida" fica pendente (não se adota semirrígida em silêncio); Si > 0,5 EIv/Lv basta para "não atende" ao rotulado sem Kv/Kp. Para requerido semirrígido (mola declarada) a comparação é de **classe**; a razão Si/k é informada e a NBR não fixa tolerância (o programa não inventa uma). Sistema/contraventamento declarados, continuidade implícita e ressalvas da resposta mecânica acompanham o resultado como ressalvas/lacunas, sem alterar o veredito.

**Serviço de pareamento.** `ConnectionRequirementService` liga a ligação definida ao par do nó pela viga e pela barra de apoio vinculada (`supMember`); par ausente, ambíguo ou sem apoio quando há vários pares resulta em Indeterminado (nenhum palpite).

**Interface (janela do nó).**
- Aviso vermelho no topo quando alguma ligação definida **não atende** (texto + cor); aviso neutro com os resultados nos demais casos.
- Aba "Comportamento requerido": coluna "Ligação definida: resultado" logo após o comportamento requerido (com a classe real).
- Aba "Definir ligação": coluna "Atende ao requerido".
- Aba "Resposta mecânica": quadro com comportamento requerido, Sj,ini e Si, critério normativo e limites, classificação da ligação, resultado e justificativa; caixa de notas com justificativas, ressalvas, lacunas e hipóteses (da classificação e da resposta mecânica).
- Dispositivo de desenvolvimento: `STRUCTURA_FLOW_SET="chave=valor;..."` altera valores da ligação criada e reabre a janela (usado só na verificação visual).

## 2. Ajuste mínimo no Incremento 3
O serviço de resposta mecânica passa a nomear a resistência do método dos componentes exatamente `"Mj,Rd"` (o `NbrClassifier` a procura por esse nome) e a da NBR `"Mj,Rd,NBR (…)"`. Antes o nome era descritivo e o classificador não achava o dado. Números, curva e testes do Incremento 3 inalterados.

## 3. Arquivos
Novos: `src/design/jointengine/RealVsRequired.{h,cpp}`, `src/app/services/ConnectionRequirementService.{h,cpp}`, `src/tests/TestRealVsRequired.cpp`.
Alterados: `RequiredBehavior.{h,cpp}` (item guarda `limits`, `storey`, `limitsAvailable`), `ConnectionMechanicalResponseService.cpp` (nomes das resistências), `NodeBehaviorDialog.{h,cpp}`, `MainWindowJoints.cpp` (laço calcula a comparação; driver de fluxo registra aviso e quadro), `CMakeLists.txt`, `src/tests/CMakeLists.txt`.

## 4. Testes (`TestRealVsRequired`, 20 testes)
- fronteiras 0,5 e 25 EIv/Lv (±1e-6) para os três requeridos; Kv/Kp = 0,0999 / 0,1 / 0,2 (limite inclusivo);
- Kv/Kp pendente (rígido e semirrígido → Indeterminado; rotulado decidido só pela rigidez);
- dados ausentes: sem resposta, sem limites, requerido indeterminado, norma não suportada, resposta só de rigidez, curva sem Mj,Rd, resposta bloqueada;
- par viga–viga → Não aplicável, sem classe inventada; quatro rótulos distintos;
- **independência:** classe e η recomputados direto de `JointClass::classify` em 14 combinações;
- contexto (continuidade implícita, sistema não contraventado, sistema não declarado) vira ressalva/lacuna sem mudar o veredito; status Valid só sem ressalvas e lacunas; razão Si/k sem tolerância; memória completa; determinismo;
- **integração:** resposta real da chapa de extremidade (Incremento 3) com 7 combinações de requerido × classe (η = 0,33 / 10 / 30);
- pareamento: ambíguo, apoio inexistente, apoio que identifica o par, viga fora do nó, par único com hipótese registrada; tipologia sem modelo → Indeterminado sem números;
- janela: aviso vermelho em "não atende", colunas, quadro e notas; ausência de ligação; "atende"; "indeterminado".

Suíte completa (build limpo, `StructuraCore.lib` removida): **ctest 45/45**. `StructuraDemo` compila (0 erros). Golden `numerics.txt` com o mesmo SHA-256 de antes do incremento (`81f82d53…`); nenhum golden regravado; `rds/mefs` e `.enc` sem alteração.

## 5. Teste visual no aplicativo real (nó 10, Metalicas Restaurante)
Registros e imagens em `visual/inc4_*` (as do Incremento 3 foram movidas para `visual/inc3_anteriores/`).
- **Sem declaração, chapa de extremidade padrão (2 fileiras):** Sj,ini = 741,2 kN·m/rad; η = 26,18 ≥ 25, mas Kv/Kp do andar não pôde ser verificado → classe "semirrígida ou rígida (pendente de Kv/Kp)"; requerido rígido por continuidade implícita → **Indeterminado**, com a lacuna declarada.
- **1 fileira (`tRows=1`), sistema/contraventamento declarados:** Sj,ini = 349,0 kN·m/rad; η = 12,32 → semirrígida; requerido rígido → **NÃO ATENDE**, aviso vermelho no topo, coluna da aba 1 em vermelho, quadro e justificativa na aba 3.
Layout corrigido durante o teste: resultado da ligação movido para perto do requerido (estava fora da área visível); frase do veredito não é repetida no aviso.

## 6. Limitações
- Só o sentido positivo do momento é classificado (o sentido inverso não é calculado no Incremento 3; não se presume simetria).
- Atende e Não aplicável foram exercitados apenas nos testes automatizados; no aplicativo real apareceram Indeterminado e Não atende (o modelo de amostra não declara liberação nem mola, então o requerido real é sempre "rígido por continuidade implícita"; rotulado/semirrígido requerido só foram cobertos por testes).
- Kv/Kp do andar não é determinável no modelo de amostra; enquanto isso a classe rígida permanece pendente (comportamento correto, mas o resultado fica Indeterminado).
- Requerido semirrígido: comparação de classe; a razão Si/k é só informativa.
- Não testados no app real: botão "Declarar sistema estrutural…" e a resposta "Não" na substituição.
- Sem capacidade de rotação, pós-pico ou análise cíclica (fora do escopo). Resultado não substitui validação experimental nem o dimensionamento da ligação.

## 7. Git
Alterações não commitadas na branch `implantacao_dll`.
