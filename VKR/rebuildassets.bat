set HOME_PATH=%~dp0

cd %HOME_PATH%
rmdir /s /q Assets\build
x64\Release\AssetPipeline.exe -Assets/
pause