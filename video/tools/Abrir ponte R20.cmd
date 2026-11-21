@echo off
cd /d "%~dp0"
py -3.12 -c "import sounddevice" 2>nul
if errorlevel 1 (
    echo A ponte precisa de Python 3.12 e sounddevice.
    echo Para instalar a dependencia: py -3.12 -m pip install sounddevice
    pause
    exit /b 1
)
py -3.12 -u r20-wifi.py %*
pause
