#!/usr/bin/env bash
set -euo pipefail

LABEL=${1:?usage: ./scripts/headline.sh LABEL}
mkdir -p bench/results

for C in 16 64; do
  for i in 1 2 3; do
    OPS=$(redis-benchmark -p 6380 -c "$C" --threads 8 -r 1000000 -P 16 \
            -t set -n 25000000 -q 2>/dev/null \
          | grep -oP '[\d.]+(?= requests per second)')
    echo "$LABEL,c=$C,run=$i,$OPS" | tee -a bench/results/headline.csv
  done
done