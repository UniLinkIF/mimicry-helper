@echo off
REM Double-click this to rebuild mimicry-helper.exe after Claude changes its C++ source.
REM Needs Git Bash (MINGW64) installed — the same one build.sh already uses successfully.
cd /d "%~dp0"
"C:\Program Files\Git\bin\bash.exe" build.sh
echo.
echo Done. Press any key to close.
pause >nul
