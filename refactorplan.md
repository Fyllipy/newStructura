# Plano de Refatoração — Structura 3D

Este documento descreve um plano completo de refatoração para o projeto Structura (versão atual encontrada em src/). O objetivo é transformar o código em um design orientado a objetos consistente, aplicar práticas de Clean Code, melhorar a manutenibilidade e escalabilidade, e preparar o projeto para testes automatizados e integração contínua.

Atenção: este é um plano. NÃO IMPLEMENTAR as mudanças neste momento.

## Sumário executivo

- Objetivo principal: tornar o código estritamente orientado a objetos, com responsabilidades bem definidas, baixo acoplamento e alta coesão.
- Escopo: todos os componentes C++/Qt/VTK sob `src/` e ajustes em build scripts (CMake) e ferramentas de desenvolvimento.
- Entregáveis: design de arquitetura, diagrama de módulos, contrato de classes, roadmap de refatoração faseado, política de testes e CI, checklist de qualidade (lint, format, análise estática).

## 1. Objetivos e critérios de sucesso

Requisitos funcionais (manter):
- A UI e funcionalidades atuais (inserção de nós/barras, grid, cargas, suportes, propriedades, salvamento/carregamento) **não** devem ser alteradas como parte da refatoração; apenas reorganizadas.

Requisitos não-funcionais:
- Código fortemente orientado a objetos: cada entidade/modelo deve ser uma classe com API clara.
- Single Responsibility Principle (SRP) e Separation of Concerns para todas as classes de alto nível.
- Dependências invertidas (Dependency Inversion) onde necessário para facilitar testes e substituição de componentes.
- Uso consistente de smart pointers, RAII e normas de gerenciamento de recursos.
- Documentação clara dos contratos públicos (comentários Doxygen estilo) e testes automatizados com cobertura satisfatória das unidades críticas.
- Ferramentas de qualidade: clang-format, clang-tidy, CMake targets para rodar linter/tests.

Critérios de aceitação:
- Compilação limpa (sem warnings novos) após cada fase.
- Suíte de testes unitários rodando localmente com sucesso antes e depois de cada migração incremental que altera comportamento.
- Nenhuma regressão funcional nos comportamentos essenciais (UI e I/O) — ver plano de validação.

## 2. Estado atual (resumo rápido)

Principais pontos encontrados no código lido:
- `MainWindow.cpp` contém grande quantidade de lógica de UI e manipulação de estado (comandos, sincronização de dados, ações do usuário, construção de widgets). Está muito grande (~3000 linhas).
- `SceneController.cpp` contém lógica VTK de rendering e gerência de entidades mudando estados visuais e dados internos (pontos, barras, grid). Também muito volumoso (~1700 linhas).
- `ModelEntities.h` já define modelos simples (`Node`, `Bar`, `Material`, `Section`, `GridLine`) porém com mistura de responsabilidades e sem interfaces abstratas/contratos.
- `SelectionModel` é um bom início de separação, mas constrói API que não está suficientemente documentada e pode melhorar com testes.
- Há múltiplos diálogos (CoordinateDialog, GridDialog, etc.) que provavelmente misturam UI e lógica de validação/transformação.

Riscos identificados:
- Alto risco de regressão ao mover código entre classes sem testes.
- Dependência forte entre UI (MainWindow) e controller/scene, dificultando testes headless.

## 3. Arquitetura alvo proposta

Arquitetura em camadas (proposta):

- Core/Model (structura::model): classes puras de domínio (Node, Bar, Material, Section, Grid), invariantes e operações atômicas. Sem dependências de Qt/VTK.
- Application/Service (structura::app): serviços que gerenciam modelos (ModelRepository, ModelSerializer, GridService, LoadService, SelectionService, UndoRedoService). Interfaces definidas para permitir mocks.
- Visualization/Controller (structura::viz): adaptadores entre o modelo e VTK/Qt (SceneController refatorado em SceneRenderer), responsável apenas por *renderizar* estado e receber comandos de alto nível (e.g., addNode(id), setNodePosition(id, vec3)). Não deve conter lógica de regras do domínio.
- UI (structura::ui): widgets Qt (MainWindow, dialogs, panels) que chamam serviços de Application/Service. A UI apenas orquestra e exibe, e delega regras/transformações para services.
- Infrastructure: entrada/saída (file IO, serialization), platform adapters (timers, logging), e integrações externas.
- Tests: diretório separado com testes unitários (googletest / Qt Test), testes de integração leve (headless quando possível).

### 3.1 Estrutura de diretórios sugerida para `src/`

Para facilitar a refatoração incremental (especialmente quando a refatoração for executada por uma IA), adote a seguinte estrutura de diretórios dentro de `src/`:

- `src/core/` (ou `src/core/model/`): código de domínio puro (Node, Bar, Material, Section, Grid, Vector3). Não deve depender de Qt ou VTK.
- `src/app/` ou `src/services/`: serviços de aplicação (ModelRepository, NodeService, BarService, GridService, LoadService, SelectionService, UndoRedoService).
- `src/viz/` ou `src/renderer/`: adaptadores de visualização e a implementação VTK (ISceneRenderer, VtkSceneRenderer, LoadVisualization). Código desta pasta pode depender de VTK/Qt.
- `src/ui/`: widgets Qt (MainWindow, dialogs, panels, presenters minimalistas). As views devem depender apenas de interfaces dos services.
- `src/infrastructure/`: serializers (ModelSerializer), file IO, logging, platform adapters, e qualquer código específico de integração.
- `src/tests/`: testes unitários e de integração (gtest/QtTest). Idealmente refletindo a mesma divisão (core, services, viz).
- `src/utils/` ou `src/common/`: utilitários compartilhados (parsers, helpers math, small adapters).

Observações práticas para a IA:
- Ao mover um arquivo, preserve seu histórico de mudanças em commits separados e atômicos (um arquivo por commit quando possível).
- Nunca remova código sem garantir cobertura de testes equivalente; prefira `refactor/` PRs que apenas movem e adaptam includes e namespaces.
- Ao criar novas interfaces (ex.: `IModelRepository`, `ISceneRenderer`), siga o padrão de nomeação e coloque-as em headers dentro da pasta do domínio ou `app/`.


Padrões a usar:
- MVC/MVP para separar View (Qt widgets) do Presenter/Controller (services). Recomendo MVP para facilitar testes de presenter isolados.
- Command pattern para operações de undo/redo (já há uso de QUndoStack — mantenha, mas extraia comandos para classes próprias que dependam de services em vez do scene controller direto).
- Repository pattern para abstrair armazenamento de modelos e providenciar transações.
- Observer/Signal pattern: usar sinais/slots do Qt para notificações; mas manter interfaces C++ puras para lógica de domínio, com adaptadores que emitem sinais Qt.

## 4. Convenções de código e ferramentas

- Estilo: Google C++ Style ou LLVM/Clang style (definir .clang-format consistente). Padronizar e aplicar.
- Linters: clang-tidy com checks mínimos: modernize, performance, readability, bugprone.
- Smart pointers: evitar raw pointers ownership — use std::unique_ptr para proprietários, std::shared_ptr apenas quando realmente necessário. Qt parent-child ownership permanece para QObject/Widgets.
- Nomes: camelCase para métodos, PascalCase para classes. Prefixos `I` para interfaces (opcional). Namespaces `Structura::Model`, `Structura::App`, `Structura::Viz`, `Structura::UI`.
- Comentários: Doxygen para API pública (/// e /** */). Comentários inline explicando "porquê" quando a decisão não é óbvia.
- Tests: google-test ou Qt Test (preferir google-test para lógica pura; Qt Test pode ser usado para widgets). Adicionar CMake target `test`.

Ferramentas a adicionar/ajustar:
- `.clang-format` e `.clang-tidy` na raiz.
- CMake: targets `format`, `lint`, `test`.
- CI: GitHub Actions com matrix (windows, linux), rodando format check, clang-tidy, build e testes.

## 5. Design de classes e contratos (alto nível)

Nota: abaixo há esboço de classes — cada classe terá comentários detalhados e contratos em header files. Incluir invariantes e pré-condições.

Core/Model (sem Qt):
- Node
  - id: QUuid
  - externalId: int
  - position: Vector3 (criar uma struct Vector3 { double x,y,z; } no módulo Model)
  - restraints: std::array<bool,6>
  - getters/setters atômicos
  - comportamento: moveTo(Vector3), distanceTo(Node)

- Bar
  - id, externalId, startNodeId, endNodeId, materialId, sectionId
  - kPoint optional, lcs cache
  - métodos: length(nodeRepo), setKPoint(), computeLocalSystem(nodeRepo)

- Material, Section — simples POJOs com validações no construtor.

- Grid (nova classe)
  - atributos: xCoords, yCoords, zCoords, dx/dy/dz
  - métodos: insertLine(axis, coord1, coord2), removeLine(id), nearestLine(axis, x, y)

- ModelRepository (interface)
  - operações CRUD para Nodes, Bars, Materials, Sections, GridLines
  - transações (begin/commit/rollback) para operações compostas
  - observadores (registro de listeners) para alterações de modelo

Application/Services:
- SelectionService
  - API: selectNodes(ids, mode), selectBars(ids, mode), signals: selectionChanged
  - persist selection state separado do modelo principal

- NodeService
  - API: createNode(pos), setNodePosition(id, pos), deleteNode(id), findNode(id)
  - Use ModelRepository e emita eventos

- BarService
  - API: createBar(startId,endId,..), setProperties, computeLCS

- GridService, LoadService, SupportService — responsabilidades separadas

- UndoRedoService
  - adaptador entre comandos de domínio e QUndoStack — comandos serão classes pequenas que aplicam e desfaçam usando serviços

Visualization/Adapters:
- SceneRenderer (refatoração de SceneController)
  - Interface pura (ISceneRenderer) que expõe operações de rendering: initialize(widget), renderModelSnapshot(const ModelSnapshot&), highlightNode(id), showGridGhostLine(...)
  - Implementação VTK: VtkSceneRenderer. Implementação testaable via mock ISceneRenderer.
  - Responsabilidade: render somente; não atualizar modelo.

UI layer:
- MainWindow (refatorado): conter apenas UI wiring e presenter references
- Presenters para cada dialog/panel (CoordinateDialogPresenter, PropertiesPanelPresenter)
  - Presenter recebe serviços (via injeção) e expõe slots para a view

Contracts e DTOs:
- ModelSnapshot: leitura imutável do estado para ser renderizado pela SceneRenderer.
- Use DTOs simples que descrevem posições, cores, IDs; evitar passagem de ponteiros Qt/VTK para domain services.


### 5.1 Integração com o solver (posição atualizada: solver próprio)

Contexto atualizado: o solver é propriedade da organização e poderá ser disponibilizado de duas formas: (A) como binário executável (.exe/.bin) ou (B) como código-fonte que podemos incorporar ao repositório. O formato final de input/contratos ainda será definido e fornecido no futuro. Portanto o plano precisa preparar a base para integrar o solver de forma flexível e segura, suportando ambos os modos e permitindo adaptação simples quando o contrato for divulgado.

Princípios de integração com solver interno:
- Não assumir hoje um formato fixo de input; prover adaptadores e pontos de extensão que permitam implementar a conversão quando o contrato estiver disponível.
- Suportar dois modos de integração desde o início (adapter pattern):
  - ExternalMode: invoca um binário/serviço (CLI, DLL, REST) usando um `IExternalSolver` com implementações `ExternalSolverCli` e `ExternalSolverDll`.
  - EmbeddedMode: integração compilada/in-process quando o código-fonte do solver for incorporado (`EmbeddedSolver` implementando a mesma interface `ISolverHost`).

Passos essenciais (quando o solver é interno):
1. Preparação (agora): criar scaffolding e stubs que não dependem do formato final
  - Criar pasta `src/infrastructure/solver/` com headers e stubs: `ISolverHost.h`, `IExternalSolver.h`, `ExternalSolverCli.h`, `ExternalSolverDll.h`, `EmbeddedSolver.h`, `SolverAdapter.h`, `SolverConfig.h`.
  - Implementar `ExternalSolverMock` e `EmbeddedSolverMock` para CI e testes locais.
  - Criar um arquivo de configuração placeholder `solver/schema-placeholder.json` e `solver/README.md` explicando onde o schema real será colocado.

2. Descoberta do contrato (quando disponível):
  - Receber do time do solver o esquema de input/output (JSON schema, headers C/C++ ou documentação de API).
  - Atualizar `SolverAdapter` para implementar `convertToExternalInput(const SolverInput&, version)` e `convertFromExternalResult(...)` conforme o contrato.
  - Versionar adaptadores: permitir múltiplas versões do conversor (v1, v2) e selecionar via `SolverConfig`.

3. Integração e testes:
  - Adicionar testes unitários para os conversores usando os exemplos fornecidos pelo time do solver.
  - Implementar testes de integração smoke que executem o binário (ExternalMode) ou chamem a API in-process (EmbeddedMode) com modelos mínimos.
  - Incluir steps no CI para rodar esses testes (usar `ExternalSolverMock` quando o binário não estiver disponível no runner).

4. Operação e métricas:
  - Executar o solver em threads de trabalho (não na UI). Fornecer timeouts, retries e logging detalhado.
  - Medir latência e throughput; documentar para decidir se embed é justificável posteriormente.

Observações de arquitetura e manutenção:
- Padronize uma interface `ISolverHost` (sincronizada com `IExternalSolver` e `EmbeddedSolver`) para que o restante do sistema invoque o solver sem conhecer o modo.
- Documente a localização do schema, versões suportadas e como adicionar um novo conversor versão.
- Preserve mocks no repositório para CI e desenvolvedores locales.


Checklist de descoberta do contrato (tarefas iniciais):
1. Solicitar/obter do time do solver:
   - Documentação do formato de input e output (ex.: esquema JSON, structs C++), inclusive unidades e convenções.
   - Exemplos de arquivos de entrada e saída para modelos simples.
   - API de integração (biblioteca, CLI ou serviço), incluindo requisitos de versão e licenciamento.
   - Requisitos de performance e limitações (tamanhos máximos de modelo, formato sparse esperado, paralelização disponível).
2. Criar testes de integração de "smoke": converter um `ModelSnapshot` pequeno para o formato do solver e enviar para o solver; verificar parse e resposta básica.
3. Definir adaptadores de conversão (naming: `SolverAdapter`, `ExternalSolverInputConverter`, `ExternalSolverResultConverter`).

Recomendações de design (para a IA):
- Colocar os adaptadores em `src/infrastructure/solver/` e mantê-los responsáveis apenas por conversão/IO com o solver; não misturar regras de domínio.
- Fornecer uma interface `IExternalSolver` que esconda a forma de transporte (DLL, IPC, REST) e ofereça `run(const SolverInput&) -> SolverResult` e `asyncRun(...)` com callbacks/promises.
- Implementar fallbacks/validations: se o solver não aceitar algum caso (ex.: barras com parâmetros faltantes), o adaptador deve validar e devolver erros legíveis para a UI.

Testes e CI para integração com solver externo:
- Adicionar testes de integração que executem o adaptador contra uma _mock_ do solver (se o time do solver fornecer um mock) ou contra o binário real em um ambiente controlado.
- Incluir steps no CI para verificar que a serialização produza o formato correto (ex.: comparar JSON gerado com exemplos fornecidos).

Notas sobre segurança e deployment:
- Se o solver for distribuído como binário nativo, documentar requisitos de distribuição (DLLs, paths) e adaptar CMake para suportar link dinâmico.
- Tratar erros de rede, timeouts e formatos inválidos com mensagens claras e logs para triagem.

Sequência de implementação recomendada (IA-friendly):
1. Implementar `ExternalSolverInputConverter`/`ExternalSolverResultConverter` com testes unitários que validem transformações em exemplos mínimos.
2. Implementar `IExternalSolver` com uma implementação `ExternalSolverCli` ou `ExternalSolverDll` que invoque o solver real; adicionar um `ExternalSolverMock` para CI.
3. Integrar o adaptador com a pipeline de execução: executar o solver em thread de trabalho e converter o resultado para `SolverResult` para consumo da UI.
4. Cobrir com testes de integração (mock ou real solver) antes de habilitar em builds de produção.

Mantêm-se as recomendações técnicas anteriores sobre representação de matrizes, DOF mapping e testes numéricos, mas apenas como sugestões caso o solver exija esse formato — o foco inicial é descobrir e adaptar ao contrato existente.


## 6. Roadmap de refatoração (faseado)

Princípio: aplicar mudanças incrementais, sempre com testes, mantendo o app compilável e executável.

Fase 0 — Preparação (1-2 dias)
- Adicionar arquivos de configuração (.clang-format, .clang-tidy), CMake targets para format/lint/test.
- Adicionar skeleton de testes (CMakeLists para tests) e CI config (esqueleto GitHub Actions) — sem testes ainda.

DONE Fase 1 — Extrair Model puro (2-4 dias)
- Mover/clean `ModelEntities.h` para `src/core/model/` com melhorias: Vector3 type, método unit tests.
- Escrever testes unitários para Node, Bar, Grid basic operations.
- Resultado: modelos testados e sem Qt dependencies.

DONE Fase 2 — Introduzir ModelRepository e Services básicos (4-7 dias)
- Criar `IModelRepository` interface e uma implementação `InMemoryModelRepository` que substitui os vetores atuais de nodes/bars em SceneController.
- Criar NodeService, BarService minimal e mover lógica de nextExternalId, find material/section para services.
- Atualizar MainWindow para usar os serviços (inicialmente mantendo SceneController funcional; usar adaptadores para manter compatibilidade).
- Tests: unit tests para repository e services.

Fase 3 — Separar SceneRenderer do domínio (5-8 dias)
- Criar interface `ISceneRenderer` e implementar `VtkSceneRenderer` transferindo a maioria de `SceneController.cpp` mas preservando as funções.
- Modificar SceneController atual para se tornar um `SceneControllerFacade` que traduz eventos do Model para chamadas de ISceneRenderer, mas preservando as funções.
- Garantir que nenhuma lógica de domínio permaneça no renderer.

DONE Fase 4 — Refatorar MainWindow / UI (5-10 dias)
- Extrair grande blocos de UI logic em Presenters.
- Reduzir `MainWindow.cpp` para wiring entre widgets e presenters; mover comandos (MoveNodesCommand, SetBarPropertiesCommand) para classes de comando do UndoRedoService.
- Substituir interações diretas com SceneController por chamadas aos serviços/presenters.
- Tests: UI presenters unit tests (sem QWidgets) e smoke test manual para a janela.

Fase 5 — Dialogs, Panels e LCS (3-5 dias)
- Refatorar dialogs para seguir MVP: view (QDialog) + presenter (lógica). Validations moved to presenters/services.
- Implementar Local Coordinate System calculations em uma classe `LocalCoordinateSystemCalculator` pura e testável.

Fase 6 — Polimento e CI (2-4 dias)
- Ajustar warnings, rodar clang-tidy, atualizar CMake.
- Completar GitHub Actions: build, format check, lint, test.
- Criar documentação de contribuição e comentários Doxygen.

Total estimado (aprox.): 3–6 semanas dependendo da disponibilidade e número de desenvolvedores.

## 7. Testes, integração contínua e qualidade

- Unit tests: google-test para módulos puros (Model, Services, Utils). Objetivo inicial: cobrir 60–80% do core.
- Integration tests: pequenas suítes que inicializam InMemoryModelRepository + VtkSceneRenderer (headless teste parcial) para validar fluxo.
- UI tests: usar Qt Test ou frameworks de automação (só para checks superficiais). Prevenir testes pesados de UI.
- CI pipeline: GitHub Actions com etapas:
  - checkout
  - run clang-format (check)
  - run clang-tidy
  - build (Debug)
  - run unit tests

## 8. Mecanismo de rollback e migração incremental

- Cada fase deve ser entregue em um branch dedicado (`refactor/core-model`, `refactor/scene-renderer`, etc.).
- Não intimidar com grandes merges — cada PR pequeno e reversível.
- Testes devem ser obrigatórios antes de merge.
- Em caso de regressão, reverter PR e analisar a falha por diffs e testes.

## 9. Documentação e comentários

- Documentar APIs públicas com Doxygen.
- Em código: preferir comentários que expliquem *porquê* (decisions) e indicar invariantes.
- Adicionar README.contributing.md com padrões de commit, revisão e CI.

## 10. Exemplos de refatoração (snippets e guidelines)

- Extraia lógica não-UI para funções puras com nomes descritivos.
- Separe validação de parsing e conversão de UI.
- Transforme grandes métodos em fluxos legíveis com early returns e helpers pequenos.

Exemplo: mover lógica de buildBarEntries de `MainWindow` para `BarService::buildBarEntries(const QSet<QUuid>&)` que recebe ModelRepository e retorna DTOs para a UI.

## 11. Lista de verificação (pre-merge)

- [ ] Compilações passam em Windows e Linux
- [ ] Todos os testes unitários passam
- [ ] Nenhum warning novo introduzido
- [ ] Documentação Doxygen gerada sem erros
- [ ] CI pipeline verde

## 12. Plano de comunicação e divisão de tarefas

Sugerido:
- Sprint 0 — Preparação infra e testes
- Sprint 1 — Extrair Model e Repository
- Sprint 2 — Services e SceneRenderer
- Sprint 3 — UI presenters, diálogos e finalização

Cada sprint com PRs pequenos (máx 300–500 linhas por PR) e revisão cruzada.

## 13. Riscos, mitigação e observações finais

Riscos:
- Regressões de UI — mitigar com testes unitários e smoke tests manuais a cada PR.
- Curva de aprendizado do VTK ao separar render; mitigação: manter um wrapper abstrato e testes de integração.

Observações finais:
- Priorize criação de testes antes da refatoração de funções críticas.
- Não tente refatorar tudo em um PR. Prefira muitos PRs pequenos, revisitáveis, com testes.

---

Autor: plano gerado automaticamente (revise e ajuste estimativas locais antes de execução).

