cd ../compressonator/src

python scripts/fetch_dependencies.py

mkdir compressonator-build
cmake -DOPTION_ENABLE_ALL_APPS=OFF -DLIB_BUILD_COMPRESSONATOR_SDK=ON -B compressonator-build
cd compressonator-build

../../../../buildsinglelib.bat Compressonator.sln CMakePredefinedTargets\ALL_BUILD x64