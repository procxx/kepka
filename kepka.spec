Name: kepka
Version: 1.0.0
Release: 1%{?dist}

License: GPLv3+
Summary: Unofficial Telegram desktop messaging app
Group: Applications/Internet
URL: https://github.com/procxx/%{name}
ExclusiveArch: i686 x86_64

Source0: %{url}/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

Requires: qt5-qtimageformats%{?_isa}
Requires: hicolor-icon-theme
Requires: gtk3%{?_isa}
Recommends: libappindicator-gtk3%{?_isa}

# Compilers and tools...
BuildRequires: desktop-file-utils
BuildRequires: libappstream-glib
BuildRequires: ninja-build
BuildRequires: gcc-c++
BuildRequires: chrpath
BuildRequires: cmake
BuildRequires: gcc

# Development packages for main application...
BuildRequires: guidelines-support-library-devel
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

# Development packages for libtgvoip...
BuildRequires: pulseaudio-libs-devel
BuildRequires: alsa-lib-devel

%description
Telegram is a messaging app with a focus on speed and security, it’s super
fast, simple and free. You can use Telegram on all your devices at the same
time — your messages sync seamlessly across any of your phones, tablets or
computers.

With Telegram, you can send messages, photos, videos and files of any type
(doc, zip, mp3, etc), as well as create groups for up to 200 people. You can
write to your phone contacts and find people by their usernames. As a result,
Telegram is like SMS and email combined — and can take care of all your
personal or business messaging needs.

%prep
# Unpacking main source archive...
%autosetup -p1
mkdir %{_target_platform}

%build
# Building application...
pushd %{_target_platform}
    %cmake -G Ninja -DPACKAGED_BUILD=1 -DCMAKE_BUILD_TYPE=Release ..
    %ninja_build
popd

%install
# Installing executables...
mkdir -p "%{buildroot}%{_bindir}"
install -m 0755 -p %{_target_platform}/Telegram/Telegram "%{buildroot}%{_bindir}/%{name}"

# Installing desktop shortcut...
desktop-file-install --dir="%{buildroot}%{_datadir}/applications" lib/xdg/%{name}.desktop

# Installing icons...
for size in 16 32 48 64 128 256 512; do
    dir="%{buildroot}%{_datadir}/icons/hicolor/${size}x${size}/apps"
    install -d "$dir"
    install -m 0644 -p Telegram/Resources/art/icon${size}.png "$dir/%{name}.png"
done

# Installing appdata for Gnome Software...
install -d "%{buildroot}%{_datadir}/metainfo"
install -m 0644 -p lib/xdg/%{name}.appdata.xml "%{buildroot}%{_datadir}/metainfo/%{name}.appdata.xml"

%check
appstream-util validate-relax --nonet "%{buildroot}%{_datadir}/metainfo/%{name}.appdata.xml"

%files
%doc README.md changelog.txt
%license LICENSE
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/metainfo/%{name}.appdata.xml

%changelog
* Thu Dec 21 2017 Vitaly Zaitsev <vitaly@easycoding.org> - 1.0.0-1
- Initial SPEC release.
