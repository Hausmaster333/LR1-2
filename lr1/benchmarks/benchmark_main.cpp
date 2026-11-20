// ./benchmark --max 100000000 --steps 1 --out lr1/benchmarks/results.csv
#include "core/array_sequence.h"
#include "bench/tree_benchmark.h"
#include <cstring>
#include <iostream>

namespace {
    void print_usage() {
        std::cout << "Usage: benchmark [--max N] [--steps 1|2|3] [--seed S] [--no-set]\n"
                     "                 [--serialize-limit N] [--out FILE]\n";
    }

    long long parse_count(const char* text) {
        double value = std::stod(text);
        if (value < 1000 || value > 2e9) throw std::invalid_argument("N must be in [1e3, 2e9]");
        return static_cast<long long>(value);
    }
}

int main(int argc, char** argv) {
    Params params;
    std::string out_path = "lr1/benchmarks/results.csv";

    try {
        for (int idx = 1; idx < argc; idx++) {
            bool has_value = idx + 1 < argc;
            if (std::strcmp(argv[idx], "--no-set") == 0) {
                params.compare_with_set = false;
            } else if (std::strcmp(argv[idx], "--max") == 0 && has_value) {
                params.max_size = parse_count(argv[++idx]);
            } else if (std::strcmp(argv[idx], "--steps") == 0 && has_value) {
                params.steps_per_decade = std::stoi(argv[++idx]);
            } else if (std::strcmp(argv[idx], "--seed") == 0 && has_value) {
                params.seed = static_cast<unsigned>(std::stoul(argv[++idx]));
            } else if (std::strcmp(argv[idx], "--serialize-limit") == 0 && has_value) {
                params.serialize_limit = parse_count(argv[++idx]);
            } else if (std::strcmp(argv[idx], "--out") == 0 && has_value) {
                out_path = argv[++idx];
            } else {
                print_usage();
                return 1;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Bad arguments: " << error.what() << "\n";
        print_usage();
        return 1;
    }

    ArraySequence<SizeResult> results;
    std::printf("%12s %12s %12s %12s %12s %12s %10s\n", "n", "insert", "find", "traverse",
                "serialize", "remove", "mem, MB");
    for (long long size : make_sizes(params)) {
        results.push_back(measure(size, params));
        const SizeResult& result = results.back();
        std::printf("%12lld", size);
        for (double value : result.tree_ms) std::printf(" %12.2f", value);
        std::printf(" %10.1f\n", result.tree_memory_mb);
        std::fflush(stdout);
        save_csv(out_path, results);
    }

    std::cout << "Saved: " << out_path << "\n";
    return 0;
}
