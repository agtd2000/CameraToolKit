#pragma once
/**
 * @file main_window.h
 * @brief 主窗口框架
 *
 * 提供CameraToolKit的主界面框架，集成所有功能模块。
 */

#include <string>

struct GLFWwindow;

namespace mvtk {
namespace ui {

class DeadPixelPanel;
class FlatFieldPanel;
class QuickColorCalibPanel;
class ColorCharCalibPanel;
class SpectralCalibPanel;
class ColorErrorPanel;

/**
 * @brief 主窗口类
 */
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    /**
     * @brief 初始化窗口和ImGui
     * @return 是否成功
     */
    bool init();

    /**
     * @brief 主循环
     */
    void run();

    /**
     * @brief 清理资源
     */
    void cleanup();

private:
    GLFWwindow* window_ = nullptr;
    bool show_demo_ = false;
    int current_tab_ = 0;

    DeadPixelPanel* dead_pixel_panel_ = nullptr;
    FlatFieldPanel* flat_field_panel_ = nullptr;
    QuickColorCalibPanel* quick_calib_panel_ = nullptr;
    ColorCharCalibPanel* color_char_panel_ = nullptr;
    SpectralCalibPanel* spectral_panel_ = nullptr;
    ColorErrorPanel* color_error_panel_ = nullptr;

    /**
     * @brief 渲染菜单栏
     */
    void renderMenuBar();

    /**
     * @brief 渲染工具栏
     */
    void renderToolBar();

    /**
     * @brief 渲染各功能面板
     */
    void renderPanels();

    /**
     * @brief 渲染状态栏
     */
    void renderStatusBar();
};

} // namespace ui
} // namespace mvtk
