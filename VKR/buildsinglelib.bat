set VCTargetsPath=%PB_VCTargetsPath%
set TARGET=%1
set PLATFORM=%2

msbuild.exe /m GLFW.sln /target:%TARGET%;Build /property:Configuration=Debug /property:Platform=%PLATFORM%
msbuild.exe /m GLFW.sln /target:%TARGET%;Build /property:Configuration=Release /property:Platform=%PLATFORM%