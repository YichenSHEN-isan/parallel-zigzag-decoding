#pragma once

#include "pde/matrix.hpp"

#include <cstddef>
#include <string_view>

namespace pde {

enum class Decoder {
    Serial,
    LockFreeParallel,
};

struct CodecConfig {
    std::size_t shards = 4;
    std::size_t elements = 1'000'000;
};

void validate_config(const CodecConfig& config);

[[nodiscard]] Matrix make_monotonic_source(const CodecConfig& config);
[[nodiscard]] Matrix encode(const Matrix& source);

void decode_serial(Matrix& code);
void decode_lockfree_parallel(Matrix& code, std::size_t threads);
void decode_in_place(Matrix& code, Decoder decoder, std::size_t threads);

[[nodiscard]] bool parse_decoder(std::string_view value, Decoder& decoder) noexcept;
[[nodiscard]] const char* decoder_name(Decoder decoder) noexcept;
[[nodiscard]] bool openmp_enabled() noexcept;

}  // namespace pde
