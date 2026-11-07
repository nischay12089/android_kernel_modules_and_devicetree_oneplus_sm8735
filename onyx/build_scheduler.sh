#!/bin/bash
# Build the scheduler family with one shared feature layout and strict modpost.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
KOUT=${KERNEL_OUT:-$HOME/onyx-v-oss/out}
DIST="${OPLUS_BUILD_OUT:-$ROOT/onyx-out}"
export PATH="$HOME/onyx-toolchain/clang-r510928/bin:$PATH"
test -s "$KOUT/Module.symvers"
test -s "$KOUT/vmlinux"
mkdir -p "$DIST"/{stage/kernel,stage/i1,stage/i2/x,logs}
ln -sfn "$ROOT/vendor/oplus/kernel/cpu" "$DIST/stage/kernel/oplus_cpu"
modules=(cpu/sched/sched_assist cpu/sched/frame_boost cpu/sched/sched_info
 cpu/sched/eas_opt cpu/sched/qos_sched cpu/sched/task_cpustats cpu/sched/task_sched
 cpu/smart_freq cpu/oplus_omrg)
configs=(OPLUS_FEATURE_SCHED_ASSIST OPLUS_FEATURE_FRAME_BOOST OPLUS_FEATURE_CPU_JANKINFO
 LOCKING_PROTECT OPLUS_FEATURE_QOS_SCHED OPLUS_FEATURE_EAS_OPT OPLUS_FEATURE_TASK_CPUSTATS
 OPLUS_FEATURE_TASK_SCHED OPLUS_FEATURE_SMART_FREQ OPLUS_OMRG OPLUS_PROCS_LOAD_STATE
 BLOCKIO_UX_OPT OPLUS_CPU_AUDIO_PERF OPLUS_FEATURE_LOADBALANCE OPLUS_FEATURE_PIPELINE
 OPLUS_FEATURE_SCHED_DDL OPLUS_SCHED_GROUP_OPT OPLUS_FEATURE_SCHED_CFBT
 OPLUS_FEATURE_GEAS_FDRIVE OPLUS_CPUFREQ_IOWAIT_PROTECT OPLUS_SYSTEM_KERNEL_QCOM)
cpp="-D__ANDROID_COMMON_KERNEL__ -I$DIST/stage/i1 -I$DIST/stage/i2/x"
args=()
for c in "${configs[@]}"; do cpp+=" -DCONFIG_$c=1"; args+=("CONFIG_$c=m"); done
# Every consumer of oplus_task_struct must use this same list.
# VT_CAP stays off: it creates a sched_assist <-> eas_opt dependency cycle.
printf '%s\n' "${configs[@]}" > "$DIST/scheduler-features.txt"
extra=""
: > "$DIST/scheduler-artifacts.txt"
for sub in "${modules[@]}"; do
 dir="$ROOT/vendor/oplus/kernel/$sub"
 log="$DIST/logs/${sub//\//_}.log"
 make -s -C "$KOUT" M="$dir" ARCH=arm64 LLVM=1 clean
 if ! make -j4 -C "$KOUT" M="$dir" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
   KCFLAGS="$cpp -I$dir -Wno-error=unused-function -Wno-error=unused-variable" \
   OPLUS_OUT_OF_TREE_KO=y KBUILD_MODPOST_WARN= KBUILD_EXTRA_SYMBOLS="$extra" \
   "${args[@]}" modules > "$log" 2>&1; then
   tail -40 "$log" >&2
   exit 1
 fi
 test -f "$dir/Module.symvers"
 mapfile -t artifacts < <(find "$dir" -maxdepth 1 -name '*.ko' -type f)
 test "${#artifacts[@]}" -eq 1
 printf '%s\n' "${artifacts[0]}" >> "$DIST/scheduler-artifacts.txt"
 extra+=" $dir/Module.symvers"
 printf 'BUILT %s\n' "$sub"
done

mapfile -t built < "$DIST/scheduler-artifacts.txt"
python3 "$ROOT/onyx/check_layouts.py" "${built[@]}"
