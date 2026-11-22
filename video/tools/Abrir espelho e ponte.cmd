@echo off
cd /d "%~dp0"
set "CORTEX_PY=python"
if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" set "CORTEX_PY=%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
start "" "%~dp0Abrir ponte espelho.cmd"
start "Servidor do Espelho Cortex" "%CORTEX_PY%" -m http.server 8774 --bind 127.0.0.1 --directory "%~dp0."
timeout /t 2 /nobreak >nul
start "" "http://127.0.0.1:8774/cortex-video-mirror.html"
