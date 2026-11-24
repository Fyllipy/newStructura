
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
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platforms/qwindows.dll"
                    $<TARGET_FILE_DIR:${TARGET_NAME}>/platforms/qwindows.dll
        )
        # Fornece os solvers cifrados para os testes unitarios
        if (EXISTS "${CMAKE_SOURCE_DIR}/rds.enc")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CMAKE_SOURCE_DIR}/rds.enc" $<TARGET_FILE_DIR:${TARGET_NAME}>/rds.enc
            )
        endif()
        if (EXISTS "${CMAKE_SOURCE_DIR}/mefs.enc")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CMAKE_SOURCE_DIR}/mefs.enc" $<TARGET_FILE_DIR:${TARGET_NAME}>/mefs.enc
            )
        endif()
        # Build de desenvolvimento: os solvers em claro (sem servidor/licenca) ao lado dos testes; em release nunca
        if (STRUCTURA_DEV_BUILD)
            foreach (_plain rds.dll mefs.dll)
                if (EXISTS "${CMAKE_SOURCE_DIR}/${_plain}")
                    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${CMAKE_SOURCE_DIR}/${_plain}" $<TARGET_FILE_DIR:${TARGET_NAME}>/${_plain}
                    )
                endif()
            endforeach()
        endif()
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES ENVIRONMENT "STRUCTURA_TEST_DLL_KEY=689e4efd8e345fb49aeeae4ab42d29689d2eba244eedfcebc3900bdf92066d10;QT_QPA_PLATFORM=offscreen")
endfunction()

add_structura_test(TestCoreModels TestCoreModels.cpp)
add_structura_test(TestIdGeneratorService TestIdGeneratorService.cpp)
add_structura_test(TestProductIdentity TestProductIdentity.cpp)
add_structura_test(TestDatPreservation TestDatPreservation.cpp)
add_structura_test(TestArchitectureBoundaries TestArchitectureBoundaries.cpp)
add_structura_test(TestEditionBoundaries TestEditionBoundaries.cpp)
target_compile_definitions(TestEditionBoundaries PRIVATE STRUCTURA_EXE_PATH="$<TARGET_FILE:StructuraRibbon3D>")
add_dependencies(TestEditionBoundaries StructuraRibbon3D)
add_structura_test(TestSectionShapes TestSectionShapes.cpp)
add_structura_test(TestProjectIoAndSolver TestProjectIoAndSolver.cpp)
add_structura_test(TestProfileUi TestProfileUi.cpp)
add_structura_test(TestUiLicenseIntegration TestUiLicenseIntegration.cpp)

add_structura_test(TestAppearanceRender TestAppearanceRender.cpp)
vtk_module_autoinit(TARGETS TestAppearanceRender MODULES ${VTK_LIBRARIES})
# Testes dos recursos exclusivos da edicao Comercial (dimensionamento, relatorio de memoria, ligacoes e rede de seguranca numerica)
if (STRUCTURA_EDITION STREQUAL "COMMERCIAL")
    add_structura_test(TestDesign TestDesign.cpp)
    add_structura_test(TestReport TestReport.cpp)
    add_structura_test(TestConnectionCatalog TestConnectionCatalog.cpp)
    add_structura_test(TestConnectionGallery TestConnectionGallery.cpp)
    add_structura_test(TestGoldenNumerics TestGoldenNumerics.cpp)
    add_structura_test(TestKFactorDialog TestKFactorDialog.cpp)
endif()
add_structura_test(TestShell TestShell.cpp)
add_structura_test(TestShellMeshQuality TestShellMeshQuality.cpp)
add_structura_test(TestShellCoupling TestShellCoupling.cpp)
add_structura_test(TestRibbonWidgets TestRibbonWidgets.cpp)
add_structura_test(TestShellUi TestShellUi.cpp)
add_structura_test(TestShellProbe TestShellProbe.cpp)
vtk_module_autoinit(TARGETS TestShellProbe MODULES ${VTK_LIBRARIES})
add_structura_test(TestShellSelection TestShellSelection.cpp)
vtk_module_autoinit(TARGETS TestShellSelection MODULES ${VTK_LIBRARIES})
add_structura_test(TestShellSupports TestShellSupports.cpp)
vtk_module_autoinit(TARGETS TestShellSupports MODULES ${VTK_LIBRARIES})
add_structura_test(TestShellLoads TestShellLoads.cpp)
vtk_module_autoinit(TARGETS TestShellLoads MODULES ${VTK_LIBRARIES})
add_structura_test(TestStairGenerator TestStairGenerator.cpp)
add_structura_test(TestTowerModel TestTowerModel.cpp)
vtk_module_autoinit(TARGETS TestShellUi MODULES ${VTK_LIBRARIES})

# Micro-benchmarks (ferramentas de desenvolvimento; fora do build padrao): cmake --build build --target BenchDesign BenchServices
add_executable(BenchDesign EXCLUDE_FROM_ALL BenchDesign.cpp)
target_link_libraries(BenchDesign PRIVATE StructuraLib Qt6::Test ${VTK_LIBRARIES})
target_include_directories(BenchDesign PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_executable(BenchServices EXCLUDE_FROM_ALL BenchServices.cpp)
target_link_libraries(BenchServices PRIVATE StructuraLib Qt6::Test ${VTK_LIBRARIES})
target_include_directories(BenchServices PRIVATE ${CMAKE_SOURCE_DIR}/src)
