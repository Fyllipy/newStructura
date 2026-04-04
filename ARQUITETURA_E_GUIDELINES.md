# Arquitetura, Estrutura de Pastas e Guidelines

## 1) Escopo desta documentacao

Este documento descreve a arquitetura atual do projeto, a funcao de cada pasta relevante e um conjunto de guidelines para futuras implementacoes.

Base usada:
- Codigo-fonte em `src/`
- Build configuration em `CMakeLists.txt`
- Guias existentes: `README.md`, `DEVELOPER_GUIDE.md`, `MAINTENANCE.md`
- Testes de arquitetura e comportamento em `src/tests/`

Observacao importante:
- A pasta `build/` e gerada por CMake/Visual Studio e nao representa codigo-fonte de manutencao manual.

---

## 2) Visao geral da arquitetura

O projeto segue um modelo em camadas, com separacao clara entre dominio, aplicacao, interface grafica e renderizacao 3D.

Fluxo principal:

`UI (Qt Widgets)` -> `App (services/facades/repository)` -> `Core (modelos)`

`Viz (VTK)` implementa gateways de cena e renderiza o estado que vem do App/UI.

### Camadas

1. `src/core/model`
- Entidades de dominio (Node, Bar, Material, Section, GridLine, Vector3).
- Deve permanecer o mais independente possivel de UI/renderizacao.
- Contem regras/invariantes de dados do modelo estrutural.

2. `src/app`
- Casos de uso e logica de negocio.
- Services para nos, barras, undo/redo e integracao com solver.
- Contratos de acesso/edicao da cena (`ISceneModelGateway`, `ISceneEditGateway`) para evitar acoplamento direto com implementacao VTK.
- Repositorio em memoria (`InMemoryModelRepository`) para persistencia em runtime.

3. `src/ui`
- MainWindow, dialogs, painel de propriedades e estado de selecao.
- Orquestra interacoes do usuario e delega mudancas para camada App.
- Nao deve concentrar regra de negocio pesada; deve chamar services/facade.

4. `src/viz`
- Implementacao concreta de renderizacao e interacao 3D usando VTK.
- `SceneController` centraliza atores, pick, grid, camera e visuais auxiliares (cargas, apoios, eixos locais).

5. `src/tests`
- Testes unitarios e de fronteira arquitetural.
- Inclui verificacoes de boundaries (por exemplo: evitar dependencia direta de UI/App em implementacoes concretas indevidas).

### Padroes recorrentes

- Facade: `SceneControllerFacade`
- Repository: `IModelRepository` + `InMemoryModelRepository`
- Gateway/Ports: `ISceneModelGateway`, `ISceneEditGateway`
- Command/Undo: `UndoRedoService` com `QUndoStack`
- Presenter de estado de UI: `MainWindowPresenter`

---

## 3) Estrutura de pastas (raiz)

### `icons/`
- Recursos visuais da aplicacao (icones usados na ribbon, botoes e acoes).

### `src/`
- Codigo-fonte principal (core + app + ui + viz + testes).

### `build/`
- Artefatos gerados por CMake/Visual Studio.
- Conteudo tipico: `.sln`, `.vcxproj`, dlls copiadas, plugins Qt, outputs Release/Debug, arquivos autogerados.
- Nao usar como referencia para implementacao de features.

### Arquivos da raiz

- `CMakeLists.txt`
  - Define targets `StructuraLib` e `StructuraRibbon3D`.
  - Organiza fontes por grupos (CORE, APP, UI, VIZ).
  - Configura Qt, VTK, autoinit VTK, teste (`enable_testing`) e copia de runtime no Windows.
  - Faz link opcional com `solver_dll` (via `STRUCTURA_SOLVER_HINT_DIR`).

- `README.md`
  - Setup de ambiente (Qt/VTK/CMake/MSVC), build e uso basico.

- `DEVELOPER_GUIDE.md`
  - Guia tecnico focado em `LoadVisualization` e algoritmos de representacao visual de cargas.

- `MAINTENANCE.md`
  - Resumo de manutencao: fluxo para adicionar features e convencoes de codigo.

- `resources.qrc`
  - Registro de recursos Qt.

- `consumeDll.md`
  - Notas para consumo de DLL externa (solver/integracao).

---

## 4) Estrutura de pastas (`src/`) com funcao detalhada

### `src/core/model/`
Responsabilidade:
- Modelos de dominio estruturais e tipos matematicos basicos.

Arquivos-chave:
- `Vector3.h`: operacoes vetoriais e utilitarios geometricos.
- `Node.h`: no estrutural, coordenadas e restraints.
- `Bar.h`: elemento entre dois nos, propriedades e geometria associada.
- `Material.h`: dados de material (ex.: modulos).
- `Section.h`: dados de secao (ex.: area, inercia, torsao).
- `GridLine.h`: representacao de linhas de grade.

### `src/app/`
Responsabilidade:
- Logica de negocio, coordenacao de fluxo, regras de manipulacao do modelo, interfaces de cena e integracao com solver/IO.

Subgrupos:
- Contratos/interfaces:
  - `IModelRepository.h`
  - `ISceneModelGateway.h`
  - `ISceneEditGateway.h`
- Repositorio:
  - `InMemoryModelRepository.h`
- Services principais:
  - `NodeService.*`
  - `BarService.*`
  - `UndoRedoService.*`
  - `SceneControllerFacade.*`
  - `SolverBridge.*`
- Tipos de dados de aplicacao:
  - `ProjectDataTypes.h`
  - `SceneDataTypes.h`
- Utilitarios:
  - `LoadSystemUtils.h`

#### `src/app/services/`
Responsabilidade:
- Servicos auxiliares desacoplados de UI:
  - `IdGeneratorService.*`: IDs externos sequenciais e normalizacao.
  - `DatModelIoService.*`: import/export `.dat`.
  - `AnalysisService.*`: montagem do modelo para solver e formatacao de resultados.

### `src/ui/`
Responsabilidade:
- Interface principal e dialogs de cadastro/edicao/atribuicao.

Partes principais:
- Janela principal e modulos de implementacao:
  - `MainWindow.*`
  - `MainWindowRibbon.cpp`
  - `MainWindowEventFilter.cpp`
  - `MainWindowViewportInteraction.cpp`
  - `MainWindowGridStatus.cpp`
  - `MainWindowCommands.cpp`
  - `MainWindowWindowState.cpp`
  - `MainWindowProperties.cpp`
  - `MainWindowLoads.cpp`
  - `MainWindowAnalysisIo.cpp`
- Estado de apresentacao:
  - `MainWindowPresenter.*`
  - `SelectionModel.*`
- Dialogs de edicao/cadastro:
  - `CoordinateDialog.*`, `GridDialog.*`, `MaterialDialog.*`, `SectionDialog.*`
  - `BarPropertiesDialog.*`, `AssignBarPropertiesDialog.*`
  - `NodalLoadDialog.*`, `DistributedLoadDialog.*`, `RestraintDialog.*`
  - `TemporaryAnalysisResultsDialog.*`
- Painel de propriedades e auxiliares:
  - `PropertiesPanel.*`
  - `PropertiesPanelEntryBuilder.*`
  - `GridInsertController.*`
  - `GridDeleteTooltipHelper.*`

### `src/viz/`
Responsabilidade:
- Renderizacao 3D e interacao grafica com VTK.

Arquivos-chave:
- `ISceneRenderer.h`: contrato de renderizacao.
- `SceneController.*`: implementacao principal da cena (atores, pick, grid, camera, atualizacao de visual).
- `CustomInteractorStyle.*`: comportamento customizado de interacao.
- `LoadVisualization.*`: representacao visual de cargas/momentos.
- `LoadSupportVisualMapper.*`: mapeamento de dados de app para estruturas visuais.
- `LocalCoordinateSystem.*`: calculo/desenho de eixos locais.

### `src/tests/`
Responsabilidade:
- Testes automaticos com QtTest + CTest.

Arquivos:
- `TestArchitectureBoundaries.cpp`: protege fronteiras arquiteturais.
- `TestCoreModels.cpp`: valida entidades/modelos de dominio.
- `TestRepositoryAndServices.cpp`: valida repositorio e services.
- `TestSceneControllerFacade.cpp`: valida coordenacao facade/render.
- `CMakeLists.txt`: target de testes.

---

## 5) Guidelines para futuras implementacoes

## 5.1 Regras de arquitetura (obrigatorias)

1. Nao acoplar UI diretamente a detalhes internos de VTK.
- Preferir contratos (`ISceneModelGateway`/`ISceneEditGateway`/facade).

2. Regras de negocio ficam em `app/`, nao em dialogs/UI.
- UI coleta input, valida entrada simples e delega para service.

3. Entidades de dominio em `core/model` devem permanecer simples.
- Evitar dependencias graficas e estado de interface.

4. Operacoes com historico devem passar por undo/redo quando fizer sentido.
- Alteracoes em lote e edicoes de geometria/propriedades devem ser reversiveis.

5. Persistencia/intercambio deve usar servicos dedicados.
- Import/export `.dat` centralizado em `DatModelIoService`.

6. IDs externos devem ser consistentes para interoperabilidade com solver/arquivo.
- Reutilizar `IdGeneratorService` para garantir sequencia/normalizacao.

## 5.2 Fluxo recomendado para novas features

1. Definir/ajustar tipos no dominio (`core/model`) se necessario.
2. Criar ou estender service em `app/` para regra de negocio.
3. Expor interacao via facade/gateway quando envolver cena.
4. Integrar na UI (`MainWindow`/dialog/panel) apenas como orquestracao.
5. Se houver visual 3D novo, implementar em `viz/`.
6. Adicionar testes:
- unidade para regra nova;
- integracao de service/facade;
- boundary test se a feature altera dependencias.

## 5.3 Convencoes praticas

- Includes por caminho completo relativo a `src` (ex.: `core/model/Node.h`).
- Nomes de classe em PascalCase, metodos em camelCase.
- Evitar duplicar estado entre UI e App: usar presenter/modelos compartilhados.
- Quando comportamento for orientado a evento, preferir signal/slot ao polling.
- Manter mensagens de erro e warnings claras em operacoes de IO/analise.

## 5.4 Checklist de PR (sugestao)

- [ ] Alteracao respeita camada correta?
- [ ] Nao introduz dependencia proibida entre camadas?
- [ ] Tem teste cobrindo o comportamento novo?
- [ ] Nao quebra import/export `.dat`?
- [ ] Nao quebra fluxo de undo/redo quando aplicavel?
- [ ] Atualizou docs se comportamento visivel mudou?

---

## 6) Observacoes finais

- O projeto esta bem orientado a separacao de responsabilidades.
- O risco principal para crescimento e concentrar regra de negocio na `MainWindow`; manter a disciplina de mover regras para `app/services` evita regressao arquitetural.
- Para expansao futura (novos tipos de elemento, novos casos de carga), manter os mapeadores (`LoadSupportVisualMapper`) como fronteira entre dado de negocio e representacao visual ajuda a preservar a manutencao.
