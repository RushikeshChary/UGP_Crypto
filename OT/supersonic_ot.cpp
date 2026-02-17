#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <thread>

#include "../network.hpp"
#include "supersonic_ot.hpp"

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::asio::io_context;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
awaitable<tcp::socket> accept_on(io_context& io, uint16_t port) {
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), port));
    tcp::socket sock = co_await acc.async_accept(use_awaitable);
    co_return sock;
}

// ------------------------------------------------------------
// P2 — Helper
// ------------------------------------------------------------
awaitable<void> run_p2(io_context& io) {
    std::cout << "[P2] Waiting for P0(:9000) and P1(:9001)...\n";

    tcp::acceptor acc0(io, tcp::endpoint(tcp::v4(), 9000));
    tcp::acceptor acc1(io, tcp::endpoint(tcp::v4(), 9001));

    tcp::socket sock0 = co_await acc0.async_accept(use_awaitable);
    tcp::socket sock1 = co_await acc1.async_accept(use_awaitable);

    NetContext net(Role::P2);
    net.add_peer(Role::P0, std::move(sock0));
    net.add_peer(Role::P1, std::move(sock1));

    std::cout << "[P2] Connected to P0 and P1\n";

    SupersonicOT ot(net);
    co_await ot.run_helper();

    std::cout << "[P2] Supersonic OT done\n";
    co_return;
}

// ------------------------------------------------------------
// P0 — Sender
// ------------------------------------------------------------
awaitable<void> run_p0(io_context& io) {
    tcp::socket sock_p2 = co_await connect_with_retry(io, "127.0.0.1", 9000);
    tcp::socket sock_p1 = co_await accept_on(io, 9100);

    NetContext net(Role::P0);
    net.add_peer(Role::P2, std::move(sock_p2));
    net.add_peer(Role::P1, std::move(sock_p1));

    std::cout << "[P0] Connected to P2 and P1\n";

    // Sender messages
    __m128i m0 = random_block();
    __m128i m1 = random_block();

    std::cout << "[P0] m0 = " << leaf_to_hex_string(m0) << "\n";
    std::cout << "[P0] m1 = " << leaf_to_hex_string(m1) << "\n";

    SupersonicOT ot(net);
    co_await ot.run_sender(m0, m1);

    std::cout << "[P0] Supersonic OT done\n";
    co_return;
}

// ------------------------------------------------------------
// P1 — Receiver
// ------------------------------------------------------------
awaitable<void> run_p1(io_context& io) {
    tcp::socket sock_p2 = co_await connect_with_retry(io, "127.0.0.1", 9001);
    tcp::socket sock_p0 = co_await connect_with_retry(io, "127.0.0.1", 9100);

    NetContext net(Role::P1);
    net.add_peer(Role::P2, std::move(sock_p2));
    net.add_peer(Role::P0, std::move(sock_p0));

    std::cout << "[P1] Connected to P2 and P0\n";

    uint8_t choice = 1; // try 0 and 1

    SupersonicOT ot(net);
    __m128i out = co_await ot.run_receiver(choice);

    std::cout << "[P1] choice = " << int(choice) << "\n";
    std::cout << "[P1] output = " << leaf_to_hex_string(out) << "\n";

    std::cout << "[P1] Supersonic OT done\n";
    co_return;
}

// ------------------------------------------------------------
// main()
// ------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./supersonic_ot_test <p0|p1|p2>\n";
        return 1;
    }

    io_context io;
    std::string role = argv[1];

    if (role == "p0")
    // Sender
        co_spawn(io, run_p0(io), detached);
    else if (role == "p1")
    // Receiver
        co_spawn(io, run_p1(io), detached);
    else if (role == "p2")
    // Helper
        co_spawn(io, run_p2(io), detached);
    else {
        std::cerr << "Invalid role\n";
        return 1;
    }

    std::thread t([&]{ io.run(); });
    io.run();
    t.join();
    return 0;
}
