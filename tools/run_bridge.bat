@echo off
cd /d "%~dp0"

echo Installing dependencies (websockets, keyboard)...
python -m pip install -q websockets keyboard

echo Starting Deej Media Bridge GUI...
start pythonw deej_media_bridge.py
exit


