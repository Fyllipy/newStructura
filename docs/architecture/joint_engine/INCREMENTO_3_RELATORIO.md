# Incremento 3 — Resposta mecânica e curva M–θ de ligações definidas

Data: 2026-11-27. Branch `implantacao_dll` (nada commitado).

## 1. Verificação manual do fluxo (antes do Incremento 3)

Executada no aplicativo real (`build-dev/Release/Structura.exe`, modelo `Conversor/saida/Metalicas Restaurante.dat`, nó 10) por um driver só de desenvolvimento (`STRUCTURA_NODE_FLOW=10`, `main.cpp` → `MainWindow::runNodeFlowForAutomation`). O driver percorre as janelas modais que o próprio programa abre. Registro e imagens: `visual/fluxo_registro.txt`, `visual/fluxo_*.png`.

| Etapa | Resultado |
|---|---|
| Seleção do nó 10 → janela "Comportamento do nó" | ok (0 ligações definidas) |
| Pedido "chapa de extremidade", par 1 | `NodeConnectionsDialog` abriu com a ligação criada (id 1, nó 10, viga 14, extremidade inicial, apoio = barra 18) |
| Aplicar e fechar → retorno | janela do nó lista a ligação definida; aba "Resposta mecânica" preenchida |
| Substituição | pergunta "Já existe uma ligação nesse nó para essa viga. Substituir?" apareceu; resposta Sim → 1 ligação no modelo (id 2), sem duplicar |
| Salvamento | `saveToDat` ok; `[CONNECTIONS]` presente com a ligação |

Problemas reais encontrados e corrigidos: rótulo "Mj,Rd" fora da área do gráfico; tabela de componentes comprimida; fração de flexibilidade calculada de forma inconsistente entre componentes do nó e fileiras.
**Não exercitado:** resposta "Não" na substituição (o driver aceita `STRUCTURA_FLOW_REPLACE=no`, não foi executado); botão "Declarar sistema estrutural…" no app real (coberto só pelo código e pela compilação; o laço reavalia após `onDesignSettings()`).

## 2. Botão de declaração
"Declarar sistema estrutural e contraventamento…" na janela do nó chama `MainWindow::onDesignSettings()` (configuração existente em Parâmetros, mesma persistência) e reavalia o nó ao voltar. Sem campos duplicados.

## 3. Tipologias com modelo mecânico (verificação objetiva)
`TypologyCapabilityService::list()` (8 legadas + 50 do catálogo) cruzada com `JointComponentService::isSupported`. Têm modelo mecânico **e** curva M–θ exatamente **quatro** entradas:
- legada `kEndPlate` (chapa de extremidade, id 7);
- catálogo 144, 145 e 146 (mesas soldadas, com e sem tala de alma).

Correspondência com "as quatro entradas": são essas. O relatório do Incremento 2 falava de "uma tipologia com EnComponentMethod" — era só a legada; correção: são 1 legada + 3 do catálogo. A 143 (perfis T, `MomentTStub`) **não** tem modelo; antes aparecia errado porque a sonda não recebia o preset da variante (corrigido). O teste agora exige legada = 1 e catálogo = 3.

## 4. Implementação
`ConnectionMechanicalResponseService::evaluate(connection, E_MPa, pontos)` reaproveita `JointComponentService` (método dos componentes, EN 1993-1-8; curva fechada EN 6.3.1, ψ = 2,7) e organiza em `MechanicalResponse` do JME com proveniência e fingerprints (configuração e análise). Entrega: curva M–θ, Sj,ini, Mj,Rd, secante/tangente em Mj,Rd, braço z, tabela de componentes (k, F,Rd, modo de ruptura, flexibilidade e fração), fileiras, 5 níveis de momento, hipóteses e limitações. UI: aba "Resposta mecânica" com gráfico. Sem E > 0 explícito não há resposta (o serviço legado trocaria silenciosamente por 200000 MPa; este não).

## 5. Comparação independente (testes)
`TestConnectionMechanicalResponse` — 14 testes, todos passam:
- θ de cada ponto = eq. (6.28) recalculada no teste a partir de Sj,ini e Mj,Rd (tol. 1e-9), curva monotônica e terminando em Mj,Rd, para 1, 2 e 3 fileiras;
- Sj,ini = E·zeq²/(Σ1/k_nó + 1/keq) recomputado das fileiras e componentes;
- keff,r = 1/Σ(1/ki); frações de flexibilidade somam 1 por fileira e no total;
- valores idênticos (bit a bit) ao serviço legado; 144 também disponível;
- indisponíveis sem números: 143, cisalhamento, cantoneira dupla, gusset, base, emenda, tubular; pilar não informado; E ≤ 0, NaN, ∞; pilar digitado e não confirmado; pontos < 2;
- fingerprints reprodutíveis e sensíveis a mudança de configuração e de E; ressalva `nbrBelowCurve` acompanhada.

Resultado numérico do fluxo real (ligação criada no nó 10): Sj,ini = 741,2 kN·m/rad; Mj,Rd = 2,862 kN·m (governa o painel de alma ao cisalhamento); fileiras = 64 % da flexibilidade total. Valores dependem da seção do pilar vinda do modelo (barra 18) e das hipóteses abaixo.

## 6. Build e testes
- build limpo (`StructuraCore.lib` removida), ctest Release: **44/44** (43 anteriores + 1 novo).
- `StructuraDemo` compila (stubs adicionados, 0 erros).
- Golden `numerics.txt`: última modificação 17:41, anterior ao Incremento 3; diferença contra o snapshot pré-S-03 são só os rótulos autorizados na S-03. Nenhum golden regravado neste incremento. DLLs `rds/mefs` e `.enc` sem alteração no `git status`.

## 7. Limitações (declaradas na UI)
- Capacidade de rotação **indisponível** (EN 6.4 não implementado); a curva termina em Mj,Rd.
- Estado individual dos componentes ao longo da curva **não existe** no modelo: a não linearidade é do nó (forma fechada, sem iteração; convergência "não aplicável").
- Sentido inverso de momento (fileiras na outra mesa) não calculado.
- `nbrBelowCurve`: a ligação cadastrada pode não sustentar o Mj,Rd da curva; a ressalva acompanha o resultado.
- Resposta não é classificação normativa da ligação e não substitui validação experimental.
- Só as 4 tipologias do item 3.

## 8. Git
Alterações não commitadas na branch `implantacao_dll` (novos: `ConnectionMechanicalResponseService.{h,cpp}`, `TestConnectionMechanicalResponse.cpp`, este relatório, imagens `visual/`).
