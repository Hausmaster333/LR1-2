#include "core/array_sequence.h"
#include "bench/vfs_benchmark.h"
#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include "bench/benchmark_run.h"

TEST(BenchmarkTest, RunStopsAndRestarts) {
    BenchmarkRun<Params, SizeResult> run;
    run.params.max_size = 2000;
    run.params.scan_queries = 2;
    run.params.index_queries = 10;
    run.start();
    run.update();
    ASSERT_TRUE(run.running());
    run.stop();
    run.stop();
    run.update();
    ASSERT_FALSE(run.running());
    EXPECT_EQ(run.status, "Остановлено");
    ASSERT_EQ(run.results.size(), 1u);
    EXPECT_EQ(run.results[0].size, 1000);

    run.start();
    run.update();
    run.update();
    ASSERT_FALSE(run.running());
    EXPECT_EQ(run.status, "Готово");
    ASSERT_EQ(run.results.size(), 2u);
    EXPECT_EQ(run.results[0].size, 1000);
    EXPECT_EQ(run.results[1].size, 2000);
}

TEST(BenchmarkTest, MeasureFillsAllValues) {
    Params params;
    params.scan_queries = 2;
    params.index_queries = 10;
    SizeResult result = measure(1000, params);

    EXPECT_EQ(result.size, 1000);
    for (double value : result.build_ms) EXPECT_FALSE(std::isnan(value));
    EXPECT_FALSE(std::isnan(result.scan_ms));
    EXPECT_FALSE(std::isnan(result.index_ms));
}

TEST(BenchmarkTest, CsvRoundTripKeepsValuesAndGaps) {
    SizeResult full;
    full.size = 1000;
    full.build_ms[ASCENDING] = 1.5;
    full.build_ms[DESCENDING] = 2.25;
    full.build_ms[RANDOM] = 3.125;
    full.scan_ms = 0.5;
    full.index_ms = 0.001;
    full.index_memory_mb = 12.0;
    SizeResult partial; // Прерванный замер: всё, кроме размера, не измерено
    partial.size = 2000;

    std::string path = (std::filesystem::temp_directory_path() / "vfs_benchmark_test.csv").u8string();
    save_csv(path, {full, partial});
    ArraySequence<SizeResult> loaded = load_csv(path);
    std::filesystem::remove(path);

    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].size, 1000);
    EXPECT_DOUBLE_EQ(loaded[0].build_ms[RANDOM], 3.125);
    EXPECT_DOUBLE_EQ(loaded[0].index_ms, 0.001);
    EXPECT_DOUBLE_EQ(loaded[0].index_memory_mb, 12.0);
    EXPECT_EQ(loaded[1].size, 2000);
    EXPECT_TRUE(std::isnan(loaded[1].scan_ms));
    EXPECT_TRUE(std::isnan(loaded[1].build_ms[ASCENDING]));
}

TEST(BenchmarkTest, SizesGoOneTwoFive) {
    Params params;
    params.max_size = 20000;
    EXPECT_EQ(make_sizes(params), (ArraySequence<long long>{1000, 2000, 5000, 10000, 20000}));
}
