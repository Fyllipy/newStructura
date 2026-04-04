
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
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

add_structura_test(TestCoreModels TestCoreModels.cpp)
add_structura_test(TestRepositoryAndServices TestRepositoryAndServices.cpp)
add_structura_test(TestSceneControllerFacade TestSceneControllerFacade.cpp)
add_structura_test(TestArchitectureBoundaries TestArchitectureBoundaries.cpp)
