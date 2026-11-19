#ifndef BENCHMARK_PANEL_H
#define BENCHMARK_PANEL_H

#include "bench/tree_benchmark.h"
#include "imgui.h"
#include "implot.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Замеры идут в фоновом потоке, чтобы окно не зависало; результаты под mutex
class BenchmarkPanel {
    private:
        bench::Params params;
        int max_exponent = 6;

        std::thread worker;
        std::atomic<bool> running{false};
        std::atomic<bool> cancel{false};
        std::mutex mutex;
        std::vector<bench::SizeResult> results;
        std::string status = "Готово к запуску";

        char csv_path[256] = "lr1/benchmarks/results.csv";
        bool shown_operations[bench::OPERATION_COUNT] = {true, true, true, true, true};
        bool show_set = true;
        int plot_mode = 1; // 0 - общее время, 1 - время на элемент

        void set_status(const std::string& text) {
            std::lock_guard<std::mutex> lock(mutex);
            status = text;
        }

        void start() {
            if (running) return;
            if (worker.joinable()) worker.join();

            params.max_size = 1;
            for (int idx = 0; idx < max_exponent; idx++) params.max_size *= 10;
            {
                std::lock_guard<std::mutex> lock(mutex);
                results.clear();
            }
            cancel = false;
            running = true;

            bench::Params run_params = params;
            worker = std::thread([this, run_params] {
                try {
                    for (long long size : bench::make_sizes(run_params)) {
                        if (cancel) break;
                        set_status("Замер n = " + std::to_string(size) + " ...");
                        bench::SizeResult result = bench::measure(size, run_params, &cancel);
                        std::lock_guard<std::mutex> lock(mutex);
                        results.push_back(result);
                    }
                    set_status(cancel ? "Остановлено" : "Готово");
                } catch (const std::exception& error) {
                    set_status(std::string("Ошибка: ") + error.what());
                }
                running = false;
            });
        }

        // Значение для графика: мс или нс на элемент; NaN - точки нет
        double plot_value(const bench::SizeResult& result, double ms) const {
            if (std::isnan(ms)) return NAN;
            return plot_mode == 0 ? ms : ms * 1e6 / static_cast<double>(result.size);
        }

        void plot_series(const std::vector<bench::SizeResult>& data, int operation, bool of_set) {
            std::vector<double> xs;
            std::vector<double> ys;
            for (const bench::SizeResult& result : data) {
                double ms = of_set ? result.set_ms[operation] : result.tree_ms[operation];
                double value = plot_value(result, ms);
                if (std::isnan(value)) continue;
                xs.push_back(static_cast<double>(result.size));
                ys.push_back(value);
            }
            if (xs.empty()) return;

            std::string label = std::string(of_set ? "std::set: " : "AVL: ")
                              + bench::OPERATION_TITLES[operation];
            ImPlotSpec spec;
            spec.Marker = of_set ? ImPlotMarker_Square : ImPlotMarker_Circle;
            spec.LineWeight = of_set ? 1.0f : 2.0f;
            spec.FillAlpha = of_set ? 0.5f : 1.0f;
            ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()),
                             spec);
        }

        void draw_controls() {
            bool locked = running;
            ImGui::BeginDisabled(locked);
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt("max n = 10^k", &max_exponent, 3, 8);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::SliderInt("точек на порядок", &params.steps_per_decade, 1, 3);
            ImGui::SameLine();
            ImGui::Checkbox("сравнить с std::set", &params.compare_with_set);
            ImGui::EndDisabled();
            if (max_exponent >= 8) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                                   "10^8: несколько минут и ~5 ГБ памяти. Удобнее консольный "
                                   "./benchmark --max 1e8, затем «Загрузить CSV».");
            }

            if (!locked && ImGui::Button("Запустить")) start();
            if (locked && ImGui::Button("Остановить")) cancel = true;
            ImGui::SameLine();
            {
                std::lock_guard<std::mutex> lock(mutex);
                ImGui::Text("%s", status.c_str());
            }

            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##csv", csv_path, sizeof(csv_path));
            ImGui::SameLine();
            ImGui::BeginDisabled(locked);
            if (ImGui::Button("Сохранить CSV")) {
                try {
                    std::lock_guard<std::mutex> lock(mutex);
                    bench::save_csv(csv_path, results);
                    status = std::string("Сохранено: ") + csv_path;
                } catch (const std::exception& error) {
                    status = std::string("Ошибка: ") + error.what();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Загрузить CSV")) {
                try {
                    std::vector<bench::SizeResult> loaded = bench::load_csv(csv_path);
                    std::lock_guard<std::mutex> lock(mutex);
                    results = std::move(loaded);
                    status = std::string("Загружено: ") + csv_path;
                } catch (const std::exception& error) {
                    set_status(std::string("Ошибка: ") + error.what());
                }
            }
            ImGui::EndDisabled();

            for (int operation = 0; operation < bench::OPERATION_COUNT; operation++) {
                if (operation > 0) ImGui::SameLine();
                ImGui::Checkbox(bench::OPERATION_TITLES[operation], &shown_operations[operation]);
            }
            ImGui::Checkbox("показывать std::set", &show_set);
            ImGui::SameLine();
            ImGui::RadioButton("общее время, мс", &plot_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("на один элемент, нс", &plot_mode, 1);
        }

        void draw_table(const std::vector<bench::SizeResult>& data) {
            int columns = 2 + bench::OPERATION_COUNT;
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                  | ImGuiTableFlags_ScrollY;
            if (!ImGui::BeginTable("##results", columns, flags)) return;

            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("n");
            for (int operation = 0; operation < bench::OPERATION_COUNT; operation++) {
                ImGui::TableSetupColumn(bench::OPERATION_NAMES[operation]);
            }
            ImGui::TableSetupColumn("память, МБ");
            ImGui::TableHeadersRow();

            for (const bench::SizeResult& result : data) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%lld", result.size);
                for (int operation = 0; operation < bench::OPERATION_COUNT; operation++) {
                    ImGui::TableNextColumn();
                    double tree_ms = result.tree_ms[operation];
                    double set_ms = result.set_ms[operation];
                    if (std::isnan(tree_ms)) {
                        ImGui::TextDisabled("-");
                    } else if (std::isnan(set_ms)) {
                        ImGui::Text("%.2f", tree_ms);
                    } else {
                        ImGui::Text("%.2f (set %.2f)", tree_ms, set_ms);
                    }
                }
                ImGui::TableNextColumn();
                if (std::isnan(result.tree_memory_mb)) {
                    ImGui::TextDisabled("-");
                } else {
                    ImGui::Text("%.1f", result.tree_memory_mb);
                }
            }
            ImGui::EndTable();
        }
    public:
        ~BenchmarkPanel() {
            cancel = true;
            if (worker.joinable()) worker.join();
        }

        void draw() {
            draw_controls();

            std::vector<bench::SizeResult> data;
            {
                std::lock_guard<std::mutex> lock(mutex);
                data = results;
            }

            float table_height = 190.0f * ImGui::GetStyle().FontScaleDpi;
            ImVec2 plot_size(-1, ImGui::GetContentRegionAvail().y - table_height);
            if (ImPlot::BeginPlot("Время работы от числа элементов", plot_size)) {
                ImPlot::SetupAxes("n (элементов)", plot_mode == 0 ? "мс" : "нс на элемент");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                if (plot_mode == 0) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                for (int operation = 0; operation < bench::OPERATION_COUNT; operation++) {
                    if (!shown_operations[operation]) continue;
                    plot_series(data, operation, false);
                    if (show_set) plot_series(data, operation, true);
                }
                ImPlot::EndPlot();
            }
            draw_table(data);
        }
};

#endif
