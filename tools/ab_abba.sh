#!/usr/bin/env bash
# A/B of a SPARTAN_TOGGLE_FILE bit in a running game, in on-off-off-on order (ABBA): a scene that
# drifts over time (enemies walking in) weighs both sides the same. Same inputs and output as
# ab.sh: "on" is the bit clear, "off" the bit set; the first "Frame stats" line after a switch is
# skipped.
# usage: ab_abba.sh <log> <toggles> <bit> [groups=4] [seconds=20] [base mask=0]
set -euo pipefail
export LC_ALL=C
log=$1 toggles=$2 bit=$3 groups=${4:-4} seconds=${5:-20} base=${6:-0}
declare -A fps_sum count
for ((i = 0; i < groups * 4; ++i)); do
    case $((i % 4)) in 0 | 3) phase=on mask=$base ;; *) phase=off mask=$((base | bit)) ;; esac
    echo $mask > "$toggles"
    start=$(wc -l < "$log")
    sleep "$seconds"
    read -r f n < <(tail -n +$((start + 1)) "$log" | grep '^Frame stats:' | tail -n +2 |
        sed -E 's/^Frame stats: ([0-9.]+) FPS.*/\1/' |
        awk '{f += $1; n++} END {if (n) printf "%.2f %d\n", f / n, n; else print "0 0"}')
    printf '%-3s phase %d: %6.2f FPS (%d samples)\n' $phase $i "$f" "$n"
    if ((n)); then
        fps_sum[$phase]=$(awk -v a="${fps_sum[$phase]:-0}" -v b="$f" -v n="$n" 'BEGIN {print a + b * n}')
        count[$phase]=$((${count[$phase]:-0} + n))
    fi
done
echo $base > "$toggles"
for phase in on off; do
    awk -v s="${fps_sum[$phase]:-0}" -v n="${count[$phase]:-0}" -v p=$phase \
        'BEGIN {printf "%s: %.2f FPS\n", p, n ? s / n : 0}'
done
