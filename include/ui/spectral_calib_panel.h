#pragma once
/**
 * @file spectral_calib_panel.h
 * @brief 光谱特性标定面板
 */

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>
#include "algo/spectral.h"
#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

class SpectralCalibPanel {
public:
    SpectralCalibPanel();

    void render();

private:
    int step_ = 0;
    int calib_mode_ = 0;
    int complexity_ = 0;
    int image_source_ = 0;

    std::string factory_file_path_;
    std::vector<mvtk::LightSourceInfo> light_sources_;
    std::vector<mvtk::LightSourceInfo> factory_sources_;

    char new_light_name_[256] = "A";
    double new_light_temp_ = 2856.0;

    double focal_length_ = 25.0;
    double working_distance_ = 500.0;
    double sensor_width_ = 5.7;
    double sensor_height_ = 4.3;
    int calib_method_ = 1;
    mvtk::CalibRegionResult calib_region_;

    cv::Mat spectral_preview_;

    // 相机相关
    cv::VideoCapture camera_;
    bool camera_opened_ = false;
    cv::Mat captured_image_;
    std::string status_msg_;

    // 图像查看器
    ImageViewer image_viewer_;

    void addLightSource();
    void captureCurve();
    void computeCalibRegion();
    void previewResult();
    void doExport();
    void importFactory();
};

} // namespace ui
} // namespace mvtk
