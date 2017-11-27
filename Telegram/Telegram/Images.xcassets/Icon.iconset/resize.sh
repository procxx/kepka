#!/bin/sh
sips -z 16 16     icon_512x512@2x.png --out icon_16x16.png
sips -z 32 32     icon_512x512@2x.png --out icon_16x16@2x.png
sips -z 32 32     icon_512x512@2x.png --out icon_32x32.png
sips -z 64 64     icon_512x512@2x.png --out icon_32x32@2x.png
sips -z 128 128   icon_512x512@2x.png --out icon_128x128.png
sips -z 256 256   icon_512x512@2x.png --out icon_128x128@2x.png
sips -z 256 256   icon_512x512@2x.png --out icon_256x256.png
sips -z 512 512   icon_512x512@2x.png --out icon_256x256@2x.png
sips -z 512 512   icon_512x512@2x.png --out icon_512x512.png

cp icon_16x16.png      ../AppIcon.appiconset/icon16.png
cp icon_16x16@2x.png   ../AppIcon.appiconset/icon16@2x.png
cp icon_32x32.png      ../AppIcon.appiconset/icon32.png
cp icon_32x32@2x.png   ../AppIcon.appiconset/icon32@2x.png
cp icon_128x128.png    ../AppIcon.appiconset/icon128.png
cp icon_128x128@2x.png ../AppIcon.appiconset/icon128@2x.png
cp icon_256x256.png    ../AppIcon.appiconset/icon256.png
cp icon_256x256@2x.png ../AppIcon.appiconset/icon256@2x.png
cp icon_512x512.png    ../AppIcon.appiconset/icon512.png
cp icon_512x512@2x.png ../AppIcon.appiconset/icon512@2x.png
