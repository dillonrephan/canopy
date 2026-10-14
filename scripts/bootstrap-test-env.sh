#!/usr/bin/env bash

set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly REQUIREMENTS_FILE="${PROJECT_DIR}/requirements-dev.txt"
readonly TOOLBOX_NAME="${CANOPY_TOOLBOX_NAME:-canopy-dev}"
readonly BUILD_DIR="$(realpath -m -- "${CANOPY_BUILD_DIR:-${PROJECT_DIR}/../lab/build}")"

for command_name in toolbox podman; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

if [[ ! -f "${REQUIREMENTS_FILE}" ]]; then
    printf 'Requirements file not found: %s\n' "${REQUIREMENTS_FILE}" >&2
    exit 1
fi

mapfile -t packages < <(
    sed \
        -e 's/[[:space:]]*#.*$//' \
        -e '/^[[:space:]]*$/d' \
        "${REQUIREMENTS_FILE}"
)

if ((${#packages[@]} == 0)); then
    printf 'No packages found in %s\n' "${REQUIREMENTS_FILE}" >&2
    exit 1
fi

if podman container exists "${TOOLBOX_NAME}"; then
    printf 'Using existing Toolbx container: %s\n' "${TOOLBOX_NAME}"
else
    printf 'Creating Toolbx container: %s\n' "${TOOLBOX_NAME}"
    toolbox --assumeyes create "${TOOLBOX_NAME}"
fi

printf 'Installing development requirements from %s\n' "${REQUIREMENTS_FILE}"
toolbox run --container "${TOOLBOX_NAME}" \
    sudo dnf install --assumeyes "${packages[@]}"

CANOPY_TOOLBOX_NAME="${TOOLBOX_NAME}" \
CANOPY_BUILD_DIR="${BUILD_DIR}" \
    "${SCRIPT_DIR}/check.sh"

printf '\nCanopy test environment is ready in Toolbx %s.\n' "${TOOLBOX_NAME}"
