#include "gui/benchmark_panel.h"
#include "gui/explorer_panel.h"
#include "gui/search_panel.h"
#include "gui/terminal.h"
#include "vfs/command.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <algorithm>
#include <cmath>


void load_font() {
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    for (const char* path : candidates) {
        if (std::FILE* file = std::fopen(path, "rb")) {
            std::fclose(file);
            ImGui::GetIO().Fonts->AddFontFromFileTTF(path, 18.0f);
            return;
        }
    }
    std::fprintf(stderr, "Cyrillic font not found, labels may be unreadable\n");
}


void apply_style() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.15f, 0.19f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.24f, 0.26f, 0.32f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.36f, 0.58f, 0.75f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.42f, 0.66f, 0.85f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.46f, 0.72f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.28f, 0.42f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.38f, 0.58f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.45f, 0.68f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.40f, 0.62f, 1.0f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.22f, 0.34f, 0.54f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.19f, 0.25f, 1.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
}

struct App {
    ShrdPtr<VirtualFileSystem> vfs = ShrdPtr<VirtualFileSystem>::make();
    FileSystemAccess terminal_access{vfs};
    TextEditor editor;
    ExplorerPanel explorer{vfs, editor};
    Terminal terminal;
    bool quitting = false;

    void poll_terminal() {
        terminal.prompt(terminal_access.current_path());
        std::string line;
        if (!terminal.poll_line(line)) return;
        auto result = execute_command(terminal_access, line);
        if (!result.output.empty()) {
            auto& stream = result.failed ? std::cerr : std::cout;
            stream << result.output;
            if (result.output.back() != '\n') stream << '\n';
            stream.flush();
        }
        if (result.changed) explorer.invalidate(false);
        if (result.changed) search.clear();
        if (result.exit) quitting = true;
        else terminal.prompt(terminal_access.current_path(), true);
    }
    SearchPanel search{*vfs};
    BenchmarkPanel benchmarks;
    bool show_explorer = false;

    void draw() {
        benchmarks.update();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration
                        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        if (ImGui::BeginTabBar("##sections")) {
            ImGuiTabItemFlags explorer_flags = show_explorer ? ImGuiTabItemFlags_SetSelected : 0;
            show_explorer = false;
            if (ImGui::BeginTabItem("Проводник", nullptr, explorer_flags)) {
                if (explorer.draw()) search.clear();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Поиск")) {
                std::string reveal = search.draw();
                if (!reveal.empty()) {
                    explorer.reveal(reveal);
                    show_explorer = true;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Бенчмарк")) {
                benchmarks.draw();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (editor.draw(*vfs, quitting)) {
            explorer.invalidate(false);
            search.clear();
        }
        ImGui::End();
    }
};

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }

    int window_width = 1500;
    int window_height = 900;
    float default_scale = 1.0f;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        int work_x = 0;
        int work_y = 0;
        int work_width = 0;
        int work_height = 0;
        glfwGetMonitorWorkarea(monitor, &work_x, &work_y, &work_width, &work_height);
        if (work_width > 0 && work_height > 0) {
            const float desktop_scale = std::min(work_width / 1920.0f, work_height / 1080.0f);
            default_scale = std::max({1.0f, ImGui_ImplGlfw_GetContentScaleForMonitor(monitor),
                                      std::round(desktop_scale * 4) / 4});
            window_width = std::min(static_cast<int>(window_width * default_scale), work_width * 9 / 10);
            window_height = std::min(static_cast<int>(window_height * default_scale), work_height * 9 / 10);
        }
    }

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "ВФС", nullptr, nullptr);

    if (window == nullptr) {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    apply_style();

    const ImGuiStyle base_style = ImGui::GetStyle();
    float scale = default_scale;
    load_font();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    {
        App app;
        do {
            glfwPollEvents();
            if (glfwWindowShouldClose(window)) {
                app.quitting = true;
                glfwSetWindowShouldClose(window, GLFW_FALSE);
            }
            app.poll_terminal();
            ImGuiStyle& style = ImGui::GetStyle();
            if (scale != style.FontScaleDpi) {
                style = base_style;
                style.ScaleAllSizes(scale);
                style.FontScaleDpi = scale;
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (ImGui::GetIO().KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false))
                    scale = std::min(std::max(4.0f, default_scale), scale + 0.25f);
                if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false))
                    scale = std::max(1.0f, scale - 0.25f);
                if (ImGui::IsKeyPressed(ImGuiKey_0, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad0, false))
                    scale = default_scale;
            }
            app.draw();

            ImGui::Render();
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.13f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        } while (!app.quitting || app.editor.active());
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
