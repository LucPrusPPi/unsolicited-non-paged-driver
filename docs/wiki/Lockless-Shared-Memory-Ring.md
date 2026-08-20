# Lockless Shared Memory Ring & Double Buffering

## Architecture

The `SharedMemoryChannel` provides high-throughput, low-latency communication between user applications and the kernel driver without invoking standard Windows IRP I/O dispatch for every packet.

---

## Memory Layout & Cacheline Alignment

To eliminate multicore False Sharing between producer and consumer threads, ring buffer control indices are isolated into separate 64-byte cache lines using `alignas(64)`:

```
+-----------------------------------------------------------------------+
|  alignas(64) RequestHead  (64 Bytes)   -> Producer Write Boundary     |
+-----------------------------------------------------------------------+
|  alignas(64) RequestTail  (64 Bytes)   -> Consumer Read Boundary      |
+-----------------------------------------------------------------------+
|  alignas(64) ResponseHead (64 Bytes)   -> Producer Response Boundary  |
+-----------------------------------------------------------------------+
|  alignas(64) ResponseTail (64 Bytes)   -> Consumer Read Response      |
+-----------------------------------------------------------------------+
|  Shared Command Ring Buffer (16 x SharedCommand)                       |
+-----------------------------------------------------------------------+
|  Shared Response Ring Buffer (16 x SharedResponse)                    |
+-----------------------------------------------------------------------+
|  Double Data Buffers (Buffer 0 / Buffer 1 — 4KB Each)                 |
+-----------------------------------------------------------------------+
```

---

## Synchronization & Memory Barriers

Ring operations utilize explicit CPU memory ordering primitives:
- **`UnpdLoadFence` (`_mm_lfence`)**: Ensures prior memory reads complete before evaluating ring indices.
- **`UnpdStoreFence` (`_mm_sfence`)**: Flushes store buffers after writing commands or responses.
- **`UnpdFastSwapBarrier` (`mfence`)**: MASM64 full memory fence guaranteeing atomic double-buffer index swaps (`ActiveBufferIndex ^= 1`).

---

## Capacity & Wrap-Around Math

Ring slot indexing uses modulo arithmetic over power-of-two capacities:

$$\text{slot} = \text{tail} \pmod{\text{SHARED\_RING\_CAPACITY}}$$

Full capacity rejection invariant:
$$\text{isFull} = (\text{head} - \text{tail}) \ge \text{SHARED\_RING\_CAPACITY}$$

---

## Latency Profile

Measured over 2,000 iterations (`bench_latency.py`):

| Operation | Min | Mean | P95 | P99 |
|---|---|---|---|---|
| **IOCTL Ping Roundtrip** | 0.30 µs | **0.39 µs** | 0.40 µs | 0.60 µs |
| **Atomic Buffer Swap** | 4.30 µs | **4.44 µs** | 4.60 µs | 5.10 µs |
| **Kernel Shared Map** | 0.40 µs | **0.47 µs** | 0.50 µs | 0.70 µs |
