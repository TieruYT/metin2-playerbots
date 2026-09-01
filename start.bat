@echo off
title Metin2 Singleplayer - Uruchamianie Serwera
chcp 65001 >nul
echo ========================================================
echo   Metin2 Singleplayer (Playerbots) - Server Launcher
echo ========================================================
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-server.ps1"
echo.
echo ========================================================
pause
