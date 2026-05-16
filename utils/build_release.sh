#!/usr/bin/env bash
#
# build_release.sh — Build every atmega32 target and pack the resulting
# .hex / .bin files into the repo's hex/ directory with the canonical
# upstream naming:
#     cheali-charger-<TARGET>_<VERSION>-<EEPROM>-<DATE>_<CPU>.{hex,bin}
# plus a .sha1 next to each.
#
# Older releases stay in hex/<version>/ subdirs (e.g. hex/2.0/) so the
# top-level hex/ always reflects the current cheali-charger-version
# from CMakeLists.txt.
#
# Implementation note: we build with enable-short-names=ON (default on
# Windows) to keep ninja build paths under the 260-char MAX_PATH limit,
# then rename to the canonical long-form when copying into hex/.
#
# Usage:
#     utils/build_release.sh           # incremental
#     utils/build_release.sh --clean   # nuke build dir first

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-avr-rel"
HEX_DIR="${REPO_ROOT}/hex"

# Make sure MSYS2 UCRT64 (avr-gcc + ninja + cmake) is reachable when run
# from a plain shell or Git Bash.
if ! command -v avr-gcc >/dev/null 2>&1; then
    export PATH="/c/msys64/ucrt64/bin:${PATH}"
fi
for tool in avr-gcc cmake ninja avr-objcopy sha1sum; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "ERROR: ${tool} not in PATH" >&2
        exit 1
    }
done

if [[ "${1:-}" == "--clean" ]]; then
    echo "Removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
    echo "Configuring ${BUILD_DIR}"
    cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE=avr-toolchain.cmake \
        -Denable-short-names=ON \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -G Ninja >/dev/null
fi

# Pull version / eeprom subversions / build-number from CMakeLists.txt
# (assemble the eeprom string in bash since CMake variable substitution
# does not happen on a raw grep of the .txt file).
read_setting() {
    grep -E "^set\($1 " "${REPO_ROOT}/CMakeLists.txt" \
        | sed -E "s/^set\($1[[:space:]]+//; s/\).*//"
}
VERSION=$(read_setting cheali-charger-version)
EEPROM_CALIB=$(read_setting cheali-charger-eeprom-calibration-version)
EEPROM_PDATA=$(read_setting cheali-charger-eeprom-programdata-version)
EEPROM_SETT=$(read_setting cheali-charger-eeprom-settings-version)
EEPROM_VER="e${EEPROM_CALIB}.${EEPROM_PDATA}.${EEPROM_SETT}"
DATE_TAG=$(date +%Y%m%d)

echo "Building cheali-charger ${VERSION} (${EEPROM_VER}) @ ${DATE_TAG}"
cmake --build "${BUILD_DIR}" 2>&1 | tail -3

mkdir -p "${HEX_DIR}"
copied=0
for ext in hex bin; do
    while IFS= read -r -d '' f; do
        # File name is <target>_atmega32.<ext>; strip "_atmega32.<ext>" to get target.
        base=$(basename "${f}" "_atmega32.${ext}")
        new="cheali-charger-${base}_${VERSION}-${EEPROM_VER}-${DATE_TAG}_atmega32.${ext}"
        cp "${f}" "${HEX_DIR}/${new}"
        copied=$((copied + 1))
    done < <(find "${BUILD_DIR}/src/hardware" -name "*_atmega32.${ext}" -print0)
done
echo "Copied ${copied} files to ${HEX_DIR}/"

(
    cd "${HEX_DIR}"
    for f in cheali-charger-*_${VERSION}-*.hex cheali-charger-*_${VERSION}-*.bin; do
        [[ -f "${f}" ]] || continue
        sha1sum "${f}" > "${f}.sha1"
    done
)
echo "Done."
