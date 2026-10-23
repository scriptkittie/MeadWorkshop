; Mead Workshop installer (NSIS). Built by packaging/windows/package.sh, which passes:
;   VERSION, SRCDIR (the packed app folder), OUTFILE, ICON
; Installs for the current user only (no admin rights), with Start menu and desktop shortcuts and an uninstaller.
; Your recipes and settings live in %APPDATA%\MeadWorkshop and are kept when you uninstall.

Unicode true
!include "MUI2.nsh"

Name "Mead Workshop"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Mead Workshop"
InstallDirRegKey HKCU "Software\MeadWorkshop" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\MeadWorkshop.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open Mead Workshop"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Mead Workshop"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Mead Workshop installer"
VIAddVersionKey "LegalCopyright" "MIT License"

Section "Mead Workshop"
  SetOutPath "$INSTDIR"
  File /r "${SRCDIR}\*.*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Mead Workshop"
  CreateShortcut "$SMPROGRAMS\Mead Workshop\Mead Workshop.lnk" "$INSTDIR\MeadWorkshop.exe"
  CreateShortcut "$SMPROGRAMS\Mead Workshop\Uninstall Mead Workshop.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\Mead Workshop.lnk" "$INSTDIR\MeadWorkshop.exe"

  WriteRegStr HKCU "Software\MeadWorkshop" "InstallDir" "$INSTDIR"
  !define UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\MeadWorkshop"
  WriteRegStr HKCU "${UNINST}" "DisplayName" "Mead Workshop"
  WriteRegStr HKCU "${UNINST}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST}" "DisplayIcon" "$INSTDIR\MeadWorkshop.exe"
  WriteRegStr HKCU "${UNINST}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "${UNINST}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Mead Workshop.lnk"
  RMDir /r "$SMPROGRAMS\Mead Workshop"
  ; Only remove the folder if it really is Mead Workshop's (never a folder someone pointed the installer at by mistake).
  IfFileExists "$INSTDIR\MeadWorkshop.exe" 0 +2
    RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\MeadWorkshop"
  DeleteRegKey HKCU "Software\MeadWorkshop"
SectionEnd
