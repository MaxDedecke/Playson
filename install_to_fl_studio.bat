@echo off
title Playson - Install to FL Studio
cd /d "%~dp0"

set "CLAP_DIR=%COMMONPROGRAMFILES%\CLAP"
if not exist "%CLAP_DIR%" (
    echo Erstelle CLAP-Verzeichnis: %CLAP_DIR%
    mkdir "%CLAP_DIR%"
)

set "BINARY="
if exist "build\Release\Playson.clap" set "BINARY=build\Release\Playson.clap"
if exist "build\Playson.clap" set "BINARY=build\Playson.clap"
if exist "Playson.clap" set "BINARY=Playson.clap"

if "%BINARY%"=="" (
    echo [!] Keine kompilierte Playson.clap gefunden!
    echo Bitte zuerst kompilieren oder die Binary aus den GitHub Actions Releases herunterladen.
    pause
    exit /b 1
)

echo Kopiere %BINARY% nach %CLAP_DIR% ...
copy /Y "%BINARY%" "%CLAP_DIR%\Playson.clap"

if %errorlevel% equ 0 (
    echo ============================================================
    echo [+] Playson.clap erfolgreich in FL Studio installiert!
    echo ============================================================
    echo 1. Oeffne FL Studio.
    echo 2. Gehe im Mixer auf deinen Master-Kanal.
    echo 3. Waehle 'Select' -> 'More plugins...' -> 'Manage plugins'.
    echo 4. Klicke auf 'Find installed plugins' -> Playson wird gelistet!
) else (
    echo [-] Fehler beim Kopieren. Bitte als Administrator ausfuehren!
)

pause
