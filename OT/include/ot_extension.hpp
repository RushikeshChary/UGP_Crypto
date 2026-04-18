#pragma once

#include "supersonic_ot.hpp"
#include "../../prg.hpp"
#include <vector>
#include <array>
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;

struct BlockPair {
    __m128i first;
    __m128i second;
};

// Let's use a robust bit-matrix transpose for m x 128.
// Columns are vectors of uint64_t.
inline void transpose_m_by_128(std::vector<__m128i>& rows, const std::vector<std::vector<uint64_t>>& cols, int m) {
    rows.assign(m, _mm_setzero_si128());
    for (int i = 0; i < 128; ++i) {
        for (int j = 0; j < m; ++j) {
            uint64_t bit = (cols[i][j / 64] >> (j % 64)) & 1;
            if (bit) {
                uint64_t* row_ptr = (uint64_t*)&rows[j];
                row_ptr[i / 64] |= (1ULL << (i % 64));
            }
        }
    }
}

// Correlation-robust hash H(j, x) = AES_k(x ^ j) ^ (x ^ j)
inline __m128i cr_hash(int j, __m128i x, const AES_KEY& key) {
    __m128i tweak = _mm_set_epi64x(0, j);
    __m128i input = _mm_xor_si128(x, tweak);
    __m128i output = input;
    AES_ecb_encrypt_blks(&output, 1, &key);
    return _mm_xor_si128(output, input);
}

class OTExtensionSender {
public:
    OTExtensionSender(NetContext& net) : net(net), base_ot(net) {}

    awaitable<void> extend(int m) {
        this->m = m;
        int num_blocks = (m + 127) / 128;
        
        // 1. Base OT phase: Sender acts as Receiver
        s = random_block();
        alignas(16) uint8_t s_bits[16];
        _mm_storeu_si128((__m128i*)s_bits, s);

        std::vector<awaitable<__m128i>> base_ot_tasks;
        for (int i = 0; i < 128; ++i) {
            uint8_t choice = (s_bits[i / 8] >> (i % 8)) & 1;
            base_ot_tasks.push_back(base_ot.run_receiver(choice, Role::P1, Role::P2));
        }

        // Run all base OTs
        std::vector<__m128i> qs(128);
        for (int i = 0; i < 128; ++i) {
            qs[i] = co_await std::move(base_ot_tasks[i]);
        }

        // 2. Extension phase
        std::vector<std::vector<uint64_t>> T_cols(128, std::vector<uint64_t>(num_blocks * 2));
        AES_KEY prg_fixed_key(_mm_set_epi64x(0x12345678, 0x9abcdef0));

        for (int i = 0; i < 128; ++i) {
            // Receive u_i from Receiver
            auto u_i = co_await net.peer(Role::P1).recv_vector<uint64_t>();
            
            // Expand q_i
            std::vector<uint64_t> G_qi(num_blocks * 2);
            crypto::fill_vector_with_prg(G_qi, prg_fixed_key, qs[i]);

            uint8_t si = (s_bits[i / 8] >> (i % 8)) & 1;
            for (size_t j = 0; j < G_qi.size(); ++j) {
                T_cols[i][j] = (si ? (u_i[j] ^ G_qi[j]) : G_qi[j]);
            }
        }

        // Transpose T to get rows
        transpose_m_by_128(T_rows, T_cols, m);

        co_return;
    }

    awaitable<void> send(const std::vector<BlockPair>& messages) {
        if (messages.size() != (size_t)m) throw std::runtime_error("Message size mismatch");

        AES_KEY hash_key(_mm_set_epi64x(0xdeadbeef, 0xfacefeed));
        std::vector<BlockPair> ciphertexts(m);

        for (int j = 0; j < m; ++j) {
            __m128i h0 = cr_hash(j, T_rows[j], hash_key);
            __m128i h1 = cr_hash(j, _mm_xor_si128(T_rows[j], s), hash_key);

            ciphertexts[j].first = _mm_xor_si128(messages[j].first, h0);
            ciphertexts[j].second = _mm_xor_si128(messages[j].second, h1);
        }

        // Send all ciphertexts at once
        uint64_t sz = m;
        co_await net.peer(Role::P1).send(sz);
        co_await async_write(net.peer(Role::P1).sock, buffer(ciphertexts.data(), sizeof(BlockPair) * m), use_awaitable);

        co_return;
    }

private:
    NetContext& net;
    SupersonicOT base_ot;
    int m;
    __m128i s;
    std::vector<__m128i> T_rows;
};

class OTExtensionReceiver {
public:
    OTExtensionReceiver(NetContext& net) : net(net), base_ot(net) {}

    awaitable<void> extend(int m, const std::vector<uint8_t>& choices) {
        this->m = m;
        this->choices = choices;
        int num_blocks = (m + 127) / 128;

        // 1. Base OT phase: Receiver acts as Sender
        std::vector<BlockPair> seeds(128);
        for (int i = 0; i < 128; ++i) {
            seeds[i].first = random_block();
            seeds[i].second = random_block();
        }

        std::vector<awaitable<void>> base_ot_tasks;
        for (int i = 0; i < 128; ++i) {
            base_ot_tasks.push_back(base_ot.run_sender(seeds[i].first, seeds[i].second, Role::P0, Role::P2));
        }

        // Run all base OTs
        for (int i = 0; i < 128; ++i) {
            co_await std::move(base_ot_tasks[i]);
        }

        // 2. Extension phase
        AES_KEY prg_fixed_key(_mm_set_epi64x(0x12345678, 0x9abcdef0));
        std::vector<std::vector<uint64_t>> V_cols(128, std::vector<uint64_t>(num_blocks * 2));

        // Prepare r vector as uint64_t for XORing
        std::vector<uint64_t> r_vec(num_blocks * 2, 0);
        for (int j = 0; j < m; ++j) {
            if (choices[j]) {
                r_vec[j / 64] |= (1ULL << (j % 64));
            }
        }

        for (int i = 0; i < 128; ++i) {
            std::vector<uint64_t> G_k0(num_blocks * 2);
            std::vector<uint64_t> G_k1(num_blocks * 2);
            crypto::fill_vector_with_prg(G_k0, prg_fixed_key, seeds[i].first);
            crypto::fill_vector_with_prg(G_k1, prg_fixed_key, seeds[i].second);

            V_cols[i] = G_k0; // V column is G(k0)

            std::vector<uint64_t> u_i(num_blocks * 2);
            for (size_t j = 0; j < u_i.size(); ++j) {
                u_i[j] = G_k0[j] ^ G_k1[j] ^ r_vec[j];
            }

            // Send u_i to Sender
            co_await net.peer(Role::P0).send(u_i);
        }

        // Transpose V to get rows
        transpose_m_by_128(V_rows, V_cols, m);

        co_return;
    }

    awaitable<std::vector<__m128i>> receive() {
        // Receive ciphertexts from Sender
        uint64_t sz = co_await net.peer(Role::P0).recv<uint64_t>();
        if (sz != (uint64_t)m) throw std::runtime_error("Size mismatch in receive");

        std::vector<BlockPair> ciphertexts(m);
        co_await async_read(net.peer(Role::P0).sock, buffer(ciphertexts.data(), sizeof(BlockPair) * m), use_awaitable);

        AES_KEY hash_key(_mm_set_epi64x(0xdeadbeef, 0xfacefeed));
        std::vector<__m128i> results(m);

        for (int j = 0; j < m; ++j) {
            __m128i hj = cr_hash(j, V_rows[j], hash_key);
            __m128i ciphertext = (choices[j] ? ciphertexts[j].second : ciphertexts[j].first);
            results[j] = _mm_xor_si128(ciphertext, hj);
        }

        co_return results;
    }

private:
    NetContext& net;
    SupersonicOT base_ot;
    int m;
    std::vector<uint8_t> choices;
    std::vector<__m128i> V_rows;
};
