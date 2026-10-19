@echo off
title Build Playson (CMake)
cd /d "%~dp0"

echo ============================================================
echo   Building Playson (CLAP Plugin)
echo ============================================================

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] CMake wurde nicht im Pfad gefunden.
    echo Tipp: Wenn du zu GitHub pushst, baut GitHub Actions die .clap Datei
    echo       automatisch in der Cloud und stellt sie unter Releases bereit!
    pause
    exit /b 1
)

cmake -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [-] CMake Konfiguration fehlgeschlagen.
    pause
    exit /b 1
)

cmake --build build --config Release --parallel
if %errorlevel% equ 0 (
    echo ============================================================
    echo [+] Erfolgreich gebaut: build\Release\Playson.clap
    echo ============================================================
    echo Moechtest du das Plugin jetzt direkt in FL Studio installieren?
    choice /M "In FL Studio installieren"
    if %errorlevel% equ 1 (
        call install_to_fl_studio.bat
    )
) else (
    echo [-] Build fehlgeschlagen.
)

pause
