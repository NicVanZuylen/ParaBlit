cd ../shaderc

py utils/git-sync-deps

mkdir shaderc-build
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/setup_build.cmake
cmake -S . -B shaderc-build
cd shaderc-build

../../../buildsinglelib.bat shaderc.sln CMakePredefinedTargets\ALL_BUILD x64

cd ../