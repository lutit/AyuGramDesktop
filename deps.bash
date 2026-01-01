#!/usr/bin/env bash
set -euo pipefail

# ===== deps via pacman / yay =====

PKGS=(
  openh264

  qt6-base qt6-svg qt6-imageformats qt6-wayland qt6-multimedia qt6-tools
  ffmpeg openssl zlib icu openal

  libx11 libxext libxrandr libxrender libxkbcommon
  xcb-util xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm xcb-util-cursor

  wayland wayland-protocols
  libpulse alsa-lib

  libjpeg-turbo libpng libwebp brotli
  freetype2 harfbuzz fontconfig

  libvpx opus libva libdrm mesa
  lz4 zstd snappy

  minizip libsrtp libsecret libnotify
  xdg-utils glib2 gobject-introspection

  rnnoise hunspell libavif vulkan-headers
)

if command -v yay >/dev/null 2>&1; then
  yay -S --needed "${PKGS[@]}"
else
  sudo pacman -S --needed "${PKGS[@]}"
fi

# ===== ada (URL parser) =====

WORKDIR="$HOME/build-deps"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

if [ ! -d ada ]; then
  git clone -b v3.2.4 https://github.com/ada-url/ada.git
fi

cd ada
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DADA_TESTING=OFF \
  -DADA_TOOLS=OFF \
  -DADA_INCLUDE_URL_PATTERN=OFF \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local

cmake --build build -j"$(nproc)"
sudo cmake --install build
sudo ldconfig

echo "✔ deps installed"
