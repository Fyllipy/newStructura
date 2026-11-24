@echo off
setlocal EnableDelayedExpansion

echo =======================================================================
echo STRUCTURA - PIPELINE COMPLETO DE RELEASE (SOLVERS + APLICATIVO)
echo =======================================================================

echo.
echo === ETAPA 1/2: Compilando solvers com hardening e gerando arquivos .enc ===
call build_solvers.bat
if errorlevel 1 (
    echo [ERRO FATAL] A etapa build_solvers.bat falhou. Abortando release.
    exit /b 1
)

echo.
echo === ETAPA 2/2: Compilando edicoes Comercial e Demo e gerando instaladores ===
call build_release.bat
if errorlevel 1 (
    echo [ERRO FATAL] A etapa build_release.bat falhou. Abortando release.
    exit /b 1
)

echo.
echo =======================================================================
echo SUCESSO TOTAL: PIPELINE DE RELEASE FINALIZADO COM EXITO!
echo =======================================================================
echo Artefatos protegidos gerados na raiz:
for %%F in (rds.dll mefs.dll rds.enc mefs.enc) do (
    if exist "%%F" echo   %%~fF  [%%~zF bytes]
)
echo.
echo Instaladores finais gerados:
for %%F in (dist\installers\*-0.9.1beta.exe) do (
    if exist "%%F" echo   %%~fF  [%%~zF bytes]
)
echo =======================================================================
exit /b 0
