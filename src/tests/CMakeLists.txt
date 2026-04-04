
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

set(TEST_SOURCES
    TestCoreModels.cpp
    # TestRepositoryAndServices.cpp
    # TestSceneControllerFacade.cpp
)

add_executable(StructuraTests
    ${TEST_SOURCES}
)

target_link_libraries(StructuraTests
    PRIVATE
        StructuraLib
        Qt6::Test
        ${VTK_LIBRARIES}
)

add_test(NAME CoreModelsTest COMMAND StructuraTests)
