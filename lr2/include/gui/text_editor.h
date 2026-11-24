#pragma once

#include "imgui.h"
#include "vfs/virtual_file_system.h"
#include <algorithm>

class TextEditor {
    ShrdPtr<const RegularFile> file;
    std::string text, error;
    bool opening = false, dirty = false, closing = false;

    static int resize_buffer(ImGuiInputTextCallbackData* data) {
        auto& buffer = *static_cast<std::string*>(data->UserData);
        buffer.resize(data->BufTextLen);
        data->Buf = buffer.data();
        return 0;
    }

    bool save(VirtualFileSystem& vfs) {
        try {
            vfs.save_text(file->get_path(), file->get_id(), text);
            dirty = false;
            error.clear();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
public:
    void open(VirtualFileSystem& vfs, const std::string& path) {
        auto target = vfs.regular_file(path);
        text = vfs.read_text(path);
        file = target;
        error.clear();
        opening = true;
        dirty = closing = false;
    }

    bool draw(VirtualFileSystem& vfs, bool& quitting) {
        if (!file) return false;
        if (opening) {
            ImGui::OpenPopup("Документ");
            opening = false;
        }
        const float scale = ImGui::GetStyle().FontScaleDpi;
        const ImVec2 available = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize(ImVec2(std::min(560 * scale, available.x * 0.95f),
                                        std::min(380 * scale, available.y * 0.95f)), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Документ")) return false;
        bool saved = false;
        ImGui::TextUnformatted(file->get_name().c_str());
        if (file->get_content().editable()) {
            if (ImGui::InputTextMultiline("##text", text.data(), text.capacity() + 1, ImVec2(-1, closing ? -ImGui::GetFrameHeightWithSpacing() * 4 : -95 * scale),
                                          ImGuiInputTextFlags_CallbackResize, resize_buffer, &text)) dirty = true;
            if (!closing) {
                if (ImGui::Button("Сохранить")) saved = save(vfs);
                ImGui::SameLine();
            }
        } else {
            ImGui::TextDisabled("Только чтение");
            ImGui::BeginChild("##contents", ImVec2(-1, -65 * scale), ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(text.data(), text.data() + text.size());
            ImGui::EndChild();
        }
        if ((!closing && ImGui::Button("Закрыть")) || quitting) closing = true;
        if (closing && dirty) {
            ImGui::TextUnformatted("Сохранить изменения перед выходом?");
            if (ImGui::Button("Сохранить и закрыть")) saved = save(vfs);
            ImGui::SameLine();
            if (ImGui::Button("Отбросить")) dirty = false;
            ImGui::SameLine();
            if (ImGui::Button("Продолжить")) closing = quitting = false;
        }
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        if (closing && !dirty) {
            file.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return saved;
    }
    bool active() const { return static_cast<bool>(file); }
};
