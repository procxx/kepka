[![Version](https://badge.fury.io/gh/procxx%2Fkepka.svg)](https://github.com/procxx/kepka/releases)
[![Build Status](https://travis-ci.org/procxx/kepka.svg?branch=dev)](https://travis-ci.org/procxx/kepka)
[![Build status](https://ci.appveyor.com/api/projects/status/2kodvgwvlua3o6hp?svg=true
)](https://ci.appveyor.com/project/procxx/tdesktop)


# Kepka - Unofficial [Telegram Desktop][telegram_desktop] fork from pro.cxx community

This is the complete source code and the build instructions for the Kepka's alpha version based on the [Telegram API][telegram_api] and the [MTProto][telegram_proto] secure protocol.

## Build instructions

### Linux, macOS

What you need to have installed:

* Qt 5.9+ (with private modules, like qtbase5-private-dev)
* OpenSSL (conan installs this if you use conan)
* OpenAL-soft
* FFmpeg with swscale and swresample libs
* zlib
* opus (libopus-dev)

#### Debian/Ubuntu:
```console
# apt-get install libssl-dev libavformat-dev libavdevice-dev libswscale-dev libavutil-dev libpulse-dev libasound2-dev libswresample-dev libavresample-dev libopus-dev qtbase5-private-dev zlib1g-dev libopenal-dev libavcodec-dev libopenal-data libopenal1
```

You most likely have to rebuild ffmpeg with `--enable-swresample --enable-swscale`

After that, go to the [next section](#configuring-and-building).

#### Fedora:
```console
# dnf install rpm-build rpmdevtools mock mock-rpmfusion-free
```

Add yourself to `mock` group (you must run this only for the first time after installing mock):
```bash
sudo usermod -a -G mock $(whoami)
```

You need to relogin to your system or run:
```bash
newgrp mock
```

Create RPM build base directories:
```bash
rpmdev-setuptree
```

Download sources:
```bash
spectool -g -R kepka.spec
```

Generate SRPM:
```bash
rpmbuild -bs kepka.spec
```

Start mock build sequence:
```bash
mock -r fedora-$(rpm -E %fedora)-$(uname -m)-rpmfusion_free --rebuild ~/rpmbuild/SRPMS/kepka*.src.rpm
```

#### Configuring and building

Provide paths to OpenAL-soft and Qt5 in CMAKE_PREFIX_PATH variable when configuring.

    ccache -o sloppiness=pch_defines,time_macros
    mkdir _conan_build_
    cd _conan_build_
    conan install .. --build missing
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="/usr/local/opt/qt5/;/usr/local/opt/openal-soft" ..
    ninja

### Windows

Install [vcpkg][] (no need to integrate, just install), [python][] 3.5, [cmake][], [Qt][qt] 5.9 or later and [Visual Studio][visual-studio] 2017 or later, and set the following environment variables:

- `QTDIR`: directory where Qt binary distribution is installed, e.g. `C:\Qt\5.9.1\msvc2017_64`
- `VCPKG`: directory where VCPKG is installed, e.g. `C:\vcpkg`

After that, execute the following `cmd` commands from Visual Studio developer command prompt:

```console
$ set PATH=c:\Python35;%PATH%
$ "%VCPKG%\vcpkg" install --triplet x64-windows openal-soft openssl opus zlib ffmpeg
$ mkdir build
$ cd build
$ set PATH=%QTDIR%\bin;%PATH%
$ cmake -G"Visual Studio 15 2017 Win64" -DCMAKE_TOOLCHAIN_FILE="%VCPKG%\scripts\buildsystems\vcpkg.cmake" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
$ cmake --build . --config RelWithDebInfo
```
![Preview of Telegram Desktop][preview_image]

The source code is published under GPLv3 with OpenSSL exception, the license is available [here][license].

## Supported systems

* Windows 7 - Windows 10 (**not** RT)
* Mac OS X 10.8 - Mac OS X 10.11
* Mac OS X 10.6 - Mac OS X 10.7 (separate build)
* Ubuntu 12.04 - Ubuntu 16.04
* Fedora 22 - Fedora 24

## Third-party

* [CMake 3.10+][cmake-build]

* Qt 5.9+ ([LGPL](http://doc.qt.io/qt-5/lgpl.html))
* OpenSSL 1.0.1g ([OpenSSL License](https://www.openssl.org/source/license.html))
* zlib 1.2.8 ([zlib License](http://www.zlib.net/zlib_license.html))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* OpenAL Soft ([LGPL](http://kcat.strangesoft.net/openal.html))
* Opus codec ([BSD License](http://www.opus-codec.org/license/))
* FFmpeg ([LGPL](https://www.ffmpeg.org/legal.html))
* Guideline Support Library ([MIT License](https://github.com/Microsoft/GSL/blob/master/LICENSE))
* Mapbox Variant ([BSD License](https://github.com/mapbox/variant/blob/master/LICENSE))
* Open Sans font ([Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0.html))
* Emoji alpha codes ([MIT License](https://github.com/emojione/emojione/blob/master/extras/alpha-codes/LICENSE.md))
* Catch test framework ([Boost License](https://github.com/philsquared/Catch/blob/master/LICENSE.txt))

[//]: # (LINKS)
[cmake]: https://cmake.org/
[python]: https://python.org/
[cmake-build]: docs/building-cmake.md
[qt]: https://www.qt.io/
[telegram]: https://telegram.org
[telegram_desktop]: https://desktop.telegram.org
[telegram_api]: https://core.telegram.org
[telegram_proto]: https://core.telegram.org/mtproto
[license]: LICENSE
[preview_image]: docs/assets/preview.png
[vcpkg]: https://github.com/Microsoft/vcpkg
[visual-studio]: https://www.visualstudio.com/
