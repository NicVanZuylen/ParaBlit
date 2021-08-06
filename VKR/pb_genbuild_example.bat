cmake -DCMAKE_TOOLCHAIN_FILE=Cmake/x86_64-w64-mingw32.cmake
mkdir glfw-build
cmake -S . -B glfw-build
cd glfw-build

../../../buildsinglelib.bat CMakePredefinedTargets\ALL_BUILD x64

cd ../