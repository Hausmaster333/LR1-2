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


void load_font() {
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    for (const char* path : candidates) {
        if (std::FILE* file = std::fopen(path, "rb")) {
            std::fclose(file);
            ImGui::GetIO().Fonts->AddFontFromFileTTF(path, 17.0f);
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
    VirtualFileSystem vfs;
    TextEditor editor;
    ExplorerPanel explorer{vfs, editor};
    Terminal terminal;
    bool quitting = false;

    void poll_terminal() {
        terminal.prompt(vfs.current_path());
        std::string line;
        if (!terminal.poll_line(line)) return;
        auto result = execute_command(vfs, line);
        if (!result.output.empty()) {
            auto& stream = result.failed ? std::cerr : std::cout;
            stream << result.output;
            if (result.output.back() != '\n') stream << '\n';
            stream.flush();
        }
        if (result.changed || result.navigated) explorer.invalidate(result.navigated);
        if (result.changed) search.clear();
        if (result.exit) quitting = true;
        else terminal.prompt(vfs.current_path(), true);
    }
    SearchPanel search{vfs};
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
        if (editor.draw(vfs, quitting)) explorer.invalidate(false);
        ImGui::End();
    }
};

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(1500, 900, "ЛР-2: виртуальная файловая система", nullptr, nullptr);
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

    float x_scale = 1.0f;
    float y_scale = 1.0f;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor != nullptr) glfwGetMonitorContentScale(monitor, &x_scale, &y_scale);
    float dpi_scale = x_scale > 0.0f ? x_scale : 1.0f;
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);
    ImGui::GetStyle().FontScaleDpi = dpi_scale;
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
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

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
