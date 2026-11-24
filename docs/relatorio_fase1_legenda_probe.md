# Fase 1 — Legenda de cores e Probe de resultados de cascas na vista principal

Escopo: funcionalidade F do plano original (auditoria da Etapa 2: "PODE AVANÇAR; probe com interpolação bilinear isoparamétrica; esforços descontínuos entre elementos, documentar o alisamento"). Nenhuma DLL, formulação de elemento, licenciamento ou proteção foi alterada.

## 1. O que foi implementado

| Item | Resultado |
|---|---|
| WP1 — escalares no `RenderMesh` | `scalars` (valor exibido por vértice), `solverScalars` (valor do próprio elemento, sem média), título, unidade, flags `smoothed`/`fixedRange`, `unitsPerMeter`, objeto e elemento por quadrilátero, `restPoints` (sem deformada) e `geometryRevision` (hash da geometria). `consistent()` confere valores × vértices. |
| WP2 — `ColorLegendOverlay` | Widget filho do `m_vtkWidget`, transparente ao mouse, sem janela própria. Título, unidade, barra de cores (mesma função `rainbow` da malha), 5 marcas (máx., 3 intermediárias, mín.), nota "faixa fixa"/"alisado nos nós". Dimensiona-se pela altura da vista. |
| WP3 — Probe | `ShellRenderer::pickPoint` (vtkCellPicker, quad + ponto da cena). `ShellProbe`: funções de forma Q4, inversão isoparamétrica por Gauss-Newton em 3D (vale para quad distorcido e empenado) e interpolação Q4 dos valores nodais. Cartão flutuante + marcador no ponto, e linha na barra de status. |
| WP4 — integração | Um único fluxo: `ShellDialog` → `callbacks.render` → `MainWindow::applyShellRenderMesh` (cena + legenda + probe). A malha sem resultados (`refreshShellVisuals`) usa o mesmo ponto de entrada, o que limpa legenda e probe. O diálogo não conhece a legenda nem o probe. |
| WP5 — controle | Ação "Consultar resultado" (checável) na aba Cascas, grupo Análise; habilitada só com resultados visíveis. Ativa: clique sobre casca consulta; clique fora de casca segue a seleção normal. Desativar restaura o cursor e some o cartão. A legenda aparece sozinha com resultados. |

## 2. Decisões técnicas
1. **Valor exibido × valor do solver.** O que colore a malha, e o que o probe mostra como principal, é o valor *exibido* (média nos nós da mesma zona quando "alisar" está ligado: recurso gráfico, como já era). O valor do **próprio elemento** é guardado intacto (`solverScalars`) e o cartão o mostra sempre que difere do exibido. O alisamento não toca os valores do solver nem a descontinuidade física entre elementos.
2. **Interpolação.** Q4 bilinear isoparamétrica nos 4 valores de canto (os mesmos 4 vértices de cada quadrilátero), nunca baricêntrica de triângulos. Valor inválido (NaN/∞) em canto com peso diferente de zero ⇒ sem valor, nunca "completado" por vizinhos.
3. **Valores inválidos.** NaN/∞ viram vértices cinza (`kInvalidValueColor`), não entram na faixa; se nenhum valor é válido a malha fica sem resultados (sem legenda/probe).
4. **Faixa constante.** `colorPosition` devolve 0,5 quando a diferença é só ruído numérico (< 1e-9 do módulo), em vez de amplificá-lo até o fundo da escala. Muda a cor só em casos degenerados.
5. **Sem reconstrução desnecessária.** `ShellRenderer::setMesh` compara `geometryRevision`: igual ⇒ só o array de cores é atualizado (contadores `geometryRebuilds`/`colorUpdates`). Trocar de grandeza, de faixa ou realçar objeto não recria a geometria. O `ShellDialog` ainda reconstrói o `RenderMesh` em CPU a cada mudança (inalterado).
6. **Coordenadas do probe.** Em metros, na geometria *sem deformada* (e indicado quando a vista está deformada); `viewPosition` guarda a deformada.
7. **Vistas de verificação** (utilização, armadura): a legenda usa o título e a unidade da verificação (`RenderOptions::overrideLabels`).

## 3. Arquivos
**Novos:** `src/shell/ShellProbe.{h,cpp}`, `src/ui/ColorLegendOverlay.{h,cpp}`, `src/ui/MainWindowShellProbe.cpp`, `src/tests/TestShellProbe.cpp`, `icons/svg/shell-probe.svg`.
**Alterados:** `src/shell/ShellVisual.{h,cpp}` (dados + hash + inválidos), `src/viz/structure/ShellRenderer.{h,cpp}` (`pickPoint`, atualização só de cores), `src/viz/SceneController.{h,cpp}` (`pickShellPoint`), `src/ui/MainWindow.h` (membros e métodos da Fase 1), `src/ui/MainWindowShell.cpp` (callbacks passam por `applyShellRenderMesh`), `src/ui/MainWindowSelection.cpp` (probe em `clickSelect`), `src/ui/MainWindowEventFilter.cpp` (reposiciona a legenda), `src/ui/MainWindowPlacement.cpp` (cartão acompanha a câmera), `src/ui/MainWindowRibbon.cpp` (ação), `src/ui/ShellDialog.cpp` (rótulos da verificação), `src/tests/TestShellUi.cpp` (fluxo real), `src/tests/CMakeLists.txt`, `CMakeLists.txt`, `resources.qrc`.

## 4. Testes
**Novo `TestShellProbe` (27 casos):** partição da unidade e valores nodais das funções de forma; interpolação no centro, cantos, meio de arestas e solução analítica; campo linear exato em quad distorcido e empenado; valor inválido; inversão (ida e volta em retângulo, distorcido e empenado, até as arestas, fora da aresta com limite e distância, degenerado e não finito); valores × vértices; positivos e negativos; constante e quase constante; NaN/∞ e todos inválidos; sem resultados e resultado de tamanho errado; alisamento preserva `solverScalars`; trocar a grandeza mantém `geometryRevision`; probe: elemento, objeto, coordenadas (cena em mm), valor, cantos e centro, exibido × solver, deformada, dados inválidos, elemento distorcido; legenda (conteúdo, faixa fixa, ocultar, tamanhos de 260×180 a 1920×1080); renderizador só recolore; picking VTK fora da tela (quad, ponto, fora da placa, oculto).
**Atualizado `TestShellUi`:** no fluxo real do diálogo com o solver (`tankAnalysisAndDesignThroughTheDialog`), `RenderMesh` consistente, analisar só recolore (mesma geometria), probe no centro de um elemento devolve elemento, objeto e valor coerentes com a média dos vértices.

**Resultados (build-dev, Comercial):** `ctest` **24/24** (23 anteriores + `TestShellProbe`). `TestShell`, `TestShellCoupling`, `TestShellMeshQuality`, `TestProjectIoAndSolver`, `TestRibbonWidgets` e demais passam. A tolerância de um caso (1e-6) acompanha a precisão `float` dos vértices do `RenderMesh`.

## 5. Builds
| Configuração | Resultado |
|---|---|
| Comercial, `STRUCTURA_DEV_BUILD=ON` (`build-dev`) | compila; `ctest` 24/24; executável sobe e permanece vivo 12 s (fumaça) |
| Comercial, `STRUCTURA_DEV_BUILD=OFF` (pasta temporária, distribuição redirecionada, removida) | compila |
| Demo, `STRUCTURA_DEV_BUILD=OFF` (idem) | compila |
Warnings novos: nenhum. Os 2 existentes (`MainWindowErrors.cpp`, `TestProjectIoAndSolver.cpp`/`TestReport.cpp` `[[nodiscard]]`) são anteriores.

## 6. Limitações e problemas não resolvidos
- **Sem teste automatizado no nível do `MainWindow`:** não existe suíte que construa a janela principal. A ligação (`applyShellRenderMesh`, ação da faixa, clique em `clickSelect`, cartão e marcador, reposicionamento por câmera/redimensionamento) **compila e o aplicativo sobe, mas não foi exercitada por teste nem por uso manual**. Os componentes que ela usa (legenda, picking, interpolação, `RenderMesh` com resultados reais do solver) estão testados isoladamente. Recomendado: roteiro manual (analisar um tanque, trocar a grandeza, ativar o probe, redimensionar, fechar o editor).
- Probe só no **ponto tocado**, não acompanha o cursor (conforme o escopo). Em elementos empenados, o picker acerta os triângulos e o probe projeta sobre a superfície bilinear (distância reportada em `ProbeHit::distance`, mostrada só no estado interno).
- Não há seletor de grandeza ou de faixa na vista principal (permanecem no `ShellDialog`, como pedido). A legenda não é arrastável.
- A Demo foi **compilada**, mas os testes não foram executados na edição Demo.
- Valores do probe em vistas **deformadas** usam as coordenadas sem deformada; o cartão indica isso.
- Limitações físicas herdadas (não alteradas): domos finos não aprovados para deslocamentos, elementos do polo (4–6% de erro de membrana), esforços descontínuos entre elementos (agora exibidos como "valor do elemento").
- O `ShellDialog` mantém sua própria legenda textual (não removida, para não alterar o fluxo existente).

## 7. Integridade
DLLs, `.enc` e executáveis comerciais: SHA-256 inalterados (`rds.dll` f40c09a9…, `mefs.dll` 09ffc08a…, `rds.enc` c3a9b8b2…, `mefs.enc` ff6c9550…, `Structura.exe` 04298a1a…, `StructuraDemo.exe` 61c48129…). `build-dev/Release/Structura.exe` (binário de desenvolvimento) foi recompilado. Nada fora do escopo da Fase 1 foi modificado nesta tarefa (as alterações de `dll_cipher.cpp`, `solver_security.h`, `SolverBridge.cpp` e `Femdll`/`Structuradll` são das Etapas 3 e 4, ainda não commitadas).
