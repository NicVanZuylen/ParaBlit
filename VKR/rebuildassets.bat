set HOME_PATH=%~dp0

cd %HOME_PATH%
rmdir /s /q Assets\build
AssetPipeline\x64\Release\AssetPipeline.exe -VKR/Assets/
pause