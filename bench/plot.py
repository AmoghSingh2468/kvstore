#!/usr/bin/env python3
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = "bench/charts/"

def save(fig, name):
    fig.tight_layout()
    fig.savefig(OUT + name, dpi=140)
    plt.close(fig)
    print("wrote", OUT + name)

# 1 — Sharding, end to end, pipelined
fig, ax = plt.subplots(figsize=(6, 3.6))
clients = [16, 64]
ax.bar([x - 0.2 for x in range(2)], [620717, 762172], 0.4,
       label="1 global lock", color="#b5541f")
ax.bar([x + 0.2 for x in range(2)], [2271901, 2848678], 0.4,
       label="64 shards", color="#4a7c4e")
ax.set_xticks(range(2)); ax.set_xticklabels([f"{c} clients" for c in clients])
ax.set_ylabel("ops/sec"); ax.set_title("Sharding: 3.7x under pipelined load")
ax.legend(); ax.grid(axis="y", alpha=0.3)
save(fig, "01-sharding-e2e.png")

# 2 — Sharding on the store itself
fig, ax = plt.subplots(figsize=(6, 3.6))
threads = [1, 2, 4, 8, 12]
ax.plot(threads, [5362104, 1083919, 1354442, 1101362, 889736],
        "o-", label="1 shard", color="#b5541f")
ax.plot(threads, [4155922, 7549783, 10400677, 16762765, 20819894],
        "o-", label="64 shards", color="#4a7c4e")
ax.set_xlabel("threads"); ax.set_ylabel("ops/sec")
ax.set_title("Store microbenchmark: contention collapse vs scaling")
ax.legend(); ax.grid(alpha=0.3)
save(fig, "02-sharding-micro.png")

# 3 — epoll resource profile at 2000 connections
fig, (a1, a2) = plt.subplots(1, 2, figsize=(7, 3.4))
a1.bar(["thread/conn", "epoll"], [2001, 13], color=["#b5541f", "#4a7c4e"])
a1.set_ylabel("OS threads"); a1.set_title("Threads @ 2000 connections")
a2.bar(["thread/conn", "epoll"], [168, 60], color=["#b5541f", "#4a7c4e"])
a2.set_ylabel("RSS (MB)"); a2.set_title("Memory @ 2000 connections")
save(fig, "03-epoll-resources.png")

# 4 — Durability cost
fig, ax = plt.subplots(figsize=(6, 3.6))
labels = ["no WAL\n(pipelined)", "fsync\nper write", "group commit\ndelay=0"]
vals   = [2848678, 407, 3493]
bars = ax.bar(labels, vals, color=["#8a8378", "#b5541f", "#4a7c4e"])
ax.set_yscale("log"); ax.set_ylabel("ops/sec (log scale)")
ax.set_title("Durability costs 7000x; batching recovers 8.6x of it")
for b, v in zip(bars, vals):
    ax.text(b.get_x() + b.get_width()/2, v, f"{v:,}", ha="center", va="bottom", fontsize=8)
ax.grid(axis="y", alpha=0.3)
save(fig, "04-wal-throughput.png")