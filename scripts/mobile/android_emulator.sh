#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
usage: ci/rendertest/android/emulator_start.sh --avd NAME [options]

Start an Android emulator and wait until adb can see it.

To list installed avd's:

  ${HOME}/Library/Android/sdk/emulator/emulator -list-avds                                                                [android-tests] 10:25:48
  Pixel_8_Pro_API_36

Options:
  --avd NAME            Android Virtual Device name. Required.
  --gpu MODE            GPU emulation mode passed to the emulator. Default: auto
  --output DIR          Output directory for emulator logs. Default: build/render-tests/android
  --emulator-arg ARG    Extra argument passed to the emulator binary. Repeatable.
  -h, --help            Show this help
EOF
}

AVD_NAME=""
GPU_MODE="auto"
OUTPUT_DIR="build/render-tests/android"
EMULATOR_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --avd)
            AVD_NAME="${2:-}"
            shift 2
            ;;
        --gpu)
            GPU_MODE="${2:-}"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="${2:-}"
            shift 2
            ;;
        --emulator-arg)
            EMULATOR_ARGS+=("${2:-}")
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ -z "${AVD_NAME}" ]]; then
    echo "--avd is required" >&2
    exit 1
fi

if [[ -z "${GPU_MODE}" ]]; then
    echo "--gpu requires a mode, for example auto, host, or software" >&2
    exit 1
fi

ADB_BIN="${ADB_BIN:-adb}"
if ! command -v "${ADB_BIN}" >/dev/null 2>&1; then
    echo "adb not found on PATH" >&2
    exit 1
fi

TIMEOUT_SECONDS="300"
if ! [[ "${TIMEOUT_SECONDS}" =~ ^[0-9]+$ ]] || [[ "${TIMEOUT_SECONDS}" -le 0 ]]; then
    echo "internal timeout must be a positive integer" >&2
    exit 1
fi

find_emulator_binary() {
    local candidate
    local candidates=(
        "${ANDROID_SDK_ROOT:-}/emulator/emulator"
        "${ANDROID_SDK_ROOT:-}/emulator/emulator.exe"
        "${ANDROID_HOME:-}/emulator/emulator"
        "${ANDROID_HOME:-}/emulator/emulator.exe"
        "${HOME}/Android/Sdk/emulator/emulator"
        "${HOME}/Android/Sdk/emulator/emulator.exe"
        "${HOME}/Library/Android/sdk/emulator/emulator"
        "${HOME}/Library/Android/sdk/emulator/emulator.exe"
        "/Applications/Android Studio.app/Contents/emulator/emulator"
        "/Applications/Android Studio.app/Contents/emulator/emulator.exe"
        "${LOCALAPPDATA:-}/Android/Sdk/emulator/emulator.exe"
        "${LOCALAPPDATA:-}/Android/Sdk/emulator/emulator"
        "${PROGRAMFILES:-}/Android/Android Studio/emulator/emulator.exe"
        "${PROGRAMFILES:-}/Android/Android Studio/emulator/emulator"
        "${PROGRAMFILES_X86:-}/Android/Android Studio/emulator/emulator.exe"
        "${PROGRAMFILES_X86:-}/Android/Android Studio/emulator/emulator"
    )

    for candidate in "${candidates[@]}"; do
        if [[ -n "${candidate}" && -x "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

list_avds() {
    "${EMULATOR_BIN}" -list-avds 2>/dev/null | awk 'NF { print }'
}

EMULATOR_BIN="${EMULATOR_BIN:-}"
if [[ -z "${EMULATOR_BIN}" ]]; then
    if command -v emulator >/dev/null 2>&1; then
        EMULATOR_BIN="emulator"
    else
        if EMULATOR_BIN="$(find_emulator_binary)"; then
            :
        else
            echo "emulator not found on PATH or in common Android SDK locations. Set ANDROID_SDK_ROOT, ANDROID_HOME, or EMULATOR_BIN." >&2
            exit 1
        fi
    fi
fi

AVAILABLE_AVDS_TEXT="$(list_avds)"
if ! printf '%s\n' "${AVAILABLE_AVDS_TEXT}" | grep -Fxq -- "${AVD_NAME}"; then
    echo "Unknown AVD name: ${AVD_NAME}" >&2
    if [[ -n "${AVAILABLE_AVDS_TEXT}" ]]; then
        printf 'Available AVDs:\n%s\n' "${AVAILABLE_AVDS_TEXT}" >&2
    else
        echo "No AVDs were found. Create one in Android Studio or with avdmanager." >&2
    fi
    echo "Use --avd with one of the names above." >&2
    exit 1
fi

OUTPUT_DIR_ABS="${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR_ABS}"

LOG_PATH="${OUTPUT_DIR_ABS}/emulator.log"
START_SERIALS_TEXT="$("${ADB_BIN}" devices | awk 'NR > 1 && $2 != "offline" && $1 ~ /^emulator-/ { print $1 }')"

EMULATOR_CMD=(
    "${EMULATOR_BIN}"
    -avd "${AVD_NAME}"
    -no-audio
    -no-window
    -no-boot-anim
    -gpu "${GPU_MODE}"
)
if [[ "${#EMULATOR_ARGS[@]}" -gt 0 ]]; then
    EMULATOR_CMD+=("${EMULATOR_ARGS[@]}")
fi

printf 'Running emulator command:\n  '
printf '%q ' "${EMULATOR_CMD[@]}"
printf '\n'

"${EMULATOR_CMD[@]}" > "${LOG_PATH}" 2>&1 &
EMULATOR_PID="$!"

deadline_epoch="$(( $(date +%s) + TIMEOUT_SECONDS ))"
ADB_SERIAL=""

while [[ "$(date +%s)" -le "${deadline_epoch}" ]]; do
    CURRENT_SERIALS_TEXT="$("${ADB_BIN}" devices | awk 'NR > 1 && $2 != "offline" && $1 ~ /^emulator-/ { print $1 }')"

    while IFS= read -r serial; do
        [[ -n "${serial}" ]] || continue
        if ! printf '%s\n' "${START_SERIALS_TEXT}" | grep -qx -- "${serial}"; then
            ADB_SERIAL="${serial}"
            break
        fi
    done <<EOF
${CURRENT_SERIALS_TEXT}
EOF

    if [[ -n "${ADB_SERIAL}" ]]; then
        break
    fi

    sleep 1
done

if [[ -z "${ADB_SERIAL}" ]]; then
    echo "Emulator process started with PID ${EMULATOR_PID}. Log: ${LOG_PATH}" >&2
    echo "Timed out waiting for the emulator to appear in adb devices." >&2
    exit 1
fi

echo "${ADB_SERIAL}"
