/**
 * @file main_window.cpp
 * @brief 主窗口框架实现
 */

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <cstdio>

#include "ui/main_window.h"
#include "ui/dead_pixel_panel.h"
#include "ui/flat_field_panel.h"
#include "ui/quick_color_calib_panel.h"
#include "ui/color_char_calib_panel.h"
#include "ui/spectral_calib_panel.h"
#include "ui/color_error_panel.h"

namespace mvtk {
namespace ui {

MainWindow::MainWindow() {
    dead_pixel_panel_ = new DeadPixelPanel();
    flat_field_panel_ = new FlatFieldPanel();
    quick_calib_panel_ = new QuickColorCalibPanel();
    color_char_panel_ = new ColorCharCalibPanel();
    spectral_panel_ = new SpectralCalibPanel();
    color_error_panel_ = new ColorErrorPanel();
}

MainWindow::~MainWindow() {
    delete dead_pixel_panel_;
    delete flat_field_panel_;
    delete quick_calib_panel_;
    delete color_char_panel_;
    delete spectral_panel_;
    delete color_error_panel_;
}

bool MainWindow::init() {
    glfwSetErrorCallback([](int error, const char* description) {
        fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    });

    if (!glfwInit()) return false;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window_ = glfwCreateWindow(1280, 720, "CameraToolKit - Image Calibration Tool", nullptr, nullptr);
    if (!window_) return false;

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void MainWindow::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderMenuBar();
        renderToolBar();
        renderPanels();
        renderStatusBar();

        if (show_demo_) {
            ImGui::ShowDemoWindow(&show_demo_);
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

void MainWindow::cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void MainWindow::renderMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Image...")) {}
        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Quick Color Calibration")) current_tab_ = 0;
        if (ImGui::MenuItem("Color Characteristic Calibration")) current_tab_ = 1;
        if (ImGui::MenuItem("Spectral Characteristic Calibration")) current_tab_ = 2;
        if (ImGui::MenuItem("Color Error Calculator")) current_tab_ = 3;
        if (ImGui::MenuItem("Dead Pixel Correction")) current_tab_ = 4;
        if (ImGui::MenuItem("Flat Field Correction (LSC)")) current_tab_ = 5;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {}
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void MainWindow::renderToolBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, 35), ImGuiCond_Always);
    ImGui::Begin("##ToolBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
    const char* tool_names[] = {
        "Quick Calib", "Color Char", "Spectral", "Color Error", "Dead Pixel", "Flat Field"
    };
    for (int i = 0; i < 6; ++i) {
        if (i > 0) ImGui::SameLine();
        bool active = (current_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button(tool_names[i])) current_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void MainWindow::renderPanels() {
    ImGui::SetNextWindowPos(ImVec2(0, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y - 80),
                             ImGuiCond_FirstUseEver);

    switch (current_tab_) {
    case 0: quick_calib_panel_->render(); break;
    case 1: color_char_panel_->render(); break;
    case 2: spectral_panel_->render(); break;
    case 3: color_error_panel_->render(); break;
    case 4: dead_pixel_panel_->render(); break;
    case 5: flat_field_panel_->render(); break;
    }
}

void MainWindow::renderStatusBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetMainViewport()->Size.y - 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, 20), ImGuiCond_Always);
    ImGui::Begin("##StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("Ready");
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::Text("CameraToolKit v1.0.0");
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace ui
} // namespace mvtk
