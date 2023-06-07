mkdir External
cd External
call ../clonerepo.bat https://github.com/NicVanZuylen/NVZ_CLib.git CLib d89ae8e
call ../clonerepo.bat https://github.com/nothings/stb.git stb 5a0bb8b
call ../clonerepo.bat https://github.com/tinyobjloader/tinyobjloader.git tinyobjloader 51908fb
call ../clonerepo.bat https://github.com/glfw/glfw.git glfw 201400b
call ../clonerepo.bat https://github.com/g-truc/glm.git glm 06ed280
call ../clonerepo.bat https://github.com/google/shaderc.git shaderc f9eb1c7
call ../clonerepo.bat https://github.com/GPUOpen-Tools/compressonator.git compressonator\src fbffb17

cd ..\