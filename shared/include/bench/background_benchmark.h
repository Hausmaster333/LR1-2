#ifndef BACKGROUND_BENCHMARK_H
#define BACKGROUND_BENCHMARK_H

#include "core/array_sequence.h"
#include <chrono>
#include <future>
#include <string>

template<class Future>
bool ready(const Future& future) {
    return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template<class Params, class Result>
class BackgroundBenchmark {
    private:
        ArraySequence<long long> sizes;
        std::future<Result> pending;
        std::promise<void> stop_signal;
        std::shared_future<void> stop_requested;

        void launch_next() {
            long long size = sizes[results.size()];
            status = "Замер n = " + std::to_string(size) + " ...";
            pending = std::async(std::launch::async, [size, settings = params, stop = stop_requested] {
                return measure(size, settings, stop);
            });
        }

    public:
        Params params;
        ArraySequence<Result> results;
        std::string status = "Готово к запуску";

        bool running() const { return pending.valid(); }

        void start() {
            if (running()) return;
            try {
                sizes = make_sizes(params);
                results.clear();
                stop_signal = std::promise<void>();
                stop_requested = stop_signal.get_future().share();
                if (sizes.empty()) status = "Готово";
                else launch_next();
            } catch (const std::exception& error) {
                status = std::string("Ошибка: ") + error.what();
            }
        }

        void stop() {
            if (running() && !ready(stop_requested)) stop_signal.set_value();
        }

        void update() {
            if (!ready(pending)) return;
            try {
                results.push_back(pending.get());
                if (ready(stop_requested)) status = "Остановлено";
                else if (results.size() == sizes.size()) status = "Готово";
                else launch_next();
            } catch (const std::exception& error) {
                status = std::string("Ошибка: ") + error.what();
            }
        }

        ~BackgroundBenchmark() { stop(); }
};

#endif
