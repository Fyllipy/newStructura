# Identidade do produto: UNICO lugar onde nome, versao, edicao, executavel e identificadores sao definidos.
# Tudo o mais (cabecalho ProductInfo.h, recursos do executavel .rc, pasta de distribuicao, instalador NSIS) e gerado a partir daqui.
#
# Edicao: cmake -DSTRUCTURA_EDITION=COMMERCIAL (padrao) ou DEMO.
#
# O que NAO muda com a marca (identificadores tecnicos): namespace C++ `Structura::`, alvos CMake StructuraLib/StructuraRibbon3D,
# macros STRUCTURA_*, funcoes exportadas structura_* e os nomes rds.dll / mefs.dll.

set(STRUCTURA_EDITION "COMMERCIAL" CACHE STRING "Edicao do produto: COMMERCIAL ou DEMO")
set_property(CACHE STRUCTURA_EDITION PROPERTY STRINGS COMMERCIAL DEMO)
if (NOT STRUCTURA_EDITION STREQUAL "COMMERCIAL" AND NOT STRUCTURA_EDITION STREQUAL "DEMO")
    message(FATAL_ERROR "STRUCTURA_EDITION invalida: '${STRUCTURA_EDITION}' (use COMMERCIAL ou DEMO)")
endif()

# --- Comum as duas edicoes ----------------------------------------------------------------------------------------------------------
set(STRUCTURA_PRODUCT_NAME      "Structura::Metalicas")      # nome comercial exibido ao usuario
set(STRUCTURA_COMPANY           "Structura")                 # fabricante (VERSIONINFO e instalador) - ajustar quando houver razao social
set(STRUCTURA_VERSION_MAJOR     0)
set(STRUCTURA_VERSION_MINOR     8)
set(STRUCTURA_VERSION_PATCH     0)
set(STRUCTURA_VERSION_STAGE     "beta")                      # vazio = versao final
set(STRUCTURA_LEGACY_APP_ID     "Structura")                 # applicationName antigo: origem da migracao dos dados do usuario

set(STRUCTURA_VERSION "${STRUCTURA_VERSION_MAJOR}.${STRUCTURA_VERSION_MINOR}.${STRUCTURA_VERSION_PATCH}")
if (STRUCTURA_VERSION_STAGE)
    set(STRUCTURA_VERSION_DISPLAY "${STRUCTURA_VERSION_MAJOR}.${STRUCTURA_VERSION_MINOR} ${STRUCTURA_VERSION_STAGE}")
    set(STRUCTURA_VERSION_FILE_FLAGS "0x2L")                 # VS_FF_PRERELEASE
else()
    set(STRUCTURA_VERSION_DISPLAY "${STRUCTURA_VERSION}")
    set(STRUCTURA_VERSION_FILE_FLAGS "0x0L")
endif()

# --- Por edicao ---------------------------------------------------------------------------------------------------------------------
# DISPLAY_NAME  : nome exibido (titulo, Sobre, relatorios)          EXE_NAME   : executavel (sem .exe)
# APP_ID        : applicationName do Qt = pasta de dados do usuario  INSTALL_DIR: pasta em Arquivos de Programas
# DIST_NAME     : pasta em dist/                                     REG_KEY    : chave de desinstalacao no registro do Windows
if (STRUCTURA_EDITION STREQUAL "DEMO")
    set(STRUCTURA_IS_DEMO           1)
    set(STRUCTURA_DISPLAY_NAME      "${STRUCTURA_PRODUCT_NAME} Demo")
    set(STRUCTURA_EXE_NAME          "StructuraDemo")
    set(STRUCTURA_APP_ID            "Structura-Metalicas-Demo")
    set(STRUCTURA_INSTALL_DIR       "Structura Metalicas Demo")
    set(STRUCTURA_DIST_NAME         "Structura-Metalicas-Demo")
    set(STRUCTURA_FILE_DESCRIPTION  "${STRUCTURA_PRODUCT_NAME} Demo - edicao demonstrativa de analise de estruturas metalicas")
else()
    set(STRUCTURA_IS_DEMO           0)
    set(STRUCTURA_DISPLAY_NAME      "${STRUCTURA_PRODUCT_NAME}")
    set(STRUCTURA_EXE_NAME          "Structura")
    set(STRUCTURA_APP_ID            "Structura-Metalicas")
    set(STRUCTURA_INSTALL_DIR       "Structura Metalicas")
    set(STRUCTURA_DIST_NAME         "Structura-Metalicas")
    set(STRUCTURA_FILE_DESCRIPTION  "${STRUCTURA_PRODUCT_NAME} - analise e dimensionamento de estruturas metalicas")
endif()
set(STRUCTURA_REG_KEY               "${STRUCTURA_APP_ID}")
set(STRUCTURA_INSTALLER_NAME        "${STRUCTURA_APP_ID}-Setup-${STRUCTURA_VERSION_MAJOR}.${STRUCTURA_VERSION_MINOR}${STRUCTURA_VERSION_STAGE}")

# --- Arquivos gerados ---------------------------------------------------------------------------------------------------------------
set(STRUCTURA_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
# rc.exe le o arquivo na pagina de codigo ANSI: caminho absoluto com acentos (ex.: "Joao") quebra. Usa caminho relativo ao .rc gerado.
file(RELATIVE_PATH STRUCTURA_ICON_PATH "${STRUCTURA_GENERATED_DIR}" "${CMAKE_SOURCE_DIR}/icons/app/app.ico")
configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/ProductInfo.h.in" "${STRUCTURA_GENERATED_DIR}/app/ProductInfo.h" @ONLY)
configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/app.rc.in" "${STRUCTURA_GENERATED_DIR}/app.rc" @ONLY)
configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/product.nsh.in" "${STRUCTURA_GENERATED_DIR}/product.nsh" @ONLY)

message(STATUS "Produto: ${STRUCTURA_DISPLAY_NAME} ${STRUCTURA_VERSION_DISPLAY} (edicao ${STRUCTURA_EDITION}, executavel ${STRUCTURA_EXE_NAME}.exe)")
