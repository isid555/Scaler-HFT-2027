#include <boost/asio.hpp>
#include "common.h"

using namespace boost::asio;
using ip::tcp;

int main() {
    io_context ioc;
    tcp::socket socket(ioc);
    socket.connect(tcp::endpoint(ip::address::from_string("127.0.0.1"), 9001));

    streambuf b;
    fmt::print("TCP Consumer started...\n");

    while (true) {
        read_until(socket, b, "\n");
        std::istream is(&b);
        std::string line;
        std::getline(is, line);
        fmt::print("[TCP] Received: {}\n", line);
    }
    return 0;
}