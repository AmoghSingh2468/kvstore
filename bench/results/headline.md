# Headline result — sharding

Pipelined SET (-P 16), random keys (-r 1000000), 8 client threads.
25,000,000 requests per run, median of 3.

| clients | 1 global lock | 64 shards | speedup |
|---------|--------------:|----------:|--------:|
| 16      |       620,717 | 2,271,901 |    3.7x |
| 64      |       762,172 | 2,848,678 |    3.7x |

Unpipelined: no significant difference. Per-request syscall and
network cost dominates, so the lock is rarely contended.

Environment: see environment.txt. Client and server on the same
12-core host under WSL2, sharing CPU.