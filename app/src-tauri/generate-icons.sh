#!/bin/bash
# Icon generation script for Tauri app
# Usage: Place a 1024x1024 source PNG as "icon-source.png" in this directory, then run this script.

set -e

SOURCE="icon-source.png"
ICONS_DIR="icons"

if [ ! -f "$SOURCE" ]; then
    echo "Error: $SOURCE not found. Place a 1024x1024 PNG as $SOURCE in this directory."
    exit 1
fi

mkdir -p "$ICONS_DIR"

echo "Generating icons from $SOURCE..."

# Generate PNG icons at various sizes
sips -z 32 32 "$SOURCE" --out "$ICONS_DIR/32x32.png" 2>/dev/null
sips -z 128 128 "$SOURCE" --out "$ICONS_DIR/128x128.png" 2>/dev/null
sips -z 256 256 "$SOURCE" --out "$ICONS_DIR/128x128@2x.png" 2>/dev/null

echo "PNG icons generated."

# For .icns (macOS) - requires iconutil
if command -v iconutil &>/dev/null; then
    mkdir -p iconset.iconset
    sips -z 16 16 "$SOURCE" --out "iconset.iconset/icon_16x16.png" 2>/dev/null
    sips -z 32 32 "$SOURCE" --out "iconset.iconset/icon_16x16@2x.png" 2>/dev/null
    sips -z 32 32 "$SOURCE" --out "iconset.iconset/icon_32x32.png" 2>/dev/null
    sips -z 64 64 "$SOURCE" --out "iconset.iconset/icon_32x32@2x.png" 2>/dev/null
    sips -z 128 128 "$SOURCE" --out "iconset.iconset/icon_128x128.png" 2>/dev/null
    sips -z 256 256 "$SOURCE" --out "iconset.iconset/icon_128x128@2x.png" 2>/dev/null
    sips -z 256 256 "$SOURCE" --out "iconset.iconset/icon_256x256.png" 2>/dev/null
    sips -z 512 512 "$SOURCE" --out "iconset.iconset/icon_256x256@2x.png" 2>/dev/null
    sips -z 512 512 "$SOURCE" --out "iconset.iconset/icon_512x512.png" 2>/dev/null
    sips -z 1024 1024 "$SOURCE" --out "iconset.iconset/icon_512x512@2x.png" 2>/dev/null
    iconutil -c icns iconset.iconset -o "$ICONS_DIR/icon.icns"
    rm -rf iconset.iconset
    echo "macOS .icns generated."
else
    echo "iconutil not found, skipping .icns generation."
fi

# For .ico (Windows) - requires ImageMagick convert or sips
if command -v convert &>/dev/null; then
    convert "$SOURCE" \
        \( -clone 0 -resize 16x16 \) \
        \( -clone 0 -resize 32x32 \) \
        \( -clone 0 -resize 48x48 \) \
        \( -clone 0 -resize 256x256 \) \
        "$ICONS_DIR/icon.ico"
    echo "Windows .ico generated."
else
    echo "ImageMagick convert not found, skipping .ico generation. Use an online converter if needed."
fi

echo "Done! Icons are in $ICONS_DIR/"
