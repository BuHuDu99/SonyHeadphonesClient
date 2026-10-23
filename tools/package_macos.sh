#!/bin/bash
set -euo pipefail

# Package an already-built application; no credentials or paid services needed.
app_path="${1:-output/SonyHeadphonesClient.app}"
dmg_path="${2:-output/SonyHeadphonesClient-macOS-arm64-Build23.dmg}"
[[ -d "$app_path/Contents/MacOS" ]] || { echo "App bundle not found: $app_path" >&2; exit 1; }
codesign --verify --deep --strict "$app_path"
staging_path="$(mktemp -d "${TMPDIR:-/tmp}/sound-connect-dmg.XXXXXX")"
trap 'rm -rf "$staging_path"' EXIT

ditto "$app_path" "$staging_path/SonyHeadphonesClient.app"
ln -s /Applications "$staging_path/Applications"
cat > "$staging_path/Huong-dan-cai-dat.txt" <<'GUIDE'
SOUND CONNECT — BUILD 23

1. Kéo SonyHeadphonesClient.app vào Applications.
2. Tháo (Eject) ổ đĩa Sound Connect.
3. Mở ứng dụng trong Applications.
4. Ghép đôi tai nghe Sony trong Cài đặt Bluetooth của macOS, rồi kết nối trong app.

Ứng dụng chạy độc lập, không cần thư mục mã nguồn, Python, CMake hoặc khóa API.
Bản này dành cho Mac Apple Silicon (arm64), macOS 27.0 trở lên.
Giấy phép nằm trong gói ứng dụng.

Ứng dụng được ký ad-hoc, chưa có chứng thực Apple Developer/notarization.
GUIDE
mkdir -p "$(dirname "$dmg_path")"
hdiutil create -volname 'Sound Connect' -srcfolder "$staging_path" -fs HFS+ -format UDZO -ov "$dmg_path"
hdiutil verify "$dmg_path"
