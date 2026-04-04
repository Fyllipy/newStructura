cmake_minimum_required(VERSION 3.21)

project(StructuraRibbon3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# User-specific paths
list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64" "D:/src/vtk")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

if (QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
endif()

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersSources
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
    src/core/model/GridLine.h
)

set(VIZ_SOURCES
    src/viz/ISceneRenderer.h
    src/viz/VtkSceneRenderer.h
    src/viz/VtkSceneRenderer.cpp
    src/viz/SceneController.h
    src/viz/SceneController.cpp
    src/viz/LoadVisualization.h
    src/viz/LoadVisualization.cpp
    src/viz/LocalCoordinateSystem.h
    src/viz/LocalCoordinateSystem.cpp
    src/viz/CustomInteractorStyle.h
    src/viz/CustomInteractorStyle.cpp
)

set(APP_SOURCES
    src/app/IModelRepository.h
    src/app/InMemoryModelRepository.h
    src/app/NodeService.h
    src/app/NodeService.cpp
    src/app/BarService.h
    src/app/BarService.cpp
    src/app/UndoRedoService.h
    src/app/UndoRedoService.cpp
    src/app/SceneControllerFacade.h
    src/app/SceneControllerFacade.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/MainWindowPresenter.h
    src/ui/MainWindowPresenter.cpp
    src/ui/CoordinateDialog.h
    src/ui/CoordinateDialog.cpp
    src/ui/MaterialDialog.h
    src/ui/MaterialDialog.cpp
    src/ui/SectionDialog.h
    src/ui/SectionDialog.cpp
    src/ui/BarPropertiesDialog.h
    src/ui/BarPropertiesDialog.cpp
    src/ui/AssignBarPropertiesDialog.h
    src/ui/AssignBarPropertiesDialog.cpp
    src/ui/NodalLoadDialog.h
    src/ui/NodalLoadDialog.cpp
    src/ui/DistributedLoadDialog.h
    src/ui/DistributedLoadDialog.cpp
    src/ui/RestraintDialog.h
    src/ui/RestraintDialog.cpp
    src/ui/GridDialog.h
    src/ui/GridDialog.cpp
    src/ui/SelectionModel.h
    src/ui/SelectionModel.cpp
    src/ui/PropertiesPanel.h
    src/ui/PropertiesPanel.cpp
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
        $<$<STREQUAL:${QT_VERSION_MAJOR},6>:Qt6::OpenGLWidgets>
        ${VTK_LIBRARIES}
)

target_include_directories(StructuraLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

vtk_module_autoinit(
    TARGETS StructuraLib
    MODULES ${VTK_LIBRARIES}
)

# --- Executable ---

add_executable(StructuraRibbon3D
    src/main.cpp
)

target_link_libraries(StructuraRibbon3D
    PRIVATE
        StructuraLib
)

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
