@echo off
rem Patches a Majora's Mask Wii VC wad with kz.
rem Drag a wad onto this file, or put the wad in this folder and double click this file.
setlocal
cd /d "%~dp0"

set "WAD=%~1"
if not "%WAD%"=="" goto patch

for %%f in (*.wad) do (
    echo %%~nf | findstr /b /i "kz-" >nul || (
        set "WAD=%%f"
        goto patch
    )
)

echo No wad found.
echo Drag your Majora's Mask wad onto build-wad.bat,
echo or copy it into this folder and run build-wad.bat again.
goto end

:patch
echo Using wad: %WAD%
if exist wadwork rmdir /s /q wadwork
mkdir wadwork
echo 45e| bin\gzinject.exe -a genkey -k wadwork\common-key.bin >nul
bin\gru.exe lua\patchwad.lua bin\gzinject.exe "%WAD%"
if errorlevel 1 (
    echo.
    echo Patching failed.
) else (
    echo.
    echo Done. The patched wad is in this folder.
)
if exist wadwork rmdir /s /q wadwork

:end
echo.
pause
