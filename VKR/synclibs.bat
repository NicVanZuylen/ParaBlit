mkdir External
cd External
call ../clonerepo.bat https://github.com/NicVanZuylen/NVZ_CLib.git CLib 46ed579
call ../clonerepo.bat https://github.com/nothings/stb.git stb 5a0bb8b
call ../clonerepo.bat https://github.com/tinyobjloader/tinyobjloader.git tinyobjloader 51908fb
call ../clonerepo.bat https://github.com/glfw/glfw.git glfw 201400b
call ../clonerepo.bat https://github.com/g-truc/glm.git glm 06ed280
call ../clonerepo.bat https://github.com/google/shaderc.git shaderc f6d6ddd
call ../clonerepo.bat https://github.com/GPUOpen-Tools/compressonator.git compressonator\src fbffb17

cd ..\