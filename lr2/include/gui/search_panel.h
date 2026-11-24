#ifndef SEARCH_PANEL_H
#define SEARCH_PANEL_H

#include "vfs/search_criteria.h"
#include "bench/vfs_benchmark.h"
#include "gui/explorer_panel.h"
#include "imgui.h"
#include "vfs/virtual_file_system.h"
#include <string>

class SearchPanel {
    private:
        VirtualFileSystem& vfs;
        int field = 0; // 0 - имя, 1 - расширение
        char query[256] = "";
        DynamicArray<ShrdPtr<const File>> found;
        bool searched = false;
        double scan_ms = 0;
        double index_ms = 0;

        double average_ms(SearchCriterion search_field, SearchMethod method) {
            double total = 0;
            int runs = 0;
            while (total < 20.0 && runs < 1000) {
                total += time_ms([&] { found = vfs.find_files(search_field, query, method); });
                runs++;
            }
            return total / runs;
        }

        void search() {
            SearchCriterion search_field = field == 0 ? by_name : by_extension;
            scan_ms = average_ms(search_field, SearchMethod::FullScan);
            vfs.find_files(search_field, query, SearchMethod::Index); // Построение индекса вне замера.
            index_ms = average_ms(search_field, SearchMethod::Index);
            searched = true;
        }

        void draw_timing() {
            ImGui::Text("Найдено: %d", found.get_size());
            ImGui::SameLine(0, 30);
            ImGui::Text("Полный обход: %.3f мс", scan_ms);
            ImGui::SameLine(0, 30);
            ImGui::Text("Индекс: %.2f мкс", index_ms * 1000);
            if (index_ms > 0) {
                ImGui::SameLine(0, 30);
                ImGui::Text("Время обхода / время индекса = %.3g", scan_ms / index_ms);
            }
        }
    public:
        explicit SearchPanel(VirtualFileSystem& vfs) : vfs(vfs) {}

        void clear() {
            found = DynamicArray<ShrdPtr<const File>>();
            searched = false;
        }

        std::string draw() {
            ImGui::RadioButton("по имени", &field, 0);
            ImGui::SameLine();
            ImGui::RadioButton("по расширению", &field, 1);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(360 * ImGui::GetStyle().FontScaleDpi);
            const char* hint = field == 0 ? "например file_0000000042.pdf" : "например pdf";
            bool submitted = ImGui::InputTextWithHint("##query", hint, query, sizeof(query),
                                                      ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Найти") || submitted) search();
            ImGui::TextDisabled("Обход проверяет каждый файл - O(n). Индекс - словарь на AVL-дереве "
                                "«ключ -> множество файлов» - O(log n) плюс число найденных.");
            if (!searched) return "";

            draw_timing();
            ImGui::Separator();
            std::string reveal;
            ImGui::BeginChild("##found");
            ImGuiListClipper clipper;
            clipper.Begin(found.get_size());
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    std::string path = found.get(row)->get_path();
                    ImGui::PushID(row);
                    gui::draw_icon(found.get(row)->is_directory());
                    if (ImGui::Selectable(path.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)
                        && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        reveal = path;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Двойной щелчок - показать в проводнике");
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            return reveal;
        }
};

#endif
