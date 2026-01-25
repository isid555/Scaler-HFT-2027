#include <iostream>
#include <boost/asio.hpp>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

using namespace boost::asio;
using ip::tcp;

int main() {
    // 1. Setup Shared Memory
    int fd = ::shm_open("/hft_ring_buffer", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(RingBuffer<1024>));
    auto* rb = (RingBuffer<1024>*)mmap(nullptr, sizeof(RingBuffer<1024>), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    memset(rb, 0, sizeof(RingBuffer<1024>)); // THIS PREVENTS GARBAGE INDEX CRASHES
    rb->init();

    // 2. Setup TCP Server
    io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 9001));
    fmt::print("Publisher waiting for TCP consumer...\n");
    tcp::socket socket = acceptor.accept();
    socket.set_option(tcp::no_delay(true));

    while (true) {
        MarketData data{"RELIANCE", 2850.25, 2850.75, get_nanos()};

        // Push to SHM
        rb->push(data);

        // Push to TCP as JSON
        std::string msg = fmt::format(
            "{{\"instrument\": \"{}\", \"bid\": {:.2f}, \"ask\": {:.2f}, \"timestamp_ns\": {}}}\n",
            data.instrument, data.bid, data.ask, data.timestamp_ns);
        
        write(socket, buffer(msg));
        
        usleep(100000); // 100ms throttle for simulation
    }
    return 0;
}