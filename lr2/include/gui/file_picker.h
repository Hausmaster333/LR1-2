#ifndef FILE_PICKER_H
#define FILE_PICKER_H

#include "imgui.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include "core/array_sequence.h"

// Окно выбора реального файла или папки на std::filesystem. Пути в UTF-8, кириллица работает.
class FilePicker {
    public:
        enum class Mode { File, Folder };
    private:
        struct Entry {
            std::string name; // UTF-8
            bool is_directory;
        };

        Mode mode = Mode::File;
        bool open_requested = false;
        std::filesystem::path current;
        ArraySequence<Entry> entries;
        std::string selected; // Имя выбранного файла в current
        std::string error;
        char path_input[1024] = "";

        // Перечитывает current: сначала папки, потом файлы, по алфавиту
        void load(const std::filesystem::path& path) {
            namespace fs = std::filesystem;
            std::error_code code;
            fs::directory_iterator iter(path, fs::directory_options::skip_permission_denied, code);
            if (code) {
                error = "Нет доступа: " + path.u8string();
                return;
            }
            current = path;
            entries.clear();
            selected.clear();
            error.clear();
            for (; iter != fs::directory_iterator(); iter.increment(code)) {
                if (code) break;
                bool is_directory = iter->is_directory(code);
                if (!is_directory && mode == Mode::Folder) continue;
                entries.push_back({iter->path().filename().u8string(), is_directory});
            }
            std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
                if (left.is_directory != right.is_directory) return left.is_directory;
                return left.name < right.name;
            });
            std::string text = current.u8string();
            text.copy(path_input, sizeof(path_input) - 1);
            path_input[std::min(text.size(), sizeof(path_input) - 1)] = '\0';
        }

        void draw_drives() {
#ifdef _WIN32
            for (char letter = 'A'; letter <= 'Z'; letter++) {
                std::string drive = std::string(1, letter) + ":/";
                std::error_code code;
                if (!std::filesystem::exists(drive, code)) continue;
                if (ImGui::SmallButton(drive.c_str())) load(drive);
                ImGui::SameLine();
            }
#endif
        }
    public:
        // Открыть окно; результат вернёт draw, когда пользователь выберет
        void open(Mode new_mode) {
            mode = new_mode;
            open_requested = true;
            std::error_code code;
            load(current.empty() ? std::filesystem::current_path(code) : current);
        }

        // true - выбор сделан, путь в UTF-8 в chosen
        bool draw(std::string& chosen) {
            const char* title = mode == Mode::File ? "Выбор файла###picker" : "Выбор папки###picker";
            if (open_requested) {
                ImGui::OpenPopup("###picker");
                open_requested = false;
            }
            ImVec2 size = ImGui::GetMainViewport()->WorkSize;
            ImGui::SetNextWindowSize(ImVec2(size.x * 0.6f, size.y * 0.7f), ImGuiCond_Appearing);
            if (!ImGui::BeginPopupModal(title, nullptr)) return false;

            bool done = false;
            draw_drives();
            if (ImGui::SmallButton("Вверх") && current.has_parent_path()) load(current.parent_path());
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##path", path_input, sizeof(path_input),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                load(std::filesystem::u8path(path_input));
            }
            if (!error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", error.c_str());

            float buttons_height = ImGui::GetFrameHeightWithSpacing() * 1.5f;
            ImGui::BeginChild("##entries", ImVec2(0, -buttons_height), ImGuiChildFlags_Borders);
            std::filesystem::path open_next;
            for (const Entry& entry : entries) {
                std::string label = (entry.is_directory ? "[папка]  " : "              ") + entry.name;
                bool is_selected = !entry.is_directory && entry.name == selected;
                if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (entry.is_directory) {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) open_next = current / std::filesystem::u8path(entry.name);
                    } else {
                        selected = entry.name;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) done = true;
                    }
                }
            }
            ImGui::EndChild();
            if (!open_next.empty()) load(open_next); // Не посреди цикла: load меняет entries

            if (mode == Mode::File) {
                ImGui::BeginDisabled(selected.empty());
                if (ImGui::Button("Выбрать файл")) done = true;
                ImGui::EndDisabled();
            } else if (ImGui::Button("Выбрать эту папку")) {
                done = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();

            if (done) {
                chosen = (mode == Mode::File ? current / std::filesystem::u8path(selected) : current).u8string();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return done;
        }
};

#endif
