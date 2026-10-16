# Mapa dos arquivos .cpp

Este arquivo resume, de forma sucinta, a funcao de cada `.cpp` encontrado em `src/`.

Total mapeado: 44 arquivos.

## App

- `src/app/services/AnalysisService.cpp`: monta modelo para analise estrutural, garante IDs externos e formata relatorio textual de resultados.
- `src/app/services/DatModelIoService.cpp`: carrega e salva modelo em formato `.dat`, com ordenacao/normalizacao de dados para interoperabilidade.
- `src/app/services/IdGeneratorService.cpp`: gera e corrige IDs externos de materiais, secoes e barras.
- `src/app/SolverBridge.cpp`: encapsula chamada ao solver externo (DLL), convertendo dados e retornando resultados/erros.
- `src/ui/SectionPlacementWidget.cpp`: desenho interativo da secao (giro, 9 pontos de referencia, no, centroide; arrastar para deslocar).
- `src/ui/SectionPlacementPanel.cpp`: janela flutuante de posicionamento da secao e articulacoes das barras selecionadas.
- `src/app/services/SectionPlacementService.cpp`: calcula a excentricidade do eixo da barra a partir do ponto de referencia da secao + deslocamento, as extensoes da secao e as inercias nos eixos de referencia (Mohr).
- `src/app/UndoRedoService.cpp`: encapsula `QUndoStack` e cria comandos de undo/redo para edicoes de cena/propriedades.

## Entrada

- `src/main.cpp`: ponto de entrada da aplicacao; configura formato OpenGL/VTK, inicializa `QApplication` e abre `MainWindow`.

## Testes

- `src/tests/TestArchitectureBoundaries.cpp`: testa fronteiras arquiteturais (dependencias proibidas entre camadas).
- `src/tests/TestCoreModels.cpp`: testa modelos de dominio (vetor, no, barra, material, secao, grid line).

## UI

- `src/ui/AssignBarPropertiesDialog.cpp`: dialog para atribuir material/secao em lote a barras selecionadas.
- `src/ui/BarPropertiesDialog.cpp`: dialog para editar material/secao de barra(s) no contexto atual.
- `src/ui/DistributedLoadDialog.cpp`: dialog para definir carga distribuida (componentes e sistema de referencia).
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
- `src/ui/ProfileManagerDialog.cpp`, `ProfileBrowser.cpp`, `ProfileEditorWidget.cpp`, `ProfilePickerDialog.cpp`: gerenciador de perfis (biblioteca por categoria laminado/moldado a frio/concreto, criacao de perfil, escolha de perfil da barra).
- `src/ui/SelectionModel.cpp`: modelo de selecao de nos/barras com modos de selecao e emissao de mudancas.
- `src/ui/TemporaryAnalysisResultsDialog.cpp`: dialog temporario para exibicao textual de resultados de analise.

## Viz

- `src/viz/CustomInteractorStyle.cpp`: comportamento customizado de interacao VTK (mouse/camera).
- `src/viz/LoadSupportVisualMapper.cpp`: converte dados de cargas/apoios da aplicacao para estruturas visuais do `SceneController`.
- `src/viz/LocalCoordinateSystem.cpp`: calcula/desenha sistema de eixos locais de barras.
- `src/viz/SceneController.cpp`: controlador principal da cena VTK (atores, pickers, grid, barras/nos, highlights e render requests).

## Nota

- Este mapa considera apenas codigo-fonte versionado em `src/`.
- `.cpp` autogerados dentro de `build/` nao fazem parte da base de manutencao funcional.
