# Incremento 6 — Expansão dos modelos mecânicos de ligações

Data: 2026-11-28. Branch `implantacao_dll` (nada commitado).

## 0. Baseline confirmado
Branch `implantacao_dll`; 159 arquivos modificados/novos não commitados (inalterados em relação ao Incremento 5); `numerics.txt` SHA-256 `81f82d53…`; ctest 47/47 e `StructuraDemo` sem erros (reexecutados no fim). Nenhuma divergência.

## 1. Levantamento e priorização (somente o necessário)
Fonte primária lida: **EN 1993-1-8:2005** (PDF local `normas/`), Tabela 6.10 (combinação de componentes por tipo de junta) e Tabela 6.11 (k1…k6, k10…k16, conferidas na imagem das páginas 95–96).

| Tipologia (catálogo) | Formulação disponível | Componentes reutilizáveis | Capacidade esperada | Prioridade / decisão |
|---|---|---|---|---|
| **147** viga–pilar flangeada fora do nó | EN: mesas soldadas ao pilar (6.2.7.1, k1–k3) + emenda por chapas de extremidade (Tab. 6.10, 6.3.3.1(5)); composição em série por cinemática de molas | `JointComponents` (soldada e emenda, ambas já validadas), `JointComponentService`, classificação viga–pilar | Sj,ini, Mj,Rd, curva M–θ nos dois sentidos, classificação | **1 — implementada (modelo completo)** |
| **143** viga–pilar com perfis T | k4, k5, k10 (uma fileira) existem; **faltam**: regra de espalhamento da compressão do T-stub (a EN não tem para esta junta), k11/k12 do T com a mesa da viga, benchmark de junta completa (o próprio `METODOLOGIA_COMPORTAMENTO_MECANICO_TSTUB.md` lista B+/C/D como PLANEJADO/LACUNA) | parcial | só resistência (nível C, NBR) | **2 — implementada como `ResistanceOnly`** (sem rigidez/curva) |
| 148 talas fora do nó, 131/136 emendas por talas, `kSplice` | EN só dá resistência (6.2.7.1) para talas; k11/k12 existem mas a Tabela 6.10 não traz regra de combinação para emenda por talas e, com parafusos protendidos, k11 = k12 = ∞ | não | — | não implementada |
| Cantoneiras de mesa (flange cleats) | EN completa (6.2.6.6, k6, Tab. 6.9/6.10) | k6, k10–k12 | seria completa | **não há configuração no catálogo** (critério 1): só as mistas 162–167 (NBR Anexo O) |
| Cantoneiras/talas de alma, chapa de extremidade flexível (101–116, 152, 107–108), mistas 161–167 | sem formulação mecânica fundamentada na base (EN não dá rigidez rotacional de ligações flexíveis; EN 1994 Anexo A não disponível) | não | — | não implementada |
| Bases de pilar (legado `kBasePlate`, 172, 173) | EN 6.2.8 e 6.3.4 (k13, k15, k16 conferidos) | parcial | seria completa | **bloqueada**: (i) a rigidez depende de N (Tabela 6.12), (ii) o modelo de dados não tem graute/Ec, (iii) o nó de apoio tem uma só barra e o fluxo do Incremento 1 exige par de barras (alterá-lo seria reescrever o Incremento 1) |
| Tubulares, gussets, nós de treliça, formados a frio | só resistência (CIDECT/NBR); rigidez do nó sem fonte | não | — | não implementada |

**Escolha:** 147 (modelo completo) e 143 (resistência apenas, aceito pela diretriz de usar `ResistanceOnly` quando só há formulação de resistência). Não foi possível fundamentar um **segundo modelo completo**; as razões estão na tabela e na seção 6. Não houve modelo artificial.

## 2. Implementação

### 2.1 147 — viga–pilar flangeada fora do nó (modelo completo)
- **Mecânica:** composição em **série** de (A) mesas da viga soldadas ao pilar (EN 6.2.7.1, k1, k2, k3 do pilar; solda de penetração total ou filetes conforme `supWeldKind`) e (B) emenda por duas chapas de extremidade (Incremento 5). Rotação total = soma das rotações: `1/Sj,ini = 1/Sj,ini,A + 1/Sj,ini,B`; `Mj,Rd = min(Mj,Rd,A ; Mj,Rd,B)`; curva pela eq. 6.28 (ψ = 2,7) com esses valores. É composição por cinemática de molas em série (declarada nas hipóteses), não regra literal da EN; o momento na emenda é tomado igual ao da face do pilar (V × distância desprezado).
- **Fração de flexibilidade:** fracionada em ROTAÇÃO (f_A = S/S_A, f_B = S/S_B, soma 1); componentes do nó pelo seu peso dentro da parte A.
- **Dois sentidos:** a parte soldada é simétrica; a emenda usa as fileiras acima/abaixo (Incremento 5). Simetria só é declarada quando `tRows = sRows`.
- **Classificação:** é viga–pilar, logo `NbrClassifier`/`RealVsRequired` classificam normalmente por sentido (não "Não aplicável").
- **Dados:** exige a seção do pilar (barra I/H vinculada ou geometria confirmada); sem ela não há números.
- **Nível C:** as verificações da NBR da emenda e da solda viga–apoio (catálogo) alimentam a ressalva `nbrBelowCurve`.

### 2.2 143 — perfis T (`ResistanceOnly`)
`Mj,Rd` = maior momento para o qual todas as verificações da NBR 8800:2024 da própria ligação (V = N = 0) ficam ≤ 1 (nível C). Resposta `ResponseKind::ResistanceOnly`, `status ValidWithCaveats`, uma resistência nomeada `Mj,Rd,NBR (…)`, **sem** curva, rigidez, componentes nem níveis de momento. Capacidade declarada como "Só resistência (NBR, nível C)" (modelo `NbrResistanceOnly`), com a lacuna de formulação explícita. A comparação/classificação fica **Indeterminado** (faltam Sj,ini): "a resposta mecânica tem só a resistência…".

### 2.3 Correção de default silencioso (necessária)
`ConnectionCatalog::momentInput` substituía um pilar ausente por 260 × 146 × 12,7 × 7,6. As famílias 143–146 passariam a calcular com esse pilar sem aviso. `JointComponentService` agora exige pilar **declarado** (barra I/H vinculada ou geometria confirmada) nas famílias soldada e de perfis T; sem isso: "Nenhum pilar padrão é assumido" e nenhum número. Os resultados de 144–146 com pilar declarado não mudaram (testes de regressão e golden intactos).

## 3. Resultados numéricos e verificação independente
Implementação independente em Python (biblioteca padrão; `docs/validacao_emenda/splice_independent.py`, saída em `saida_splice_independent.txt`): emenda (Incremento 5) e **agora** a parte soldada e a composição. Conferência no C++ (`TestBeamSplice`):

| Caso (cadastro padrão 147, IPE300, pilar 260×146×12,7×7,6, E = 200 000 MPa) | Python | C++ | Erro relativo | Tolerância |
|---|---|---|---|---|
| Parte A (soldada): k1 / k2 / k3 (mm) | 2,915112 / 3,078956 / 3,078956 | idem | < 1e-6 | 2e-5 (casas impressas) |
| Parte A: Sj,ini / Mj,Rd | 16 863,5060 kN·m/rad / 55,84295 kN·m | idem | < 2e-8 / 2e-7 | 2e-8 / 2e-7 |
| 147, 1 fileira (+): Sj,ini / Mj,Rd | 14 147,2810 / 45,90029 | idem | < 2e-8 / 2e-7 | idem |
| 147, 2 fileiras (+): Sj,ini / Mj,Rd | 14 429,2925 / 55,84295 (governa a parte soldada) | idem | idem | idem |
| 147, 2×1 (−): Sj,ini / Mj,Rd | 14 147,2810 / 45,90029 | idem | idem | idem |
| Partes B (emenda, 2 fileiras): Sj,ini / Mj,Rd | 99 961,8416 / 84,05505 | idem | idem | idem |

**Natureza da verificação:** reprodução independente das mesmas equações (consistência de implementação e aritmética). **Não é validação experimental nem profissional**: não há ensaio, MEF nem exemplo publicado desta ligação. Para 143 não há número de rigidez a verificar (só a resistência da NBR, que é a soma das verificações existentes).

## 4. Testes
Suíte completa (build limpo, `StructuraCore.lib` removida): **ctest 47/47**. `StructuraDemo` compila (0 erros).
- `TestBeamSplice` (21, +9 neste incremento): Python independente da composição 147; frações de rotação somam 1 e série é mais flexível que qualquer parte; Mj,Rd = mínimo (e componente de teto identificado); espelho/simetria/fingerprints por sentido; pilar ausente/não confirmado e geometria inválida sem números; classificação por sentido escolhendo o par viga–pilar (e não o viga–viga); capacidade; 143 `ResistanceOnly` (um resultado, sem curva/rigidez/componentes, simétrico, AFP por sentido, comparação Indeterminada); pilar ausente → sem números em 143–146; pilar digitado e não confirmado → sem números; confirmado → calcula.
- Expectativas anteriores **substituídas por mudança deliberada** (não ocultadas): `TestRequiredBehavior` (curva M–θ no catálogo: 4 → 5 com a 147; 143 aparece como "só resistência", sem curva); `TestConnectionMechanicalResponse` e `TestMechanicalDirections` (143 deixou de ser "sem números" e passou a só resistência); `TestJointEngineRepresentability` (143 = `NbrResistanceOnly`).
- Goldens: `numerics.txt` `81f82d53…` e `momento_rotacao.txt` `a17c4238…` inalterados; `rds.dll`, `mefs.dll` e `.enc` sem alteração.

## 5. Integração e evidências no app real (nó 10, Metalicas Restaurante; `visual/inc6_*`)
| Caso | Resultado |
|---|---|
| 147 criada pelo fluxo (par 14–18) | Positivo: Sj,ini 432,45 kN·m/rad, Mj,Rd 1,1625 kN·m (governa a parte soldada: alma do pilar comprimida); negativo: 429,73 kN·m/rad, 1,1625 kN·m; classe semirrígida (η = 15,27); requerido rígido por continuidade implícita → **Não atende** nos dois sentidos; flexibilidade dominada por k1 (painel de alma); curvas + e − no gráfico (`inc6_t147_*`) |
| 143 com geometria padrão (viga 1228) | resistência **indisponível**: a ligação padrão reprova verificações independentes do momento (altura da tala × alma da viga entre mesas); mensagem com a causa, sem números |
| 143 com `rows=1;lines=1` | **só resistência**: Mj,Rd = 1,4574 kN·m (verificação governante: "Apoio: flexão local da mesa"), sem Sj,ini e sem curva; capacidade exibida como "Só resistência (NBR, nível C)"; comparação **Indeterminado** (`inc6_t143_*`) |
Interface: a aba "Resposta mecânica" mostra "só resistência" onde aplicável; o combo indica `+ só resistência / − só resistência`; eixos sem números quando nenhuma curva foi calculada; matriz de tipologias distingue "Disponível (EnComponentMethod)" de "Só resistência (NBR, nível C)".

## 6. Limitações — tipologias ainda não suportadas e o motivo técnico específico
- **143 (perfis T):** sem Sj,ini e sem curva — falta regra de espalhamento da compressão do T-stub (EN não traz para esta junta), k11/k12 do T com a mesa da viga e benchmark de junta completa. Resistência apenas (nível C).
- **148, 131, 136, `kSplice` (talas):** EN não traz regra de combinação para emenda por talas; com parafusos protendidos k11 = k12 = ∞.
- **Bases de pilar (legado, 172, 173):** rigidez depende de N (Tab. 6.12); sem graute/Ec no modelo de dados; nó de apoio com uma só barra não entra no fluxo do Incremento 1.
- **Ligações de cisalhamento (talas, cantoneiras de alma, chapa flexível, 101–116, 107–108, 152):** sem fonte para rigidez rotacional.
- **Mistas (161–167):** EN 1994-1-1 Anexo A não disponível; NBR Anexo O só resistência.
- **Cantoneiras de mesa de aço:** EN completa, mas não há tipologia no catálogo.
- **Gussets, treliças, tubulares, formados a frio:** só resistência/CIDECT.
- **147:** composição em série declarada (não literal da EN); momento na emenda = momento na face do pilar; chapas iguais; sem capacidade de rotação nem estado individual de componentes; sem ensaio/MEF.
- Nenhuma capacidade não implementada é apresentada como disponível; nenhum modelo foi criado só para atingir quantidade.

## 7. Arquivos
Novos: nenhum arquivo de produção novo; relatório e `docs/validacao_emenda/*` atualizados (script Python estendido).
Alterados: `src/app/services/JointComponentService.{h,cpp}` (família fora do nó em série, `resistanceOnly`, guarda de pilar declarado, `Part`), `ConnectionMechanicalResponseService.{h,cpp}` (frações de rotação, `resistanceOnly`, simetria), `JointTopologyAdapter.cpp` (capacidade `NbrResistanceOnly`), `TypologyCapabilityService.{h,cpp}` (modelo no `DefinedConnection`), `ConnectionRequirementService.cpp`, `src/design/jointengine/RealVsRequired.cpp` (mensagem para só resistência), `src/ui/NodeBehaviorDialog.cpp` (estados "só resistência", eixos, combo), testes `TestBeamSplice.cpp`, `TestRequiredBehavior.cpp`, `TestMechanicalDirections.cpp`, `TestConnectionMechanicalResponse.cpp`, `TestJointEngineRepresentability.cpp`, `docs/validacao_emenda/splice_independent.py` e `saida_splice_independent.txt`, `MATRIZ_COBERTURA_LIGACOES.md`.

## 8. Git
Alterações não commitadas na branch `implantacao_dll`.
