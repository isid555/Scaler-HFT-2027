#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <pthread.h>
#include "common.h"


void set_cpu_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main() {
    const char* shm_name = "/hft_ring_buffer";
    set_cpu_affinity(1); // NUMA
    // 1. Open SHM
    int fd = ::shm_open(shm_name, O_RDWR, 0666);
    if (fd == -1) {
        fmt::print(stderr, "[-] SHM open failed. Is publisher running?\n");
        return 1;
    }

    // 2. Verify Size (Crucial to prevent SegFault)
    struct stat s;
    if (fstat(fd, &s) == -1 || s.st_size < sizeof(RingBuffer<1024>)) {
        fmt::print(stderr, "[-] Invalid SHM size: {} bytes\n", s.st_size);
        return 1;
    }

    // 3. Map
    void* ptr = mmap(nullptr, sizeof(RingBuffer<1024>), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    auto* rb = static_cast<RingBuffer<1024>*>(ptr);
    MarketData data;
    fmt::print("[+] SHM Consumer connected to 0x{:x}\n", reinterpret_cast<uintptr_t>(ptr));

    while (true) {
        if (rb->pop(data)) {
            fmt::print("[SHM] {:<10} | B: {:.2f} | A: {:.2f} | TS: {}\n", 
                       data.instrument, data.bid, data.ask, data.timestamp_ns);
        } else {
            // "Busy wait" hint for CPU
            std::this_thread::yield(); 
        }
    }
    return 0;
}