> AI-generated documentation for this personal fork. Verify the commands and hardware behavior before reuse.

# Rebuild the Honor Pad 9 / DDJ-FLX6 APK

This builds the Android 14 (API 34), ARM64 APK from this fork's checked-out source. The tested setup is an Honor Pad 9 with a Pioneer DDJ-FLX6 connected through USB, with separate master and headphone outputs. It is a recovery recipe for this setup, not a general Android release qualification.

Use Linux or WSL2 Ubuntu 24.04 with CMake, Ninja, Python 3, `pkg-config`, `libxcb-xinput0`, JDK 17, Android SDK platforms 34 and 35, Android build tools 35.0.0, and NDK 27.2.12479018. A working Android SDK installation must also provide Qt's Android packaging tools through the Mixxx dependency archive. Build on a Linux filesystem with enough space for the extracted dependencies and APK intermediates.

Download the upstream Mixxx Android dependency archive and verify its SHA-256 **before** extracting it:

```text
https://downloads.mixxx.org/dependencies/2.7-rel/Linux/mixxx-deps-2.7-arm64-android-rel-ee6dc7fe.zip
SHA-256: 35e68a27bf0a8b1298a141cd69f2a509978b45eea810fd7a9c262ec8e556a131
```

The archive extracts to `mixxx-deps-2.7-arm64-android-rel-ee6dc7fe` and must contain `scripts/buildsystems/vcpkg.cmake`. Use a private copy of the **existing** Android signing keystore and environment file. That file must export `QT_ANDROID_KEYSTORE_ALIAS`, `QT_ANDROID_KEYSTORE_PATH`, `QT_ANDROID_KEYSTORE_STORE_PASS`, and `QT_ANDROID_KEYSTORE_KEY_PASS`. After moving the private signing folder, update its environment file's keystore path to the restored location. Restrict access to the private files; the build does not generate a replacement key. A replacement key would prevent an in-place update of an installation signed by the original key.

From the fork checkout, set these paths for your machine and run:

```bash
export MIXXX_SOURCE_ROOT="$(pwd)"
export MIXXX_WORK_ROOT="$HOME/.cache/mixxx-android-flx6"
export MIXXX_DEPS_ROOT="/path/to/mixxx-deps-2.7-arm64-android-rel-ee6dc7fe"
export ANDROID_SDK_ROOT="/path/to/android-sdk"
export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
export JAVA_HOME="/path/to/jdk-17"
export MIXXX_PRIVATE_SIGNING_ENV="/private/path/to/signing/environment.sh"
bash tools/android-flx6/build.sh
```

`MIXXX_SOURCE_ROOT`, `MIXXX_WORK_ROOT`, `MIXXX_DEPS_ROOT`, `ANDROID_SDK_ROOT`, and `ANDROID_NDK_HOME` are configurable. `MIXXX_BUILD_JOBS` defaults to 6, `ANDROID_BUILD_TOOLS_VERSION` defaults to 35.0.0, and `MIXXX_OUTPUT_APK` can select the destination. The build enables Qt/QML, HID and bulk USB, and uses the same API 34 ARM64 target as the tested APK. It verifies the APK signature with `apksigner` and writes a `.sha256` file beside the signed APK. The script touches Qt's deployment settings after configuration so Java changes trigger packaging.

The public fork contains the Android app changes but does not include a user profile, music, library database, saved delay/jog/filter settings, or the signing key. Install the APK using the original signing identity first, then restore the private profile backup into the app's external files area while Mixxx is closed. A new tablet can receive the APK and profile without the signing key; rebuilding compatible updates requires the key. Check file paths and Android permissions on the new device before relying on the restored library, since tracks may be stored at device-specific paths.

The working controller preset is derived from a community DDJ-FLX6 mapping with no explicit redistribution licence. It is kept in the private backup and is **not** included in this public source tree or the APK. Restore its XML/JavaScript files from that private backup into the profile's `controllers` directory before using the controller. Keep that mapping private unless its licence is clarified.

> End of AI-generated documentation. Verify the commands and hardware behavior before reuse.
