#include "pde/codec.hpp"

#include <algorithm>
#include <stdexcept>

#if PDE_HAS_OPENMP
#include <omp.h>
#endif

namespace pde {
namespace {

void validate_code_matrix(const Matrix& matrix) {
    if (matrix.rows() < 2) {
        throw std::invalid_argument("at least two shards are required");
    }
    if (matrix.cols() == 0) {
        throw std::invalid_argument("at least one element is required");
    }
}

std::size_t normalized_threads(std::size_t threads, std::size_t rows) noexcept {
    if (threads == 0) {
        return rows;
    }
    return std::max<std::size_t>(1, std::min(threads, rows));
}

}  // namespace

void validate_config(const CodecConfig& config) {
    if (config.shards < 2) {
        throw std::invalid_argument("shards must be >= 2");
    }
    if (config.elements == 0) {
        throw std::invalid_argument("elements must be > 0");
    }
}

Matrix make_monotonic_source(const CodecConfig& config) {
    validate_config(config);

    Matrix source(config.shards, config.elements);
    Word value = 1;
    for (std::size_t row = 0; row < source.rows(); ++row) {
        for (std::size_t col = 0; col < source.cols(); ++col) {
            source(row, col) = value++;
        }
    }
    return source;
}

Matrix encode(const Matrix& source) {
    validate_code_matrix(source);

    Matrix code(source.rows(), source.cols());
    const std::size_t shards = source.rows();
    const std::size_t elements = source.cols();

    for (std::size_t row = 0; row < shards; ++row) {
        code(row, 0) = source(row, 0);
        for (std::size_t col = 1; col < elements; ++col) {
            Word value = source(row, col);
            for (std::size_t other = 0; other < shards; ++other) {
                if (other != row) {
                    value ^= source(other, col - 1);
                }
            }
            code(row, col) = value;
        }
    }

    return code;
}

void decode_serial(Matrix& code) {
    validate_code_matrix(code);

    const std::size_t shards = code.rows();
    const std::size_t elements = code.cols();

    for (std::size_t col = 0; col + 1 < elements; ++col) {
        for (std::size_t pivot = 0; pivot < shards; ++pivot) {
            const Word pivot_value = code(pivot, col);
            for (std::size_t row = 0; row < shards; ++row) {
                if (row != pivot) {
                    code(row, col + 1) ^= pivot_value;
                }
            }
        }
    }
}

void decode_lockfree_parallel(Matrix& code, std::size_t threads) {
    validate_code_matrix(code);

    const std::size_t shards = code.rows();
    const std::size_t elements = code.cols();
    const std::size_t worker_count = normalized_threads(threads, shards - 1);
    const int omp_threads = static_cast<int>(worker_count);
    const auto row_begin = static_cast<long long>(1);
    const auto row_end = static_cast<long long>(shards);

#if PDE_HAS_OPENMP
#pragma omp parallel for num_threads(omp_threads) schedule(static)
#endif
    for (long long row_id = row_begin; row_id < row_end; ++row_id) {
        const auto row = static_cast<std::size_t>(row_id);
        code(row, 0) ^= code(0, 0);
        for (std::size_t col = 1; col < elements; ++col) {
            code(row, col) ^= code(0, col) ^ code(row, col - 1);
        }
    }

    for (std::size_t col = 1; col < elements; ++col) {
        Word value = code(0, col);
        for (std::size_t row = 1; row < shards; ++row) {
            value ^= code(row, col - 1);
        }
        if ((shards & 1U) == 0U) {
            value ^= code(0, col - 1);
        }
        code(0, col) = value;
    }

#if PDE_HAS_OPENMP
#pragma omp parallel for num_threads(omp_threads) schedule(static)
#endif
    for (long long row_id = row_begin; row_id < row_end; ++row_id) {
        const auto row = static_cast<std::size_t>(row_id);
        for (std::size_t col = 0; col < elements; ++col) {
            code(row, col) ^= code(0, col);
        }
    }
}

void decode_in_place(Matrix& code, Decoder decoder, std::size_t threads) {
    switch (decoder) {
    case Decoder::Serial:
        decode_serial(code);
        break;
    case Decoder::LockFreeParallel:
        decode_lockfree_parallel(code, threads);
        break;
    }
}

bool parse_decoder(std::string_view value, Decoder& decoder) noexcept {
    if (value == "serial") {
        decoder = Decoder::Serial;
        return true;
    }
    if (value == "lockfree" || value == "parallel") {
        decoder = Decoder::LockFreeParallel;
        return true;
    }
    return false;
}

const char* decoder_name(Decoder decoder) noexcept {
    switch (decoder) {
    case Decoder::Serial:
        return "serial";
    case Decoder::LockFreeParallel:
        return "lockfree";
    }
    return "unknown";
}

bool openmp_enabled() noexcept {
#if PDE_HAS_OPENMP
    return true;
#else
    return false;
#endif
}

}  // namespace pde
