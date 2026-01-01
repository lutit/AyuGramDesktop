set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TG_OWT_DIR="${TG_OWT_DIR:-$ROOT/tg_owt}"
BUILD_DIR="${BUILD_DIR:-$TG_OWT_DIR/out/Release}"
PREFIX="${PREFIX:-/usr/local}"
SPECIAL_TARGET="${TG_OWT_SPECIAL_TARGET:-}"
PATCH_ABSL_NONNULL="${PATCH_ABSL_NONNULL:-1}"

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Не найдено: $1" >&2
    exit 1
  }
}

need_cmd git
need_cmd cmake
need_cmd ninja
need_cmd pkg-config

if [[ ! -d "$TG_OWT_DIR/.git" ]]; then
  echo "Клонирую tg_owt..."
  git clone https://github.com/desktop-app/tg_owt.git "$TG_OWT_DIR"
fi

cd "$TG_OWT_DIR"
git fetch --depth=1 origin 5c5c71258777d0196dbb3a09cc37d2f56ead28ab
git checkout 5c5c71258777d0196dbb3a09cc37d2f56ead28ab

git submodule update --init --recursive

if [[ "$PATCH_ABSL_NONNULL" == "1" ]]; then
  while IFS= read -r -d '' f; do
    if grep -q "absl::Nonnull<" "$f"; then
      cp -n "$f" "$f.bak" || true
      sed -i 's/absl::Nonnull<\([^>]*\)>/\1/g' "$f"
    fi
    if grep -q "absl::Nullable<" "$f"; then
      cp -n "$f" "$f.bak" || true
      sed -i 's/absl::Nullable<\([^>]*\)>/\1/g' "$f"
    fi
  done < <(find "$TG_OWT_DIR/src" -type f \( -name '*.h' -o -name '*.cc' \) -print0)
fi

pkg_inc() {
  local pkg="$1"
  local inc
  inc="$(pkg-config --cflags-only-I "$pkg" 2>/dev/null | tr ' ' '\n' | sed -n 's/^-I//p' | head -n1)"
  if [[ -n "$inc" ]]; then
    echo "$inc"
  fi
}

LIBJPEG_INC="${LIBJPEG_INC:-$(pkg_inc libjpeg || true)}"
OPENSSL_INC="${OPENSSL_INC:-$(pkg_inc openssl || true)}"
OPUS_INC="${OPUS_INC:-$(pkg_inc opus || true)}"
VPX_INC="${VPX_INC:-$(pkg_inc vpx || true)}"
OPENH264_INC="${OPENH264_INC:-$(pkg_inc openh264 || true)}"
FFMPEG_INC="${FFMPEG_INC:-$(pkg_inc libavcodec || true)}"

mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="$PREFIX"
  -DCMAKE_PREFIX_PATH="$PREFIX;/usr;/usr/local"
  -DTG_OWT_BUILD_AUDIO_BACKENDS=OFF
)

if [[ -n "$SPECIAL_TARGET" ]]; then
  CMAKE_ARGS+=("-DTG_OWT_SPECIAL_TARGET=$SPECIAL_TARGET")
fi

[[ -n "$LIBJPEG_INC" ]] && CMAKE_ARGS+=("-DTG_OWT_LIBJPEG_INCLUDE_PATH=$LIBJPEG_INC")
[[ -n "$OPENSSL_INC" ]] && CMAKE_ARGS+=("-DTG_OWT_OPENSSL_INCLUDE_PATH=$OPENSSL_INC")
[[ -n "$OPUS_INC" ]] && CMAKE_ARGS+=("-DTG_OWT_OPUS_INCLUDE_PATH=$OPUS_INC")
[[ -n "$VPX_INC" ]] && CMAKE_ARGS+=("-DTG_OWT_LIBVPX_INCLUDE_PATH=$VPX_INC")
[[ -n "$OPENH264_INC" ]] && CMAKE_ARGS+=("-DTG_OWT_OPENH264_INCLUDE_PATH=$OPENH264_INC")
[[ -n "$FFMPEG_INC" ]] && CMAKE_ARGS+=("-DTG_OWT_FFMPEG_INCLUDE_PATH=$FFMPEG_INC")

if [[ -f /usr/lib/cmake/absl/abslConfig.cmake ]]; then
  CMAKE_ARGS+=("-Dabsl_DIR=/usr/lib/cmake/absl")
fi

cmake -S . -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j"$(nproc)"

sudo cmake --install "$BUILD_DIR"

echo "tg_owt собран. Если CMake не видит его, укажи: -Dtg_owt_DIR=$PREFIX/lib/cmake/tg_owt"
