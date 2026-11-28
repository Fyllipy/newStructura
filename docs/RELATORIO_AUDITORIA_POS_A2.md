# Relatório de auditoria pós-implementação A2 e verificação de regressão

Escopo: diagnóstico e validação. Nenhuma equação, coeficiente normativo ou critério de aprovação foi alterado; a Camada A continua restrita a 1 fileira; o T-stub da ferramenta momento-rotação não foi migrado. Durante as medições dois arquivos de produção foram alterados **temporariamente** (gancho do A2 em `EndPlateConnection.cpp`; filtro em `JointComponentService.cpp`) e restaurados de cópia; os SHA-256 de `EndPlateConnection.cpp`, `JointComponentService.cpp`, `JointComponents.cpp`, `ConnectionService.cpp`, `ColumnFlangeLever.cpp` e `src/tests/CMakeLists.txt` foram conferidos idênticos aos do início. Este relatório **não declara validação profissional** do módulo.

Evidências brutas e fontes das sondas: `docs/validacao_alavanca/auditoria_pos_a2/` (`before.txt`, `after.txt`, `nofilter.txt`, `cat.txt`, `probe_ui_a2.txt`, `ProbeMR.cpp`, `ProbeUiA2.cpp`, `ProbeCat.cpp`). As sondas não fazem parte do build.

## 1. Resumo executivo

| Ponto | Resultado |
|---|---|
| Momento-rotação, comparação quantitativa | **Sem diferença.** 14 casos, saída completa (rigidez, Mj,Rd, 42 pontos da curva, métricas, classificação, rótula bilinear, secante/tangente, memória de cálculo) idêntica byte a byte entre "A2 desligado" e "A2 ligado" (`diff` com 0 linhas). |
| Filtro `isColumnFlangeLeverCheck` | Exclui exatamente as 3 verificações do A2 (comparação por nome exato) e nenhuma legítima. Sem o filtro o Mj,Rd da NBR e `nbrBelowCurve` mudam em 10 dos 12 casos de chapa de extremidade avaliáveis (C01–C12); a curva em si não mudaria (usa o Mj,Rd da EN). |
| Interface | Exercitado o **diálogo real** (`NodeConnectionsDialog`), a memória de cálculo e o relatório (HTML e DOCX), fora da tela. **A janela principal do aplicativo não foi executada.** Estado `Complementary` distinguível pelo texto; **não pela cor** (mesmo cinza de INFORMAÇÃO e NÃO APLICÁVEL). |
| Inconsistências | 1 nova do A2 (linha da advertência de divergência apresenta R_A como "demanda" e η > 1), 1 do A1 (mensagem da ferramenta momento-rotação com pilar não informado), 1 pré-existente (DOCX com UTF-8 inválido em equações), 3 menores. Seção 6. |
| Catálogo padrão (2 fileiras) | Recomendação: manter a regra, **informar o usuário antes** e oferecer a variante de 1 fileira como ação explícita; não trocar o padrão automaticamente (a troca altera o Mj,Rd da ferramenta momento-rotação de 84 para 46 kN·m na ligação de exemplo). Seção 4. |
| Suíte automatizada | `ctest`: 31/31 após restaurar as fontes e recompilar. |

## 2. Resultados da regressão quantitativa (momento-rotação)

### 2.1 Como a referência "antes" foi reproduzida
Não há resultado armazenado de curvas. A ferramenta (`JointComponentService`, `JointComponents`, testes) é **arquivo não rastreado** (`git status`: `??`) e o `HEAD` não a contém; portanto não é possível reconstruir "antes" pelo git. Foi usado o método **A/B no mesmo binário-base**:

1. **DEPOIS** = estado atual.
2. **ANTES** = estado atual com **uma linha** desligada: o laço de `EndPlateConnection.cpp` que acrescenta `columnFlangeLeverChecks(in, ft)` (`for (Check c : std::vector<Check>{})`). Isso reproduz o programa sem o A2, **mantendo** A1/M2.
3. **SEM FILTRO** (controle negativo) = A2 ligado e a linha `if (D::isColumnFlangeLeverCheck(c))` de `ratiosOf` neutralizada.

Cada variante foi recompilada e executada pela sonda `ProbeMR` (chama `JointComponentService::evaluate`, `JS::classify`, `JC::deriveEquivalent`, `secantStiffnessAt`, `tangentStiffnessAt`).

**Referência independente armazenada, anterior a todas as alterações:** `docs/AUDITORIA_DIMENSIONAMENTO_LIGACOES.md` registra, para a ligação padrão de exemplo, Mj,Rd = **83,97 kN·m** (EN) e **66,28 kN·m** (NBR). O caso C01 reproduz **83,9668** e **66,2801**.

### 2.2 Casos e resultados (ANTES = DEPOIS)
Todas as colunas abaixo são idênticas nas duas variantes; a coluna "Mj,Rd NBR sem filtro" mostra o que o filtro evita.

| Caso | Sj,ini (kN·m/rad) | Mj,Rd usado = EN (kN·m) | Mj,Rd NBR (nível C) | Mj,Rd NBR sem filtro | `nbrBelowCurve` (filtrado → sem filtro) |
|---|---|---|---|---|---|
| C01 IPE300 / pilar 260×146×12,7, padrão (2 fil.) | 23 683,2 | 83,97 | 66,28 | **0 (indisponível)** | 1 → 0 |
| C02 idem, 1 fileira | 19 301,3 | 45,90 | 66,28 | 63,71 | 0 → 0 |
| C03 idem, 3 fileiras | 25 273,8 | 91,82 | 66,28 | 0 | 1 → 0 |
| C04 IPE300 / HEB200 / M24 / chapa 25 / 1 fil. | 29 715,5 | 45,90 | 92,46 | 101,36 | 0 → 0 |
| C05 idem, 2 fil. | 34 871,7 | 93,43 | 92,46 | 0 | 1 → 0 |
| C06 mesa do pilar tf = 8 / 1 fil. | 13 547,0 | 29,31 | 26,30 | 26,30 (igual) | 1 → 1 |
| C07 idem, 2 fil. | 17 238,5 | 56,51 | 26,30 | 0 | 1 → 0 |
| C08 IPE400 / HEB300 / M24 / chapa 22 / 2 fil. | 66 601,0 | 132,41 | 124,30 | 0 | 1 → 0 |
| C09 idem, 1 fil. | 57 413,8 | 80,45 | 161,25 | 146,42 | 0 → 0 |
| C10 idem, viga dos dois lados | 90 794,0 | 80,45 | 161,25 | 146,42 | 0 → 0 |
| C11 idem, enrijecedores + reforço de alma | 110 800,6 | 80,45 | 161,25 | 146,42 | 0 → 0 |
| C12 chapa 12 / M16 / 1 fil. | 15 518,7 | 28,38 | 43,52 | 43,52 (igual) | 0 → 0 |
| C13 controle: mesas soldadas (cat. 144) | 16 863,5 | 55,84 | 66,28 | = | 0 |
| C14 pilar não informado (A1) | — | — | — | — | `supported = 0` nas duas variantes |

Itens comparados (todos idênticos): rigidez inicial; resistência última; **42 pontos** (θ, M) da curva (corrigido: a versão anterior deste relatório dizia 40); início da não linearidade (2/3 Mj,Rd) e rotação correspondente; rotação em Mj,Rd; secante e tangente em Mj,Rd; M e θ na rigidez secante Sj,ini/2; secante/tangente em 25, 50, 66,7, 80 e 100 % de Mj,Rd; classificação (`byStiffness`, `final`, η); rótula bilinear equivalente (k, M_y, razão de endurecimento) usada na análise global; texto completo da memória.

### 2.3 O que a comparação prova e o que não prova
- **Prova:** o A2 não altera nenhum número produzido pela ferramenta momento-rotação nos 14 casos, incluindo a resistência que alimenta `deriveEquivalent`. A **equivalência vale com A1/M2 mantidos**; não foi possível isolar A1/M2 pelo git.
- **O controle negativo é informativo:** sem o filtro, em 10 dos 12 casos de chapa de extremidade o Mj,Rd da NBR mudou (C06 e C12 não) e, nos 5 de várias fileiras (C01, C03, C05, C07, C08), deixou de existir (`nbrCheckAvailable = 0`), o que **apagaria o aviso `nbrBelowCurve`** de ligações que hoje o recebem. O Mj,Rd da curva (EN) não mudaria; o dano seria ao confronto de nível C.
- **Não coberto:** execução do solver global com a rótula/mola derivada (a sonda para em `deriveEquivalent`); janelas `JointAnalysisDialog`/gráficos; o exemplo SCIA HEB140/IPE220 de `AUDITORIA_LIGACOES.md` (Mj,Rd = 34,91 kN·m), cujos dados completos de entrada não foram reconstruídos nesta etapa.

### 2.4 Observação sobre A1 na ferramenta momento-rotação
C14: sem pilar informado, a ferramenta devolve `supported = 0` com a mensagem "Rigidez não calculável: Geometria invalida (altura livre da alma do pilar, espessuras ou E)". Antes do A1, um pilar padrão embutido era usado silenciosamente. O comportamento é coerente com A1, mas a mensagem não diz ao usuário o que fazer (inconsistência I3).

## 3. Resultados da inspeção da interface

### 3.1 O que foi (e não foi) executado
- **Executado:** o `NodeConnectionsDialog` real (classe de produção) instanciado fora da tela (`QT_QPA_PLATFORM=offscreen`), com leitura das tabelas, rótulos, cores, "Copiar memorial" e captura `grab()`; `ConnectionService::evaluate`; `ReportService::appendDesignChapters` + `toHtml` + `writeDocx` com 5 ligações.
- **Não executado:** **a janela principal do aplicativo** (não há automação de interface de área de trabalho neste ambiente), seleção/edição de nós e ligações na viewport, exportação real para PDF (só o HTML que o PDF consome), abertura do DOCX no Word.
- **Limitação das capturas:** o ambiente `offscreen` não tem fontes; os PNG saem com quadrados no lugar do texto. Servem para ver **cores e disposição**; o texto foi verificado pelos dados lidos das tabelas.

### 3.2 Cenários
| # | Ligação | Veredito do diálogo | Camada A | Camada B |
|---|---|---|---|---|
| S1 | padrão, 2 fileiras (M = 30, V = 40) | NÃO VERIFICÁVEL — DADOS INSUFICIENTES; "NÃO está aprovada (1 ocorrência)" | NÃO VERIFICÁVEL (2 fileiras) com causa e instrução | MÉTODO COMPLEMENTAR, 103,70 / 486,06 kN, η informativo 0,21 |
| S2 | 1 fileira, chapa 25, M24 (M = 20, V = 20) | Atende (0,882, governada por largura da chapa) | 15,00 / 6,13 mm = 0,409, OK | MÉTODO COMPLEMENTAR, 69,13 / 315,74 kN |
| S3 | 1 fileira, mesa tf = 8 | Atende | 8,00 / 6,16 mm = 0,770, OK | 69,13 / 173,34 kN |
| S4 | 2 fileiras, esforço baixo (M = 10, V = 10) | NÃO VERIFICÁVEL | NÃO VERIFICÁVEL | 34,57 / 629,49 kN (η 0,05, "favorável") — **continua não aprovada**, como exigido |
| S5 | 1 fileira, fu do pilar 700 MPa | Atende | 15,00 / 4,91 mm = 0,328 | 69,13 / 315,74 kN + **advertência de divergência** (R_A 398,08 kN ≥ 1,10 R_B 348,83 kN) |

Serviço (`Evaluation`): S1 e S4 `approved = 0`, `incomplete = 1`, `maxRatio = 1e9` (tratado como estado, não exibido como número); S2, S3, S5 `approved = 1`.

### 3.3 O estado `Complementary` é distinguível?
- **Por texto: sim.** Rótulo "MÉTODO COMPLEMENTAR — NÃO PARTICIPA DA APROVAÇÃO" na coluna de estado; o nome da verificação também diz "(nao participa da aprovacao global)"; a memória e o relatório repetem o aviso e listam os coeficientes EN usados.
- **Por cor: não.** O código atribui `#6b7785` (cinza) a NÃO APLICÁVEL, INFORMAÇÃO e `Complementary`; NÃO VERIFICÁVEL/reprovado é vermelho `#c0392b`, aprovado é verde `#1e8449`. Nas capturas (S1, S5) o método complementar aparece cinza entre verdes/vermelhos, o que sugere "neutro", mas não o diferencia de INFORMAÇÃO.
- **O usuário entende que não participa do global?** O veredito de topo, a coluna de estado e o texto do memorial dizem; em S4 o diálogo mostra a camada B com folga grande e o veredito NÃO VERIFICÁVEL, sem contradição textual. Não houve teste com usuários.

## 4. Análise do catálogo padrão

### 4.1 Estado atual
`ConnectionService::makeDefault(kEndPlate)` define `tRows = 2` (`db 20`, `plateT 22,4`); `toEndPlate` também assume 2 como valor ausente. A emenda viga–viga parte do mesmo padrão, mas não é afetada (descarta as verificações de pilar). Toda ligação de chapa de extremidade criada com o padrão sai **NÃO VERIFICÁVEL** até o usuário reduzir para 1 fileira.

### 4.2 Capacidade medida (kN·m, V = 0, pior verificação ≤ 1), geometria padrão de cada viga
Regra atual × semântica anterior ao A2 (sem a camada A). Fonte: `cat.txt`.

| Viga | Pilar | 1 fileira (atual) | 2 fileiras (atual) | 2 fileiras, semântica anterior ao A2 |
|---|---|---|---|---|
| IPE200 | HEB200 / HEB300 | 39,7 (solda da mesa) | NÃO VERIFICÁVEL | 36,3 (alavanca da chapa) |
| IPE300 | HEB200 | 71,0 (painel de alma) | NÃO VERIFICÁVEL | 71,0 |
| IPE300 | HEB300 | 83,8 | NÃO VERIFICÁVEL | 84,2 |
| IPE400 | HEB200 | 94,3 | NÃO VERIFICÁVEL | 94,9 |
| IPE400 | HEB300 | 111,9 | NÃO VERIFICÁVEL | 125,8 |
| IPE500 | HEB200 | 113,7 | NÃO VERIFICÁVEL | 118,8 |
| IPE500 | HEB300 | 138,8 | NÃO VERIFICÁVEL | 165,0 |

Leitura: na verificação de dimensionamento (o que o usuário vê), a configuração de 1 fileira atinge **≥ 95 %** da capacidade de 2 fileiras (semântica anterior) em 6 dos 8 pares (109 % no IPE200, onde a alavanca da chapa governa as 2 fileiras) e 89 % (IPE400/HEB300) e 84 % (IPE500/HEB300) nos demais; a pior verificação costuma ser solda, painel de alma ou parafusos. Vale só para a verificação de dimensionamento com V = 0 e a geometria padrão de cada viga (db 20, chapa 22,4).

**Contraponto importante:** o `makeDefault` também alimenta a ferramenta momento-rotação. Na ligação de exemplo, passar de 2 para 1 fileira muda o Mj,Rd da curva de **83,97 para 45,90 kN·m** e Sj,ini de 23 683 para 19 301 kN·m/rad (C01 × C02). Trocar o padrão afeta a modelagem global.

### 4.3 Alternativas
| Alternativa | Vantagens | Riscos |
|---|---|---|
| (a) Padrão de 1 fileira | Primeira abertura aprova se os esforços couberem; honesto com o escopo validado | Muda Mj,Rd/Sj,ini da ferramenta momento-rotação e a classificação de todo projeto novo; muda desenhos e quantitativos do padrão; vigas maiores que a capacidade de 1 fileira exigirão mais fileiras e voltarão a NÃO VERIFICÁVEL |
| (b) Manter 2 fileiras, deixar o NÃO VERIFICÁVEL falar sozinho | Nenhuma mudança de comportamento | O usuário vê "dados insuficientes" logo na criação, sem saber que é por decisão de escopo (o texto atual cita "MEF independente", jargão) |
| (c) Manter 2 fileiras **e** orientar antes do dimensionamento | Sem efeito colateral nos outros módulos; transparente | Exige texto de interface |

### 4.4 Recomendação (não aplicada)
**(c)**, com uma ação opcional: (i) mensagem curta já na criação/abertura da ligação de 2+ fileiras: "A verificação da mesa do pilar (NBR 6.3.5.4) só está liberada para 1 fileira tracionada. Com 2 ou mais, a ligação fica NÃO VERIFICÁVEL. Escolha 1 fileira ou verifique este item fora do programa."; (ii) botão "Usar 1 fileira" no diálogo (altera `tRows`, nunca silencioso); (iii) manter o padrão em 2 fileiras enquanto a ferramenta momento-rotação depender dele; reavaliar (a) quando houver MEF para várias fileiras ou quando a migração do T-stub definir o padrão da curva. Justificativa: (a) é a única alternativa que altera resultados de outro módulo para resolver um problema de comunicação.

## 5. Verificação do filtro de momento-rotação

`ratiosOf` descarta `D::isColumnFlangeLeverCheck(c)`, definido como igualdade exata do nome com `kLeverNbrName`, `kLeverEnName` ou `kLeverWarnName`.

- **Cobertura das emissões do A2:** todas as verificações de `columnFlangeLeverChecks` usam essas 3 constantes (Camada A: não aplicável, não verificável, razão de resistência de parafuso e regra mínima, todas com `kLeverNbrName`; Camada B: `kLeverEnName`; advertência: `kLeverWarnName`). Nenhuma outra emissão foi encontrada por leitura de `ColumnFlangeLever.cpp`.
- **Nenhuma verificação legítima oculta:** "Efeito de alavanca - espessura da placa (flexivel) - chapa de extremidade" (6.3.5.4 da **chapa**, pré-existente) tem nome diferente e permanece (S1 mostra 22,40 / 13,37 mm = 0,597 nas verificações). A verificação informativa "Seção do pilar utilizada" não é filtrada (razão 0, sem efeito sobre as razões que dependem do momento).
- **Prova experimental:** ANTES = DEPOIS (seção 2) e SEM FILTRO ≠ DEPOIS (seção 2.2).
- **Fragilidade:** a identificação por nome é uma convenção de texto. Renomear uma constante é seguro (a comparação usa a mesma constante), mas uma futura verificação legítima com nome igual seria ocultada em silêncio. Recomenda-se um campo explícito no `Check` (por exemplo, `origin`/`group`) em vez de comparar nomes.
- **Consequência a conhecer:** a ferramenta momento-rotação continua sem **nenhuma** verificação de alavanca da mesa do pilar em seu confronto de nível C, para qualquer número de fileiras, como antes do A2. A ferramenta não herda a restrição NÃO VERIFICÁVEL de várias fileiras.
- **Escopo:** o filtro cobre `checkEndPlate`. A família de mesas soldadas (`checkMomentConnection`) não emite verificações do A2 (C13 é idêntico).

## 6. Inconsistências encontradas

| # | Origem | Descrição | Gravidade | Evidência |
|---|---|---|---|---|
| I1 | **A2** | A linha "Advertencia de divergencia entre metodos" é exibida com **R_A como "demanda" (398,08 kN)**, **R_B como "resistência" (348,83 kN)** e "η informativo 1,14" no diálogo e no relatório. Os números não são demanda/resistência de ligação e o 1,14 parece reprovação, embora a advertência seja informativa. | Média (leitura enganosa; não afeta o resultado global) | S5, `probe_ui_a2.txt` |
| I2 | pré-existente (`src/report`, sem alterações) | O `document.xml` do DOCX contém **UTF-8 inválido** em equações com letra grega seguida de subscrito (`\xce_`, "γ" cortado no meio): 10 ocorrências no relatório de 5 ligações e 5 no controle **sem A2** (chapa de cortante e mesas soldadas). Possível aviso de arquivo corrompido no Word; não verificado no Word. | Média (fora do escopo do A2) | `relatorio.docx`, `relatorio_controle.docx` |
| I3 | A1 | Ferramenta momento-rotação com pilar não informado: "Rigidez não calculável: Geometria invalida (altura livre da alma do pilar, espessuras ou E)" — não diz que o pilar precisa ser informado/confirmado. | Baixa | C14 |
| I4 | A2 | Cinza compartilhado por INFORMAÇÃO, NÃO APLICÁVEL e MÉTODO COMPLEMENTAR (seção 3.3). | Baixa | `NodeConnectionsDialog.cpp:451` |
| I5 (errata A3: `1,5·db` é a estimativa de largura entre faces; o `dw` é `1,5·db·1,1547`) | A2 / documento | `docs/NUCLEO_COMPARTILHADO...` §4: "Nenhum campo tem valor padrão silencioso" — porém `TStub::boltHeadAcrossPoints` usa `1,5·db` para diâmetros fora da tabela de abertura de chave (ex.: M14, M18), o que entra em `ew`. Efeito pequeno no método 2, mas é um padrão silencioso. | Baixa | `TStub.cpp:35-41` |
| I6 | A2 | Nomes de verificações da Camada B/advertência saem sem acentos e longos ("T-stub EN 1993-1-8 - METODO COMPLEMENTAR (nao participa da aprovacao global)") em tabela e relatório, em contraste com o rótulo de estado acentuado. | Cosmético | `probe_ui_a2.txt` |
| I7 | — | O cenário de **Camada A reprovada** (e não só NÃO VERIFICÁVEL ou aprovada) não foi exercitado no diálogo: S3 com tf = 8 ainda aprova (0,770). Está coberto apenas pelos testes unitários. | Lacuna de teste | S3 |

## 7. Auditoria da documentação (`docs/NUCLEO_COMPARTILHADO_COMPONENTES_TSTUB.md`)

Conferido contra o código:

| Afirmação | Situação |
|---|---|
| Estados `Evaluated/Unverifiable/NotApplicable/Information/Complementary` e quem entra em `Summary` (§8) | **Fiel** (`Connections.cpp:99-139`; só `Evaluated`/`Unverifiable` contam; lista só com NA não aprova). |
| `TStub.h` não inclui nada da NBR (§5) | **Fiel** (somente `<string>`, `<utility>`, `<vector>`). |
| Camada A não usa `EnCoefficients`; Camada B usa (§5) | **Fiel** no fluxo; o arquivo `ColumnFlangeLever.cpp` abriga as duas camadas, o que o texto não diz. |
| Fluxo de dados (§7), comportamento de 2+ fileiras, advertência a ≥ 10 % | **Fiel** (confirmado nos cenários S1, S4, S5). |
| Integração com momento-rotação (§6): `JointComponents.cpp` inalterado; filtro em `ratiosOf` | **Fiel** e agora **comprovada quantitativamente** (seção 2). O documento cita apenas os 51 testes; deve passar a citar esta comparação. |
| Limitações de A e B (§10) | **Fiel**; falta registrar que, na ferramenta momento-rotação, a alavanca da mesa do pilar permanece sem verificação em qualquer número de fileiras. |
| "Nenhum campo tem valor padrão silencioso" (§4) | **Parcialmente incorreta** (I5). |
| Testes (§9): P398, IDEA 2 %, NBR, estados | Existem em `TestTStub.cpp` (14 casos) e passam (31/31). A frase "diferenças reais até ≈ 1,9 %" não foi reverificada nesta tarefa. |
| Interface: estado complementar, cinza, advertência | **Omitido** no documento: não descreve que o complementar partilha cor com INFORMAÇÃO nem o problema I1. |
| Funcionalidades planejadas marcadas **[PLANEJADO]** | **Fiel**: migração, rigidez, lei força-deformação, Anexo Nacional, mesa enrijecida, MEF para várias fileiras. |

Não foi declarada validação profissional no documento (confere).

## 8. Recomendações
1. **I1:** exibir a advertência de divergência como texto ("Camada A estima 398 kN; Camada B, 349 kN; diferença +14 %"), sem colunas demanda/resistência/η. Sem tocar em equações.
2. **I2:** corrigir o escritor de equações para tratar caracteres multibyte antes de aplicar `_`/`^` e validar o DOCX (UTF-8 e abertura no Word); fora do A2, mas afeta todo relatório de ligações.
3. Dar ao `Check` um marcador explícito de origem para o filtro de momento-rotação (seção 5).
4. Dar ao `Complementary` uma identidade visual própria (cor/ícone/itálico) diferente de INFORMAÇÃO.
5. Catálogo: alternativa (c) da seção 4.4; não trocar o padrão automaticamente.
6. Ferramenta momento-rotação: mensagem de pilar não informado que diga "informe ou confirme a seção do pilar" (I3).
7. Atualizar o documento do núcleo com: a regressão desta tarefa, I5, a omissão de cor, e a observação de que a alavanca da mesa do pilar não participa do nível C do momento-rotação.
8. Antes da migração do T-stub do momento-rotação: transformar `ProbeMR.cpp` em teste automatizado (mesmos 14 casos, `before.txt` como referência numérica), para que a comparação de curvas seja reproduzível sem editar produção.

## 9. Testes realizados e pendentes

**Realizados**
- Regressão A/B de momento-rotação, 14 casos × 3 variantes (seção 2); controle negativo do filtro.
- Conferência dos SHA-256 das fontes alteradas temporariamente; recompilação completa; `ctest -C Release`: **31/31** aprovados.
- Diálogo real, memória de cálculo, relatório HTML e DOCX, para 1 e 2 fileiras (S1–S5).
- Varredura de capacidade 1 × 2 fileiras (`ProbeCat`) para 4 vigas × 2 pilares.
- Leitura do documento contra o código.

**Pendentes / não executados**
- **Janela principal do aplicativo real** (seleção, edição, viewport), exportação para PDF e visualização do PDF; abertura do DOCX no Word (I2).
- Execução do solver global com a rótula/mola equivalente derivada e janelas de momento-rotação.
- Reconstrução de "antes" **sem** A1/M2 (impossível pelo git; só o valor armazenado de 83,97/66,28 kN·m foi conferido).
- Exemplo SCIA HEB140/IPE220 (34,91 kN·m) pela ferramenta atual.
- Camada A reprovada no diálogo (I7); ligações com aço do pilar de grau diferente, parafusos de outras classes e ligações pela face da alma.
- Teste com usuários da compreensão do estado complementar.
- Validação profissional ou por MEF independente de 2 ou mais fileiras (fora do escopo desta tarefa).
