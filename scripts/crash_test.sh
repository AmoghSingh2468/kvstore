#!/usr/bin/env bash
set -euo pipefail

pkill -9 -x kvserver 2>/dev/null || true; sleep 1
rm -f kvstore.wal

./build-rel/kvserver > /dev/null 2>&1 & sleep 1
redis-cli -p 6380 SET k1 alpha > /dev/null
redis-cli -p 6380 SET k2 beta  > /dev/null
redis-cli -p 6380 DEL k1       > /dev/null

kill -9 "$(pgrep -x kvserver)"; sleep 1
./build-rel/kvserver > /dev/null 2>&1 & sleep 1

V1=$(redis-cli -p 6380 GET k1)
V2=$(redis-cli -p 6380 GET k2)
pkill -9 -x kvserver

[ -z "$V1" ] && [ "$V2" = "beta" ] \
  && echo "PASS: k1 deleted, k2 recovered" \
  || { echo "FAIL: k1='$V1' k2='$V2'"; exit 1; }
