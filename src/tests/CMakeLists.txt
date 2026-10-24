
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

function(add_structura_test TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME}
        ${SOURCE_FILE}
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            StructuraLib
            Qt6::Test
            ${VTK_LIBRARIES}
    )

    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            STRUCTURA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    if (MSVC)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${TARGET_NAME}> $<TARGET_FILE_DIR:${TARGET_NAME}>
            COMMAND_EXPAND_LISTS
        )
        # testes de interface rodam em modo offscreen: o plugin de plataforma fica ao lado de cada executavel
        # (sem ele o Qt informa que o unico plugin disponivel e "windows")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/platforms
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platforms/qoffscreen.dll"
                    $<TARGET_FILE_DIR:${TARGET_NAME}>/platforms/qoffscreen.dll
        )
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

add_structura_test(TestCoreModels TestCoreModels.cpp)
add_structura_test(TestIdGeneratorService TestIdGeneratorService.cpp)
add_structura_test(TestArchitectureBoundaries TestArchitectureBoundaries.cpp)
add_structura_test(TestSectionShapes TestSectionShapes.cpp)
add_structura_test(TestProjectIoAndSolver TestProjectIoAndSolver.cpp)
add_structura_test(TestProfileUi TestProfileUi.cpp)

add_structura_test(TestAppearanceRender TestAppearanceRender.cpp)
vtk_module_autoinit(TARGETS TestAppearanceRender MODULES ${VTK_LIBRARIES})
add_structura_test(TestDesign TestDesign.cpp)
add_structura_test(TestReport TestReport.cpp)
add_structura_test(TestShell TestShell.cpp)
add_structura_test(TestShellCoupling TestShellCoupling.cpp)
add_structura_test(TestConnectionCatalog TestConnectionCatalog.cpp)
add_structura_test(TestConnectionGallery TestConnectionGallery.cpp)
add_structura_test(TestRibbonWidgets TestRibbonWidgets.cpp)
add_structura_test(TestGoldenNumerics TestGoldenNumerics.cpp)
add_structura_test(TestShellUi TestShellUi.cpp)
vtk_module_autoinit(TARGETS TestShellUi MODULES ${VTK_LIBRARIES})

# Micro-benchmarks (ferramentas de desenvolvimento; fora do build padrao): cmake --build build --target BenchDesign BenchServices
add_executable(BenchDesign EXCLUDE_FROM_ALL BenchDesign.cpp)
target_link_libraries(BenchDesign PRIVATE StructuraLib Qt6::Test ${VTK_LIBRARIES})
target_include_directories(BenchDesign PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_executable(BenchServices EXCLUDE_FROM_ALL BenchServices.cpp)
target_link_libraries(BenchServices PRIVATE StructuraLib Qt6::Test ${VTK_LIBRARIES})
target_include_directories(BenchServices PRIVATE ${CMAKE_SOURCE_DIR}/src)
