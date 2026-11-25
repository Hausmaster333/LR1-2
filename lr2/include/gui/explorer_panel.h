#ifndef EXPLORER_PANEL_H
#define EXPLORER_PANEL_H

#include "bench/vfs_benchmark.h"
#include "gui/file_picker.h"
#include "gui/text_editor.h"
#include "imgui.h"
#include "vfs/virtual_file_system.h"
#include <cstdio>
#include <functional>
#include <algorithm>
#include <climits>
#include <string>

namespace gui {
    const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.4f, 1.0f);
    const ImVec4 SUCCESS_COLOR(0.45f, 0.85f, 0.5f, 1.0f);
    const char* const PATH_PAYLOAD = "VFS_PATH";

    // "/a" + "b" -> "/a/b", "/" + "b" -> "/b"
    inline std::string join_path(const std::string& directory, const std::string& name) {
        return (directory == "/" ? "" : directory) + "/" + name;
    }

    inline std::string parent_path(const std::string& path) {
        std::size_t slash = path.rfind('/');
        return slash == 0 || slash == std::string::npos ? "/" : path.substr(0, slash);
    }

    inline std::string format_size(long long bytes) {
        if (bytes < 0) return "нет доступа";
        char buffer[32];
        if (bytes < 1024) {
            std::snprintf(buffer, sizeof(buffer), "%lld Б", bytes);
        } else if (bytes < 1024 * 1024) {
            std::snprintf(buffer, sizeof(buffer), "%.1f КБ", bytes / 1024.0);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.1f МБ", bytes / (1024.0 * 1024.0));
        }
        return buffer;
    }

    inline void draw_icon(bool is_directory) {
        float size = ImGui::GetTextLineHeight();
        ImVec2 corner = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (is_directory) {
            ImU32 color = IM_COL32(232, 184, 80, 255);
            draw->AddRectFilled(ImVec2(corner.x, corner.y + size * 0.15f),
                                ImVec2(corner.x + size * 0.45f, corner.y + size * 0.35f), color, 2.0f);
            draw->AddRectFilled(ImVec2(corner.x, corner.y + size * 0.3f),
                                ImVec2(corner.x + size, corner.y + size * 0.9f), color, 2.0f);
        } else {
            draw->AddRectFilled(ImVec2(corner.x + size * 0.15f, corner.y + size * 0.05f),
                                ImVec2(corner.x + size * 0.85f, corner.y + size * 0.95f),
                                IM_COL32(150, 170, 200, 255), 2.0f);
            for (float line : {0.35f, 0.55f, 0.75f}) {
                draw->AddLine(ImVec2(corner.x + size * 0.28f, corner.y + size * line),
                              ImVec2(corner.x + size * 0.72f, corner.y + size * line),
                              IM_COL32(60, 70, 90, 255));
            }
        }
        ImGui::Dummy(ImVec2(size, size));
        ImGui::SameLine();
    }
}

class ExplorerPanel {
    private:
        enum class Dialog { None, NewDirectory, Rename, Remove, Generate };
        enum class Pick { AttachFile, ImportFolder };

        FileSystemAccess access;
        TextEditor& editor;
        std::string selected;
        DynamicArray<ShrdPtr<const File>> entries; // Содержимое текущего каталога
        bool stale = true;
        bool scroll_to_selected = false;
        bool changed = false;

        std::function<void()> pending;
        std::string pending_success;
        std::string message = "Добавьте реальные файлы или сгенерируйте тестовые";
        bool message_is_error = false;

        Dialog requested_dialog = Dialog::None;
        std::string dialog_target;
        char name_input[256] = "";
        int generate_files = 100000;
        int generate_directories = 100;

        FilePicker picker;
        Pick pick_purpose = Pick::AttachFile;

        std::string preview_path;
        std::string preview_bytes;

        void open_entry(const File& entry, const std::string& path) {
            try {
                auto target = entry.kind() == FileKind::Link
                    ? static_cast<const Link&>(entry).destination(access.filesystem().get_root()) : ShrdPtr<File>{};
                if (entry.is_directory() || (target && target->is_directory())) go_to(path);
                else editor.open(access.filesystem(), path);
            } catch (const std::exception& e) { message = e.what(); message_is_error = true; }
        }

        void go_to(const std::string& path) {
            try { access.change_directory(path); }
            catch (const std::exception& e) { message = e.what(); message_is_error = true; return; }
            selected.clear();
            stale = true;
        }

        void schedule(const std::string& success, std::function<void()> action) {
            pending = std::move(action);
            pending_success = success;
        }

        void run_pending() {
            if (!pending) return;
            try {
                pending();
                if (!pending_success.empty()) {
                    message = pending_success;
                    message_is_error = false;
                }
            } catch (const std::exception& error) {
                message = error.what();
                message_is_error = true;
            }
            pending = nullptr;
            stale = true;
            changed = true;
        }

        void refresh() {
            const Directory& directory = access.current_directory();
            entries = directory.list();
            if (!selected.empty() && !directory.find_child(selected)) selected.clear();
            preview_path.clear();
            stale = false;
        }

        void open_dialog(Dialog dialog, const std::string& target, const std::string& initial_name) {
            requested_dialog = dialog;
            dialog_target = target;
            std::snprintf(name_input, sizeof(name_input), "%s", initial_name.c_str());
        }

        void drag_source(const std::string& path, const std::string& name) {
            if (!ImGui::BeginDragDropSource()) return;
            ImGui::SetDragDropPayload(gui::PATH_PAYLOAD, path.c_str(), path.size() + 1);
            ImGui::Text("Переместить %s", name.c_str());
            ImGui::EndDragDropSource();
        }

        void drop_target(const std::string& directory_path) {
            if (!ImGui::BeginDragDropTarget()) return;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(gui::PATH_PAYLOAD)) {
                std::string source = static_cast<const char*>(payload->Data);
                schedule("Перемещено в " + directory_path,
                         [this, source, directory_path] { access.filesystem().move(source, directory_path); });
            }
            ImGui::EndDragDropTarget();
        }

        void draw_toolbar() {
            const std::string current = access.current_path();
            ImGui::BeginDisabled(current == "/");
            if (ImGui::Button("Вверх")) go_to(gui::parent_path(current));
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Новый каталог")) open_dialog(Dialog::NewDirectory, current, "");
            ImGui::SameLine();
            if (ImGui::Button("Добавить файл...")) {
                pick_purpose = Pick::AttachFile;
                picker.open(FilePicker::Mode::File);
            }
            ImGui::SameLine();
            if (ImGui::Button("Импорт папки...")) {
                pick_purpose = Pick::ImportFolder;
                picker.open(FilePicker::Mode::Folder);
            }
            ImGui::SameLine();
            if (ImGui::Button("Генератор...")) open_dialog(Dialog::Generate, current, "");

            ImGui::SameLine(0, 24);
            if (ImGui::Button("/")) go_to("/");
            drop_target("/");
            std::size_t begin = 1;
            while (begin < current.size()) {
                std::size_t end = current.find('/', begin);
                if (end == std::string::npos) end = current.size();
                std::string prefix = current.substr(0, end);
                ImGui::SameLine(0, 2);
                ImGui::TextDisabled("›");
                ImGui::SameLine(0, 2);
                ImGui::PushID(prefix.c_str());
                if (ImGui::Button(current.substr(begin, end - begin).c_str())) go_to(prefix);
                drop_target(prefix);
                ImGui::PopID();
                begin = end + 1;
            }
        }

        void draw_tree(const Directory& directory) {
            const std::string current = access.current_path();
            std::string path = directory.get_path();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                     | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (path == current) flags |= ImGuiTreeNodeFlags_Selected;
            const bool leaf = !directory.has_subdirectories();
            if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (path == "/") ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            const char* label = path == "/" ? "/ (корень)" : directory.get_name().c_str();
            bool open = ImGui::TreeNodeEx(&directory, flags, "%s", label);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) go_to(path);
            if (path != "/") drag_source(path, directory.get_name());
            drop_target(path);
            if (!open || leaf) return;

            DynamicArray<ShrdPtr<const File>> children = directory.list();
            for (std::size_t position = 0; position < children.get_size(); position++) {
                const ShrdPtr<const File>& child = children.get(position);
                if (!child->is_directory()) break;
                draw_tree(static_cast<const Directory&>(*child));
            }
            ImGui::TreePop();
        }

        void draw_context_menu(const File& node, const std::string& path) {
            if (!ImGui::BeginPopupContextItem("##node_menu")) return;
            selected = node.get_name();
            if (ImGui::MenuItem("Открыть")) open_entry(node, path);
            if (ImGui::MenuItem("Переименовать")) open_dialog(Dialog::Rename, path, node.get_name());
            if (ImGui::MenuItem("Удалить")) open_dialog(Dialog::Remove, path, node.get_name());
            ImGui::EndPopup();
        }

        void draw_table() {
            const std::string current = access.current_path();
            ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
                                  | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
            if (!ImGui::BeginTable("##entries", 3, flags)) return;
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Имя", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Размер", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Источник", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableHeadersRow();

            if (current != "/") {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                gui::draw_icon(true);
                if (ImGui::Selectable("..", false, ImGuiSelectableFlags_SpanAllColumns
                                      | ImGuiSelectableFlags_AllowDoubleClick)
                    && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    go_to(gui::parent_path(current));
                }
                drop_target(gui::parent_path(current));
            }

            const int rows = static_cast<int>(std::min<std::size_t>(entries.get_size(), INT_MAX - 1));
            int target = -1;
            if (scroll_to_selected) {
                for (int row = 0; row < rows; row++) {
                    if (entries.get(row)->get_name() == selected) target = row;
                }
                scroll_to_selected = false;
            }

            ImGuiListClipper clipper;
            clipper.Begin(rows);
            if (target >= 0) clipper.IncludeItemByIndex(target);
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    draw_row(row);
                    if (row == target) ImGui::SetScrollHereY();
                }
            }
            ImGui::EndTable();
        }

        void draw_row(int row) {
            const File& node = *entries.get(row);
            std::string path = node.get_path();
            ImGui::PushID(row);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            gui::draw_icon(node.is_directory());
            std::string label = (node.kind() == FileKind::Link ? "→ " : "") + node.get_name();
            if (ImGui::Selectable(label.c_str(), node.get_name() == selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                selected = node.get_name();
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    open_entry(node, path);
                }
            }
            drag_source(path, node.get_name());
            if (node.is_directory()) drop_target(path);
            draw_context_menu(node, path);

            ImGui::TableNextColumn();
            if (node.is_directory()) {
                ImGui::TextDisabled("%d элем.", static_cast<const Directory&>(node).get_child_count());
            } else {
                if (node.kind() == FileKind::Link) {
                    ImGui::TextDisabled("Ссылка");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(static_cast<const Link&>(node).target_path(access.filesystem().get_root()).c_str());
                    ImGui::PopID();
                    return;
                }
                const IContentSource& content = static_cast<const RegularFile&>(node).get_content();
                ImGui::TextUnformatted(gui::format_size(content.get_size()).c_str());
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", content.get_location().c_str());
            }
            ImGui::PopID();
        }

        void draw_preview() {
            const std::string current = access.current_path();
            if (stale) return;
            ShrdPtr<const File> node = selected.empty() ? ShrdPtr<const File>()
                                       : access.current_directory().find_child(selected);
            if (!node) {
                ImGui::TextDisabled("Выберите файл или каталог");
                return;
            }
            std::string path = gui::join_path(current, node->get_name());
            gui::draw_icon(node->is_directory());
            ImGui::TextWrapped("%s", node->get_name().c_str());
            ImGui::TextDisabled("Путь:");
            ImGui::TextWrapped("%s", path.c_str());
            if (node->is_directory()) {
                ImGui::Text("Элементов: %d", static_cast<const Directory&>(*node).get_child_count());
                return;
            }

            if (node->kind() == FileKind::Link) {
                ImGui::TextWrapped("Ссылка: %s", static_cast<const Link&>(*node).target_path(access.filesystem().get_root()).c_str());
                return;
            }
            const IContentSource& content = static_cast<const RegularFile&>(*node).get_content();
            ImGui::Text("Размер: %s", gui::format_size(content.get_size()).c_str());
            const std::string location = content.get_location();
            if (!location.empty()) ImGui::TextWrapped("Источник: %s", location.c_str());
            ImGui::Separator();

            if (preview_path != path) {
                preview_path = path;
                preview_bytes = content.read_beginning(4096);
            }
            draw_bytes(preview_bytes);
        }

        static void draw_bytes(const std::string& bytes) {
            if (bytes.empty()) {
                ImGui::TextDisabled("Нет данных для просмотра");
                return;
            }
            bool is_text = true;
            for (char symbol : bytes) {
                unsigned char code = static_cast<unsigned char>(symbol);
                if (code < 9 || (code > 13 && code < 32)) is_text = false;
            }
            ImGui::BeginChild("##bytes", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            if (is_text) {
                ImGui::TextUnformatted(bytes.data(), bytes.data() + bytes.size());
            } else {
                std::string dump;
                char cell[8];
                for (std::size_t position = 0; position < bytes.size() && position < 1024; position++) {
                    std::snprintf(cell, sizeof(cell), "%02X ", static_cast<unsigned char>(bytes[position]));
                    dump += cell;
                    if (position % 16 == 15) dump += '\n';
                }
                ImGui::TextUnformatted(dump.c_str());
            }
            ImGui::EndChild();
        }

        void generate() {
            std::string root = "/generated";
            for (int number = 2; access.filesystem().exists(root); number++) root = "/generated_" + std::to_string(number);

            int files = generate_files;
            int directories = generate_directories;
            int per_directory = (files + directories - 1) / directories;
            double ms = time_ms([&] {
                access.filesystem().make_directory(root);
                for (int number = 0; number < directories; number++) {
                    char name[32];
                    std::snprintf(name, sizeof(name), "/dir_%03d", number);
                    access.filesystem().make_directory(root + name);
                }
                for (int number = 0; number < files; number++) {
                    char name[32];
                    std::snprintf(name, sizeof(name), "/dir_%03d", number / per_directory);
                    access.filesystem().add_file(root + name, file_name(number), UnqPtr<GeneratedSource>::make());
                }
            });
            char text[160];
            std::snprintf(text, sizeof(text), "Создано %d файлов в %s за %.0f мс", files, root.c_str(), ms);
            message = text;
            message_is_error = false;
            go_to(root);
        }

        bool name_prompt(const char* button) {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool confirmed = ImGui::InputText("##name", name_input, sizeof(name_input),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
            confirmed |= ImGui::Button(button);
            ImGui::SameLine();
            if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
            if (confirmed) ImGui::CloseCurrentPopup();
            return confirmed;
        }

        void draw_dialogs() {
            const std::string current = access.current_path();
            const char* titles[] = {"", "Новый каталог", "Переименовать", "Удалить", "Генератор тестовых данных"};
            if (requested_dialog != Dialog::None) {
                ImGui::OpenPopup(titles[static_cast<int>(requested_dialog)]);
                requested_dialog = Dialog::None;
            }
            ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::BeginPopupModal("Новый каталог", nullptr, flags)) {
                ImGui::Text("Каталог в %s", dialog_target.c_str());
                if (name_prompt("Создать")) {
                    std::string path = gui::join_path(dialog_target, name_input);
                    schedule("Создан " + path, [this, path] { access.filesystem().make_directory(path); });
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Переименовать", nullptr, flags)) {
                ImGui::Text("Новое имя для %s", dialog_target.c_str());
                if (name_prompt("Переименовать")) {
                    std::string path = dialog_target;
                    std::string name = name_input;
                    selected = name;
                    schedule("Переименовано в " + name, [this, path, name] { access.filesystem().rename(path, name); });
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Удалить", nullptr, flags)) {
                ImGui::Text("Удалить %s со всем содержимым?", dialog_target.c_str());
                ImGui::TextDisabled("Реальные файлы на диске не пострадают");
                if (ImGui::Button("Удалить")) {
                    std::string path = dialog_target;
                    schedule("Удалено " + path, [this, path] { access.filesystem().remove(path); });
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Генератор тестовых данных", nullptr, flags)) {
                ImGui::SetNextItemWidth(200 * ImGui::GetStyle().FontScaleDpi);
                ImGui::InputInt("файлов", &generate_files, 1000, 100000);
                ImGui::SetNextItemWidth(200 * ImGui::GetStyle().FontScaleDpi);
                ImGui::InputInt("каталогов", &generate_directories, 1, 100);
                generate_files = std::max(1, std::min(generate_files, 1000000));
                generate_directories = std::max(1, std::min(generate_directories, 1000));
                ImGui::TextDisabled("Пустые файлы file_0000000000.txt, ... с расширениями txt, log, pdf, png, md");
                if (ImGui::Button("Создать")) {
                    schedule("", [this] { generate(); });
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            std::string chosen;
            if (picker.draw(chosen)) {
                std::string target = current;
                if (pick_purpose == Pick::AttachFile) {
                    schedule("Добавлен файл " + chosen, [this, chosen, target] { access.filesystem().attach_file(chosen, target); });
                } else {
                    schedule("Импортирована папка " + chosen,
                             [this, chosen, target] { access.filesystem().import_directory(chosen, target); });
                }
            }
        }
    public:
        ExplorerPanel(const ShrdPtr<VirtualFileSystem>& vfs, TextEditor& editor) : access(vfs), editor(editor) {}

        void invalidate(bool navigated) {
            stale = true;
            if (navigated) selected.clear();
        }

        void reveal(const std::string& path) {
            go_to(gui::parent_path(path));
            selected = path.substr(path.rfind('/') + 1);
            scroll_to_selected = true;
        }

        bool draw() {
            const std::string current = access.current_path();
            changed = false;
            if (stale) refresh();
            draw_toolbar();

            float status_height = ImGui::GetFrameHeightWithSpacing();
            float height = ImGui::GetContentRegionAvail().y - status_height;
            float width = ImGui::GetContentRegionAvail().x;

            ImGui::BeginChild("##tree", ImVec2(width * 0.22f, height),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            draw_tree(access.filesystem().get_root());
            ImGui::EndChild();
            ImGui::SameLine();

            float preview_width = width * 0.26f;
            ImGui::BeginChild("##table", ImVec2(ImGui::GetContentRegionAvail().x - preview_width, height),
                              ImGuiChildFlags_Borders);
            draw_table();
            if (ImGui::BeginPopupContextWindow("##empty_menu",
                                               ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Новый каталог")) open_dialog(Dialog::NewDirectory, current, "");
                if (ImGui::MenuItem("Добавить файл...")) {
                    pick_purpose = Pick::AttachFile;
                    picker.open(FilePicker::Mode::File);
                }
                if (ImGui::MenuItem("Импорт папки...")) {
                    pick_purpose = Pick::ImportFolder;
                    picker.open(FilePicker::Mode::Folder);
                }
                ImGui::EndPopup();
            }
            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("##preview", ImVec2(0, height), ImGuiChildFlags_Borders);
            draw_preview();
            ImGui::EndChild();

            ImGui::TextColored(message_is_error ? gui::ERROR_COLOR : gui::SUCCESS_COLOR, "%s", message.c_str());
            draw_dialogs();
            run_pending();
            return changed;
        }
};

#endif
