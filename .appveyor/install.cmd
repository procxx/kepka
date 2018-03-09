vcpkg install --triplet x64-windows openal-soft openssl opus zlib ffmpeg
appveyor DownloadFile https://github.com/ninja-build/ninja/releases/download/v1.8.2/ninja-win.zip
mkdir %APPVEYOR_BUILD_FOLDER%\build
7z x ninja-win.zip -o%APPVEYOR_BUILD_FOLDER%\build > nul
