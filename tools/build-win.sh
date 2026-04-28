#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/build-win.sh [options] [target...]

Build the configured Windows CMake tree from WSL.

Options:
  -c, --config CONFIG     CMake configuration to build. Default: Release
  -b, --build-dir DIR     Build directory. Default: build/win
  -h, --help             Show this help.

Environment overrides:
  VSDEV_CMD              Path to VsDevCmd.bat. Use a path without spaces.
  CMAKE_EXE              Path to Windows cmake.exe. Use a path without spaces.
  VS_ARCH                Visual Studio architecture. Default: x64

Examples:
  tools/build-win.sh renegade
  tools/build-win.sh renegade renegadeserver
  tools/build-win.sh --config Debug renegade
USAGE
}

config="${CONFIG:-Release}"
build_dir="${BUILD_DIR:-build/win}"
vs_arch="${VS_ARCH:-x64}"
vsdev_cmd="${VSDEV_CMD:-C:\\PROGRA~1\\MIB055~1\\2022\\COMMUN~1\\Common7\\Tools\\VsDevCmd.bat}"
cmake_exe="${CMAKE_EXE:-C:\\PROGRA~1\\MIB055~1\\2022\\COMMUN~1\\Common7\\IDE\\COMMON~1\\MICROS~1\\CMake\\CMake\\bin\\cmake.exe}"
targets=()

while (($#)); do
    case "$1" in
        -c|--config)
            if (($# < 2)); then
                echo "error: $1 requires a value" >&2
                exit 2
            fi
            config="$2"
            shift 2
            ;;
        -b|--build-dir)
            if (($# < 2)); then
                echo "error: $1 requires a value" >&2
                exit 2
            fi
            build_dir="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            targets+=("$@")
            break
            ;;
        -*)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            targets+=("$1")
            shift
            ;;
    esac
done

if ((${#targets[@]} == 0)); then
    targets=(renegade)
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_root}/${build_dir}"
fi

if ! command -v cmd.exe >/dev/null 2>&1; then
    echo "error: cmd.exe is required; run this from WSL or a Windows-aware shell" >&2
    exit 1
fi

if command -v wslpath >/dev/null 2>&1; then
    build_dir="$(wslpath -w "${build_dir}")"
fi

cmd_command="call ${vsdev_cmd} -arch=${vs_arch} && ${cmake_exe} --build ${build_dir} --config ${config}"
for target in "${targets[@]}"; do
    cmd_command+=" --target ${target}"
done

cmd.exe /S /C "${cmd_command}"
