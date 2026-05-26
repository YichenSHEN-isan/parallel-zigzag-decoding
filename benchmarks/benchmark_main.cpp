#include "pde/codec.hpp"

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    pde::CodecConfig config;
    std::size_t threads = 0;
    std::size_t iterations = 3;
    bool verify = true;
    bool csv = false;
    bool compare = false;
    pde::Decoder decoder = pde::Decoder::LockFreeParallel;
};

struct Result {
    pde::Decoder decoder;
    std::size_t shards;
    std::size_t elements;
    std::size_t threads;
    std::size_t iterations;
    double avg_ms;
    double min_ms;
    double throughput_gib_s;
    bool verified;
};

std::size_t parse_size(const char* value, const char* option_name) {
    std::string text(value);
    std::size_t consumed = 0;
    const unsigned long long parsed = std::stoull(text, &consumed, 10);
    if (consumed != text.size()) {
        throw std::invalid_argument(std::string(option_name) + " expects an integer");
    }
    if (parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::out_of_range(std::string(option_name) + " is too large");
    }
    return static_cast<std::size_t>(parsed);
}

const char* require_value(int argc, char** argv, int& index) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string(argv[index]) + " requires a value");
    }
    return argv[++index];
}

void print_help(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Options:\n"
        << "  --shards N              Number of data/code shards (default: 4)\n"
        << "  --elements N            Elements per shard (default: 1000000)\n"
        << "  --threads N             Worker threads for lockfree decoder; 0 = shards - 1\n"
        << "  --iterations N          Timed decode iterations (default: 3)\n"
        << "  --decoder NAME          serial | lockfree | parallel | compare\n"
        << "  --csv                   Emit CSV instead of human-readable output\n"
        << "  --no-verify             Skip post-decode correctness check\n"
        << "  --help                  Show this message\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            std::exit(0);
        }
        if (arg == "--shards") {
            options.config.shards = parse_size(require_value(argc, argv, i), "--shards");
        } else if (arg == "--elements") {
            options.config.elements = parse_size(require_value(argc, argv, i), "--elements");
        } else if (arg == "--threads") {
            options.threads = parse_size(require_value(argc, argv, i), "--threads");
        } else if (arg == "--iterations") {
            options.iterations = parse_size(require_value(argc, argv, i), "--iterations");
        } else if (arg == "--decoder") {
            const std::string decoder_name(require_value(argc, argv, i));
            if (decoder_name == "compare") {
                options.compare = true;
            } else if (!pde::parse_decoder(decoder_name, options.decoder)) {
                throw std::invalid_argument("unknown decoder: " + decoder_name);
            }
        } else if (arg == "--csv") {
            options.csv = true;
        } else if (arg == "--no-verify") {
            options.verify = false;
        } else {
            throw std::invalid_argument("unknown option: " + arg);
        }
    }
    if (options.iterations == 0) {
        throw std::invalid_argument("--iterations must be > 0");
    }
    pde::validate_config(options.config);
    return options;
}

Result run_one(
    const pde::Matrix& source,
    const pde::Matrix& encoded,
    pde::Decoder decoder,
    const Options& options) {
    std::vector<double> samples_ms;
    samples_ms.reserve(options.iterations);

    bool verified = true;
    for (std::size_t iteration = 0; iteration < options.iterations; ++iteration) {
        pde::Matrix working = encoded;
        const auto started = std::chrono::steady_clock::now();
        pde::decode_in_place(working, decoder, options.threads);
        const auto finished = std::chrono::steady_clock::now();

        const auto elapsed = std::chrono::duration<double, std::milli>(finished - started).count();
        samples_ms.push_back(elapsed);

        if (options.verify && !pde::equal(working, source)) {
            verified = false;
        }
    }

    const double sum = std::accumulate(samples_ms.begin(), samples_ms.end(), 0.0);
    const double avg_ms = sum / static_cast<double>(samples_ms.size());
    const double min_ms = *std::min_element(samples_ms.begin(), samples_ms.end());
    const double bytes = static_cast<double>(encoded.size() * sizeof(pde::Word));
    const double gib = bytes / (1024.0 * 1024.0 * 1024.0);
    const double throughput = gib / (avg_ms / 1000.0);

    return Result{
        decoder,
        encoded.rows(),
        encoded.cols(),
        options.threads,
        options.iterations,
        avg_ms,
        min_ms,
        throughput,
        verified,
    };
}

void print_csv_header() {
    std::cout
        << "decoder,shards,elements,threads,iterations,avg_ms,min_ms,"
        << "throughput_gib_s,verified,openmp\n";
}

void print_csv(const Result& result) {
    std::cout << pde::decoder_name(result.decoder) << ','
              << result.shards << ','
              << result.elements << ','
              << result.threads << ','
              << result.iterations << ','
              << std::fixed << std::setprecision(3) << result.avg_ms << ','
              << result.min_ms << ','
              << result.throughput_gib_s << ','
              << (result.verified ? "true" : "false") << ','
              << (pde::openmp_enabled() ? "true" : "false") << '\n';
}

void print_human(const Result& result) {
    std::cout << "decoder=" << pde::decoder_name(result.decoder)
              << " shards=" << result.shards
              << " elements=" << result.elements
              << " threads=" << result.threads
              << " iterations=" << result.iterations
              << " openmp=" << (pde::openmp_enabled() ? "on" : "off") << '\n'
              << "avg_ms=" << std::fixed << std::setprecision(3) << result.avg_ms
              << " min_ms=" << result.min_ms
              << " throughput_gib_s=" << result.throughput_gib_s
              << " verified=" << (result.verified ? "true" : "false") << "\n\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const pde::Matrix source = pde::make_monotonic_source(options.config);
        const pde::Matrix encoded = pde::encode(source);

        std::vector<pde::Decoder> decoders;
        if (options.compare) {
            decoders = {pde::Decoder::Serial, pde::Decoder::LockFreeParallel};
        } else {
            decoders = {options.decoder};
        }

        if (options.csv) {
            print_csv_header();
        }

        bool all_verified = true;
        for (const auto decoder : decoders) {
            const Result result = run_one(source, encoded, decoder, options);
            all_verified = all_verified && result.verified;
            if (options.csv) {
                print_csv(result);
            } else {
                print_human(result);
            }
        }

        return all_verified ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        std::cerr << "run with --help for usage\n";
        return 1;
    }
}
