#!/usr/bin/env bash

set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
readonly TOOLBOX_NAME="${CANOPY_TOOLBOX_NAME:-canopy-dev}"
readonly BUILD_DIR="$(realpath -m -- "${CANOPY_BUILD_DIR:-${PROJECT_DIR}/../lab/build}")"

for command_name in toolbox podman; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

if ! podman container exists "${TOOLBOX_NAME}"; then
    printf 'Toolbx container not found: %s\n' "${TOOLBOX_NAME}" >&2
    printf 'Run %s first.\n' "${PROJECT_DIR}/scripts/bootstrap-test-env.sh" >&2
    exit 1
fi

printf 'Configuring Canopy in %s\n' "${BUILD_DIR}"
toolbox run --container "${TOOLBOX_NAME}" \
    cmake \
        -S "${PROJECT_DIR}" \
        -B "${BUILD_DIR}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTING=ON

printf 'Building Canopy\n'
toolbox run --container "${TOOLBOX_NAME}" \
    cmake --build "${BUILD_DIR}" --parallel "${CANOPY_BUILD_JOBS:-4}"

printf 'Linting QML\n'
toolbox run --container "${TOOLBOX_NAME}" \
    cmake --build "${BUILD_DIR}" --target canopy_qmllint

printf 'Running Canopy tests\n'
toolbox run --container "${TOOLBOX_NAME}" \
    ctest --test-dir "${BUILD_DIR}" --output-on-failure

printf '\nAll Canopy checks passed.\n'
