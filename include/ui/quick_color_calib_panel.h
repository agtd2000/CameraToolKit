#pragma once
/**
 * @file quick_color_calib_panel.h
 * @brief 快速色彩标定面板
 */

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>
#include "algo/quick_color_calib.h"
#include "algo/roi.h"
#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

class QuickColorCalibPanel {
public:
    QuickColorCalibPanel();

    void render();

    void setImage(const cv::Mat& image) { src_image_ = image; }

private:
    cv::Mat src_image_;
    cv::Mat ref_image_;
    cv::Mat corrected_image_;

    int calib_mode_ = 0;
    int step_ = 0;

    mvtk::WBMethod wb_method_ = mvtk::WBMethod::AVERAGE;
    mvtk::CCMMethod ccm_method_ = mvtk::CCMMethod::OPTIMAL;
    mvtk::RGBStatMethod stat_method_ = mvtk::RGBStatMethod::MEDIAN;

    std::vector<mvtk::ROIInfo> rois_;
    mvtk::QuickCalibResult calib_result_;
    double current_brightness_ = 0.0;

    int chart_x_ = 100, chart_y_ = 50;
    int chart_w_ = 400, chart_h_ = 600;
    bool auto_roi_ = true;

    std::string src_path_;
    std::string ref_path_;

    // 图像来源选择
    int src_source_ = 0;
    int ref_source_ = 0;

    // 相机相关
    cv::VideoCapture camera_;
    bool camera_opened_ = false;
    std::string status_msg_;

    // 图像查看器
    ImageViewer src_viewer_;
    ImageViewer ref_viewer_;

    void doCalibrate();
    void doAutoExposure();
    void generateROI();
    void reset();
};

} // namespace ui
} // namespace mvtk
