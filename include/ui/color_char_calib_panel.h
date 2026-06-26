#pragma once
/**
 * @file color_char_calib_panel.h
 * @brief 色彩特性标定面板
 */

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>
#include "algo/white_balance.h"
#include "algo/ccm.h"
#include "algo/roi.h"
#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

struct LightSourceUIData {
    std::string name;
    double color_temp = 5000.0;
    cv::Mat image;
    std::vector<mvtk::ROIInfo> rois;
    double brightness = 0.0;
};

class ColorCharCalibPanel {
public:
    ColorCharCalibPanel();

    void render();

private:
    int step_ = 0;
    int current_light_idx_ = -1;

    std::vector<LightSourceUIData> light_sources_;
    char new_light_name_[256] = "D65";
    double new_light_temp_ = 6500.0;

    mvtk::WBMethod wb_method_ = mvtk::WBMethod::AVERAGE;
    mvtk::CCMMethod ccm_method_ = mvtk::CCMMethod::OPTIMAL;
    int standard_color_ = 0;

    bool quantize_ = false;
    int wb_quant_ = 1024;
    int ccm_quant_ = 1024;

    std::vector<mvtk::WBCorrectParams> wb_results_;
    std::vector<mvtk::CCMCorrectParams> ccm_results_;
    cv::Mat corrected_preview_;

    // 图像来源选择
    int image_source_ = 0;

    // 相机相关
    cv::VideoCapture camera_;
    bool camera_opened_ = false;
    std::string status_msg_;

    // 图像查看器
    ImageViewer image_viewer_;

    void addLightSource();
    void removeLightSource(int idx);
    void doCalibrate();
    void doExport();
};

} // namespace ui
} // namespace mvtk
