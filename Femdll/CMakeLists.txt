cmake_minimum_required(VERSION 3.15)
project(FemShell CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)

# Eigen do solver de barras (somente cabecalhos)
set(EIGEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Structuradll/libs)

add_library(fem_core STATIC
    src/fem_types.h
    src/shell_q4.h
    src/shell_q4.cpp
    src/fem_model.h
    src/fem_model.cpp)
target_include_directories(fem_core PUBLIC ${EIGEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
if (MSVC)
    target_compile_options(fem_core PRIVATE /O2 /bigobj)
endif()

add_library(fem_dll SHARED src/fem_api.cpp include/fem_interface.h)
target_include_directories(fem_dll PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fem_dll PRIVATE fem_core)
target_compile_definitions(fem_dll PRIVATE FEM_DLL_EXPORTS)
set_target_properties(fem_dll PROPERTIES OUTPUT_NAME "mefs")

add_executable(fem_tests tests/fem_tests.cpp)
target_include_directories(fem_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fem_tests PRIVATE fem_core)
enable_testing()
add_test(NAME fem_tests COMMAND fem_tests)
