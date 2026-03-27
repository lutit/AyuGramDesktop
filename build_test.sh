#!/usr/bin/env bash
set -euo pipefail

REMOTE_HOST="${REMOTE_HOST:-macserver}"
REMOTE_PATH="${REMOTE_PATH:-/home/user/TBuild/ayuhalal/tdesktop}"
REMOTE_BUILD_SCRIPT="${REMOTE_BUILD_SCRIPT:-/home/user/build-ayugram.sh}"

rsync -av \
	--exclude '.cache/' \
	--exclude '.git' \
	--exclude 'out/' \
	--exclude '**/*.o' \
	--exclude '**/*.obj' \
	--exclude '**/*.a' \
	--exclude '**/*.so' \
	--exclude '**/*.dylib' \
	--exclude '**/*.dll' \
	--exclude '**/*.tmp' \
	--exclude 'Telegram/ThirdParty/**' \
	--exclude 'Telegram/codegen' \
	--exclude 'Telegram/lib_base' \
	--exclude 'Telegram/lib_crl' \
	--exclude 'Telegram/lib_icu' \
	--exclude 'Telegram/lib_lottie' \
	--exclude 'Telegram/lib_qr' \
	--exclude 'Telegram/lib_rpl' \
	--exclude 'Telegram/lib_spellcheck' \
	--exclude 'Telegram/lib_storage' \
	--exclude 'Telegram/lib_tl' \
	--exclude 'Telegram/lib_ui' \
	--exclude 'Telegram/lib_webrtc' \
	--exclude 'Telegram/lib_webview' \
	--exclude 'cmake' \
	./ "${REMOTE_HOST}:${REMOTE_PATH}"

ssh -tt "${REMOTE_HOST}" "${REMOTE_BUILD_SCRIPT}"
