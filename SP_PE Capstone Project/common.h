#pragma once
#include <atomic>
#include <cstdint>
#include <fmt/format.h>
#include <chrono>

struct MarketData {
    char instrument[16];
    double bid;
    double ask;
    uint64_t timestamp_ns;
};

template<size_t Size>
struct RingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};
    MarketData buffer[Size];

    void init() {
        write_idx.store(0);
        read_idx.store(0);
    }

    //prioducer tries to wrt data

    bool push(const MarketData& data) {
        size_t w = write_idx.load(std::memory_order_relaxed);
        size_t r = read_idx.load(std::memory_order_acquire);
        if (((w + 1) & (Size - 1)) == r) return false;

        buffer[w] = data;
        write_idx.store((w + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    // consumer tries to read the data
    bool pop(MarketData& data) {
        size_t r = read_idx.load(std::memory_order_relaxed);
        size_t w = write_idx.load(std::memory_order_acquire);
        if (r == w) return false;

        data = buffer[r];
        read_idx.store((r + 1) & (Size - 1), std::memory_order_release);
        return true;
    }
};

inline uint64_t get_nanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}


/*
 * 
 * mem_relax = no sync , just get the current position or indec
 * 
 * mem_acquire , ensure to see ebrything is visible before produder wrote b4 updating
 * 
 * mem_release , all thigs must become visilble b4 this update happens , happens b4 type
 */