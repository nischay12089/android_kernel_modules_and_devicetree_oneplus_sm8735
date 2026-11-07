#!/bin/bash
# Build one explicitly configured OPLUS module; scheduler consumers use onyx/build_scheduler.sh.
set -euo pipefail
if (( $# < 2 )); then
  echo "usage: $0 SUBDIR CONFIG_OPLUS_FEATURE_X=m [CONFIG_OPLUS_FEATURE_Y=m ...]" >&2
  exit 2
fi
HERE=$(cd "$(dirname "$0")" && pwd)
KOUT=${KERNEL_OUT:-$HOME/onyx-v-oss/out}
export PATH="$HOME/onyx-toolchain/clang-r510928/bin:$PATH"
MOD=$(realpath -e "$HERE/vendor/oplus/kernel/$1"); shift
case "$MOD/" in "$HERE/vendor/oplus/kernel/"*) ;; *) echo "invalid module path" >&2; exit 2;; esac
test -d "$MOD"
if grep -qE 'oplus_cpu/|sched_assist/sa_common|oplus_task_struct|sa_common_struct' "$MOD"/*.c "$MOD"/*.h 2>/dev/null; then
  echo "This module shares scheduler structures; use the coherent scheduler build." >&2
  exit 2
fi
defs="-D__ANDROID_COMMON_KERNEL__"
for arg in "$@"; do
  [[ $arg =~ ^CONFIG_OPLUS_[A-Z0-9_]+=m$ ]] || {
    echo "Unsupported config: $arg (kernel configuration must not be overridden)" >&2
    exit 2
  }
  defs+=" -D${arg%=m}=1"
done
test -s "$KOUT/Module.symvers"
make -s -C "$KOUT" M="$MOD" ARCH=arm64 LLVM=1 clean
make -j4 -C "$KOUT" M="$MOD" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
  KCFLAGS="$defs -Wno-error=unused-function -Wno-error=unused-variable" \
  OPLUS_OUT_OF_TREE_KO=y KBUILD_MODPOST_WARN= \
  KBUILD_EXTRA_SYMBOLS="${KBUILD_EXTRA_SYMBOLS:-}" "$@" modules
mapfile -t built < <(find "$MOD" -maxdepth 1 -name '*.ko' -type f)
(( ${#built[@]} > 0 )) || { echo "No fresh module produced" >&2; exit 1; }
printf '%s\n' "${built[@]}"
