# Design notes

## Architecture
Client → epoll event loop (1 per core) → RESP parser → command dispatch
→ sharded store + write-ahead log

Each connection is owned by exactly one worker for its entire life
(per-worker epoll instances), so Connection needs no locking. The only
shared state is Store, which locks per shard, and Wal, which is
internally synchronised.

## Decisions and the evidence behind them

### 64 shards, not 256
The store microbenchmark kept improving up to 4096 shards, because it
does no work outside the lock — the worst case for contention. End to
end, 256 shards gave no gain over 64 (2.00M vs 2.27M at c=16). Chose
from the end-to-end measurement; microbenchmarks exaggerate the thing
they isolate.

### Cache-line padding kept
Invisible in the realistic random-key workload (below the ~20%
run-to-run noise). Isolated with a dedicated-shard workload that
removes real contention: 1.5x at 2 threads, non-overlapping ranges
over three runs. Costs 32 bytes per shard; kept.

### Sharding over a lock-free map
Sharding gets most of the benefit at a fraction of the complexity.
Lock-free structures need safe memory reclamation (hazard pointers or
epochs), which is where most implementations go subtly wrong.

### Level-triggered epoll
Edge-triggered needs every socket fully drained per event or it goes
silent. Chose the safer mode; the syscall saving wasn't worth the
failure mode.

### size() is approximate
Locking 64 shards one at a time gives a count that was never
simultaneously true. Locking all 64 gives consistency but stalls the
server. Chose cheap and approximate.

### Commit delay = 0
See notes.md section 8. The delay doubles batch size but stretches
the cycle more; throughput falls 38%.

## Known limitations
- WAL writes block the event loop worker, capping batch size at the
  worker count. Fix is an async WAL with completion callbacks.
- Log grows without bound; no compaction or snapshotting.
- No key expiry, replication, or clustering.
- Five commands (PING/SET/GET/DEL/COMMAND + WALSTATS).
- Benchmarks run client and server on the same 12-CPU host under
  WSL2, so they are bounded by total machine CPU.

## What I'd do differently
- Async WAL from the start — the blocking design undermined group commit.
- A proper test framework rather than assert-based smoke tests.
- Benchmark from a second machine to remove client CPU contention.