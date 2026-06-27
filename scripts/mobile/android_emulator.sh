#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
usage: scripts/mobile/android_emulator.sh (--avd NAME [options] | --run-apk APK | --list-avds | --stop)

Start an Android emulator and wait until adb can see it, run an APK on a running emulator, or stop a running emulator.

To list installed AVDs:

  scripts/mobile/android_emulator.sh --list-avds

Options:
  --avd NAME            Android Virtual Device name. Required when starting.
  --run-apk APK         Install and launch an APK on the running emulator. Can be used with --avd.
  --list-avds           Print installed Android Virtual Device names and exit.
  --gpu MODE            GPU emulation mode passed to the emulator. Default: auto
  --output DIR          Output directory for emulator logs. Default: build/render-tests/android
  --emulator-arg ARG    Extra argument passed to the emulator binary. Repeatable.
  --stop                Stop the currently running emulator.
  -h, --help            Show this help
EOF
}

if [[ $# -eq 0 ]]; then
    usage
    exit 0
fi

AVD_NAME=""
LIST_AVDS=0
GPU_MODE="auto"
OUTPUT_DIR="build/render-tests/android"
EMULATOR_ARGS=()
RUN_APK=""
STOP_EMULATOR="0"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --avd)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "--avd requires a name" >&2
                usage >&2
                exit 1
            fi
            AVD_NAME="${2:-}"
            shift 2
            ;;
        --run-apk)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "--run-apk requires an APK path" >&2
                usage >&2
                exit 1
            fi
            RUN_APK="${2:-}"
            shift 2
            ;;
        --list-avds)
            LIST_AVDS=1
            shift
            ;;
        --gpu)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "--gpu requires a mode, for example auto, host, or software" >&2
                usage >&2
                exit 1
            fi
            GPU_MODE="${2:-}"
            shift 2
            ;;
        --output)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "--output requires a directory" >&2
                usage >&2
                exit 1
            fi
            OUTPUT_DIR="${2:-}"
            shift 2
            ;;
        --emulator-arg)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                echo "--emulator-arg requires an argument" >&2
                usage >&2
                exit 1
            fi
            EMULATOR_ARGS+=("${2:-}")
            shift 2
            ;;
        --stop)
            STOP_EMULATOR="1"
            shift
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

ACTION_COUNT=0
if [[ "${STOP_EMULATOR}" == "1" ]]; then
    ACTION_COUNT=$((ACTION_COUNT + 1))
fi
if [[ "${LIST_AVDS}" -eq 1 ]]; then
    ACTION_COUNT=$((ACTION_COUNT + 1))
fi
if [[ "${ACTION_COUNT}" -gt 1 ]]; then
    echo "--stop and --list-avds cannot be used together" >&2
    usage >&2
    exit 1
fi

if [[ "${ACTION_COUNT}" -gt 0 ]] && [[ -n "${RUN_APK}" ]]; then
    echo "--run-apk cannot be used with --stop or --list-avds" >&2
    usage >&2
    exit 1
fi

if [[ "${STOP_EMULATOR}" == "0" ]] && [[ "${LIST_AVDS}" -eq 0 ]] && [[ -z "${AVD_NAME}" ]] && [[ -z "${RUN_APK}" ]]; then
    echo "--avd is required" >&2
    usage >&2
    exit 1
fi

if [[ -n "${RUN_APK}" ]] && [[ ! -f "${RUN_APK}" ]]; then
    echo "APK not found: ${RUN_APK}" >&2
    exit 1
fi

if [[ -z "${GPU_MODE}" ]]; then
    echo "--gpu requires a mode, for example auto, host, or software" >&2
    exit 1
fi

TIMEOUT_SECONDS="300"
if ! [[ "${TIMEOUT_SECONDS}" =~ ^[0-9]+$ ]] || [[ "${TIMEOUT_SECONDS}" -le 0 ]]; then
    echo "internal timeout must be a positive integer" >&2
    exit 1
fi

list_running_emulators() {
    "${ADB_BIN}" devices | awk 'NR > 1 && $2 != "offline" && $1 ~ /^emulator-/ { print $1 }'
}

require_adb_binary() {
    ADB_BIN="${ADB_BIN:-adb}"
    if ! command -v "${ADB_BIN}" >/dev/null 2>&1; then
        echo "adb not found on PATH" >&2
        exit 1
    fi
}

find_android_build_tool_binary() {
    local tool_name="$1"
    local candidate
    local sdk_root
    local sdk_roots=(
        "${ANDROID_SDK_ROOT:-}"
        "${ANDROID_HOME:-}"
        "${HOME}/Android/Sdk"
        "${HOME}/Library/Android/sdk"
        "${LOCALAPPDATA:-}/Android/Sdk"
    )

    if command -v "${tool_name}" >/dev/null 2>&1; then
        command -v "${tool_name}"
        return 0
    fi

    for sdk_root in "${sdk_roots[@]}"; do
        if [[ -z "${sdk_root}" || ! -d "${sdk_root}/build-tools" ]]; then
            continue
        fi

        while IFS= read -r candidate; do
            if [[ -x "${candidate}" ]]; then
                printf '%s\n' "${candidate}"
                return 0
            fi
        done < <(find "${sdk_root}/build-tools" -type f \( -name "${tool_name}" -o -name "${tool_name}.exe" \) 2>/dev/null | sort -r)
    done

    return 1
}

require_aapt2_binary() {
    AAPT2_BIN="${AAPT2_BIN:-}"
    if [[ -n "${AAPT2_BIN}" ]]; then
        if command -v "${AAPT2_BIN}" >/dev/null 2>&1; then
            return 0
        fi

        echo "aapt2 not found at AAPT2_BIN=${AAPT2_BIN}" >&2
        exit 1
    fi

    if AAPT2_BIN="$(find_android_build_tool_binary "aapt2")"; then
        return 0
    fi

    echo "aapt2 not found on PATH or in common Android SDK locations. Set ANDROID_SDK_ROOT, ANDROID_HOME, or AAPT2_BIN." >&2
    exit 1
}

get_single_running_emulator() {
    local action_name="${1:-use}"
    local running_serials
    local running_count

    running_serials="$(list_running_emulators)"
    running_count="$(printf '%s\n' "${running_serials}" | awk 'NF { count++ } END { print count + 0 }')"

    if [[ "${running_count}" -eq 0 ]]; then
        echo "No running emulator found in adb devices." >&2
        exit 1
    fi

    if [[ "${running_count}" -gt 1 ]]; then
        echo "More than one running emulator found; refusing to guess which one to ${action_name}." >&2
        printf 'Running emulators:\n%s\n' "${running_serials}" >&2
        exit 1
    fi

    printf '%s\n' "${running_serials}"
}

get_apk_package_name() {
    local apk_path="$1"
    local package_name

    require_aapt2_binary
    if ! package_name="$("${AAPT2_BIN}" dump badging "${apk_path}" 2>/dev/null | awk -F"'" '/^package: name=/ { print $2; exit }')"; then
        echo "Failed to read APK metadata from ${apk_path}" >&2
        exit 1
    fi
    if [[ -z "${package_name}" ]]; then
        echo "Failed to determine package name from ${apk_path}" >&2
        exit 1
    fi

    printf '%s\n' "${package_name}"
}

install_and_run_apk() {
    local adb_serial="$1"
    local apk_path="$2"
    local package_name

    package_name="$(get_apk_package_name "${apk_path}")"

    echo "Installing ${apk_path} on ${adb_serial}"
    "${ADB_BIN}" -s "${adb_serial}" install -r "${apk_path}"

    echo "Launching ${package_name}"
    "${ADB_BIN}" -s "${adb_serial}" shell monkey -p "${package_name}" -c android.intent.category.LAUNCHER 1
}

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

stop_running_emulator() {
    local adb_serial
    local deadline_epoch

    adb_serial="$(get_single_running_emulator "stop")"
    "${ADB_BIN}" -s "${adb_serial}" emu kill >/dev/null

    deadline_epoch="$(( $(date +%s) + TIMEOUT_SECONDS ))"
    while [[ "$(date +%s)" -le "${deadline_epoch}" ]]; do
        if ! list_running_emulators | grep -qx -- "${adb_serial}"; then
            echo "${adb_serial}"
            return 0
        fi

        sleep 1
    done

    echo "Timed out waiting for emulator ${adb_serial} to stop." >&2
    exit 1
}

if [[ "${STOP_EMULATOR}" == "1" ]]; then
    require_adb_binary
    stop_running_emulator
    exit 0
fi

if [[ -n "${RUN_APK}" && -z "${AVD_NAME}" ]]; then
    require_adb_binary
    ADB_SERIAL="$(get_single_running_emulator "use")"
    install_and_run_apk "${ADB_SERIAL}" "${RUN_APK}"
    echo "${ADB_SERIAL}"
    exit 0
fi

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
if [[ "${LIST_AVDS}" -eq 1 ]]; then
    if [[ -n "${AVAILABLE_AVDS_TEXT}" ]]; then
        printf '%s\n' "${AVAILABLE_AVDS_TEXT}"
        exit 0
    fi

    echo "No AVDs were found. Create one in Android Studio or with avdmanager." >&2
    exit 1
fi

if [[ -z "${AVD_NAME}" ]]; then
    usage
    exit 1
fi

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

require_adb_binary

OUTPUT_DIR_ABS="${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR_ABS}"

LOG_PATH="${OUTPUT_DIR_ABS}/emulator.log"
START_SERIALS_TEXT="$(list_running_emulators)"

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
    CURRENT_SERIALS_TEXT="$(list_running_emulators)"

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

if [[ -n "${RUN_APK}" ]]; then
    install_and_run_apk "${ADB_SERIAL}" "${RUN_APK}"
fi

echo "${ADB_SERIAL}"
