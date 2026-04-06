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

int main() {
    std::ifstream in0("p0_data.bin", std::ios::binary);
    std::ifstream in1("p1_data.bin", std::ios::binary);

    if (!in0 || !in1) {
        std::cerr << "Error opening data files (p0_data.bin or p1_data.bin)\n";
        return 1;
    }

    uint64_t m0, m1;
    in0.read((char*)&m0, sizeof(uint64_t));
    in1.read((char*)&m1, sizeof(uint64_t));

    if (m0 != m1) {
        std::cerr << "Size mismatch: m0=" << m0 << ", m1=" << m1 << "\n";
        return 1;
    }

    uint64_t m = m0;
    std::vector<BlockPair> messages(m);
    in0.read((char*)messages.data(), sizeof(BlockPair) * m);

    std::vector<uint8_t> choices(m);
    in1.read((char*)choices.data(), m);

    std::vector<__m128i> results(m);
    in1.read((char*)results.data(), sizeof(__m128i) * m);

    int passed = 0;
    for (uint64_t i = 0; i < m; ++i) {
        __m128i expected = (choices[i] ? messages[i].second : messages[i].first);
        if (leaf_equal(results[i], expected)) {
            passed++;
        } else {
            std::cout << "OT " << i << " FAILED!\n";
        }
    }

    if (passed == m) {
        std::cout << "ALL " << m << " EXTENSION OTs PASSED (Verified from files)\n";
        return 0;
    } else {
        std::cout << (m - passed) << " / " << m << " FAILED\n";
        return 1;
    }
}
