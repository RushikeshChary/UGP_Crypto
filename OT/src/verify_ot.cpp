#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <immintrin.h>
#include <cstring>

struct BlockPair {
    __m128i first;
    __m128i second;
};

bool leaf_equal(const __m128i &a, const __m128i &b) {
    return std::memcmp(&a, &b, sizeof(__m128i)) == 0;
}

__m128i hex_to_m128i(const std::string& hex) {
    if (hex.length() != 32) throw std::runtime_error("Invalid hex length: " + std::to_string(hex.length()));
    alignas(16) uint8_t bytes[16];
    for (int i = 0; i < 16; ++i) {
        std::string byteString = hex.substr(i * 2, 2);
        bytes[i] = (uint8_t)std::strtol(byteString.c_str(), nullptr, 16);
    }
    return _mm_loadu_si128((__m128i*)bytes);
}

int main() {
    std::ifstream in0("logs/p0_data.log");
    std::ifstream in1("logs/p1_data.log");

    if (!in0 || !in1) {
        std::cerr << "Error opening data files (logs/p0_data.log or logs/p1_data.log)\n";
        return 1;
    }

    uint64_t m0, m1;
    if (!(in0 >> m0) || !(in1 >> m1)) {
        std::cerr << "Error reading number of OTs\n";
        return 1;
    }

    if (m0 != m1) {
        std::cerr << "Size mismatch: m0=" << m0 << ", m1=" << m1 << "\n";
        return 1;
    }

    uint64_t m = m0;
    std::vector<BlockPair> messages(m);
    std::vector<uint8_t> choices(m);
    std::vector<__m128i> results(m);

    for (uint64_t i = 0; i < m; ++i) {
        std::string h1, h2;
        if (!(in0 >> h1 >> h2)) break;
        messages[i].first = hex_to_m128i(h1);
        messages[i].second = hex_to_m128i(h2);
    }

    int passed = 0;
    for (uint64_t i = 0; i < m; ++i) {
        int c;
        std::string h;
        if (!(in1 >> c >> h)) break;
        choices[i] = (uint8_t)c;
        results[i] = hex_to_m128i(h);

        __m128i expected = (choices[i] ? messages[i].second : messages[i].first);
        if (leaf_equal(results[i], expected)) {
            passed++;
        } else {
            std::cout << "OT " << i << " FAILED!\n";
        }
    }

    if (passed == (int)m) {
        std::cout << "ALL " << m << " EXTENSION OTs PASSED (Verified from human-readable logs)\n";
        return 0;
    } else {
        std::cout << (m - passed) << " / " << m << " FAILED\n";
        return 1;
    }
}
