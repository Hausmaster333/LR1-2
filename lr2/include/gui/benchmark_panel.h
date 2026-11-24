#ifndef BENCHMARK_PANEL_H
#define BENCHMARK_PANEL_H

#include "bench/benchmark_run.h"
#include "bench/vfs_benchmark.h"
#include "imgui.h"
#include "implot.h"
#include <cmath>
#include <string>
#include "core/array_sequence.h"

class BenchmarkPanel {
    private:
        BenchmarkRun<Params, SizeResult> run;
        int max_exponent = 6;

        char csv_path[256] = "lr2/benchmarks/results.csv";
        bool per_file = false; // Время построения на один файл вместо общего

        // Одна линия графика; точки без замера (NaN) пропускаются
        static void plot_series(const char* label, const ArraySequence<SizeResult>& data,
                                double (*value_of)(const SizeResult&), bool per_file) {
            ArraySequence<double> xs;
            ArraySequence<double> ys;
            for (const SizeResult& result : data) {
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
            bool locked = run.running();
            ImGui::BeginDisabled(locked);
            ImGui::SetNextItemWidth(200 * ImGui::GetStyle().FontScaleDpi);
            ImGui::SliderInt("max n = 10^k", &max_exponent, 3, 7);
            ImGui::EndDisabled();
            if (max_exponent >= 7) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                                   "10^7: минуты и несколько ГБ памяти. Удобнее ./benchmark --max 1e7 "
                                   "и «Загрузить CSV».");
            }

            if (!locked && ImGui::Button("Запустить")) {
                run.params.max_size = 1;
                for (int i = 0; i < max_exponent; ++i) run.params.max_size *= 10;
                run.start();
            }
            if (locked && ImGui::Button("Остановить")) run.stop();
            ImGui::SameLine();
            ImGui::Text("%s", run.status.c_str());

            ImGui::SetNextItemWidth(300 * ImGui::GetStyle().FontScaleDpi);
            ImGui::InputText("##csv", csv_path, sizeof(csv_path));
            ImGui::SameLine();
            ImGui::BeginDisabled(run.running());
            if (ImGui::Button("Сохранить CSV")) {
                try {
                    save_csv(csv_path, run.results);
                    run.status = std::string("Сохранено: ") + csv_path;
                } catch (const std::exception& error) {
                    run.status = std::string("Ошибка: ") + error.what();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Загрузить CSV")) {
                try {
                    run.results = load_csv(csv_path);
                    run.status = std::string("Загружено: ") + csv_path;
                } catch (const std::exception& error) {
                    run.status = std::string("Ошибка: ") + error.what();
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0, 30);
            ImGui::Checkbox("построение: нс на один файл", &per_file);
        }

        void draw_plots(const ArraySequence<SizeResult>& data, float height) {
            float width = ImGui::GetContentRegionAvail().x * 0.5f - ImGui::GetStyle().ItemSpacing.x;
            if (ImPlot::BeginPlot("Один поиск по имени", ImVec2(width, height))) {
                ImPlot::SetupAxes("файлов", "мс");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                plot_series("полный обход, O(n)", data, [](const SizeResult& r) { return r.scan_ms; }, false);
                plot_series("индекс, O(log n)", data, [](const SizeResult& r) { return r.index_ms; }, false);
                ImPlot::EndPlot();
            }
            ImGui::SameLine();
            if (ImPlot::BeginPlot("Построение индекса", ImVec2(width, height))) {
                ImPlot::SetupAxes("файлов", per_file ? "нс на файл" : "мс");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                if (!per_file) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                plot_series("имена по возрастанию", data,
                            [](const SizeResult& r) { return r.build_ms[ASCENDING]; }, per_file);
                plot_series("по убыванию", data,
                            [](const SizeResult& r) { return r.build_ms[DESCENDING]; }, per_file);
                plot_series("вразнобой", data,
                            [](const SizeResult& r) { return r.build_ms[RANDOM]; }, per_file);
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

        static void draw_table(const ArraySequence<SizeResult>& data) {
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (!ImGui::BeginTable("##results", 7, flags)) return;
            ImGui::TableSetupScrollFreeze(0, 1);
            for (const char* title : {"файлов", "построение: возр., мс", "убыв., мс", "вразнобой, мс",
                                      "обход, мс", "индекс, мс", "память индекса, МБ"}) {
                ImGui::TableSetupColumn(title);
            }
            ImGui::TableHeadersRow();
            for (const SizeResult& result : data) {
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
        void update() { run.update(); }

        void draw() {
            draw_controls();

            float table_height = 190.0f * ImGui::GetStyle().FontScaleDpi;
            draw_plots(run.results, ImGui::GetContentRegionAvail().y - table_height);
            draw_table(run.results);
        }
};

#endif
