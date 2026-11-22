@echo off
setlocal EnableDelayedExpansion

echo =======================================================================
echo STRUCTURA - COMPILACAO DE RELEASE E GERACAO DE INSTALADORES
echo =======================================================================

echo.
echo [1/6] Verificando pre-requisitos de seguranca (DLLs cifradas em repouso)...
if not exist "rds.enc" (
    echo [ERRO] rds.enc nao encontrado na raiz. Execute build_solvers.bat primeiro.
    exit /b 1
)
if not exist "mefs.enc" (
    echo [ERRO] mefs.enc nao encontrado na raiz. Execute build_solvers.bat primeiro.
    exit /b 1
)
if not exist "server\release.key" (
    echo [ERRO] Chave privada server\release.key nao encontrada para assinar manifestos.
    exit /b 1
)

set "PROD_URL="
if exist "server\deploy\production_url.txt" set /p PROD_URL=<"server\deploy\production_url.txt"
if not defined PROD_URL (
    echo [ERRO] Crie server\deploy\production_url.txt com a URL HTTPS do servidor de licencas de producao.
    echo        Ex.: https://129-146-10-20.sslip.io   ^(ver server\deploy\oracle\DEPLOY.md^)
    exit /b 1
)
echo URL de producao embutida no app: !PROD_URL!

rem Versao do manifesto: automatica e crescente (server\deploy\manifest_version.txt guarda a ultima USADA).
rem Versoes anteriores continuam validas no servidor, entao instalacoes antigas nao quebram ao lancar uma nova.
set "LAST_MANIFEST=2"
if exist "server\deploy\manifest_version.txt" set /p LAST_MANIFEST=<"server\deploy\manifest_version.txt"
set /a MANIFEST_VERSION=LAST_MANIFEST+1
echo Versao do manifesto deste build: !MANIFEST_VERSION!

set "MAKENSIS="
if exist "C:\Program Files (x86)\NSIS\makensis.exe" set "MAKENSIS=C:\Program Files (x86)\NSIS\makensis.exe"
if exist "C:\Program Files\NSIS\makensis.exe" set "MAKENSIS=C:\Program Files\NSIS\makensis.exe"
if not defined MAKENSIS (
    where makensis >nul 2>&1
    if not errorlevel 1 set "MAKENSIS=makensis"
)
if not defined MAKENSIS (
    echo [ERRO] Compilador NSIS makensis.exe nao encontrado no sistema.
    exit /b 1
)

echo.
echo =======================================================================
echo [2/6] EDICAO COMERCIAL: Configurando e compilando executavel...
echo =======================================================================
if not exist "build" mkdir "build"
cmake -B build -S . -DSTRUCTURA_EDITION=COMMERCIAL -DCMAKE_BUILD_TYPE=Release -DSTRUCTURA_DEV_BUILD=OFF -DSTRUCTURA_PRODUCTION_SERVER_URL=!PROD_URL!
if errorlevel 1 (
    echo [ERRO] Falha na configuracao CMake da edicao Comercial.
    exit /b 1
)
cmake --build build --config Release --target StructuraRibbon3D
if errorlevel 1 (
    echo [ERRO] Falha na compilacao do executavel Comercial.
    exit /b 1
)

echo.
echo [3/6] EDICAO COMERCIAL: Atualizando manifesto de integridade e instalador...
for %%D in (rds.dll mefs.dll solver_dll.dll fem_dll.dll dxcompiler.dll dxil.dll D3Dcompiler_47.dll opengl32sw.dll vtkx11-9.5.dll vtkRenderingVolume-9.5.dll) do (
    if exist "dist\Structura-Metalicas\%%D" del /Q "dist\Structura-Metalicas\%%D"
)
copy /Y "rds.enc" "dist\Structura-Metalicas\rds.enc" >nul
copy /Y "mefs.enc" "dist\Structura-Metalicas\mefs.enc" >nul

python server\tools\make_manifest.py --dir dist\Structura-Metalicas --version !MANIFEST_VERSION! --exe Structura.exe --key-file server\release.key
if errorlevel 1 (
    echo [ERRO] Falha na geracao do manifesto assinado para edicao Comercial.
    exit /b 1
)

set "PYTHONPATH=%CD%\server"
python -m app.manifest_cli publish --text dist\Structura-Metalicas\manifest.txt --sig dist\Structura-Metalicas\manifest.sig --status current --overwrite
if errorlevel 1 (
    echo [ERRO] Falha ao publicar manifesto no banco de dados do servidor.
    exit /b 1
)

> "server\deploy\manifest_version.txt" echo !MANIFEST_VERSION!
python server\deploy\export_bundle.py
if errorlevel 1 (
    echo [ERRO] Falha ao exportar o pacote de release para o servidor de producao.
    exit /b 1
)

if not exist "dist\installers" mkdir "dist\installers"
"%MAKENSIS%" /V2 "/DPRODUCT_NSH=%CD%\build\generated\product.nsh" installer\structura.nsi
if errorlevel 1 (
    echo [ERRO] Falha no empacotamento NSIS da edicao Comercial.
    exit /b 1
)

echo.
echo =======================================================================
echo [4/6] EDICAO DEMONSTRATIVA: Configurando e compilando executavel...
echo =======================================================================
if not exist "build-demo" mkdir "build-demo"
cmake -B build-demo -S . -DSTRUCTURA_EDITION=DEMO -DCMAKE_BUILD_TYPE=Release -DSTRUCTURA_DEV_BUILD=OFF -DSTRUCTURA_PRODUCTION_SERVER_URL=!PROD_URL!
if errorlevel 1 (
    echo [ERRO] Falha na configuracao CMake da edicao Demonstrativa.
    exit /b 1
)
cmake --build build-demo --config Release --target StructuraRibbon3D
if errorlevel 1 (
    echo [ERRO] Falha na compilacao do executavel Demonstrativo.
    exit /b 1
)

echo.
echo [5/6] EDICAO DEMONSTRATIVA: Atualizando manifesto de integridade e instalador...
for %%D in (rds.dll mefs.dll solver_dll.dll fem_dll.dll dxcompiler.dll dxil.dll D3Dcompiler_47.dll opengl32sw.dll vtkx11-9.5.dll vtkRenderingVolume-9.5.dll) do (
    if exist "dist\Structura-Metalicas-Demo\%%D" del /Q "dist\Structura-Metalicas-Demo\%%D"
)
copy /Y "rds.enc" "dist\Structura-Metalicas-Demo\rds.enc" >nul
copy /Y "mefs.enc" "dist\Structura-Metalicas-Demo\mefs.enc" >nul

python server\tools\make_manifest.py --dir dist\Structura-Metalicas-Demo --version !MANIFEST_VERSION! --exe StructuraDemo.exe --key-file server\release.key
if errorlevel 1 (
    echo [ERRO] Falha na geracao do manifesto assinado para edicao Demonstrativa.
    exit /b 1
)

"%MAKENSIS%" /V2 "/DPRODUCT_NSH=%CD%\build-demo\generated\product.nsh" installer\structura.nsi
if errorlevel 1 (
    echo [ERRO] Falha no empacotamento NSIS da edicao Demonstrativa.
    exit /b 1
)

echo.
echo =======================================================================
echo [6/6] SUCESSO: RELEASE GERADA E INSTALADORES CRIADOS
echo =======================================================================
for %%F in (dist\installers\*.exe) do (
    echo   Instalador: %%~fF  [%%~zF bytes]
)
echo =======================================================================
exit /b 0
