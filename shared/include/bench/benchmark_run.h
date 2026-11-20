#ifndef BENCHMARK_RUN_H
#define BENCHMARK_RUN_H

#include "core/array_sequence.h"
#include <exception>
#include <string>

template<class Params, class Result>
class BenchmarkRun {
    private:
        ArraySequence<long long> sizes;

    public:
        Params params;
        ArraySequence<Result> results;
        std::string status = "Готово к запуску";

        bool running() const { return results.size() < sizes.size(); }

        void start() {
            if (running()) return;
            try {
                sizes = make_sizes(params);
                results.clear();
                status = running() ? "Замер n = " + std::to_string(sizes[0]) : "Готово";
            } catch (const std::exception& error) {
                sizes.clear();
                status = std::string("Ошибка: ") + error.what();
            }
        }

        void stop() {
            if (!running()) return;
            sizes.clear();
            status = "Остановлено";
        }

        void update() {
            if (!running()) return;
            try {
                results.push_back(measure(sizes[results.size()], params));
                if (running()) status = "Замер n = " + std::to_string(sizes[results.size()]);
                else {
                    sizes.clear();
                    status = "Готово";
                }
            } catch (const std::exception& error) {
                sizes.clear();
                status = std::string("Ошибка: ") + error.what();
            }
        }
};

#endif
