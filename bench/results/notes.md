# Day 3 findings

## 1. The first benchmark measured the client, not the server
Default redis-benchmark: 45.9k ops/sec, flat across client counts.
--threads 4: 117.6k. --threads 8 -r 1000000: ~240k. No server changes.
Causes: single-threaded benchmark client; default benchmark uses one key,
so every request hit the same shard.

## 2. Per-request overhead dominates unpipelined requests
Pipelining 16 commands per round trip: ~240k -> ~2.66M ops/sec (11x).
Roughly 90% of an unpipelined request is syscalls, loopback networking
and thread scheduling, not the command itself.

## 3. Sharding: 23x on the data structure, 3.7x end to end
Store microbenchmark (no network), 12 threads, random keys:
  1 shard  (global lock): 0.89M ops/sec
  64 shards:             20.8M ops/sec  -> 23x
With a global lock, 2 threads were 5x slower than 1 (contention collapse).
End to end, pipelined, 64 clients, median of 3:
  1 lock: 0.76M   64 shards: 2.85M      -> 3.7x
Unpipelined: no significant difference.
The gap between 23x and 3.7x is Amdahl's law: the store is only part
of each request's cost.

## 4. False sharing: invisible in practice, clear in isolation
Random keys: padded vs unpadded within the ~20% run-to-run noise.
Isolation test (each thread owns one shard, no real contention):
  2 threads: 29.4M -> 44.7M (1.52x), non-overlapping ranges
  4 threads: 50.9M -> 70.6M (1.39x)
  8 threads: 81.5M -> 98.1M (1.20x)
  1 thread (control): no difference, as expected.
Kept padding: 32 extra bytes per shard, never measurably hurt.

## 5. Shard count: chosen end to end, not from the microbenchmark
Microbenchmark kept improving up to 4096 shards (zero work outside
the lock = worst-case contention).
End to end, 256 shards: 2.00M (c=16), 2.63M (c=64) — no gain over 64.
Kept 64.

## Measurement lessons
- redis-benchmark --threads measures time in ~250 ms steps; short runs
  can be off by up to 25%. Use runs of 10+ seconds.
- Report medians of 3+ runs with ranges.
- 12 threads on 12 CPUs is noisy: nothing is left for the OS.

## 6. epoll: no throughput gain at 64 connections, decisive at 2000

At 64 connections, epoll vs thread-per-connection was within noise
(pipelined: 2.27M vs 2.17M at c=16; ranges overlap). Expected —
thread-per-connection is fine at that scale.

At 2000 connections, measured under sustained load:

| model               | threads | RSS    | ops/sec |
|---------------------|--------:|-------:|--------:|
| thread-per-conn     |    2001 | 168 MB |  ~98k   |
| epoll, 12 loops     |      13 |  60 MB | ~130k   |

154x fewer threads, 2.8x less memory, 1.3x throughput. The throughput
gain only appears once scheduling 2000 runnable threads across 12 CPUs
becomes a real cost. Thread count is constant regardless of connections,
so memory and scheduling stop scaling with load.

## 7. Durability costs 7000x without batching

WAL with one fsync per write, 32 clients:
  407 ops/sec, p50 = 60 ms
Same workload without a WAL:
  ~2,849,000 ops/sec (pipelined), ~266,000 (unpipelined)

2.46 ms per operation. append_and_sync holds a mutex across
write()+fsync(), so all writers serialise behind one disk round trip.
On WSL2 an fsync crosses a VHD image and NTFS, so it is slower than
a bare-metal SSD (~1 ms) — the group commit gain here is therefore
larger than it would be on native Linux.