#!/usr/bin/env bash
set -euo pipefail

PORT=${PORT:-6380}
LABEL=${LABEL:-baseline}
EXTRA=${EXTRA:-}                     # e.g. "-P 16"
N=${N:-200000}                       # requests per measured run
CLIENT_THREADS=${CLIENT_THREADS:-8}
KEYSPACE=${KEYSPACE:-1000000}
OUT="bench/results/${LABEL}.csv"
mkdir -p bench/results

echo "label,clients,test,ops_per_sec" > "$OUT"

for C in 1 2 4 8 16 32 64; do
  # can't have more client threads than client connections
  CT=$(( C < CLIENT_THREADS ? C : CLIENT_THREADS ))
  ARGS="-p $PORT -c $C -q --threads $CT -r $KEYSPACE $EXTRA"

  for T in set get; do
    redis-benchmark $ARGS -t "$T" -n $(( N / 10 )) > /dev/null 2>&1   # warmup
    R=$(redis-benchmark $ARGS -t "$T" -n "$N" 2>/dev/null)
    OPS=$(echo "$R" | grep -oP '[\d.]+(?= requests per second)' | tail -1)
    echo "${LABEL},${C},${T},${OPS}" >> "$OUT"
    echo "  c=${C} ${T}: ${OPS} ops/sec"
  done
done

echo "wrote $OUT"