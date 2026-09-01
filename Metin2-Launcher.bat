@echo off
title Metin2 Singleplayer - Launcher
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Metin2-Launcher.ps1"
echo.
pause
