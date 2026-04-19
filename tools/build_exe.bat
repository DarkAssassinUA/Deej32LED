@echo off
echo Installing PyInstaller and dependencies...
python -m pip install -q pyinstaller websockets keyboard pystray pillow

echo Building executable...
cd /d "%~dp0"
pyinstaller --noconfirm --onefile --windowed --icon "icon.png" --add-data "icon.png;." --name "Deej32LED_Companion" deej_media_bridge.py

echo Build finished! 
echo The executable is located in the tools\dist folder!
pause
