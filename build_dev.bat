@echo off
rem =======================================================================
rem STRUCTURA - BUILD DE DESENVOLVIMENTO (servidor LOCAL, sem instalador)
rem
rem Uso:   build_dev.bat            compila (Release, com simbolos) e publica o manifesto no servidor local
rem        build_dev.bat debug      compila em Debug
rem        build_dev.bat run        alem de compilar, abre o servidor local numa nova janela
rem
rem Diferencas para o build_release.bat:
rem   - STRUCTURA_DEV_BUILD=ON: aceita DLLs do solver em claro e usa a chave publica do servidor LOCAL
rem   - fala com http://127.0.0.1:8000 (ou a URL em STRUCTURA_LICENSE_SERVER)
rem   - pasta propria (build-dev / dist-dev): nao mexe no build/dist de release nem nos instaladores
rem   - manifesto de versao fixa 9001, republicado a cada build (nao consome numeracao de release)
rem   - nao gera instalador, nao exporta pacote de producao
rem =======================================================================
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "CONFIG=Release"
set "RUN_SERVER="
for %%A in (%*) do (
    if /i "%%A"=="debug" set "CONFIG=Debug"
    if /i "%%A"=="run" set "RUN_SERVER=1"
)

set "PY=python"
if exist "server\.venv\Scripts\python.exe" set "PY=%CD%\server\.venv\Scripts\python.exe"

echo =======================================================================
echo STRUCTURA - BUILD DE DESENVOLVIMENTO ^(%CONFIG%^)
echo =======================================================================

if not exist "server\release.key" (
    echo [ERRO] server\release.key nao encontrada ^(necessaria para assinar o manifesto local^).
    exit /b 1
)

rem Caminhos do Qt e do VTK: variaveis STRUCTURA_QT_PREFIX / STRUCTURA_VTK_DIR, ou reaproveitados do cache
rem de um build anterior (build\ ou build-demo\ ou build-dev\).
set "QT_PREFIX=%STRUCTURA_QT_PREFIX%"
set "VTK_PATH=%STRUCTURA_VTK_DIR%"
for %%D in (build build-demo build-dev) do (
    if exist "%%D\CMakeCache.txt" (
        if not defined QT_PREFIX for /f "tokens=2 delims==" %%A in ('findstr /b /c:"CMAKE_PREFIX_PATH:" "%%D\CMakeCache.txt"') do set "QT_PREFIX=%%A"
        if not defined VTK_PATH for /f "tokens=2 delims==" %%A in ('findstr /b /c:"VTK_DIR:" "%%D\CMakeCache.txt"') do set "VTK_PATH=%%A"
    )
)
if not defined QT_PREFIX (
    echo [ERRO] Caminho do Qt desconhecido. Defina STRUCTURA_QT_PREFIX ^(ex.: D:/Qt/6.9.3/msvc2022_64/lib/cmake^)
    exit /b 1
)
if not defined VTK_PATH (
    echo [ERRO] Caminho do VTK desconhecido. Defina STRUCTURA_VTK_DIR ^(ex.: D:/src/VTK/build/lib/cmake/vtk-9.5^)
    exit /b 1
)
if not "!VTK_PATH:NOTFOUND=!"=="!VTK_PATH!" set "VTK_PATH="
if not defined VTK_PATH (
    echo [ERRO] VTK_DIR nao encontrado nos caches. Defina STRUCTURA_VTK_DIR.
    exit /b 1
)
echo Qt:  !QT_PREFIX!
echo VTK: !VTK_PATH!

echo.
echo [1/3] Configurando CMake ^(build-dev^)...
cmake -B build-dev -S . -DSTRUCTURA_EDITION=COMMERCIAL -DCMAKE_BUILD_TYPE=%CONFIG% -DSTRUCTURA_DEV_BUILD=ON "-DCMAKE_PREFIX_PATH=!QT_PREFIX!" "-DVTK_DIR=!VTK_PATH!" "-DSTRUCTURA_DIST_DIR=%CD%\dist-dev\Structura-Metalicas"
if errorlevel 1 (
    echo [ERRO] Falha na configuracao do CMake.
    exit /b 1
)

echo.
echo [2/3] Compilando...
cmake --build build-dev --config %CONFIG% --target StructuraRibbon3D
if errorlevel 1 (
    echo [ERRO] Falha na compilacao.
    exit /b 1
)

echo.
echo [3/3] Manifesto de integridade local ^(versao 9001^)...
"%PY%" server\tools\make_manifest.py --dir dist-dev\Structura-Metalicas --version 9001 --exe Structura.exe --key-file server\release.key
if errorlevel 1 (
    echo [ERRO] Falha ao gerar o manifesto.
    exit /b 1
)
set "PYTHONPATH=%CD%\server"
"%PY%" -m app.manifest_cli publish --text dist-dev\Structura-Metalicas\manifest.txt --sig dist-dev\Structura-Metalicas\manifest.sig --status current --overwrite
if errorlevel 1 (
    echo [ERRO] Falha ao publicar o manifesto no banco do servidor LOCAL.
    exit /b 1
)

echo.
echo =======================================================================
echo OK. Executavel de desenvolvimento: %CD%\dist-dev\Structura-Metalicas\Structura.exe
echo.
echo Servidor local ^(em outro terminal^):
echo   cd server ^&^& .venv\Scripts\python.exe -m uvicorn app.main:app --reload
echo Contas locais: python -m app.create_admin / python -m app.create_user ^(dentro de server^)
echo =======================================================================

if defined RUN_SERVER (
    start "Structura - servidor local" /D "%CD%\server" cmd /k ".venv\Scripts\python.exe -m uvicorn app.main:app --reload"
)
exit /b 0
