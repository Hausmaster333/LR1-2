#ifndef BENCHMARK_PANEL_H
#define BENCHMARK_PANEL_H

#include "bench/vfs_benchmark.h"
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

        char csv_path[256] = "lr2/benchmarks/results.csv";
        bool per_file = false; // Время построения на один файл вместо общего

        void set_status(const std::string& text) {
            std::lock_guard<std::mutex> lock(mutex);
            status = text;
        }

        void start() {
            if (running) return;
            if (worker.joinable()) worker.join();

            params.max_size = 1;
            for (int step = 0; step < max_exponent; step++) params.max_size *= 10;
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

        // Одна линия графика; точки без замера (NaN) пропускаются
        static void plot_series(const char* label, const std::vector<bench::SizeResult>& data,
                                double (*value_of)(const bench::SizeResult&), bool per_file) {
            std::vector<double> xs;
            std::vector<double> ys;
            for (const bench::SizeResult& result : data) {
                double value = value_of(result);
                if (std::isnan(value)) continue;
                xs.push_back(static_cast<double>(result.size));
                ys.push_back(per_file ? value * 1e6 / static_cast<double>(result.size) : value);
            }
            if (xs.empty()) return;
            ImPlotSpec spec;
            spec.Marker = ImPlotMarker_Circle;
            spec.LineWeight = 2.0f;
            ImPlot::PlotLine(label, xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
        }

        void draw_controls() {
            bool locked = running;
            ImGui::BeginDisabled(locked);
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt("max n = 10^k", &max_exponent, 3, 7);
            ImGui::EndDisabled();
            if (max_exponent >= 7) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                                   "10^7: минуты и несколько ГБ памяти. Удобнее ./benchmark --max 1e7 "
                                   "и «Загрузить CSV».");
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
            ImGui::SameLine(0, 30);
            ImGui::Checkbox("построение: нс на один файл", &per_file);
        }

        void draw_plots(const std::vector<bench::SizeResult>& data, float height) {
            float width = ImGui::GetContentRegionAvail().x * 0.5f - ImGui::GetStyle().ItemSpacing.x;
            if (ImPlot::BeginPlot("Один поиск по имени", ImVec2(width, height))) {
                ImPlot::SetupAxes("файлов", "мс");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                plot_series("полный обход, O(n)", data, [](const bench::SizeResult& r) { return r.scan_ms; }, false);
                plot_series("индекс, O(log n)", data, [](const bench::SizeResult& r) { return r.index_ms; }, false);
                ImPlot::EndPlot();
            }
            ImGui::SameLine();
            if (ImPlot::BeginPlot("Построение индекса", ImVec2(width, height))) {
                ImPlot::SetupAxes("файлов", per_file ? "нс на файл" : "мс");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                if (!per_file) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                plot_series("имена по возрастанию", data,
                            [](const bench::SizeResult& r) { return r.build_ms[bench::ASCENDING]; }, per_file);
                plot_series("по убыванию", data,
                            [](const bench::SizeResult& r) { return r.build_ms[bench::DESCENDING]; }, per_file);
                plot_series("вразнобой", data,
                            [](const bench::SizeResult& r) { return r.build_ms[bench::RANDOM]; }, per_file);
                ImPlot::EndPlot();
            }
        }

        static void cell(double value, const char* format) {
            ImGui::TableNextColumn();
            if (std::isnan(value)) {
                ImGui::TextDisabled("-");
            } else {
                ImGui::Text(format, value);
            }
        }

        static void draw_table(const std::vector<bench::SizeResult>& data) {
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (!ImGui::BeginTable("##results", 7, flags)) return;
            ImGui::TableSetupScrollFreeze(0, 1);
            for (const char* title : {"файлов", "построение: возр., мс", "убыв., мс", "вразнобой, мс",
                                      "обход, мс", "индекс, мс", "память индекса, МБ"}) {
                ImGui::TableSetupColumn(title);
            }
            ImGui::TableHeadersRow();
            for (const bench::SizeResult& result : data) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%lld", result.size);
                for (double ms : result.build_ms) cell(ms, "%.1f");
                cell(result.scan_ms, "%.3f");
                cell(result.index_ms, "%.5f");
                cell(result.index_memory_mb, "%.1f");
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
            draw_plots(data, ImGui::GetContentRegionAvail().y - table_height);
            draw_table(data);
        }
};

#endif
