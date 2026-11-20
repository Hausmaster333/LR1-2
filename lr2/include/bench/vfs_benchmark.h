#ifndef VFS_BENCHMARK_H
#define VFS_BENCHMARK_H

#include "bench/process_memory.h"
#include "vfs/virtual_file_system.h"
#include <algorithm>
#include "bench/background_benchmark.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include "core/array_sequence.h"

enum Order { ASCENDING, DESCENDING, RANDOM, ORDER_COUNT };

struct SizeResult {
    long long size = 0;
    double build_ms[ORDER_COUNT] = {NAN, NAN, NAN};
    double scan_ms = NAN;
    double index_ms = NAN;
    double index_memory_mb = NAN;
};

struct Params {
    long long max_size = 1000000;
    unsigned seed = 42;
    int scan_queries = 20;
    int index_queries = 10000;
};

inline ArraySequence<long long> make_sizes(const Params& params) {
    ArraySequence<long long> sizes;
    for (long long decade = 1000; decade <= params.max_size; decade *= 10) {
        for (long long multiplier : {1, 2, 5}) {
            if (decade * multiplier <= params.max_size) sizes.push_back(decade * multiplier);
        }
    }
    return sizes;
}

template <class F>
double time_ms(F action) {
    auto start = std::chrono::steady_clock::now();
    action();
    auto finish = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

inline volatile long long sink = 0;

inline void keep(long long value) {
    sink = value;
}

inline std::string file_name(long long number) {
    static const char* extensions[] = {"txt", "log", "pdf", "png", "md"};
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "file_%010lld.%s", number, extensions[number % 5]);
    return buffer;
}

inline SizeResult measure(long long size, const Params& params,
                          const std::shared_future<void>& stop = {}) {
    if (size <= 0 || size > 100000000LL) throw std::invalid_argument("Bad benchmark size");

    int count = static_cast<int>(size);
    std::mt19937 random(params.seed);
    SizeResult result;
    result.size = size;

    ArraySequence<ShrdPtr<File>> files;
    files.reserve(count);
    for (int number = 0; number < count; number++) {
        files.push_back(ShrdPtr<File>::make(file_name(number), UnqPtr<GeneratedSource>::make()));
    }
    ArraySequence<int> order(count);
    for (int kind = 0; kind < ORDER_COUNT; kind++) {
        std::iota(order.begin(), order.end(), 0);
        if (kind == DESCENDING) std::reverse(order.begin(), order.end());
        if (kind == RANDOM) std::shuffle(order.begin(), order.end(), random);

        FileIndex index;
        std::size_t memory_before = get_process_memory();
        result.build_ms[kind] = time_ms([&] { for (int number : order) index.add(files[number]); });
        std::size_t memory_after = get_process_memory();
        if (kind == ASCENDING && memory_before != 0 && memory_after > memory_before) {
            result.index_memory_mb = (memory_after - memory_before) / (1024.0 * 1024.0);
        }
        if (ready(stop)) return result;
    }
    files.clear();

    VirtualFileSystem vfs;
    ArraySequence<std::string> directories;
    for (int top = 0; top < 10; top++) {
        std::string path = "/d" + std::to_string(top);
        vfs.make_directory(path);
        for (int sub = 0; sub < 10; sub++) {
            directories.push_back(path + "/d" + std::to_string(sub));
            vfs.make_directory(directories.back());
        }
    }
    for (int number = 0; number < count; number++) {
        vfs.add_file(directories[number % directories.size()], file_name(number), UnqPtr<GeneratedSource>::make());
    }
    if (ready(stop)) return result;

    std::uniform_int_distribution<int> any_file(0, count - 1);
    auto average_search_ms = [&](SearchMethod method, int queries) {
        long long found = 0;
        double total = time_ms([&] {
            for (int query = 0; query < queries; query++) {
                found += vfs.find_files(SearchField::Name, file_name(any_file(random)), method).get_size();
            }
        });
        keep(found);
        return total / queries;
    };
    result.scan_ms = average_search_ms(SearchMethod::FullScan, params.scan_queries);
    result.index_ms = average_search_ms(SearchMethod::Index, params.index_queries);
    return result;
}

inline std::string format_value(double value) {
    if (std::isnan(value)) return "";
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    return buffer;
}

inline void save_csv(const std::string& path, const ArraySequence<SizeResult>& results) {
    std::ofstream file(path);
    file << "size,build_ascending_ms,build_descending_ms,build_random_ms,scan_ms,index_ms,index_memory_mb\n";
    for (const SizeResult& result : results) {
        file << result.size << ',' << format_value(result.build_ms[ASCENDING]) << ','
             << format_value(result.build_ms[DESCENDING]) << ',' << format_value(result.build_ms[RANDOM]) << ','
             << format_value(result.scan_ms) << ',' << format_value(result.index_ms) << ','
             << format_value(result.index_memory_mb) << '\n';
    }
    if (!file) throw std::runtime_error("Cannot write " + path);
}

inline double parse_cell(const std::string& cell) {
    return cell.empty() ? NAN : std::stod(cell);
}

inline ArraySequence<SizeResult> load_csv(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open " + path);

    ArraySequence<SizeResult> results;
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (std::count(line.begin(), line.end(), ',') != 6) throw std::runtime_error("Need 7 cells: " + line);

        std::istringstream row(line);
        std::string cells[7];
        for (std::string& cell : cells) std::getline(row, cell, ',');

        SizeResult result;
        result.size = std::stoll(cells[0]);
        for (int kind = 0; kind < ORDER_COUNT; kind++) result.build_ms[kind] = parse_cell(cells[1 + kind]);
        result.scan_ms = parse_cell(cells[4]);
        result.index_ms = parse_cell(cells[5]);
        result.index_memory_mb = parse_cell(cells[6]);
        results.push_back(result);
    }
    return results;
}

#endif
