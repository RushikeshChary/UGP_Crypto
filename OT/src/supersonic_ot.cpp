#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <thread>
#include <vector>

#include "../../network.hpp"
#include "../include/supersonic_ot.hpp"

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
awaitable<void> run_p2(io_context& io, int iterations) {
    tcp::acceptor acc0(io, tcp::endpoint(tcp::v4(), 9000));
    tcp::acceptor acc1(io, tcp::endpoint(tcp::v4(), 9001));

    tcp::socket sock0 = co_await acc0.async_accept(use_awaitable);
    tcp::socket sock1 = co_await acc1.async_accept(use_awaitable);

    NetContext net(Role::P2);
    net.add_peer(Role::P0, std::move(sock0));
    net.add_peer(Role::P1, std::move(sock1));

    std::cout << "[P2] Connected. Running " << iterations << " iterations...\n";

    SupersonicOT ot(net);
    for (int i = 0; i < iterations; ++i) {
        co_await ot.run_helper();
    }

    std::cout << "[P2] Done\n";
    co_return;
}

// ------------------------------------------------------------
// P0 — Sender
// ------------------------------------------------------------
awaitable<void> run_p0(io_context& io, int iterations) {
    tcp::socket sock_p2 = co_await connect_with_retry(io, "127.0.0.1", 9000);
    tcp::socket sock_p1 = co_await accept_on(io, 9100);

    NetContext net(Role::P0);
    net.add_peer(Role::P2, std::move(sock_p2));
    net.add_peer(Role::P1, std::move(sock_p1));

    std::cout << "[P0] Connected. Running " << iterations << " iterations...\n";

    SupersonicOT ot(net);
    for (int i = 0; i < iterations; ++i) {
        __m128i m0 = random_block();
        __m128i m1 = random_block();

        co_await ot.run_sender(m0, m1);

        // Send actual messages for verification
        co_await net.peer(Role::P1).send(m0);
        co_await net.peer(Role::P1).send(m1);
    }

    std::cout << "[P0] Done\n";
    co_return;
}

// ------------------------------------------------------------
// P1 — Receiver
// ------------------------------------------------------------
awaitable<void> run_p1(io_context& io, int iterations) {
    tcp::socket sock_p2 = co_await connect_with_retry(io, "127.0.0.1", 9001);
    tcp::socket sock_p0 = co_await connect_with_retry(io, "127.0.0.1", 9100);

    NetContext net(Role::P1);
    net.add_peer(Role::P2, std::move(sock_p2));
    net.add_peer(Role::P0, std::move(sock_p0));

    std::cout << "[P1] Connected. Running " << iterations << " iterations...\n";

    SupersonicOT ot(net);
    int passed = 0;
    for (int i = 0; i < iterations; ++i) {
        uint8_t choice = i % 2;
        __m128i out = co_await ot.run_receiver(choice);

        // Receive original messages from P0
        __m128i m0 = co_await net.peer(Role::P0).recv<__m128i>();
        __m128i m1 = co_await net.peer(Role::P0).recv<__m128i>();

        __m128i expected = (choice == 0) ? m0 : m1;

        if (leaf_equal(out, expected)) {
            passed++;
        } else {
            std::cout << "[P1] Iteration " << i << " FAILED! choice=" << (int)choice << "\n";
        }
    }

    if (passed == iterations) {
        std::cout << "[P1] ALL " << iterations << " TESTS PASSED\n";
    } else {
        std::cout << "[P1] " << (iterations - passed) << " / " << iterations << " TESTS FAILED\n";
    }

    std::cout << "[P1] Done\n";
    co_return;
}

// ------------------------------------------------------------
// main()
// ------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./supersonic_ot_test <p0|p1|p2> [iterations]\n";
        return 1;
    }

    io_context io;
    std::string role = argv[1];
    int iterations = (argc >= 3) ? std::stoi(argv[2]) : 20;

    if (role == "p0")
        co_spawn(io, run_p0(io, iterations), detached);
    else if (role == "p1")
        co_spawn(io, run_p1(io, iterations), detached);
    else if (role == "p2")
        co_spawn(io, run_p2(io, iterations), detached);
    else {
        std::cerr << "Invalid role\n";
        return 1;
    }

    std::thread t([&]{ io.run(); });
    io.run();
    t.join();
    return 0;
}
