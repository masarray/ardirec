#!/usr/bin/env bash
# Debian GUI installer that embeds the Qt-bundled AppImage.
set -euo pipefail
if [[ $# -ne 3 ]]; then
  echo "Usage: $0 VERSION APPIMAGE OUTPUT_DIR" >&2
  exit 2
fi
version="$1"
appimage="$(realpath "$2")"
output="$(realpath -m "$3")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-(alpha|beta|rc)\.[0-9]+)?$ ]] || { echo "Invalid SemVer version" >&2; exit 1; }
[[ -s "$appimage" ]] || { echo "Missing AppImage" >&2; exit 1; }
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$output"
temp="$(mktemp -d)"
trap 'rm -rf "$temp"' EXIT
package="$temp/package"
mkdir -p "$package/DEBIAN" "$package/opt/ardirec" "$package/usr/bin" \
  "$package/usr/share/applications" "$package/usr/share/icons/hicolor/512x512/apps"
install -m 0755 "$appimage" "$package/opt/ardirec/ArDiRec.AppImage"
install -m 0644 "$root/packaging/linux/ardirec.desktop" "$package/usr/share/applications/ardirec.desktop"
install -m 0644 "$root/icon/android-chrome-512x512.png" \
  "$package/usr/share/icons/hicolor/512x512/apps/ardirec.png"
cat > "$package/usr/bin/ardirec" <<'EOF'
#!/usr/bin/env bash
# Avoid requiring libfuse2: AppImage extracts to a temporary directory.
export APPIMAGE_EXTRACT_AND_RUN=1
exec /opt/ardirec/ArDiRec.AppImage "$@"
EOF
chmod 0755 "$package/usr/bin/ardirec"
cat > "$package/DEBIAN/control" <<EOF
Package: ardirec
Version: $version
Section: science
Priority: optional
Architecture: amd64
Maintainer: ArDiRec contributors <noreply@github.com>
Depends: libc6 (>= 2.35), libx11-6, libxcb1
Description: ArDiRec COMTRADE disturbance analysis workstation
 Qt-based, vendor-neutral disturbance viewer.
 Includes Qt runtime inside an AppImage for reliable desktop startup.
EOF
deb="$output/ArDiRec-v$version-Linux-amd64.deb"
dpkg-deb --build --root-owner-group "$package" "$deb"
dpkg-deb --info "$deb"
dpkg-deb --contents "$deb" | grep -F '/opt/ardirec/ArDiRec.AppImage'
