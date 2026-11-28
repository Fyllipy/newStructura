# Relatório de saneamento pós-A2

Escopo: correções apontadas em `docs/RELATORIO_AUDITORIA_POS_A2.md` e consolidação arquitetural. Não foram alteradas equações normativas, coeficientes EN/NBR, critérios de aprovação, a limitação da Camada A a 1 fileira, a regra NÃO VERIFICÁVEL para várias fileiras, o método 2 da Camada B nem o comportamento matemático da ferramenta momento-rotação (`JointComponents.cpp` intocado). O T-stub da ferramenta não foi migrado e rigidez/lei força-deformação não foram implementadas. Este relatório **não declara validação profissional** do módulo.

## 1. Problemas corrigidos

| # | Item | Correção |
|---|---|---|
| I1 | Advertência de divergência apresentada como demanda/resistência/η | Passa a ser mensagem informativa, sem números de verificação: "Divergência entre metodologias: Camada A (NBR) = X kN; Camada B (EN) = Y kN. A estimativa da Camada A é Z% superior à da Camada B. Resultado informativo, sem influência na aprovação global." Condição R_A ≥ 1,10·R_B **inalterada**. |
| I2 | DOCX com UTF-8 inválido em equações | Causa raiz encontrada e corrigida na origem (seção 5). |
| I3 | Filtro por comparação de nomes | Campo `Check::origin` (`CheckOrigin`) e filtro por origem (seção 3). |
| I4 | `Complementary` sem identidade visual própria | Índigo + glifo "◇" + itálico (diálogo) e célula índigo no relatório; rótulo "MÉTODO COMPLEMENTAR — NÃO PARTICIPA DA APROVAÇÃO GLOBAL". |
| I3 (A1) | Mensagem de pilar não informado | "Rigidez não calculável: informe ou confirme a seção do pilar e suas propriedades geométricas e mecânicas." Nenhum pilar padrão restaurado. |
| I5 | Fallback `1,5·db` de `dw` | Mantido (decisão abaixo), identificado e registrado na memória. |
| — | Catálogo padrão (2 fileiras) | Aviso e ação explícita "Usar 1 fileira"; padrão inalterado. |
| — | Regressão momento-rotação por sondas temporárias | Suíte automatizada reproduzível (seção 4). |
| — | Erro de contagem do relatório anterior | A curva tem **42** pontos, não 40; relatório de auditoria corrigido. |

**Decisão I5 (fundamentada).** Foi escolhida a terceira alternativa: *manter o fallback como hipótese empírica, identificado e registrado*. Motivos: (a) o fallback só atua em diâmetros sem largura entre faces tabelada (M14, M18, M33 etc.), enquanto a área resistente `As` desses diâmetros existe na tabela; tornar o resultado "não verificável" removeria a Camada B (informativa) justamente para parafusos usuais, sem ganho de segurança porque ela não participa da aprovação; (b) pedir o valor ao usuário exigiria novo campo de entrada e alteraria o fluxo de dados, fora do escopo; (c) o efeito de `dw` no método 2 é pequeno (≤ 1 % nos exemplos tabelados do P398/IDEA, **não medido** para os diâmetros sem tabela). A garantia pedida ("valor estimado nunca apresentado como dado normativo") está implementada: `TStub::boltHeadWidth` devolve `tabulated = false` e a memória da Camada B inclui "ATENÇÃO — VALOR ESTIMADO (hipótese empírica, não é dado normativo nem de catálogo)…". O número calculado não mudou (`boltHeadAcrossPoints` devolve o mesmo valor). A estimativa **não foi conferida com catálogo de parafusos**.

> **Errata (A3):** neste relatório, "fallback `1,5·db`" refere-se à estimativa da largura entre **faces** da cabeça; o `dw` efetivamente implementado é a largura entre **vértices**, `dw = 1,5·db·1,1547 = √3·db ≈ 1,732·db` (como descrito em `docs/NUCLEO_COMPARTILHADO_COMPONENTES_TSTUB.md`). Nenhum número mudou. Ver `docs/METODOLOGIA_COMPORTAMENTO_MECANICO_TSTUB.md`, seção 1.2.

## 2. Arquivos modificados e criados

**Modificados**
- `src/design/Connections.h/.cpp` — `CheckOrigin`, `Check::origin`, `informationCheck` com origem, rótulo "…APROVAÇÃO GLOBAL".
- `src/design/EndPlateConnection.cpp` — origem `ColumnSectionInfo` na seção do pilar.
- `src/design/ColumnFlangeLever.h/.cpp` — origem estrutural das 3 verificações do A2; `isColumnFlangeLeverCheck` por origem; advertência como mensagem; linha de `dw` estimado; nome da advertência acentuado.
- `src/design/TStub.h/.cpp` — `HeadWidth`/`boltHeadWidth` (`boltHeadAcrossPoints` mantida).
- `src/app/services/JointComponentService.cpp` — mensagem de pilar não informado (o filtro `ratiosOf` já usava `isColumnFlangeLeverCheck`, agora por origem).
- `src/app/services/ConnectionCatalogEndPlateSplice.cpp` — descarte de verificações de pilar do A2 por origem.
- `src/app/services/ReportServiceDesign.cpp` — célula e título complementares, mensagem da advertência.
- `src/ui/NodeConnectionsDialog.h/.cpp` — cor/itálico/glifo, mensagem da advertência, `updateRowsGuidance`, `useOneRow`, botão e aviso.
- `src/report/FormulaText.cpp` — correção do corte de multibyte; `src/report/DocxWriter.cpp` — descarte de caracteres de controle ilegais em XML 1.0 e estilo `Complementary`; `src/report/ReportHtml.cpp` e `src/report/ReportModel.h` — `CellStatus::Complementary`.
- `src/tests/TestTStub.cpp` (teste da advertência reescrito para a nova semântica), `src/tests/CMakeLists.txt`.
- `docs/NUCLEO_COMPARTILHADO_COMPONENTES_TSTUB.md`, `docs/RELATORIO_AUDITORIA_POS_A2.md` (correção 40 → 42 pontos).

**Criados**
- `src/tests/TestDocxEncoding.cpp`, `src/tests/TestMomentRotationRegression.cpp`, `src/tests/TestPostA2Sanitation.cpp`.
- `src/tests/golden/momento_rotacao.txt` (referência de regressão).
- `docs/RELATORIO_SANEAMENTO_POS_A2.md`.

Nenhum golden existente foi alterado; `numerics.txt` continua passando sem regravação. Nenhum teste existente foi removido; um (a advertência) foi reescrito porque o seu contrato mudou por exigência desta tarefa (de demanda/resistência para mensagem), com a condição R_A ≥ 1,10·R_B preservada e verificada a partir dos números impressos na mensagem.

## 3. Arquitetura da identificação de origem

```
CheckOrigin { Design | ColumnFlangeLeverNbr | ColumnFlangeLeverEn | DivergenceWarning | ColumnSectionInfo | Information }
```
- `Design` é o padrão: verificações tradicionais, inclusive "Efeito de alavanca - espessura da placa" (anterior ao A2), continuam na varredura de resistência.
- A **Camada A** é marcada pela estrutura de `columnFlangeLeverChecks`: tudo o que entra em `out` até o fim do bloco da Camada A (avaliada, não verificável ou não aplicável) recebe `ColumnFlangeLeverNbr`; a Camada B e a advertência recebem a origem no construtor. Não há comparação de texto.
- `isColumnFlangeLeverCheck(check)` = origem ∈ {Nbr, En, Warning}. Usado por `JointComponentService::ratiosOf` (filtro de momento-rotação) e pelo descarte da emenda viga–viga.
- A origem não participa de `ratio()`, `summarize`, `maxRatio` nem da aprovação; é metadado. As constantes de nome (`kLever*Name`) ficam só para exibição.
- Limite: as demais verificações de pilar da emenda ("Apoio:", "Painel de alma", enrijecedores) ainda são identificadas por nome (pré-existente, fora do escopo).

Testes (`TestPostA2Sanitation`): origens atribuídas por estrutura (2 fileiras e 1 fileira, advertência); coincidência de nome com origem `Design` **não** é excluída; qualquer nome com origem A2 é excluída; as outras origens nunca; verificação de chapa pré-A2 permanece com `Design` e fora do filtro; `maxRatio`/`Summary` recalculados de forma independente iguais aos do serviço; `summarize` com e sem os complementares idêntico; a emenda viga–viga não contém verificações A2/seção do pilar.

## 4. Resultados da regressão momento-rotação

`TestMomentRotationRegression` (automatizada, sem edição de produção): 14 casos, comparação textual linha a linha com `src/tests/golden/momento_rotacao.txt`, valores com 12 algarismos significativos, **sem tolerância**. Cobre rigidez inicial, Mj,Rd (EN usado, EN, NBR), `nbrCheckAvailable`/`nbrBelowCurve`, 42 pontos da curva, métricas (início da não linearidade, rotações, secante/tangente em Mj,Rd, rigidez Sj,ini/2), secante/tangente a 25, 50, 66,7, 80 e 100 % de Mj,Rd, classificação, rótula bilinear equivalente e memória de cálculo.

Integridade da referência: `before.txt` (A2 desligado) e `after.txt` (A2 ligado) de `docs/validacao_alavanca/auditoria_pos_a2/` foram verificados **byte-idênticos** antes de serem usados; a referência é essa saída, com **uma** linha alterada (mensagem do caso C14, item 6 desta tarefa). Resultado: a saída atual coincide exatamente com a referência.

Verificações adicionais: (i) o Mj,Rd do caso padrão continua 83,9668 kN·m (EN) e 66,2801 kN·m (NBR), os valores registrados em `docs/AUDITORIA_DIMENSIONAMENTO_LIGACOES.md` antes de qualquer alteração; (ii) **teste de mutação**: com o filtro desligado temporariamente, o teste **falha**; fonte restaurada e SHA-256 conferido.

Limites: a referência mantém A1/M2; a suíte não executa o solver global com a rótula equivalente (só `deriveEquivalent`); são 14 casos; comparação textual (mudança de compilador pode exigir regravar com justificativa, via `STRUCTURA_MR_OUT`).

## 5. Testes DOCX

**Causa raiz.** `FormulaText::mapIdentifier` tratava a inicial de um identificador como 1 **byte** (`std::string base(1, id[0])`, `id.substr(1)`): "γa1" (CE B3 + "a1") virava `\xCE` + `_{\xB3a1}`, sequência UTF-8 inválida dentro de `<m:t>`. Afetava todo relatório com `γ`, `λ` etc. em fórmula, com ou sem A2. Correção geral (comprimento do primeiro ponto de código UTF-8), sem substituições pontuais de caracteres; além disso, `esc()` agora descarta caracteres de controle ilegais em XML 1.0.

`TestDocxEncoding` (3 testes): 11 expressões e 12 fórmulas LaTeX com letras gregas, acentuadas, subscritos/sobrescritos e texto em português geram LaTeX/OMML com UTF-8 válido e XML bem formado; "γa1" produz `<m:sSub>` com "γ" inteiro e o subscrito "a1"; `α^{2}_{x}` produz `<m:sSubSup>`; pacote completo (capa, título, cabeçalho, parágrafos, 23 equações, tabela, caractere de controle): partes obrigatórias presentes, todo `.xml`/`.rels` com UTF-8 válido e bem formado (`QXmlStreamReader`), todo destino de relação existe, texto em português preservado.

Evidência do defeito: com a biblioteca ainda **sem** a correção (build incremental defasado), o teste falhou em 3 de 5 casos pelos motivos acima; após reconstruir, 5/5 passam. Não há comparação com um relatório "antes" gerado pelo mesmo teste além disso.

**Limite explícito:** a validação é uma análise estrutural própria do pacote OOXML. **O arquivo não foi aberto no Word nem validado por um validador OOXML externo.**

## 6. Testes de interface

`TestPostA2Sanitation` instancia o `NodeConnectionsDialog` real fora da tela (`offscreen`) e o relatório HTML/DOCX:
- Complementar: texto "◇ MÉTODO COMPLEMENTAR — NÃO PARTICIPA DA APROVAÇÃO GLOBAL", cor `#5b4bb7`, itálico; INFORMAÇÃO continua `#6b7785` (cores distintas verificadas).
- Advertência: mensagem na coluna do nome; solicitação/resistência "-"; sem "η".
- Catálogo (2 fileiras): botão "Usar 1 fileira" visível e aviso com "6.3.5.4" e "NÃO VERIFICÁVEL"; **antes do clique `tRows` continua 2**; após o clique `tRows = 1`, a Camada A passa de NÃO VERIFICÁVEL a avaliada, o botão some e o aviso registra "alteradas de 2 para 1 por ação do usuário". Ligação de 1 fileira e emenda viga–viga: sem botão.
- Relatório: HTML com o rótulo complementar, cor `#4b3fa0` e a mensagem; sem "η informativo" na advertência; DOCX com o preenchimento `ECE9FA`.

Não executado: a janela principal do aplicativo, a exportação para PDF, o Word, e inspeção visual das capturas (o ambiente `offscreen` não tem fontes; verifiquei texto e cores por leitura de widgets).

## 7. Situação do catálogo

Padrão de 2 fileiras **mantido**. O diálogo informa o limite da verificação NBR da mesa do pilar (1 fileira), que 2 ou mais fileiras resultam em NÃO VERIFICÁVEL, que o T-stub EN é só informativo, e oferece "Usar 1 fileira" (a resistência da ligação diminui; o campo pode ser aumentado de volta). A ação é visível e intencional, e atualiza dado, desenho (via `showEvaluation`) e verificações pelo mesmo caminho da edição manual do campo. Nenhuma geometria muda sem o clique. Não há diálogo de confirmação (o clique é a ação e o resultado é exibido); não foi testado com usuários.

## 8. Limitações pendentes
- Várias fileiras continuam NÃO VERIFICÁVEL (decisão mantida); falta MEF independente para habilitá-las.
- A alavanca da mesa do pilar não participa do confronto de nível C da ferramenta momento-rotação, para nenhum número de fileiras.
- Regressão momento-rotação: sem A1/M2 isolado, sem solver global, 14 casos, comparação textual.
- DOCX/PDF: não abertos em Word/leitores; disponibilidade do glifo "◇" em fontes do Word/PDF não verificada.
- `dw` estimado não conferido com catálogo; efeito não medido fora dos diâmetros tabelados.
- Janela principal do aplicativo não executada; interface verificada pelo diálogo real em modo offscreen.
- Verificações de pilar da emenda viga–viga ("Apoio:", "Painel de alma", enrijecedores) ainda por nome.
- Migração do T-stub da ferramenta, rigidez e lei força-deformação no núcleo, Anexo Nacional: planejados, não iniciados.
- Observação de processo: durante este trabalho o `StructuraCore.lib` ficou defasado em um build incremental; a biblioteca foi removida e reconstruída, e o ciclo completo (build + `ctest`) foi refeito ao final. Fica o alerta de que um teste novo deve ser confirmado após reconstrução completa.

## 9. Resultado completo da suíte

`ctest -C Release` após reconstrução completa: **34/34 aprovados, 0 falhas** (31 anteriores + `TestDocxEncoding`, `TestMomentRotationRegression`, `TestPostA2Sanitation`). Inclui `TestTStub` (benchmarks SCI P398 e as 26 amostras IDEA a 2 %, Camada A, estados `Complementary`, advertência), `TestJointClassification` (51 casos), `TestGoldenNumerics` (sem regravação), `TestDesign`, `TestConnectionCatalog`, `TestReport`, `TestInputValidation`.

Os requisitos "Camada A", "Complementary" e "benchmarks" são cobertos pelos testes já existentes do `TestTStub` (todos aprovados após as alterações); nesta tarefa só foi reescrito o da advertência.
