@echo off
title Metin2 Singleplayer - Zatrzymywanie Serwera
chcp 65001 >nul
echo ========================================================
echo   Metin2 Singleplayer (Playerbots) - Server Stop
echo ========================================================
echo.
cd /d "%~dp0linux-port\docker"
docker compose stop
echo.
echo Serwer zostal zatrzymany.
echo ========================================================
pause
