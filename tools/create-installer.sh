#!/bin/bash
set -euo pipefail

# Apple 1 Monitor — macOS DMG Installer Builder
# Usage: ./tools/create-installer.sh [--rebuild]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
APP_NAME="Apple 1 Monitor"
BUNDLE_ID="com.apple1.monitor"
VERSION="1.0.0"
DMG_NAME="Apple1Monitor"
BUILD_DIR="$PROJECT_DIR/build/installer"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
TAURI_DIR="$PROJECT_DIR/apple1-monitor/src-tauri"
BINARY="$TAURI_DIR/target/release/apple1-monitor"
ICON="$TAURI_DIR/icons/icon.icns"
DMG_OUTPUT="$PROJECT_DIR/$DMG_NAME.dmg"
DMG_BG="$SCRIPT_DIR/dmg-background.png"

echo "=== Apple 1 Monitor — macOS Installer ==="
echo ""

# Rebuild if requested or binary missing
if [[ "${1:-}" == "--rebuild" ]] || [[ ! -f "$BINARY" ]]; then
    echo "Building Tauri release binary..."
    (cd "$TAURI_DIR" && cargo build --release)
    echo ""
fi

if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: Binary not found at $BINARY"
    echo "Run: cd apple1-monitor/src-tauri && cargo build --release"
    exit 1
fi

echo "Binary: $BINARY ($(du -h "$BINARY" | cut -f1))"
echo ""

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create .app bundle structure
echo "Creating app bundle..."
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

# Copy binary
cp "$BINARY" "$APP_BUNDLE/Contents/MacOS/$APP_NAME"
chmod +x "$APP_BUNDLE/Contents/MacOS/$APP_NAME"

# Copy icon
cp "$ICON" "$APP_BUNDLE/Contents/Resources/AppIcon.icns"

# Write Info.plist
cat > "$APP_BUNDLE/Contents/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleDisplayName</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>$BUNDLE_ID</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.education</string>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
PLIST

# Write PkgInfo
echo -n "APPL????" > "$APP_BUNDLE/Contents/PkgInfo"

echo "App bundle created: $APP_BUNDLE"
echo "  Size: $(du -sh "$APP_BUNDLE" | cut -f1)"
echo ""

# Remove old DMG if it exists
rm -f "$DMG_OUTPUT"

# Create DMG using create-dmg
echo "Creating DMG installer..."
create-dmg \
    --volname "$APP_NAME" \
    --volicon "$ICON" \
    --background "$DMG_BG" \
    --window-pos 200 120 \
    --window-size 600 400 \
    --icon-size 80 \
    --icon "$APP_NAME.app" 160 200 \
    --app-drop-link 440 200 \
    --text-size 14 \
    --hide-extension "$APP_NAME.app" \
    --no-internet-enable \
    "$DMG_OUTPUT" \
    "$APP_BUNDLE"

echo ""
echo "=== Done ==="
echo "DMG: $DMG_OUTPUT ($(du -h "$DMG_OUTPUT" | cut -f1))"
echo ""
echo "To test: open $DMG_OUTPUT"
