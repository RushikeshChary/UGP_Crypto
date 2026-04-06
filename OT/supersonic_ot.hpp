#pragma once
#include "../network.hpp"
#include "../types.hpp"
#include <random>

class SupersonicOT {
public:
    SupersonicOT(NetContext& net);

    awaitable<void> run_sender(__m128i m0, __m128i m1, Role receiver_role = Role::P1, Role helper_role = Role::P2);
    awaitable<__m128i> run_receiver(uint8_t choice, Role sender_role = Role::P0, Role helper_role = Role::P2);
    awaitable<void> run_helper(Role sender_role = Role::P0, Role receiver_role = Role::P1);

private:
    NetContext& net;
};


inline uint8_t random_bit() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    return rng() & 1;
}

inline __m128i random_block() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    uint64_t lo = rng();
    uint64_t hi = rng();
    return _mm_set_epi64x(hi, lo);
}

inline void controlled_swap(uint8_t b, __m128i& x, __m128i& y) {
    if (b) std::swap(x, y);
}


SupersonicOT::SupersonicOT(NetContext& n) : net(n) {}

awaitable<void> SupersonicOT::run_sender(__m128i m0, __m128i m1, Role receiver_role, Role helper_role) {
    // Phase 1: receive k0, k1 from Receiver
    auto k0 = co_await net.peer(receiver_role).recv<__m128i>();
    auto k1 = co_await net.peer(receiver_role).recv<__m128i>();

    // Phase 2: receive s1
    uint8_t s1 = co_await net.peer(receiver_role).recv<uint8_t>();

    // Phase 3: encrypt
    __m128i e0 = leaf_xor(m0, k0);
    __m128i e1 = leaf_xor(m1, k1);

    controlled_swap(s1, e0, e1);

    // send to Helper
    co_await net.peer(helper_role).send(e0);
    co_await net.peer(helper_role).send(e1);

    co_return;
}


awaitable<void> SupersonicOT::run_helper(Role sender_role, Role receiver_role) {
    // receive s2 from Receiver
    uint8_t s2 = co_await net.peer(receiver_role).recv<uint8_t>();

    // receive ciphertexts from Sender
    auto e0 = co_await net.peer(sender_role).recv<__m128i>();
    auto e1 = co_await net.peer(sender_role).recv<__m128i>();

    controlled_swap(s2, e0, e1);

    // forward only one ciphertext
    co_await net.peer(receiver_role).send(e0);

    co_return;
}


awaitable<__m128i> SupersonicOT::run_receiver(uint8_t s, Role sender_role, Role helper_role) {
    // Phase 1: setup
    __m128i k0 = random_block();
    __m128i k1 = random_block();

    co_await net.peer(sender_role).send(k0);
    co_await net.peer(sender_role).send(k1);

    // Phase 2: split choice bit
    uint8_t s1 = random_bit();
    uint8_t s2 = s ^ s1;

    co_await net.peer(sender_role).send(s1);
    co_await net.peer(helper_role).send(s2);

    // Phase 4: receive filtered ciphertext
    auto c = co_await net.peer(helper_role).recv<__m128i>();

    // Phase 5: decrypt
    __m128i ks = (s == 0 ? k0 : k1);
    __m128i ms = leaf_xor(c, ks);

    co_return ms;
}


awaitable<void> party_main(Role role, NetContext& net) {
    SupersonicOT ot(net);

    if (role == Role::P0) {
        __m128i m0 = random_block();
        __m128i m1 = random_block();
        co_await ot.run_sender(m0, m1);
    }

    if (role == Role::P1) {
        uint8_t choice = 1; // try both 0 and 1
        __m128i out = co_await ot.run_receiver(choice);
        std::cout << "[Receiver] output = "
                  << leaf_to_hex_string(out) << std::endl;
    }

    if (role == Role::P2) {
        co_await ot.run_helper();
    }

    co_return;
}

