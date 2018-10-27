$ErrorActionPreference = 'Stop'

Push-Location "$env:APPVEYOR_BUILD_FOLDER\build"
try {
    cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=c:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
    if (!$?) { throw 'Error running cmake' }
    cmake --build . --config RelWithDebInfo
    if (!$?) { throw 'Error building with cmake' }
    ctest .
    if (!$?) { throw 'ctest execution error' }
} finally {
    Pop-Location
}
