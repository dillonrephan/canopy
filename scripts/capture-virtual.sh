#!/usr/bin/bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"

output_path="${1:-/tmp/canopy-screenshot.png}"
open_path="${2:-}"
build_dir="${CANOPY_BUILD_DIR:-${project_root}/build-toolbox}"
app_path="${build_dir}/canopy"
capture_delay="${CANOPY_CAPTURE_DELAY:-2}"
virtual_width="${CANOPY_CAPTURE_WIDTH:-1410}"
virtual_height="${CANOPY_CAPTURE_HEIGHT:-958}"

for command_name in dbus-run-session kwin_wayland spectacle; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "Missing host command: ${command_name}" >&2
        exit 1
    fi
done

if [[ ! -x "${app_path}" ]]; then
    echo "Canopy is not built at ${app_path}. Run ./scripts/check.sh first." >&2
    exit 1
fi

output_path="$(realpath -m -- "${output_path}")"
if [[ ! -d "$(dirname -- "${output_path}")" ]]; then
    echo "Output directory does not exist: $(dirname -- "${output_path}")" >&2
    exit 1
fi

# A private session bus prevents the nested KWin and Spectacle from contacting
# their counterparts in the user's real desktop session.
if [[ "${CANOPY_CAPTURE_PRIVATE_SESSION:-0}" != 1 ]]; then
    export CANOPY_CAPTURE_PRIVATE_SESSION=1
    exec dbus-run-session -- "${BASH_SOURCE[0]}" "${output_path}" "${open_path}"
fi

socket_name="canopy-capture-${$}"
kwin_pid=""
app_pid=""

cleanup() {
    if [[ -n "${app_pid}" ]]; then
        kill "${app_pid}" 2>/dev/null || true
        wait "${app_pid}" 2>/dev/null || true
    fi
    if [[ -n "${kwin_pid}" ]]; then
        kill "${kwin_pid}" 2>/dev/null || true
        wait "${kwin_pid}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

kwin_wayland \
    --virtual \
    --socket "${socket_name}" \
    --width "${virtual_width}" \
    --height "${virtual_height}" \
    --scale 1 \
    --no-lockscreen \
    --no-global-shortcuts &
kwin_pid=$!

for _ in {1..50}; do
    if [[ -S "${XDG_RUNTIME_DIR}/${socket_name}" ]]; then
        break
    fi
    if ! kill -0 "${kwin_pid}" 2>/dev/null; then
        echo "The virtual KWin compositor exited before creating its socket." >&2
        exit 1
    fi
    sleep 0.1
done

if [[ ! -S "${XDG_RUNTIME_DIR}/${socket_name}" ]]; then
    echo "Timed out waiting for the virtual KWin compositor." >&2
    exit 1
fi

app_command=("${app_path}")
if [[ -n "${open_path}" ]]; then
    app_command+=("${open_path}")
fi

WAYLAND_DISPLAY="${socket_name}" \
QT_QPA_PLATFORM=wayland \
    "${app_command[@]}" &
app_pid=$!

sleep "${capture_delay}"

if ! kill -0 "${app_pid}" 2>/dev/null; then
    echo "Canopy exited before the screenshot could be taken." >&2
    exit 1
fi

WAYLAND_DISPLAY="${socket_name}" \
QT_QPA_PLATFORM=wayland \
    spectacle -i -b -f -n -o "${output_path}"

if [[ ! -s "${output_path}" ]]; then
    echo "Spectacle did not create ${output_path}." >&2
    exit 1
fi

echo "Captured isolated Canopy frame: ${output_path}"
