; Instalador do Structura::Metalicas (edicoes Comercial e Demonstrativa), um instalador por edicao.
; A identidade (nome, versao, executavel, pasta, chave de registro) vem de build/generated/product.nsh (gerado por cmake/ProductInfo.cmake):
;   makensis /DPRODUCT_NSH=<pasta do build>\generated\product.nsh installer\structura.nsi
; Cada edicao tem pasta de instalacao, atalhos, chave de registro e desinstalador proprios: as duas coexistem no mesmo computador e
; desinstalar uma nao afeta a outra. Os dados do usuario (%APPDATA%) nunca sao removidos. Nao associa a extensao .dat.

Unicode true
!ifndef PRODUCT_NSH
  !error "Informe /DPRODUCT_NSH=<caminho do product.nsh gerado pelo CMake>"
!endif
!include "${PRODUCT_NSH}"
!include "MUI2.nsh"
!include "x64.nsh"

Name "${PRODUCT_NAME}"
OutFile "..\dist\installers\${PRODUCT_SETUP_NAME}"
InstallDir "$PROGRAMFILES64\${PRODUCT_INSTALL_DIR}"
InstallDirRegKey HKLM "Software\${PRODUCT_REG_KEY}" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION_TXT}"

VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey /LANG=1046 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1046 "CompanyName" "${PRODUCT_COMPANY}"
VIAddVersionKey /LANG=1046 "FileDescription" "Instalador do ${PRODUCT_NAME}"
VIAddVersionKey /LANG=1046 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1046 "ProductVersion" "${PRODUCT_VERSION_TXT}"

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_REG_KEY}"

!define MUI_ABORTWARNING
!define MUI_ICON "..\icons\app\app.ico"
!define MUI_UNICON "..\icons\app\app.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PRODUCT_EXE}"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "PortugueseBR"

Section "Programa" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${PRODUCT_DIST_DIR}\*.*"

  WriteRegStr HKLM "Software\${PRODUCT_REG_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\${PRODUCT_REG_KEY}" "Version" "${PRODUCT_VERSION_TXT}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; nomes de atalho sem "::" (caracter invalido em nome de arquivo)
  CreateDirectory "$SMPROGRAMS\${PRODUCT_INSTALL_DIR}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_INSTALL_DIR}\${PRODUCT_INSTALL_DIR}.lnk" "$INSTDIR\${PRODUCT_EXE}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT_INSTALL_DIR}\Desinstalar.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\${PRODUCT_INSTALL_DIR}.lnk" "$INSTDIR\${PRODUCT_EXE}"

  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION_TXT}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${PRODUCT_COMPANY}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${PRODUCT_EXE}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; remove somente o que esta edicao instalou; dados do usuario (%APPDATA%) e a outra edicao nao sao tocados
  Delete "$SMPROGRAMS\${PRODUCT_INSTALL_DIR}\${PRODUCT_INSTALL_DIR}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_INSTALL_DIR}\Desinstalar.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_INSTALL_DIR}"
  Delete "$DESKTOP\${PRODUCT_INSTALL_DIR}.lnk"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
  DeleteRegKey HKLM "Software\${PRODUCT_REG_KEY}"
SectionEnd
