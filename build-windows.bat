@echo off
REM ==========================================================================
REM  Dean Amp - Windows build + install launcher
REM
REM  Just double-click this file, or run it from a terminal. It builds the
REM  VST3 in Release and copies it into your system VST3 folder (it will ask
REM  for admin via a UAC prompt only for the final copy).
REM
REM  Pass-through options (see build-windows.ps1 for details), e.g.:
REM      build-windows.bat -Standalone
REM      build-windows.bat -NoInstall
REM      build-windows.bat -Clean
REM      build-windows.bat -VstDir "D:\VstPlugins\VST3"
REM ==========================================================================
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %*
set RC=%ERRORLEVEL%
echo.
if not "%RC%"=="0" echo Build/install reported an error. Exit code %RC%.
echo Press any key to close...
pause >nul
exit /b %RC%
