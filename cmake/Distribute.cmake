# Monta a pasta de distribuicao do programa (executavel, DLLs, plugins do Qt, solver, exemplos e documentacao).
# Executado como etapa final de cada compilacao do alvo principal:
#   cmake -DSOURCE_DIR=<saida do executavel> -DDIST_DIR=<destino> -DREPO_DIR=<raiz do repositorio> -P Distribute.cmake

if (NOT DEFINED SOURCE_DIR OR NOT DEFINED DIST_DIR OR NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "Distribute.cmake: defina SOURCE_DIR, DIST_DIR e REPO_DIR")
endif()

# pasta limpa a cada compilacao: nada de arquivos velhos na distribuicao
file(REMOVE_RECURSE "${DIST_DIR}")
file(MAKE_DIRECTORY "${DIST_DIR}")

file(GLOB_RECURSE _files RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")
set(_copied 0)
foreach (_file IN LISTS _files)
    # fora da distribuicao: simbolos de depuracao, bibliotecas de importacao, objetos e executaveis de teste
    if (_file MATCHES "\\.(pdb|ilk|exp|lib|obj|log|tlog|idb)$" OR _file MATCHES "^Test" OR _file MATCHES "^StructuraRibbon3D"
            OR _file MATCHES "^Gen[A-Za-z]*Tmp")
        continue()
    endif()
    get_filename_component(_dir "${_file}" DIRECTORY)
    file(COPY "${SOURCE_DIR}/${_file}" DESTINATION "${DIST_DIR}/${_dir}")
    math(EXPR _copied "${_copied} + 1")
endforeach()

# exemplos e documentacao
foreach (_folder Examples docs)
    if (EXISTS "${REPO_DIR}/${_folder}")
        file(COPY "${REPO_DIR}/${_folder}" DESTINATION "${DIST_DIR}")
    endif()
endforeach()
if (EXISTS "${REPO_DIR}/README.md")
    file(COPY "${REPO_DIR}/README.md" DESTINATION "${DIST_DIR}")
endif()

message(STATUS "Distribuicao atualizada em ${DIST_DIR} (${_copied} arquivos)")
