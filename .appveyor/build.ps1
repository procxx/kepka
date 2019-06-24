$ErrorActionPreference = 'Stop'

Push-Location "$env:APPVEYOR_BUILD_FOLDER\build"

function execute-cmake {
    # We have to merge the stderr and stdout here because otherwise the build will fail on any random warning
    cmd /c 'cmake 2>&1' @args
    if ($LASTEXITCODE -ne 0) {
        throw "CMake execution error: $LASTEXITCODE"
    }
}

try {
    execute-cmake '-GNinja' `
        '-DCMAKE_TOOLCHAIN_FILE=c:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake' `
        '-DCMAKE_BUILD_TYPE=RelWithDebInfo' `
        ..
    execute-cmake --build . --config RelWithDebInfo
    ctest .
    if (!$?) { throw 'ctest execution error' }
} finally {
    Pop-Location
}
