#ifndef TREE_BENCHMARK_H
#define TREE_BENCHMARK_H

#include "bench/process_memory.h"
#include "tree/bst.h"
#include <algorithm>
#include "bench/background_benchmark.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include "core/array_sequence.h"

enum Operation { INSERT, FIND, TRAVERSE, SERIALIZE, REMOVE, OPERATION_COUNT };

inline const char* const OPERATION_NAMES[] = {"insert", "find", "traverse", "serialize", "remove"};
inline const char* const OPERATION_TITLES[] = {
    "Вставка n элементов", "Поиск n элементов", "Обход ЛКП",
    "Строка: запись + чтение", "Удаление n элементов"
};

struct SizeResult {
    long long size = 0;
    double tree_ms[OPERATION_COUNT] = {NAN, NAN, NAN, NAN, NAN};
    double set_ms[OPERATION_COUNT] = {NAN, NAN, NAN, NAN, NAN};
    double tree_memory_mb = NAN;
};

struct Params {
    long long max_size = 1000000;
    int steps_per_decade = 3; // 1: 10^k, 2: 1/3 * 10^k, 3: 1/2/5 * 10^k
    unsigned seed = 42;
    bool compare_with_set = true;
    long long serialize_limit = 10000000;
};

inline ArraySequence<long long> make_sizes(const Params& params) {
    static const double multipliers[3][3] = {{1, 0, 0}, {1, 3, 0}, {1, 2, 5}};
    int steps = std::clamp(params.steps_per_decade, 1, 3);
    ArraySequence<long long> sizes;
    for (long long decade = 1000; decade <= params.max_size; decade *= 10) {
        for (int step = 0; step < steps; step++) {
            long long size = static_cast<long long>(decade * multipliers[steps - 1][step]);
            if (size <= params.max_size) sizes.push_back(size);
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

inline SizeResult measure(long long size, const Params& params, const std::shared_future<void>& stop = {}) {
    if (size <= 0 || size > 2000000000LL) throw std::invalid_argument("Bad benchmark size");

    int count = static_cast<int>(size);
    std::mt19937 random(params.seed);
    ArraySequence<int> keys(count);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), random);
    ArraySequence<int> lookups = keys;
    std::shuffle(lookups.begin(), lookups.end(), random);

    SizeResult result;
    result.size = size;

    {
        BinarySearchTree<int> tree;
        std::size_t memory_before = get_process_memory();
        result.tree_ms[INSERT] = time_ms([&] { for (int key : keys) tree.insert(key); });
        std::size_t memory_after = get_process_memory();
        if (memory_before != 0 && memory_after > memory_before) {
            result.tree_memory_mb = (memory_after - memory_before) / (1024.0 * 1024.0);
        }
        if (ready(stop)) return result;

        result.tree_ms[FIND] = time_ms([&] {
            long long found = 0;
            for (int key : lookups) found += tree.contains(key);
            keep(found);
        });
        result.tree_ms[TRAVERSE] = time_ms([&] {
            long long sum = 0;
            auto iter = tree.get_enumerator();
            while (iter->move_next()) sum += iter->get_current();
            keep(sum);
        });
        if (ready(stop)) return result;

        if (size <= params.serialize_limit) {
            result.tree_ms[SERIALIZE] = time_ms([&] {
                std::string text = tree.to_string();
                keep(BinarySearchTree<int>::from_string(text).get_count());
            });
        }
        if (ready(stop)) return result;

        result.tree_ms[REMOVE] = time_ms([&] { for (int key : lookups) tree.remove(key); });
    }

    if (!params.compare_with_set || ready(stop)) return result;

    std::set<int> reference;
    result.set_ms[INSERT] = time_ms([&] { for (int key : keys) reference.insert(key); });
    result.set_ms[FIND] = time_ms([&] {
        long long found = 0;
        for (int key : lookups) found += reference.count(key);
        keep(found);
    });
    result.set_ms[TRAVERSE] = time_ms([&] {
        long long sum = 0;
        for (int key : reference) sum += key;
        keep(sum);
    });
    result.set_ms[REMOVE] = time_ms([&] { for (int key : lookups) reference.erase(key); });
    return result;
}

inline std::string format_value(double value) {
    if (std::isnan(value)) return "";
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.4f", value);
    return buffer;
}

inline void save_csv(const std::string& path, const ArraySequence<SizeResult>& results) {
    std::ofstream file(path);
    file << "size,operation,tree_ms,set_ms,tree_memory_mb\n";
    for (const SizeResult& result : results) {
        for (int operation = 0; operation < OPERATION_COUNT; operation++) {
            file << result.size << ',' << OPERATION_NAMES[operation] << ','
                 << format_value(result.tree_ms[operation]) << ','
                 << format_value(result.set_ms[operation]) << ','
                 << format_value(result.tree_memory_mb) << '\n';
        }
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
        std::istringstream row(line);
        std::string cells[5];
        for (std::string& cell : cells) std::getline(row, cell, ',');

        int operation = -1;
        for (int candidate = 0; candidate < OPERATION_COUNT; candidate++) {
            if (cells[1] == OPERATION_NAMES[candidate]) operation = candidate;
        }
        if (operation < 0) throw std::runtime_error("Unknown operation: " + cells[1]);

        long long size = std::stoll(cells[0]);
        if (results.empty() || results.back().size != size) {
            results.push_back({});
            results.back().size = size;
        }
        results.back().tree_ms[operation] = parse_cell(cells[2]);
        results.back().set_ms[operation] = parse_cell(cells[3]);
        results.back().tree_memory_mb = parse_cell(cells[4]);
    }
    return results;
}

#endif
