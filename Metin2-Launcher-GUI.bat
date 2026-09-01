@echo off
title Metin2 Singleplayer Playerbots - All in One
chcp 65001 >nul
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Metin2-Launcher-GUI.ps1"
if errorlevel 1 (
  echo.
  echo Launcher zakonczyl sie bledem. Zrob zrzut tego okna lub uruchom opcje logow.
  pause
)
