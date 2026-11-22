@echo off
setlocal EnableDelayedExpansion

echo =======================================================================
echo STRUCTURA - COMPILACAO DE SOLVERS COM HARDENING E CIFRA EM REPOUSO
echo =======================================================================

echo.
echo [1/4] Configurando e compilando Structuradll (rds.dll) em Release...
if not exist "Structuradll\build" mkdir "Structuradll\build"
cmake -B Structuradll\build -S Structuradll
if errorlevel 1 (
    echo [ERRO] Falha na configuracao do CMake para Structuradll.
    exit /b 1
)
cmake --build Structuradll\build --config Release --target solver_dll
if errorlevel 1 (
    echo [ERRO] Falha na compilacao do alvo solver_dll - rds.dll.
    exit /b 1
)

echo.
echo [2/4] Configurando e compilando Femdll em Release...
if not exist "Femdll\build" mkdir "Femdll\build"
cmake -B Femdll\build -S Femdll
if errorlevel 1 (
    echo [ERRO] Falha na configuracao do CMake para Femdll.
    exit /b 1
)
cmake --build Femdll\build --config Release --target fem_dll
if errorlevel 1 (
    echo [ERRO] Falha na compilacao do alvo fem_dll - mefs.dll.
    exit /b 1
)

echo.
echo [3/4] Copiando bibliotecas compiladas para a raiz do repositorio...
copy /Y "Structuradll\build\bin\Release\rds.dll" "rds.dll" >nul
if errorlevel 1 (
    echo [ERRO] Nao foi possivel copiar rds.dll para a raiz.
    exit /b 1
)
if exist "Structuradll\build\bin\Release\rds.lib" copy /Y "Structuradll\build\bin\Release\rds.lib" "rds.lib" >nul

copy /Y "Femdll\build\bin\Release\mefs.dll" "mefs.dll" >nul
if errorlevel 1 (
    echo [ERRO] Nao foi possivel copiar mefs.dll para a raiz.
    exit /b 1
)
if exist "Femdll\build\bin\Release\mefs.lib" copy /Y "Femdll\build\bin\Release\mefs.lib" "mefs.lib" >nul

echo.
echo [4/4] Cifrando solvers para distribuicao segura em repouso (.enc)...
python server\tools\encrypt_dll.py --version 2 --out-dir .
if errorlevel 1 (
    echo [ERRO] Falha na execucao de server\tools\encrypt_dll.py.
    exit /b 1
)

if not exist "rds.enc" (
    echo [ERRO] Arquivo rds.enc nao foi gerado.
    exit /b 1
)
if not exist "mefs.enc" (
    echo [ERRO] Arquivo mefs.enc nao foi gerado.
    exit /b 1
)

echo.
echo [5/5] Sincronizando solvers protegidos (.enc) para as pastas de distribuicao...
if not exist "dist\Structura-Metalicas" mkdir "dist\Structura-Metalicas"
if not exist "dist\Structura-Metalicas-Demo" mkdir "dist\Structura-Metalicas-Demo"
if exist "dist\Structura-Metalicas\rds.dll" del /Q "dist\Structura-Metalicas\rds.dll"
if exist "dist\Structura-Metalicas\mefs.dll" del /Q "dist\Structura-Metalicas\mefs.dll"
if exist "dist\Structura-Metalicas-Demo\rds.dll" del /Q "dist\Structura-Metalicas-Demo\rds.dll"
if exist "dist\Structura-Metalicas-Demo\mefs.dll" del /Q "dist\Structura-Metalicas-Demo\mefs.dll"
copy /Y "rds.enc" "dist\Structura-Metalicas\rds.enc" >nul
copy /Y "mefs.enc" "dist\Structura-Metalicas\mefs.enc" >nul
copy /Y "rds.enc" "dist\Structura-Metalicas-Demo\rds.enc" >nul
copy /Y "mefs.enc" "dist\Structura-Metalicas-Demo\mefs.enc" >nul

echo.
echo =======================================================================
echo SUCESSO: SOLVERS COMPILADOS E CIFRADOS COM EXITO
echo =======================================================================
for %%F in (rds.dll mefs.dll rds.enc mefs.enc) do (
    if exist "%%F" (
        echo   Arquivo gerado: %%~fF  [%%~zF bytes]
    )
)
echo =======================================================================
exit /b 0
