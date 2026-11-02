; WinClock installer: per-user, no admin rights needed.
; Build:  makensis -DVERSION=2.2.0 winclock.nsi   (expects ..\src\app.ico and WinClock.exe next to this file)
Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"

!ifndef VERSION
  !define VERSION "2.2.0"
!endif
!define APP      "WinClock"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\WinClock"
!define RUNKEY   "Software\Microsoft\Windows\CurrentVersion\Run"

Name "${APP}"
OutFile "WinClock-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\WinClock"
InstallDirRegKey HKCU "Software\WinClock" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "WinClock ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "WinClock"
VIAddVersionKey "FileDescription" "WinClock Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "whileTrueFalse"

!define MUI_ICON "..\src\app.ico"
!define MUI_UNICON "..\src\app.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\WinClock.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start WinClock now"
!define MUI_FINISHPAGE_TITLE "WinClock is installed"
!define MUI_FINISHPAGE_TEXT "Your clocks will appear on the desktop and next to the taskbar clock.$\r$\n$\r$\nRight-click any clock for the menu: add cities, layouts, themes, alarms and more."

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!macro CloseWinClock
  FindWindow $0 "WinClockMain" ""
  ${If} $0 <> 0
    SendMessage $0 ${WM_CLOSE} 0 0
    Sleep 700
  ${EndIf}
!macroend

Section "WinClock (required)" SecMain
  SectionIn RO
  !insertmacro CloseWinClock
  SetOutPath "$INSTDIR"
  File "WinClock.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\WinClock.lnk" "$INSTDIR\WinClock.exe"
  WriteRegStr HKCU "Software\WinClock" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "WinClock"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "whileTrueFalse"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\WinClock.exe"
  WriteRegStr HKCU "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "URLInfoAbout" "https://github.com/whileTrueFalse/WinClock"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize" 450
SectionEnd

Section "Start with Windows" SecStartup
  WriteRegStr HKCU "${RUNKEY}" "WinClock" '"$INSTDIR\WinClock.exe"'
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\WinClock.lnk" "$INSTDIR\WinClock.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The WinClock app (about 400 KB). Installs for your account only; no admin rights needed."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartup} "Show your clocks automatically every time you sign in."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Add a WinClock shortcut to the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  !insertmacro CloseWinClock
  DeleteRegValue HKCU "${RUNKEY}" "WinClock"
  DeleteRegKey HKCU "${UNINSTKEY}"
  DeleteRegKey HKCU "Software\WinClock"
  Delete "$SMPROGRAMS\WinClock.lnk"
  Delete "$DESKTOP\WinClock.lnk"
  Delete "$INSTDIR\WinClock.exe"
  Delete "$INSTDIR\WinClock.ini"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
