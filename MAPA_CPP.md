# Mapa dos arquivos .cpp

Este arquivo resume, de forma sucinta, a funcao de cada `.cpp` encontrado em `src/`.

Total mapeado: 44 arquivos.

## App

- `src/app/BarService.cpp`: implementa operacoes de barras (criacao, remocao, conectividade, atribuicao de material/secao, k-point e utilitarios de comprimento).
- `src/app/NodeService.cpp`: implementa operacoes de nos (criacao, remocao, reposicionamento, restraints, busca e contagem).
- `src/app/SceneControllerFacade.cpp`: coordena sincronizacao entre services/repository e camada de renderizacao, montando snapshots e refletindo selecao/highlight.
- `src/app/services/AnalysisService.cpp`: monta modelo para analise estrutural, garante IDs externos e formata relatorio textual de resultados.
- `src/app/services/DatModelIoService.cpp`: carrega e salva modelo em formato `.dat`, com ordenacao/normalizacao de dados para interoperabilidade.
- `src/app/services/IdGeneratorService.cpp`: gera e corrige IDs externos de materiais, secoes e barras.
- `src/app/SolverBridge.cpp`: encapsula chamada ao solver externo (DLL), convertendo dados e retornando resultados/erros.
- `src/app/UndoRedoService.cpp`: encapsula `QUndoStack` e cria comandos de undo/redo para edicoes de cena/propriedades.

## Entrada

- `src/main.cpp`: ponto de entrada da aplicacao; configura formato OpenGL/VTK, inicializa `QApplication` e abre `MainWindow`.

## Testes

- `src/tests/TestArchitectureBoundaries.cpp`: testa fronteiras arquiteturais (dependencias proibidas entre camadas).
- `src/tests/TestCoreModels.cpp`: testa modelos de dominio (vetor, no, barra, material, secao, grid line).
- `src/tests/TestRepositoryAndServices.cpp`: testa repositorio em memoria e services de aplicacao.
- `src/tests/TestSceneControllerFacade.cpp`: testa comportamento da facade de cena e reacoes a eventos/atualizacoes.

## UI

- `src/ui/AssignBarPropertiesDialog.cpp`: dialog para atribuir material/secao em lote a barras selecionadas.
- `src/ui/BarPropertiesDialog.cpp`: dialog para editar material/secao de barra(s) no contexto atual.
- `src/ui/CoordinateDialog.cpp`: dialog simples de entrada de coordenadas XYZ para insercao de no.
- `src/ui/DistributedLoadDialog.cpp`: dialog para definir carga distribuida (componentes e sistema de referencia).
- `src/ui/GridDeleteTooltipHelper.cpp`: helper de exibicao/ocultacao de tooltip no modo de exclusao de linhas de grid.
- `src/ui/GridDialog.cpp`: dialog para parametrizar geracao de grid (passos e contagens por eixo).
- `src/ui/GridInsertController.cpp`: controla estado e atualizacao visual do fluxo de insercao de linhas de grid.
- `src/ui/MainWindow.cpp`: implementacao base da janela principal, integrando viewport, painel de propriedades e wiring geral.
- `src/ui/MainWindowAnalysisIo.cpp`: fluxo de rodar analise estrutural e operacoes de abrir/salvar modelo `.dat`.
- `src/ui/MainWindowCommands.cpp`: handlers de comandos de modelagem e visualizacao (inserir no/barra, grid, camera etc).
- `src/ui/MainWindowEventFilter.cpp`: roteamento de eventos de mouse/teclado por modo/comando ativo.
- `src/ui/MainWindowGridStatus.cpp`: gerencia estado de comandos de grid, labels/status e commit/cancelamento.
- `src/ui/MainWindowLoads.cpp`: aplica/edita cargas e apoios e sincroniza visualizacao correspondente na cena.
- `src/ui/MainWindowPresenter.cpp`: presenter com estado compartilhado da UI e handlers de edicao de propriedades.
- `src/ui/MainWindowProperties.cpp`: integra painel de propriedades com selecao atual e dialogs de propriedades/IO.
- `src/ui/MainWindowRibbon.cpp`: construcao da ribbon, tabs, botoes e organizacao de acoes.
- `src/ui/MainWindowViewportInteraction.cpp`: logica de interacao no viewport (pick, highlight, insercao de no/barra, grid).
- `src/ui/MainWindowWindowState.cpp`: comportamento de estado da janela (maximize, toggles de visualizacao, eventos de mudanca).
- `src/ui/MaterialDialog.cpp`: dialog de cadastro/edicao de material.
- `src/ui/NodalLoadDialog.cpp`: dialog de carga nodal (forcas/momentos).
- `src/ui/PropertiesPanel.cpp`: implementa painel lateral de propriedades de nos, barras e grid.
- `src/ui/PropertiesPanelEntryBuilder.cpp`: monta entradas de painel de propriedades a partir da cena/repositorio.
- `src/ui/RestraintDialog.cpp`: dialog para definir restraints/graus de liberdade bloqueados.
- `src/ui/SectionDialog.cpp`: dialog de cadastro/edicao de secao.
- `src/ui/SelectionModel.cpp`: modelo de selecao de nos/barras com modos de selecao e emissao de mudancas.
- `src/ui/TemporaryAnalysisResultsDialog.cpp`: dialog temporario para exibicao textual de resultados de analise.

## Viz

- `src/viz/CustomInteractorStyle.cpp`: comportamento customizado de interacao VTK (mouse/camera).
- `src/viz/LoadSupportVisualMapper.cpp`: converte dados de cargas/apoios da aplicacao para estruturas visuais do `SceneController`.
- `src/viz/LoadVisualization.cpp`: cria e atualiza atores/geometrias de cargas nodais/distribuidas e momentos.
- `src/viz/LocalCoordinateSystem.cpp`: calcula/desenha sistema de eixos locais de barras.
- `src/viz/SceneController.cpp`: controlador principal da cena VTK (atores, pickers, grid, barras/nos, highlights e render requests).

## Nota

- Este mapa considera apenas codigo-fonte versionado em `src/`.
- `.cpp` autogerados dentro de `build/` nao fazem parte da base de manutencao funcional.
