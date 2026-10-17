cmake_minimum_required(VERSION 3.21)

project(Structura LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# User-specific paths
list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64" "D:/src/vtk")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

if (QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
endif()

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersSources
    FiltersCore          # vtkPolyDataNormals
    FiltersGeneral       # vtkContourTriangulator
    RenderingCore
    RenderingOpenGL2
    InteractionStyle
    RenderingAnnotation
    GUISupportQt
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Source Groups ---

set(CORE_SOURCES
    src/core/model/Vector3.h
    src/core/model/Node.h
    src/core/model/Bar.h
    src/core/model/Material.h
    src/core/model/Section.h
)

set(VIZ_SOURCES
    src/viz/SceneController.h
    src/viz/SceneController.cpp
    src/viz/LocalCoordinateSystem.h
    src/viz/LocalCoordinateSystem.cpp
    src/viz/LoadSupportVisualMapper.h
    src/viz/LoadSupportVisualMapper.cpp
    src/viz/CustomInteractorStyle.h
    src/viz/CustomInteractorStyle.cpp
    # ── Plano de construcao (grid, eixos e snaps) ────────────────────
    src/viz/construction/ConstructionPlane.h
    src/viz/AreaPanelRenderer.h
    src/viz/AreaPanelRenderer.cpp
    src/viz/construction/ConstructionGridRenderer.h
    src/viz/construction/ConstructionGridRenderer.cpp
    src/viz/construction/ReferenceAxesRenderer.h
    src/viz/construction/ReferenceAxesRenderer.cpp
    src/viz/construction/ConstructionScene.h
    src/viz/construction/ConstructionScene.cpp
    src/viz/construction/PlacementSolver.h
    src/viz/construction/PlacementSolver.cpp
    # ── Structure Rendering ──────────────────────────────────────────
    src/viz/structure/NodeRenderer.h
    src/viz/structure/NodeRenderer.cpp
    src/viz/structure/MemberRenderer.h
    src/viz/structure/MemberRenderer.cpp
    src/viz/structure/StructureRenderer.h
    src/viz/structure/StructureRenderer.cpp
    src/viz/loads/LoadActionRenderer.h
    src/viz/loads/LoadActionRenderer.cpp
    src/viz/structure/SolidMemberRenderer.h
    src/viz/structure/SolidMemberRenderer.cpp
    src/viz/structure/SectionCatalog.h
    src/viz/structure/SectionCatalog.cpp
    src/viz/structure/SectionShapes.h
    src/viz/structure/SectionShapes.cpp
    src/viz/structure/SupportMeshBuilder.h
    src/viz/structure/SupportMeshBuilder.cpp
    # ── Pós-Processador ──────────────────────────────────────────────
    src/viz/postprocessor/PostProcessorDataTypes.h
    src/viz/postprocessor/VisualizationTheme.h
    src/viz/postprocessor/InternalForceEvaluator.h
    src/viz/postprocessor/UniformLoadForceEvaluator.h
    src/viz/postprocessor/UniformLoadForceEvaluator.cpp
    src/viz/postprocessor/DiagramGeometryCache.h
    src/viz/postprocessor/DiagramGeometryCache.cpp
    src/viz/postprocessor/DiagramScaleManager.h
    src/viz/postprocessor/DiagramScaleManager.cpp
    src/viz/postprocessor/DiagramMeshBuilder.h
    src/viz/postprocessor/DiagramMeshBuilder.cpp
    src/viz/postprocessor/DiagramExtremaFinder.h
    src/viz/postprocessor/DiagramExtremaFinder.cpp
    src/viz/postprocessor/DiagramLabelManager.h
    src/viz/postprocessor/DiagramLabelManager.cpp
    src/viz/postprocessor/SupportGlyphFactory.h
    src/viz/postprocessor/SupportGlyphFactory.cpp
    src/viz/postprocessor/DeformedShapeRenderer.h
    src/viz/postprocessor/PostProcessorRenderer.h
    src/viz/postprocessor/PostProcessorRenderer.cpp
)

set(APP_SOURCES
    src/app/ISceneEditGateway.h
    src/app/ISceneModelGateway.h
    src/app/ProjectDataTypes.h
    src/app/SceneDataTypes.h
    src/app/UndoRedoService.h
    src/app/UndoRedoService.cpp
    src/app/SolverBridge.h
    src/app/SolverBridge.cpp
    src/app/services/IdGeneratorService.h
    src/app/services/IdGeneratorService.cpp
    src/app/services/DatModelIoService.h
    src/app/services/DatModelIoService.cpp
    src/app/services/AnalysisService.h
    src/app/services/AnalysisService.cpp
    src/app/services/CombinationService.h
    src/app/services/CombinationService.cpp
    src/app/services/SectionPlacementService.h
    src/app/services/SectionPlacementService.cpp

    src/design/DesignTypes.h
    src/design/GrossSection.h
    src/design/GrossSection.cpp
    src/design/DesignSection.h
    src/design/DesignSection.cpp
    src/design/Connections.h
    src/design/Connections.cpp
    src/design/ShearConnections.h
    src/design/ShearConnections.cpp
    src/design/ConnectionHelpers.h
    src/design/TrussConnections.h
    src/design/TrussConnections.cpp
    src/design/BasePlate.h
    src/design/BasePlate.cpp
    src/design/ColdFormedConnections.h
    src/design/ColdFormedConnections.cpp
    src/design/EndPlateConnection.h
    src/design/EndPlateConnection.cpp
    src/design/TubularJoints.h
    src/design/TubularJoints.cpp
    src/design/CompositeFloor.h
    src/design/CompositeFloor.cpp
    src/design/SupportChecks.h
    src/design/SupportChecks.cpp
    src/design/Nbr8800.h
    src/design/Nbr8800.cpp
    src/design/Nbr14762.h
    src/design/Nbr14762.cpp
    src/design/FiniteStrip.h
    src/design/FiniteStrip.cpp
    src/design/BarDesign.h
    src/design/BarDesign.cpp
    src/report/MathParser.h
    src/report/MathParser.cpp
    src/report/FormulaText.h
    src/report/FormulaText.cpp
    src/report/MathRenderer.h
    src/report/MathRenderer.cpp
    src/report/ReportModel.h
    src/report/DocxWriter.h
    src/report/DocxWriter.cpp
    src/report/ReportHtml.h
    src/report/ReportHtml.cpp
    src/app/services/ReportService.h
    src/app/services/ReportService.cpp
    src/app/services/DesignService.h
    src/app/services/DesignService.cpp
    src/app/services/MaterialLibrary.h
    src/app/services/MaterialLibrary.cpp
    src/app/services/UserLibrary.h
    src/app/services/UserLibrary.cpp
    src/app/services/ModelCheckService.h
    src/app/services/ModelCheckService.cpp
    src/app/services/ModelTemplateService.h
    src/app/services/ModelTemplateService.cpp
    src/app/services/StructuralChecks.h
    src/app/services/StructuralChecks.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/MainWindowRibbon.cpp
    src/ui/MainWindowEventFilter.cpp
    src/ui/MainWindowViewportInteraction.cpp
    src/ui/MainWindowGridStatus.cpp
    src/ui/MainWindowCommands.cpp
    src/ui/MainWindowWindowState.cpp
    src/ui/MainWindowProperties.cpp
    src/ui/MainWindowLoads.cpp
    src/ui/MainWindowAnalysisIo.cpp
    src/ui/MainWindowPlacement.cpp
    src/ui/MainWindowViewOptions.cpp
    src/ui/MainWindowEditing.cpp
    src/ui/MainWindowTopology.cpp
    src/ui/MainWindowProfiles.cpp
    src/ui/MainWindowDesign.cpp
    src/ui/MainWindowGroups.cpp
    src/ui/MainWindowBarTypes.cpp
    src/ui/MainWindowReport.cpp
    src/ui/ReportDialogs.h
    src/ui/ReportDialogs.cpp
    src/ui/MainWindowAreaLoads.cpp
    src/ui/MainWindowUndoState.cpp
    src/ui/MainWindowModelCheck.cpp
    src/ui/MainWindowConnections.cpp
    src/ui/ConnectionDialog.h
    src/ui/ConnectionDialog.cpp
    src/ui/NodeConnectionsDialog.h
    src/ui/NodeConnectionsDialog.cpp
    src/ui/MainWindowModal.cpp
    src/ui/ModalDialog.h
    src/ui/DynamicsDialog.h
    src/ui/DynamicsDialog.cpp
    src/ui/LineChartWidget.h
    src/ui/LineChartWidget.cpp
    src/ui/MainWindowDynamics.cpp
    src/ui/MainWindowSession.cpp
    src/ui/MainWindowErrors.cpp
    src/ui/ConnectionPreview.h
    src/ui/ConnectionPreview.cpp
    src/ui/CompositeDialog.h
    src/ui/CompositeDialog.cpp
    src/ui/MainWindowComposite.cpp
    src/ui/BusyProgress.h
    src/ui/BusyProgress.cpp
    src/ui/MainWindowCompare.cpp
    src/ui/CompareResultsDialog.h
    src/ui/CompareResultsDialog.cpp
    src/ui/MainWindowViews.cpp
    src/ui/ViewCubeWidget.h
    src/ui/ViewCubeWidget.cpp
    src/ui/SplitBarDialog.h
    src/ui/SplitBarDialog.cpp
    src/ui/MainWindowDiagramProbe.cpp
    src/ui/ModalDialog.cpp
    src/app/services/ModalService.h
    src/app/services/ModalService.cpp
    src/app/services/DynamicsService.h
    src/app/services/DynamicsService.cpp
    src/app/services/WindDynamicsService.h
    src/app/services/WindDynamicsService.cpp
    src/app/services/TrussGenerator.h
    src/app/services/TrussGenerator.cpp
    src/ui/NewProjectWizard.h
    src/ui/NewProjectWizard.cpp
    src/ui/TrussDialog.h
    src/ui/TrussDialog.cpp
    src/app/services/ConnectionSolids.h
    src/app/services/ConnectionSolids.cpp
    src/viz/structure/ConnectionRenderer.h
    src/viz/structure/ConnectionRenderer.cpp
    src/app/services/NotionalLoadService.h
    src/app/services/NotionalLoadService.cpp
    src/ui/MainWindowDocs.cpp
    src/ui/MainWindowDrawings.cpp
    src/ui/DrawingDialog.h
    src/ui/DrawingDialog.cpp
    src/ui/TitleBlockDialog.h
    src/ui/TitleBlockDialog.cpp
    src/ui/View3DDialog.h
    src/ui/View3DDialog.cpp
    src/ui/QuantityDialog.h
    src/ui/QuantityDialog.cpp
    src/app/services/QuantityService.h
    src/app/services/QuantityService.cpp
    src/app/services/ResultsPersistence.h
    src/app/services/ResultsPersistence.cpp
    src/app/services/ConnectionService.h
    src/app/services/ConnectionService.cpp
    src/app/services/SheetBuilder.h
    src/app/services/SheetBuilder.cpp
    src/app/services/SheetBuilderInternal.h
    src/app/services/ConnectionDrawing.cpp
    src/drawing/DrawingModel.h
    src/drawing/SheetPainter.h
    src/drawing/SheetPainter.cpp
    src/drawing/HiddenLine.h
    src/drawing/HiddenLine.cpp
    src/app/services/DxfExporter.h
    src/app/services/DxfExporter.cpp
    src/ui/MainWindowBuckling.cpp
    src/ui/BucklingDialog.h
    src/ui/BucklingDialog.cpp
    src/viz/ModeShapeRenderer.h
    src/viz/ModeShapeRenderer.cpp
    src/viz/BarTypeRenderer.h
    src/viz/BarTypeRenderer.cpp
    src/app/services/BucklingService.h
    src/app/services/BucklingService.cpp
    src/ui/ModelCheckDialog.h
    src/ui/ModelCheckDialog.cpp
    src/ui/TemplateDialog.h
    src/ui/TemplateDialog.cpp
    src/ui/MainWindowDeleteLoads.cpp
    src/ui/DeleteLoadsDialog.h
    src/ui/DeleteLoadsDialog.cpp
    src/ui/AreaLoadDialog.h
    src/ui/AreaLoadDialog.cpp
    src/app/services/AreaLoadService.h
    src/app/services/AreaLoadService.cpp
    src/ui/GroupsPanel.h
    src/ui/GroupsPanel.cpp
    src/ui/DesignDialogs.h
    src/ui/DesignDialogs.cpp
    src/ui/DesignPanel.h
    src/ui/DesignPanel.cpp
    src/ui/DesignColors.h
    src/ui/ProfileLibrary.h
    src/ui/ProfileLibrary.cpp
    src/ui/ProfileManagerDialog.h
    src/ui/ProfileManagerDialog.cpp
    src/ui/SectionPreviewWidget.h
    src/ui/SectionPreviewWidget.cpp
    src/ui/ProfileBrowser.h
    src/ui/ProfileBrowser.cpp
    src/ui/ProfileEditorWidget.h
    src/ui/ProfileEditorWidget.cpp
    src/ui/ProfilePickerDialog.h
    src/ui/ProfilePickerDialog.cpp
    src/ui/LoadsPanel.h
    src/ui/LoadsPanel.cpp
    src/ui/MainWindowSelection.cpp
    src/ui/MainWindowAppearance.cpp
    src/ui/SelectionFilterDialog.h
    src/ui/SelectionFilterDialog.cpp
    src/ui/TransformDialogs.h
    src/ui/TransformDialogs.cpp
    src/ui/ViewOptionsBar.h
    src/ui/ViewOptionsBar.cpp
    src/ui/AxesPanel.h
    src/ui/AxesPanel.cpp
    src/ui/SectionPlacementWidget.h
    src/ui/SectionPlacementWidget.cpp
    src/ui/SectionPlacementPanel.h
    src/ui/SectionPlacementPanel.cpp
    src/ui/SvgIcons.h
    src/ui/SvgIcons.cpp
    src/ui/PlacementController.h
    src/ui/PlacementController.cpp
    src/ui/PlacementOverlay.h
    src/ui/PlacementOverlay.cpp
    src/ui/MainWindowPresenter.h
    src/ui/MainWindowPresenter.cpp
    src/ui/TemporaryAnalysisResultsDialog.h
    src/ui/TemporaryAnalysisResultsDialog.cpp
    src/ui/MaterialDialog.h
    src/ui/MaterialDialog.cpp
    src/ui/BarPropertiesDialog.h
    src/ui/BarPropertiesDialog.cpp
    src/ui/AssignBarPropertiesDialog.h
    src/ui/AssignBarPropertiesDialog.cpp
    src/ui/NodalLoadDialog.h
    src/ui/NodalLoadDialog.cpp
    src/ui/DistributedLoadDialog.h
    src/ui/DistributedLoadDialog.cpp
    src/ui/MemberMomentDialog.h
    src/ui/MemberMomentDialog.cpp
    src/ui/LoadCasesDialog.h
    src/ui/LoadCasesDialog.cpp
    src/ui/CombinationGeneratorDialog.h
    src/ui/CombinationGeneratorDialog.cpp
    src/app/services/CombinationGenerator.h
    src/app/services/CombinationGenerator.cpp
    src/ui/RestraintDialog.h
    src/ui/RestraintDialog.cpp
    src/ui/SelectionModel.h
    src/ui/SelectionModel.cpp
    src/ui/PropertiesPanel.h
    src/ui/PropertiesPanel.cpp
    src/ui/PropertiesPanelEntryBuilder.h
    src/ui/PropertiesPanelEntryBuilder.cpp
)

set(RESOURCES
    resources.qrc
)

# --- Library ---

add_library(StructuraLib STATIC
    ${CORE_SOURCES}
    ${VIZ_SOURCES}
    ${APP_SOURCES}
    ${UI_SOURCES}
    ${RESOURCES}
)

target_link_libraries(StructuraLib
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Svg
        $<$<STREQUAL:${QT_VERSION_MAJOR},6>:Qt6::OpenGLWidgets>
        ${VTK_LIBRARIES}
)

target_include_directories(StructuraLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Structuradll/libs)

vtk_module_autoinit(
    TARGETS StructuraLib
    MODULES ${VTK_LIBRARIES}
)

# --- Executable ---

# WIN32: executavel de interface grafica (nao abre terminal); o ponto de entrada main() e mantido pelo Qt6::EntryPoint
add_executable(StructuraRibbon3D WIN32
    src/main.cpp
    $<$<BOOL:${WIN32}>:app.rc>
)

target_link_libraries(StructuraRibbon3D
    PRIVATE
        StructuraLib
)

# o executavel se chama Structura.exe (o alvo CMake mantem o nome interno)
set_target_properties(StructuraRibbon3D PROPERTIES OUTPUT_NAME Structura)

set(STRUCTURA_SOLVER_HINT_DIR "" CACHE PATH "Optional path containing solver_dll shared library")

if (WIN32)
    find_library(STRUCTURA_SOLVER_LIBRARY
        NAMES solver_dll
        HINTS ${STRUCTURA_SOLVER_HINT_DIR}
        PATH_SUFFIXES . bin lib
    )
else()
    find_library(STRUCTURA_SOLVER_LIBRARY
        NAMES solver_dll libsolver_dll
        HINTS ${STRUCTURA_SOLVER_HINT_DIR}
        PATH_SUFFIXES . bin lib
    )
endif()

if (STRUCTURA_SOLVER_LIBRARY)
    add_library(structura_solver_dll SHARED IMPORTED GLOBAL)
    set_target_properties(structura_solver_dll PROPERTIES
        IMPORTED_LOCATION "${STRUCTURA_SOLVER_LIBRARY}"
    )

    target_link_libraries(StructuraRibbon3D PRIVATE structura_solver_dll)

    add_custom_command(TARGET StructuraRibbon3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying solver shared library to output dir"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${STRUCTURA_SOLVER_LIBRARY}" "$<TARGET_FILE_DIR:StructuraRibbon3D>"
        VERBATIM
    )

    message(STATUS "Using solver shared library: ${STRUCTURA_SOLVER_LIBRARY}")
elseif (EXISTS "${CMAKE_SOURCE_DIR}/solver_dll.dll")
    # O solver e carregado em tempo de execucao (QLibrary): basta copiar a DLL da raiz.
    add_custom_command(TARGET StructuraRibbon3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/solver_dll.dll" "$<TARGET_FILE_DIR:StructuraRibbon3D>"
        VERBATIM
    )
    message(STATUS "Using solver DLL from repository root")
else()
    message(WARNING "solver_dll shared library not found. Set STRUCTURA_SOLVER_HINT_DIR to the folder containing the DLL/.so.")
endif()

# --- Enable Testing ---
enable_testing()
add_subdirectory(src/tests)

# --- Link Libraries ---

target_link_libraries(StructuraRibbon3D
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        $<$<STREQUAL:${QT_VERSION_MAJOR},6>:Qt6::OpenGLWidgets>
        ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS StructuraRibbon3D
    MODULES ${VTK_LIBRARIES}
)

# Windows runtime deployment: copy dependent DLLs and deploy Qt plugins
if (MSVC)
    # Copy all runtime DLLs of the target and its dependencies (CMake >= 3.21)
    add_custom_command(TARGET StructuraRibbon3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying runtime DLLs to output dir"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:StructuraRibbon3D> $<TARGET_FILE_DIR:StructuraRibbon3D>
        COMMAND_EXPAND_LISTS
    )

    # Try to locate windeployqt and deploy Qt runtime/plugins next to the exe
    if (QT_VERSION_MAJOR EQUAL 6)
        # Common hints: from Qt6_DIR and CMAKE_PREFIX_PATH
        get_filename_component(_qt6_dir_root "${Qt6_DIR}/../../.." ABSOLUTE)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt
            HINTS
                "${_qt6_dir_root}/bin"
                "$ENV{Qt6_DIR}/../../bin"
                "$ENV{QT_ROOT}/bin"
        )
        if (WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET StructuraRibbon3D POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-compiler-runtime
                        --dir "$<TARGET_FILE_DIR:StructuraRibbon3D>"
                        "$<TARGET_FILE:StructuraRibbon3D>"
                VERBATIM
            )
        endif()
    endif()
endif()

# --- Distribuicao ---
# A cada compilacao, a pasta dist/Structura recebe o executavel, as DLLs (Qt, VTK e solver), os plugins do Qt,
# os exemplos e a documentacao, pronta para ser copiada ou compactada.
set(STRUCTURA_DIST_DIR "${CMAKE_SOURCE_DIR}/dist/Structura" CACHE PATH "Pasta de distribuicao do programa")
add_custom_command(TARGET StructuraRibbon3D POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=$<TARGET_FILE_DIR:StructuraRibbon3D>
            -DDIST_DIR=${STRUCTURA_DIST_DIR}
            -DREPO_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/Distribute.cmake
    COMMENT "Montando a pasta de distribuicao em ${STRUCTURA_DIST_DIR}"
    VERBATIM
)
