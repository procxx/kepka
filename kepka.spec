# Setting some build conditions...
%bcond_with clang
%bcond_without ipo

# Applying workaround to RHBZ#1559007...
%if %{with clang}
%global optflags %(echo %{optflags} | sed -e 's/-mcet//g' -e 's/-fcf-protection//g' -e 's/-fstack-clash-protection//g' -e 's/$/-Qunused-arguments -Wno-unknown-warning-option/')
%endif

Name: kepka
Version: 2.0.0
Release: 1%{?dist}

License: GPLv3+
Summary: Unofficial Telegram desktop messaging app
URL: https://github.com/procxx/%{name}
Source0: %{url}/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
ExclusiveArch: i686 x86_64

# Additional runtime requirements...
%{?_qt5:Requires: %{_qt5}%{?_isa} = %{_qt5_version}}
Requires: qt5-qtimageformats%{?_isa}
Requires: hicolor-icon-theme

# Compilers and tools...
BuildRequires: desktop-file-utils
BuildRequires: libappstream-glib
BuildRequires: ninja-build
BuildRequires: gcc-c++
BuildRequires: cmake
BuildRequires: gcc

# Clang compiler and tools if enabled...
%if %{with clang}
BuildRequires: compiler-rt
BuildRequires: clang
BuildRequires: llvm
%endif

# Development packages for main application...
BuildRequires: guidelines-support-library-devel
BuildRequires: qt5-qtbase-private-devel
BuildRequires: libappindicator-devel
BuildRequires: mapbox-variant-devel
BuildRequires: ffmpeg-devel >= 3.1
BuildRequires: openal-soft-devel
BuildRequires: qt5-qtbase-devel
BuildRequires: libstdc++-devel
BuildRequires: range-v3-devel
BuildRequires: openssl-devel
BuildRequires: minizip-devel
BuildRequires: opus-devel
BuildRequires: zlib-devel
BuildRequires: xz-devel
BuildRequires: python3

# Development packages for libtgvoip...
BuildRequires: pulseaudio-libs-devel
BuildRequires: alsa-lib-devel

%description
Kepka is a messaging app with a focus on speed and security, it’s super
fast, simple and free. You can use Kepka on all your devices at the same
time — your messages sync seamlessly across any of your phones, tablets or
computers.

With Kepka you can send messages, photos, videos and files of any type
(doc, zip, mp3, etc), as well as create groups for up to 200 people. You can
write to your phone contacts and find people by their usernames. As a result,
Kepka is like SMS and email combined — and can take care of all your
personal or business messaging needs.

%prep
# Unpacking main source archive...
%autosetup -p1
mkdir -p %{_target_platform}

%build
# Configuring application...
pushd %{_target_platform}
    %cmake -G Ninja \
%if %{with clang}
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_AR=%{_bindir}/llvm-ar \
    -DCMAKE_RANLIB=%{_bindir}/llvm-ranlib \
    -DCMAKE_LINKER=%{_bindir}/llvm-ld \
    -DCMAKE_OBJDUMP=%{_bindir}/llvm-objdump \
    -DCMAKE_NM=%{_bindir}/llvm-nm \
%else
    -DCMAKE_AR=%{_bindir}/gcc-ar \
    -DCMAKE_RANLIB=%{_bindir}/gcc-ranlib \
    -DCMAKE_NM=%{_bindir}/gcc-nm \
%endif
    -DPACKAGED_BUILD:BOOL=ON \
%if %{with ipo}
    -DENABLE_IPO:BOOL=ON \
%endif
    -DCMAKE_BUILD_TYPE=Release \
    ..
popd

# Building application...
%ninja_build -C %{_target_platform}

%install
# Installing application...
%ninja_install -C %{_target_platform}

%check
# Checking AppStream manifest and desktop file...
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/%{name}.appdata.xml
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%files
%doc README.md
%license LICENSE
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/kservices5/tg.protocol
%{_datadir}/metainfo/%{name}.appdata.xml

%changelog
* Fri Jul 27 2018 Vitaly Zaitsev <vitaly@easycoding.org> - 2.0.0-1
- Updated to version 2.0.0.

* Thu Dec 21 2017 Vitaly Zaitsev <vitaly@easycoding.org> - 1.0.0-1
- Initial SPEC release.
