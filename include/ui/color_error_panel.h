#pragma once
/**
 * @file color_error_panel.h
 * @brief 色彩误差计算面板
 */

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>
#include "algo/color_error.h"
#include "algo/roi.h"
#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

class ColorErrorPanel {
public:
    ColorErrorPanel();

    void render();

    void setInputImage(const cv::Mat& image) { src_image_ = image; }

private:
    cv::Mat src_image_;
    cv::Mat ref_image_;

    mvtk::ColorErrorSettings params_;
    mvtk::ColorErrorResult result_;
    bool has_result_ = false;

    std::vector<mvtk::ROIInfo> rois_;
    std::vector<cv::Vec3d> src_colors_;
    std::vector<cv::Vec3d> ref_colors_;

    std::string src_path_;
    std::string ref_path_;
    std::string color_file_path_;

    // 图像来源选择
    int src_source_ = 0;
    int ref_source_ = 0;

    // 相机相关
    cv::VideoCapture camera_;
    bool camera_opened_ = false;

    // 状态提示
    std::string status_msg_;
    bool has_src_image_ = false;
    bool has_ref_image_ = false;

    // 图像查看器
    ImageViewer src_viewer_;
    ImageViewer ref_viewer_;

    void doCalculate();
    void doExport();
    void doImportColors();
};

} // namespace ui
} // namespace mvtk
