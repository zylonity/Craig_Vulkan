#!/bin/bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
driver_root="${CRAIG_MOLTENVK_RT_ROOT:-$HOME/Library/Caches/Craig_Vulkan/MoltenVK-ray-query}"
driver_dir="$driver_root/Package/Release/MoltenVK/dynamic/dylib/macOS"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This launcher is for experimental MoltenVK on macOS" >&2
    exit 1
fi

if [[ ! -f "$driver_dir/MoltenVK_icd.json" || ! -f "$driver_dir/libMoltenVK.dylib" ]]; then
    echo "Experimental MoltenVK build missing from $driver_dir" >&2
    echo "Set CRAIG_MOLTENVK_RT_ROOT to your built ray-query checkout" >&2
    exit 1
fi

# restrict discovery to this build so Craig's normal KosmicKrisp preference cannot select another driver
export VK_DRIVER_FILES="$driver_dir/MoltenVK_icd.json"
export VK_ICD_FILENAMES="$VK_DRIVER_FILES"
export MVK_CONFIG_ENABLE_EXPERIMENTAL_RAY_TRACING=1

# optionally run another command with the same driver, e.g. vulkaninfo --summary
if [[ $# -gt 0 ]]; then
    exec "$@"
fi

# assets are relative to the executable's working directory, not its location
cd "$project_dir/Craig_Vulkan"
exec "$project_dir/cmake-build-debug/Craig_Vulkan"
