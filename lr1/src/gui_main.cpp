#include "gui/benchmark_panel.h"
#include "gui/tree_panel.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>

namespace {
    // Шрифт с кириллицей: стандартный шрифт ImGui её не содержит
    void load_font() {
        const char* candidates[] = {
            "C:/Windows/Fonts/segoeui.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
        };
        for (const char* path : candidates) {
            if (std::ifstream(path).good()) {
                ImGui::GetIO().Fonts->AddFontFromFileTTF(path, 17.0f);
                return;
            }
        }
        std::cerr << "Cyrillic font not found, labels may be unreadable" << std::endl;
    }

    struct App {
        UnqPtr<ITreePanel> panels[6] = {
            UnqPtr<TreePanel<int>>::make(),
            UnqPtr<TreePanel<double>>::make(),
            UnqPtr<TreePanel<std::string>>::make(),
            UnqPtr<TreePanel<Complex>>::make(),
            UnqPtr<TreePanel<Student>>::make(),
            UnqPtr<TreePanel<Teacher>>::make(),
        };
        int type_index = 0;
        BenchmarkPanel benchmarks;

        void draw() {
            benchmarks.update();
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration
                         | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

            if (ImGui::BeginTabBar("##sections")) {
                if (ImGui::BeginTabItem("AVL-дерево поиска")) {
                    ImGui::Text("Тип элементов:");
                    for (int idx = 0; idx < 6; idx++) {
                        ImGui::SameLine();
                        ImGui::RadioButton(panels[idx]->type_name(), &type_index, idx);
                    }
                    ImGui::PushID(type_index); // У каждого типа своё состояние виджетов
                    panels[type_index]->draw();
                    ImGui::PopID();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Бенчмарки")) {
                    benchmarks.draw();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }
    };
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(1500, 900, "ЛР-1: AVL-дерево", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();

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
        App app; // Разрушается до DestroyContext: поток бенчмарков успевает остановиться
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            app.draw();

            ImGui::Render();
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
