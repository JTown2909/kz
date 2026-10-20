@echo off
rem Patches a Majora's Mask rom with kz-lite.
rem Drag a rom onto this file, or put the rom in this folder and double click this file.
setlocal
cd /d "%~dp0"

set "ROM=%~1"
if not "%ROM%"=="" goto patch

for %%f in (*.z64 *.n64 *.v64) do (
    echo %%~nf | findstr /b /i "kz-lite" >nul || (
        set "ROM=%%f"
        goto patch
    )
)

echo No rom found.
echo Drag your Majora's Mask rom onto build-rom-lite.bat,
echo or copy it into this folder and run build-rom-lite.bat again.
goto end

:patch
echo Using rom: %ROM%
bin\gru.exe lua\patch.lua "%ROM%"
if errorlevel 1 (
    echo.
    echo Patching failed.
) else (
    echo.
    echo Done. The patched rom is in this folder.
)

:end
echo.
pause
