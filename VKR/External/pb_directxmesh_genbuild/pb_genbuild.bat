cd ../directxmesh

mkdir directxmesh-build
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/setup_build.cmake
cmake -S . -B directxmesh-build
cd directxmesh-build

../../../buildsinglelib.bat DirectXMesh.sln ALL_BUILD x64

cd ../