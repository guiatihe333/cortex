@echo off
rem CORTEX VIDEO: o espelho por um servidor local (http://127.0.0.1:8770), nao pelo arquivo: assim o Chrome
rem lembra as permissoes (MIDI, microfone) e deixa escolher a saida de audio (o R20 tocando no FM-1).
rem Deixe esta janela aberta enquanto usa o espelho; feche-a para parar.
cd /d "%~dp0"
set PY=python
if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" set PY="%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
start "" "http://127.0.0.1:8770/cortex-video-mirror.html"
%PY% -m http.server 8770 --bind 127.0.0.1
