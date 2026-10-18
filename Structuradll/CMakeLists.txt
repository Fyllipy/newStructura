cmake_minimum_required(VERSION 3.15)
project(Structura CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/headers)
set(EIGEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

set(CORE_SOURCES
	src/core/solver_core.cpp
	../Femdll/src/shell_q4.cpp
	src/material.cpp
	src/section.cpp
	src/node.cpp
	src/member.cpp
	src/load.cpp
)

set(CORE_HEADERS
	src/core/solver_core.h
	src/core/solver_types.h
	headers/material.h
	headers/section.h
	headers/node.h
	headers/member.h
	headers/load.h
)

set(CLI_SOURCES
	src/cli/main.cpp
	src/cli/input_parser.cpp
	src/cli/output_writer.cpp
)

set(CLI_HEADERS
	src/cli/input_parser.h
	src/cli/output_writer.h
)

set(DLL_SOURCES
	src/bridge_dll/interface_dll.cpp
)

set(DLL_HEADERS
	src/bridge_dll/interface_dll.h
)

add_library(solver_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(solver_core
	PUBLIC
		${PROJECT_INCLUDE_DIR}
		${EIGEN_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/core
		${CMAKE_CURRENT_SOURCE_DIR}/../Femdll/src
)

add_executable(solver_cli ${CLI_SOURCES} ${CLI_HEADERS})
target_include_directories(solver_cli
	PRIVATE
		${PROJECT_INCLUDE_DIR}
		${EIGEN_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/core
		${CMAKE_CURRENT_SOURCE_DIR}/src/cli
)
target_link_libraries(solver_cli PRIVATE solver_core)

add_library(solver_dll SHARED ${DLL_SOURCES} ${DLL_HEADERS})
target_include_directories(solver_dll
	PRIVATE
		${PROJECT_INCLUDE_DIR}
		${EIGEN_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/core
		${CMAKE_CURRENT_SOURCE_DIR}/src/bridge_dll
)
target_link_libraries(solver_dll PRIVATE solver_core)
target_compile_definitions(solver_dll PRIVATE SOLVER_DLL_EXPORTS)

set_target_properties(solver_cli PROPERTIES OUTPUT_NAME "solver_cli")
set_target_properties(solver_dll PROPERTIES OUTPUT_NAME "rds")

message(STATUS "Projeto ${PROJECT_NAME} configurado com sucesso!")
message(STATUS "Core static library: solver_core")
message(STATUS "CLI executable: solver_cli")
message(STATUS "Shared library: rds")
message(STATUS "Executáveis irão para: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")