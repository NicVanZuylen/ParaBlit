set VCTargetsPath=%PB_VCTargetsPath%
set SOLUTION_PATH=%1
set TARGET=%2
set PLATFORM=%3

MSBuild.exe /m %SOLUTION_PATH% /target:%TARGET%;Build /property:Configuration=Debug /property:Platform=%PLATFORM%
MSBuild.exe /m %SOLUTION_PATH% /target:%TARGET%;Build /property:Configuration=Release /property:Platform=%PLATFORM%