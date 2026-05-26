#include "pde/codec.hpp"

#include <iostream>
#include <vector>

namespace {

bool expect_equal(const pde::Matrix& actual, const pde::Matrix& expected, const char* label) {
    if (pde::equal(actual, expected)) {
        return true;
    }

    std::cerr << "FAILED: " << label << '\n';
    return false;
}

bool round_trip(std::size_t shards, std::size_t elements) {
    const pde::CodecConfig config{shards, elements};
    const pde::Matrix source = pde::make_monotonic_source(config);
    const pde::Matrix encoded = pde::encode(source);

    pde::Matrix serial = encoded;
    pde::decode_serial(serial);

    pde::Matrix lockfree = encoded;
    pde::decode_lockfree_parallel(lockfree, shards - 1);

    bool ok = true;
    ok = expect_equal(serial, source, "serial round trip") && ok;
    ok = expect_equal(lockfree, source, "lockfree round trip") && ok;
    ok = expect_equal(lockfree, serial, "decoder equivalence") && ok;
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    const std::vector<std::size_t> lengths{1, 2, 17, 64, 257};

    for (std::size_t shards = 4; shards <= 9; ++shards) {
        for (const auto elements : lengths) {
            ok = round_trip(shards, elements) && ok;
        }
    }

    if (!ok) {
        return 1;
    }

    std::cout << "codec tests passed\n";
    return 0;
}
