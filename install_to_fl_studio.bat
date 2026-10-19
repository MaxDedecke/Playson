@echo off
title Playson - Install to FL Studio
cd /d "%~dp0"

:: Check for Administrator permissions
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Anfordern von Administrator-Rechten...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set "CLAP_DIR=%COMMONPROGRAMFILES%\CLAP"
if not exist "%CLAP_DIR%" (
    echo Erstelle offizielles CLAP-Verzeichnis: "%CLAP_DIR%" ...
    mkdir "%CLAP_DIR%"
)

set "BINARY="
if exist "dist\Release\Playson.clap" set "BINARY=dist\Release\Playson.clap"
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
    echo.
    echo ============================================================
    echo [+] Playson.clap erfolgreich in FL Studio installiert!
    echo     Ziel: %CLAP_DIR%\Playson.clap
    echo ============================================================
    echo.
    echo 1. Schliesse FL Studio und starte es neu.
    echo 2. Oeffne 'Options' -^> 'Manage plugins'.
    echo 3. Klicke auf 'Find installed plugins'.
    echo 4. Playson erscheint sofort unter 'New' / 'Effect'!
    echo.
) else (
    echo [-] Fehler beim Kopieren nach %CLAP_DIR%.
)

pause
