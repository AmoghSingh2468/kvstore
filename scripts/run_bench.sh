#!/usr/bin/env bash
set -euo pipefail

PORT=${PORT:-6380}
LABEL=${LABEL:-baseline}
OUT="bench/results/${LABEL}.csv"
mkdir -p bench/results

echo "label,clients,test,ops_per_sec" > "$OUT"

for C in 1 2 4 8 16 32 64; do
  for T in set get; do
    redis-benchmark -p "$PORT" -t "$T" -c "$C" -n 20000 -q > /dev/null 2>&1
    R=$(redis-benchmark -p "$PORT" -t "$T" -c "$C" -n 200000 -q 2>/dev/null)
    OPS=$(echo "$R" | grep -oP '[\d.]+(?= requests per second)' | head -1)
    echo "${LABEL},${C},${T},${OPS}" >> "$OUT"
    echo "  c=${C} ${T}: ${OPS} ops/sec"
  done
done

echo "wrote $OUT"