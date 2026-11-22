@echo off
rem CORTEX VIDEO: a ponte do espelho para o app do iPhone pela rede local (WebRTC). Deixe aberta enquanto transmite.
cd /d "%~dp0"
set PY=python
if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" set PY="%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
%PY% -u espelho-wifi.py %*
pause
