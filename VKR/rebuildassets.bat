set HOME_PATH=%~dp0

cd %HOME_PATH%
rmdir /s /q Assets\build
call buildassets.bat
pause