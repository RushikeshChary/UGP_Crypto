#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#include "../../network.hpp"
#include "../include/ot_extension.hpp"

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
// P2 — Helper (for Base OTs)
// ------------------------------------------------------------
awaitable<void> run_p2(io_context& io, int m) {
    tcp::acceptor acc0(io, tcp::endpoint(tcp::v4(), 9000));
    tcp::acceptor acc1(io, tcp::endpoint(tcp::v4(), 9001));

    tcp::socket sock0 = co_await acc0.async_accept(use_awaitable);
    tcp::socket sock1 = co_await acc1.async_accept(use_awaitable);

    NetContext net(Role::P2);
    net.add_peer(Role::P0, std::move(sock0));
    net.add_peer(Role::P1, std::move(sock1));

    std::cout << "[P2] Connected. Helping with base OTs...\n";

    SupersonicOT ot(net);
    for (int i = 0; i < 128; ++i) {
        co_await ot.run_helper(Role::P1, Role::P0);
    }

    std::cout << "[P2] Done\n";
    co_return;
}

// ------------------------------------------------------------
// P0 — Extension Sender
// ------------------------------------------------------------
awaitable<void> run_p0(io_context& io, int m) {
    tcp::socket sock_p2 = co_await connect_with_retry(io, "127.0.0.1", 9000);
    tcp::socket sock_p1 = co_await accept_on(io, 9100);

    NetContext net(Role::P0);
    net.add_peer(Role::P2, std::move(sock_p2));
    net.add_peer(Role::P1, std::move(sock_p1));

    std::cout << "[P0] Connected. Extending for $m=" << m << "$ OTs...\n";

    OTExtensionSender ote(net);
    co_await ote.extend(m);

    std::cout << "[P0] Extension done. Sending messages...\n";

    std::vector<BlockPair> messages(m);
    for (int i = 0; i < m; ++i) {
        messages[i].first = random_block();
        messages[i].second = random_block();
    }

    co_await ote.send(messages);

    // Write to logs/p0_data.log for verification (Human Readable)
    std::ofstream out("logs/p0_data.log");
    out << m << "\n";
    for (int i = 0; i < m; ++i) {
        out << leaf_to_hex_string(messages[i].first) << " " 
            << leaf_to_hex_string(messages[i].second) << "\n";
    }
    out.close();

    std::cout << "[P0] Done\n";
    co_return;
}

// ------------------------------------------------------------
// P1 — Extension Receiver
// ------------------------------------------------------------
awaitable<void> run_p1(io_context& io, int m) {
    tcp::socket sock_p2 = co_await connect_with_retry(io, "127.0.0.1", 9001);
    tcp::socket sock_p0 = co_await connect_with_retry(io, "127.0.0.1", 9100);

    NetContext net(Role::P1);
    net.add_peer(Role::P2, std::move(sock_p2));
    net.add_peer(Role::P0, std::move(sock_p0));

    std::cout << "[P1] Connected. Extending for $m=" << m << "$ OTs...\n";

    std::vector<uint8_t> choices(m);
    for (int i = 0; i < m; ++i) choices[i] = random_bit();

    OTExtensionReceiver ote(net);
    co_await ote.extend(m, choices);

    std::cout << "[P1] Extension done. Receiving...\n";

    std::vector<__m128i> results = co_await ote.receive();

    // Write to logs/p1_data.log for verification (Human Readable)
    std::ofstream out("logs/p1_data.log");
    out << m << "\n";
    for (int i = 0; i < m; ++i) {
        out << (int)choices[i] << " " << leaf_to_hex_string(results[i]) << "\n";
    }
    out.close();

    std::cout << "[P1] Done\n";
    co_return;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./ot_extension_test <p0|p1|p2> [m]\n";
        return 1;
    }

    io_context io;
    std::string role = argv[1];
    int m = (argc >= 3) ? std::stoi(argv[2]) : 1024;

    if (role == "p0")
        co_spawn(io, run_p0(io, m), detached);
    else if (role == "p1")
        co_spawn(io, run_p1(io, m), detached);
    else if (role == "p2")
        co_spawn(io, run_p2(io, m), detached);
    else {
        std::cerr << "Invalid role\n";
        return 1;
    }

    std::thread t([&]{ io.run(); });
    io.run();
    t.join();
    return 0;
}
