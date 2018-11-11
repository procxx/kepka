param (
    $VcpkgPath = 'c:\tools\vcpkg\',
    $VcpkgPlatform = 'x64-windows'
)

$ErrorActionPreference = 'Stop'

function Pack-Files($ext, $includeExe, $targetPath, $archiveName) {
    New-Item $targetPath\imageformats -Type Directory -Force | Out-Null
    New-Item $targetPath\platforms -Type Directory -Force | Out-Null

    foreach ($name in @('Qt5Core', 'Qt5Gui', 'Qt5Network', 'Qt5Widgets')) {
        Copy-Item $env:QT_DIR\bin\$name.$ext $targetPath\
    }

    Copy-Item $env:QT_DIR\plugins\imageformats\qwebp.$ext $targetPath\imageformats\
    Copy-Item $env:QT_DIR\plugins\imageformats\qjpeg.$ext $targetPath\imageformats\
    Copy-Item $env:QT_DIR\plugins\platforms\qwindows.$ext $targetPath\platforms\
    Copy-Item $VcpkgPath\installed\$VcpkgPlatform\bin\ssleay32.$ext $targetPath\
    Copy-Item $VcpkgPath\installed\$VcpkgPlatform\bin\libeay32.$ext $targetPath\

    $itemsToPack = @("$targetPath\*.$ext", "$targetPath\platforms\*.$ext", "$targetPath\imageformats\*.$ext")
    if ($includeExe) {
        $itemsToPack += @("$targetPath\kepka.exe")
    }

    7z a -mmt $archiveName @itemsToPack
}

Push-Location "$PSScriptRoot\..\build"
try {
    Pack-Files dll $true Telegram kepka.zip
    Pack-Files pdb $false Telegram pdb.zip
} finally {
    Pop-Location
}
