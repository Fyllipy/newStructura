cmake_minimum_required(VERSION 3.21)

project(StructuraRibbon3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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

if (QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(StructuraRibbon3D
        src/main.cpp
        src/MainWindow.h
        src/MainWindow.cpp
        src/CoordinateDialog.h
        src/CoordinateDialog.cpp
        src/CustomInteractorStyle.h
        src/CustomInteractorStyle.cpp
        src/MaterialDialog.h
        src/MaterialDialog.cpp
        src/SectionDialog.h
        src/SectionDialog.cpp
        src/BarPropertiesDialog.h
        src/BarPropertiesDialog.cpp
        src/AssignBarPropertiesDialog.h
        src/AssignBarPropertiesDialog.cpp
        src/NodalLoadDialog.h
        src/NodalLoadDialog.cpp
        src/DistributedLoadDialog.h
        src/DistributedLoadDialog.cpp
        src/GridDialog.h
        src/GridDialog.cpp
        src/SceneController.h
        src/SceneController.cpp
        src/SelectionModel.h
        src/SelectionModel.cpp
        src/ModelEntities.h
        src/PropertiesPanel.h
        src/PropertiesPanel.cpp
        resources.qrc
    )
else()
    add_executable(StructuraRibbon3D
        src/main.cpp
        src/MainWindow.h
        src/MainWindow.cpp
        src/CoordinateDialog.h
        src/CoordinateDialog.cpp
        src/CustomInteractorStyle.h
        src/CustomInteractorStyle.cpp
        src/MaterialDialog.h
        src/MaterialDialog.cpp
        src/SectionDialog.h
        src/SectionDialog.cpp
        src/BarPropertiesDialog.h
        src/BarPropertiesDialog.cpp
        src/AssignBarPropertiesDialog.h
        src/AssignBarPropertiesDialog.cpp
        src/NodalLoadDialog.h
        src/NodalLoadDialog.cpp
        src/DistributedLoadDialog.h
        src/DistributedLoadDialog.cpp
        src/GridDialog.h
        src/GridDialog.cpp
        src/SceneController.h
        src/SceneController.cpp
        src/SelectionModel.h
        src/SelectionModel.cpp
        src/ModelEntities.h
        src/PropertiesPanel.h
        src/PropertiesPanel.cpp
        resources.qrc
    )
endif()

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
