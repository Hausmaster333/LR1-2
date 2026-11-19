// Консольный прогон: поиск обходом против индекса и построение индекса, результаты в CSV.
// Пример: ./benchmark --max 1000000 --out lr2/benchmarks/results.csv
#include "bench/vfs_benchmark.h"
#include <cstring>

namespace {
    void print_usage() {
        std::printf("Usage: benchmark [--max N] [--seed S] [--out FILE]\n");
    }

    long long parse_count(const char* text) {
        double value = std::stod(text); // Разрешаем запись вида 1e6
        if (value < 1000 || value > 1e8) throw std::invalid_argument("N must be in [1e3, 1e8]");
        return static_cast<long long>(value);
    }
}

int main(int argc, char** argv) {
    bench::Params params;
    std::string out_path = "lr2/benchmarks/results.csv";

    try {
        for (int idx = 1; idx < argc; idx++) {
            bool has_value = idx + 1 < argc;
            if (std::strcmp(argv[idx], "--max") == 0 && has_value) {
                params.max_size = parse_count(argv[++idx]);
            } else if (std::strcmp(argv[idx], "--seed") == 0 && has_value) {
                params.seed = static_cast<unsigned>(std::stoul(argv[++idx]));
            } else if (std::strcmp(argv[idx], "--out") == 0 && has_value) {
                out_path = argv[++idx];
            } else {
                print_usage();
                return 1;
            }
        }
    } catch (const std::exception& error) {
        std::printf("Bad arguments: %s\n", error.what());
        print_usage();
        return 1;
    }

    std::vector<bench::SizeResult> results;
    // Построение индекса (asc/desc/random) и один поиск по имени (scan/index) - в мс
    std::printf("%10s | %7s %8s %7s | %12s %12s | %8s\n", "files", "asc", "desc", "random",
                "scan", "index", "mem, MB");
    for (long long size : bench::make_sizes(params)) {
        results.push_back(bench::measure(size, params));
        const bench::SizeResult& result = results.back();
        std::printf("%10lld | %7.1f %8.1f %7.1f | %12.3f %12.5f | %8.1f\n", size,
                    result.build_ms[bench::ASCENDING], result.build_ms[bench::DESCENDING],
                    result.build_ms[bench::RANDOM], result.scan_ms, result.index_ms,
                    result.index_memory_mb);
        std::fflush(stdout);
        bench::save_csv(out_path, results); // Сохраняем по ходу: большие n идут долго
    }

    std::printf("Saved: %s\n", out_path.c_str());
    return 0;
}
