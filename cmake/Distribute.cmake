# Monta a pasta de distribuicao do programa (executavel, DLLs, plugins do Qt, solver).
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
    # alem de solvers antigos/abertos e DLLs desnecessarias identificadas na auditoria
    if (_file MATCHES "\\.(pdb|ilk|exp|lib|obj|log|tlog|idb)$" OR _file MATCHES "^Test" OR _file MATCHES "^StructuraRibbon3D"
            OR _file MATCHES "^Gen[A-Za-z]*Tmp"
            OR _file MATCHES "solver_dll\\.dll" OR _file MATCHES "fem_dll\\.dll"
            OR _file MATCHES "rds\\.dll" OR _file MATCHES "mefs\\.dll"
            OR _file MATCHES "vtkx11" OR _file MATCHES "vtkRenderingVolume"
            OR _file MATCHES "dxcompiler\\.dll" OR _file MATCHES "dxil\\.dll"
            OR _file MATCHES "D3Dcompiler_47\\.dll" OR _file MATCHES "opengl32sw\\.dll")
        continue()
    endif()
    get_filename_component(_dir "${_file}" DIRECTORY)
    file(COPY "${SOURCE_DIR}/${_file}" DESTINATION "${DIST_DIR}/${_dir}")
    math(EXPR _copied "${_copied} + 1")
endforeach()

# Exemplos, documentacao e README sao internos: nao vao para o cliente.

# Marco M5: Distribuicao de solvers protegidos (.enc exclusivamente)
if (EXISTS "${REPO_DIR}/rds.enc")
    file(COPY "${REPO_DIR}/rds.enc" DESTINATION "${DIST_DIR}")
endif()
if (EXISTS "${REPO_DIR}/mefs.enc")
    file(COPY "${REPO_DIR}/mefs.enc" DESTINATION "${DIST_DIR}")
endif()

# Seguranca e Limpeza M5: Garante que solvers abertos/obsoletos e DLLs desnecessarias nao fiquem na distribuicao
file(REMOVE
    "${DIST_DIR}/rds.dll"
    "${DIST_DIR}/mefs.dll"
    "${DIST_DIR}/solver_dll.dll"
    "${DIST_DIR}/fem_dll.dll"
    "${DIST_DIR}/vtkx11-9.5.dll"
    "${DIST_DIR}/vtkRenderingVolume-9.5.dll"
    "${DIST_DIR}/dxcompiler.dll"
    "${DIST_DIR}/dxil.dll"
    "${DIST_DIR}/D3Dcompiler_47.dll"
    "${DIST_DIR}/opengl32sw.dll"
)

message(STATUS "Distribuicao atualizada em ${DIST_DIR} (${_copied} arquivos)")
