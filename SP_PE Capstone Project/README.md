# HFT_System — Low-Latency Market Data Fanout (SHM + TCP)


# Submission Details :

### Name : Siddharth R

### Roll : 23bcs10076

### Email : siddharth.23bcs10076@sst.scaler.com




## Executive Summary

This project is a **high-performance market data distribution system** designed with **HFT-style constraints** in mind:
- minimal latency
- predictable memory behavior
- lock-free concurrency
- zero-copy IPC where possible

A **single publisher** produces market data and fans it out through:
1. **Shared Memory (SHM)** using a **lock-free ring buffer** for ultra-low latency consumers
2. **TCP** using Boost.Asio for remote / non-co-located consumers

This mirrors real-world exchange → trading engine → analytics pipelines.

---

## Worflow Run 

<video controls src="Screencast from 2026-01-26 02-53-36.mp4" title="Title"></video>

---

## High-Level Architecture

```
                         +-------------------+
                         |   Publisher       |
                         |-------------------|
                         | Market Data Gen   |
                         | Lock-Free Writes  |
                         +---------+---------+
                                   |
                +------------------+------------------+
                |                                     |
        Shared Memory (mmap)                    TCP Socket
        Lock-Free RingBuffer                   Boost.Asio
                |                                     |
      +---------+----------+                +---------+---------+
      | SHM Consumer       |                | TCP Consumer       |
      | (Same Host, NUMA)  |                | (Remote / Local)   |
      +--------------------+                +-------------------+
```

---

## Components Overview

### Publisher
- Creates and initializes shared memory
- Single producer for the ring buffer
- Publishes market data via SHM and TCP

### SHM Consumer
- Attaches to shared memory using mmap
- Lock-free single-consumer design
- CPU affinity pinned for NUMA locality
- Ultra-low latency read path

### TCP Consumer
- Uses Boost.Asio TCP sockets
- Receives serialized JSON messages
- Intended for logging, dashboards, or remote systems

---

## Core Data Structures

### MarketData

```cpp
struct MarketData {
    char instrument[16];
    double bid;
    double ask;
    uint64_t timestamp_ns;
};
```

Fixed-size fields ensure:
- No heap allocations
- Cache predictability
- Shared-memory safety

---

## Lock-Free Ring Buffer Design

### Why Power-of-Two Size?

The ring buffer size must be a power of two to enable efficient wrap-around using bit masking instead of modulo operations.

```cpp
(w + 1) & (Size - 1)
```

This is faster and branch-free.

---

### Ring Buffer Layout

```cpp
template<size_t Size>
struct RingBuffer {
    alignas(64) std::atomic<size_t> write_idx;
    alignas(64) std::atomic<size_t> read_idx;
    MarketData buffer[Size];
};
```

- Cache-line alignment avoids false sharing
- Single-producer / single-consumer guarantees correctness without locks

---

## Memory Ordering Explained

### Producer Push Path

- `memory_order_relaxed` for local index
- `memory_order_acquire` to observe consumer progress
- `memory_order_release` to publish written data

Guarantee:
> Data is fully written before the index update becomes visible.

---

### Consumer Pop Path

- `memory_order_acquire` ensures visibility of producer writes
- `memory_order_release` publishes consumer progress

Guarantee:
> Consumer never sees partially written data.

---

### Mental Model

```
Producer:
[write data] ---> (release) ---> [update index]

Consumer:
[read index] ---> (acquire) ---> [read data]
```

---

## Shared Memory Lifecycle

### Publisher
1. shm_open (create)
2. ftruncate (size allocation)
3. mmap (shared mapping)
4. memset + init

### Consumer
1. shm_open (open existing)
2. fstat (size verification)
3. mmap
4. Attach and consume

Size verification prevents segmentation faults and ABI mismatches.

---

## CPU Affinity & NUMA

Threads are pinned to specific cores using `pthread_setaffinity_np` to:
- Avoid scheduler migration
- Reduce cache misses
- Lower latency variance

---

## TCP Path Design

- Boost.Asio synchronous TCP
- JSON serialization
- Kernel-mediated I/O

This path trades latency for flexibility and reach.

---

## Build System

### CMake Configuration

- C++17
- `-O3` optimization
- `-march=native` for CPU-specific instructions

Targets:
- publisher
- consumer_shm
- consumer_tcp

---

## Build & Run

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run Order

```bash
./publisher
./consumer_shm
./consumer_tcp
```

---

## Performance Characteristics

| Path | Latency | Copies | Kernel |
|----|-------|--------|--------|
| SHM | Nanoseconds | Zero | No |
| TCP | Microseconds | Yes | Yes |

---

## What This Project Demonstrates

- Lock-free concurrency
- Correct atomic memory ordering
- NUMA-aware design
- Shared memory IPC
- Network-based data fanout
- Production-grade safety checks

---

## Possible Extensions

- Multi-consumer SHM
- Sequence numbers and gap detection
- Busy-spin with CPU pause instructions
- Huge pages (MAP_HUGETLB)
- RDMA or DPDK networking
- Exchange-style feed handler architecture

---

## Final Notes

This project reflects how real low-latency trading systems are structured.

Latency is constrained by physics.  
Correctness is constrained by discipline.

This code respects both.
