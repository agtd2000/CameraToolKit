#pragma once
/**
 * @file dead_pixel_panel.h
 * @brief 坏点校正面板
 */

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>
#include "algo/dead_pixel.h"
#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

class DeadPixelPanel {
public:
    DeadPixelPanel();

    void render();

    void setImage(const cv::Mat& image) { src_image_ = image; }

    const cv::Mat& getCorrectedImage() const { return corrected_image_; }

private:
    cv::Mat src_image_;
    cv::Mat corrected_image_;
    cv::Mat vis_image_;

    mvtk::DeadPixelDetectParams detect_params_;
    mvtk::DeadPixelCorrectParams correct_params_;
    std::vector<mvtk::DeadPixel> dead_pixels_;

    bool show_detected_ = true;
    int image_source_ = 0;
    std::string csv_path_ = "dead_pixels.csv";

    // 相机相关
    cv::VideoCapture camera_;
    bool camera_opened_ = false;
    std::string status_msg_;

    // 图像查看器
    ImageViewer src_viewer_;
    ImageViewer result_viewer_;

    void doDetect();
    void doCorrect();
    void doExport();
    void doImport();
    void doClear();
};

} // namespace ui
} // namespace mvtk
