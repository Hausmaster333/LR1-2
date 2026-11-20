#ifndef BENCHMARK_PANEL_H
#define BENCHMARK_PANEL_H

#include "bench/tree_benchmark.h"
#include "imgui.h"
#include "implot.h"
#include <cmath>
#include <string>
#include "core/array_sequence.h"

class BenchmarkPanel {
    private:
        BackgroundBenchmark<Params, SizeResult> run;
        int max_exponent = 6;

        char csv_path[256] = "lr1/benchmarks/results.csv";
        bool shown_operations[OPERATION_COUNT] = {true, true, true, true, true};
        bool show_set = true;
        int plot_mode = 1; // 0 - общее время, 1 - время на элемент

        // Значение для графика: мс или нс на элемент; NaN - точки нет
        double plot_value(const SizeResult& result, double ms) const {
            if (std::isnan(ms)) return NAN;
            return plot_mode == 0 ? ms : ms * 1e6 / static_cast<double>(result.size);
        }

        void plot_series(const ArraySequence<SizeResult>& data, int operation, bool of_set) {
            ArraySequence<double> xs;
            ArraySequence<double> ys;
            for (const SizeResult& result : data) {
                double ms = of_set ? result.set_ms[operation] : result.tree_ms[operation];
                double value = plot_value(result, ms);
                if (std::isnan(value)) continue;
                xs.push_back(static_cast<double>(result.size));
                ys.push_back(value);
            }
            if (xs.empty()) return;

            std::string label = std::string(of_set ? "std::set: " : "AVL: ")
                              + OPERATION_TITLES[operation];
            ImPlotSpec spec;
            spec.Marker = of_set ? ImPlotMarker_Square : ImPlotMarker_Circle;
            spec.LineWeight = of_set ? 1.0f : 2.0f;
            spec.FillAlpha = of_set ? 0.5f : 1.0f;
            ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()),
                             spec);
        }

        void draw_controls() {
            bool locked = run.running();
            ImGui::BeginDisabled(locked);
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt("max n = 10^k", &max_exponent, 3, 8);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::SliderInt("точек на порядок", &run.params.steps_per_decade, 1, 3);
            ImGui::SameLine();
            ImGui::Checkbox("сравнить с std::set", &run.params.compare_with_set);
            ImGui::EndDisabled();
            if (max_exponent >= 8) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                                   "10^8: несколько минут и ~5 ГБ памяти. Удобнее консольный "
                                   "./benchmark --max 1e8, затем «Загрузить CSV».");
            }

            if (!locked && ImGui::Button("Запустить")) {
                run.params.max_size = 1;
                for (int i = 0; i < max_exponent; ++i) run.params.max_size *= 10;
                run.start();
            }
            if (locked && ImGui::Button("Остановить")) run.stop();
            ImGui::SameLine();
            ImGui::Text("%s", run.status.c_str());

            ImGui::SetNextItemWidth(300);
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

            for (int operation = 0; operation < OPERATION_COUNT; operation++) {
                if (operation > 0) ImGui::SameLine();
                ImGui::Checkbox(OPERATION_TITLES[operation], &shown_operations[operation]);
            }
            ImGui::Checkbox("показывать std::set", &show_set);
            ImGui::SameLine();
            ImGui::RadioButton("общее время, мс", &plot_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("на один элемент, нс", &plot_mode, 1);
        }

        void draw_table(const ArraySequence<SizeResult>& data) {
            int columns = 2 + OPERATION_COUNT;
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                  | ImGuiTableFlags_ScrollY;
            if (!ImGui::BeginTable("##results", columns, flags)) return;

            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("n");
            for (int operation = 0; operation < OPERATION_COUNT; operation++) {
                ImGui::TableSetupColumn(OPERATION_NAMES[operation]);
            }
            ImGui::TableSetupColumn("память, МБ");
            ImGui::TableHeadersRow();

            for (const SizeResult& result : data) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%lld", result.size);
                for (int operation = 0; operation < OPERATION_COUNT; operation++) {
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
        void update() { run.update(); }

        void draw() {
            draw_controls();


            float table_height = 190.0f * ImGui::GetStyle().FontScaleDpi;
            ImVec2 plot_size(-1, ImGui::GetContentRegionAvail().y - table_height);
            if (ImPlot::BeginPlot("Время работы от числа элементов", plot_size)) {
                ImPlot::SetupAxes("n (элементов)", plot_mode == 0 ? "мс" : "нс на элемент");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                if (plot_mode == 0) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                for (int operation = 0; operation < OPERATION_COUNT; operation++) {
                    if (!shown_operations[operation]) continue;
                    plot_series(run.results, operation, false);
                    if (show_set) plot_series(run.results, operation, true);
                }
                ImPlot::EndPlot();
            }
            draw_table(run.results);
        }
};

#endif
