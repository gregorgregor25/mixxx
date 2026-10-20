#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_root="${MIXXX_SOURCE_ROOT:-$(cd -- "$script_dir/../.." && pwd)}"
work_root="${MIXXX_WORK_ROOT:-${XDG_CACHE_HOME:-$HOME/.cache}/mixxx-android-flx6}"
deps_root="${MIXXX_DEPS_ROOT:?Set MIXXX_DEPS_ROOT to the extracted Android dependency archive}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_SDK:-}}"
ndk_root="${ANDROID_NDK_HOME:-${sdk_root:+$sdk_root/ndk/27.2.12479018}}"
signing_env="${MIXXX_PRIVATE_SIGNING_ENV:?Set MIXXX_PRIVATE_SIGNING_ENV to the private signing environment file}"
build_tools_version="${ANDROID_BUILD_TOOLS_VERSION:-35.0.0}"

test -f "$source_root/CMakeLists.txt" || { echo "MIXXX_SOURCE_ROOT is not a Mixxx source tree" >&2; exit 1; }
test -f "$deps_root/scripts/buildsystems/vcpkg.cmake" || { echo "MIXXX_DEPS_ROOT is not an extracted Mixxx dependency archive" >&2; exit 1; }
test -n "$sdk_root" && test -d "$sdk_root/platforms/android-34" || { echo "Android SDK platform 34 is missing" >&2; exit 1; }
test -d "$ndk_root" || { echo "Android NDK is missing" >&2; exit 1; }
test -n "${JAVA_HOME:-}" && test -x "$JAVA_HOME/bin/java" || { echo "JAVA_HOME must point to JDK 17" >&2; exit 1; }
test -f "$signing_env" || { echo "Private signing environment file is missing" >&2; exit 1; }
test -x "$sdk_root/build-tools/$build_tools_version/apksigner" || { echo "Android apksigner is missing" >&2; exit 1; }

source "$signing_env"
test -n "${QT_ANDROID_KEYSTORE_ALIAS:-}" && test -f "${QT_ANDROID_KEYSTORE_PATH:-}" &&
    test -n "${QT_ANDROID_KEYSTORE_STORE_PASS:-}" && test -n "${QT_ANDROID_KEYSTORE_KEY_PASS:-}" || {
        echo "Private signing environment must provide the existing Qt Android keystore settings" >&2
        exit 1
    }

mkdir -p "$work_root"
work_root="$(cd -- "$work_root" && pwd)"
source_root="$(cd -- "$source_root" && pwd)"
deps_root="$(cd -- "$deps_root" && pwd)"
sdk_root="$(cd -- "$sdk_root" && pwd)"
ndk_root="$(cd -- "$ndk_root" && pwd)"

export ANDROID_SDK="$sdk_root"
export ANDROID_SDK_ROOT="$sdk_root"
export ANDROID_NDK_HOME="$ndk_root"
export MIXXX_VCPKG_ROOT="$deps_root"
export GRADLE_USER_HOME="${GRADLE_USER_HOME:-$work_root/gradle}"
export PKG_CONFIG_LIBDIR="$deps_root/installed/arm64-android-release/lib/pkgconfig:$deps_root/installed/arm64-android-release/share/pkgconfig"
export QT_ANDROID_KEYSTORE_ALIAS QT_ANDROID_KEYSTORE_PATH QT_ANDROID_KEYSTORE_STORE_PASS QT_ANDROID_KEYSTORE_KEY_PASS

build_dir="$work_root/build-android"
cmake -U 'FFmpeg_*_VERSION' -S "$source_root" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$deps_root/scripts/buildsystems/vcpkg.cmake" \
    -DMIXXX_VCPKG_ROOT="$deps_root" \
    -DVCPKG_TARGET_TRIPLET=arm64-android-release \
    -DVCPKG_DEFAULT_HOST_TRIPLET=x64-linux-release \
    -DCMAKE_SYSTEM_NAME=Android -DCMAKE_SYSTEM_VERSION=34 \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DCMAKE_BUILD_TYPE=Release -DQT6=ON -DQML=ON -DHID=ON -DBULK=ON \
    -DBUILD_TESTING=OFF -DBUILD_BENCH=OFF \
    -DDEBUG_ASSERTIONS_FATAL=OFF -DDOWNLOAD_MANUAL=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

touch "$build_dir/android-mixxx-deployment-settings.json"
cmake --build "$build_dir" --target apk --parallel "${MIXXX_BUILD_JOBS:-6}"

signed_apk="$build_dir/android-build/build/outputs/apk/release/android-build-release-signed.apk"
test -f "$signed_apk" || { echo "Signed APK was not produced" >&2; exit 1; }
"$sdk_root/build-tools/$build_tools_version/apksigner" verify --verbose "$signed_apk"

output_apk="${MIXXX_OUTPUT_APK:-$work_root/mixxx-android14-arm64-signed.apk}"
mkdir -p "$(dirname -- "$output_apk")"
cp -- "$signed_apk" "$output_apk"
sha256sum "$output_apk" > "$output_apk.sha256"
echo "Signed APK: $output_apk"
