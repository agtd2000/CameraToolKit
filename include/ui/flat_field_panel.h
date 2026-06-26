#pragma once
/**
 * @file flat_field_panel.h
 * @brief 平场校正面板
 */

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include "algo/flat_field.h"
#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

class FlatFieldPanel {
public:
    FlatFieldPanel();

    void render();

    void setImage(const cv::Mat& image) { src_image_ = image; }

private:
    cv::Mat src_image_;
    cv::Mat dark_image_;
    cv::Mat corrected_image_;
    cv::Mat gain_map_vis_;

    mvtk::FlatFieldCalibParams calib_params_;
    mvtk::FlatFieldResult calib_result_;

    bool use_dark_ = false;
    std::string flat_path_;
    std::string dark_path_;
    std::string gain_path_;

    // 图像来源选择
    int flat_source_ = 0;
    int dark_source_ = 0;

    // 相机相关
    cv::VideoCapture camera_;
    bool camera_opened_ = false;
    std::string status_msg_;

    // 图像查看器
    ImageViewer flat_viewer_;
    ImageViewer dark_viewer_;
    ImageViewer result_viewer_;

    void doCalibrate();
    void doApply();
    void doExportGain();
    void doImportGain();
};

} // namespace ui
} // namespace mvtk
