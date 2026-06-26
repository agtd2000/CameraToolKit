/**
 * @file quick_color_calib.cpp
 * @brief 快速色彩标定算法实现
 */

#include "algo/quick_color_calib.h"
#include "algo/color_error.h"
#include <algorithm>
#include <cmath>

namespace mvtk {

std::vector<cv::Vec3d> StandardColorChart::getXriteClassic24() {
    // X-Rite ColorChecker Classic 24色卡 sRGB D65参考值
    return {
        {115, 82, 68},    // Dark Skin
        {194, 150, 130},  // Light Skin
        {98, 122, 157},   // Blue Sky
        {87, 108, 67},    // Foliage
        {133, 128, 177},  // Blue Flower
        {103, 189, 170},  // Bluish Green
        {214, 126, 44},   // Orange
        {80, 91, 166},    // Purplish Blue
        {193, 90, 99},    // Moderate Red
        {94, 60, 108},    // Purple
        {157, 188, 64},   // Yellow Green
        {224, 163, 46},   // Orange Yellow
        {56, 61, 150},    // Blue
        {70, 148, 73},    // Green
        {175, 54, 60},    // Red
        {231, 199, 31},   // Yellow
        {187, 86, 149},   // Magenta
        {8, 133, 161},    // Cyan
        {243, 243, 242},  // White
        {200, 200, 200},  // Neutral 8
        {160, 160, 160},  // Neutral 6.5
        {122, 122, 121},  // Neutral 5
        {85, 85, 85},     // Neutral 3.5
        {52, 52, 52}      // Black
    };
}

std::vector<cv::Vec3d> StandardColorChart::getXriteDigitalSG140() {
    // 简化返回24色卡数据，实际140色卡需完整数据集
    return getXriteClassic24();
}

QuickCalibResult QuickColorCalibrator::calibrateStandard(
    const cv::Mat& image,
    const std::vector<cv::Rect>& rois,
    WBMethod wb_method,
    CCMMethod ccm_method,
    RGBStatMethod stat_method) {

    QuickCalibResult result;

    // 1. 提取输入图像中的颜色
    std::vector<cv::Vec3d> src_colors = WhiteBalance::extractColors(image, rois, stat_method);

    // 2. 获取标准24色卡参考颜色
    std::vector<cv::Vec3d> ref_colors = StandardColorChart::getXriteClassic24();

    // 确保颜色数量匹配
    size_t n = std::min(src_colors.size(), ref_colors.size());
    if (n == 0) return result;

    src_colors.resize(n);
    ref_colors.resize(n);

    // 3. 白平衡标定
    WBCalibParams wb_calib_params;
    wb_calib_params.method = wb_method;
    wb_calib_params.stat_method = stat_method;
    result.wb_params = WhiteBalance::calibrateFromColors(src_colors, ref_colors, wb_calib_params);

    // 4. 应用白平衡
    cv::Mat wb_image = WhiteBalance::apply(image, result.wb_params);

    // 5. 重新提取白平衡后的颜色
    std::vector<cv::Vec3d> wb_colors = WhiteBalance::extractColors(wb_image, rois, stat_method);
    wb_colors.resize(n);

    // 6. CCM标定
    CCMCalibParams ccm_calib_params;
    ccm_calib_params.method = ccm_method;
    result.ccm_params = ColorCorrectionMatrix::calibrate(wb_colors, ref_colors, ccm_calib_params);

    // 7. 应用CCM
    cv::Mat ccm_image = ColorCorrectionMatrix::apply(wb_image, result.ccm_params);
    result.corrected_image = ccm_image;

    // 8. 计算各色块误差
    std::vector<cv::Vec3d> final_colors = WhiteBalance::extractColors(ccm_image, rois, stat_method);
    final_colors.resize(n);

    double sum_de = 0;
    for (size_t i = 0; i < n; ++i) {
        cv::Vec3d lab1 = ColorErrorCalculator::rgbToLab(final_colors[i]);
        cv::Vec3d lab2 = ColorErrorCalculator::rgbToLab(ref_colors[i]);
        double de = ColorErrorCalculator::deltaE00(lab1, lab2);
        result.deltaE00.push_back(de);
        sum_de += de;
    }
    result.mean_deltaE00 = sum_de / n;

    return result;
}

QuickCalibResult QuickColorCalibrator::calibrateReference(
    const cv::Mat& image,
    const cv::Mat& ref_image,
    const std::vector<cv::Rect>& rois,
    const std::vector<cv::Rect>& ref_rois,
    WBMethod wb_method,
    CCMMethod ccm_method,
    RGBStatMethod stat_method) {

    QuickCalibResult result;

    const std::vector<cv::Rect>& effective_ref_rois = ref_rois.empty() ? rois : ref_rois;

    // 1. 提取输入和参考图像中的颜色
    std::vector<cv::Vec3d> src_colors = WhiteBalance::extractColors(image, rois, stat_method);
    std::vector<cv::Vec3d> ref_colors = WhiteBalance::extractColors(ref_image, effective_ref_rois, stat_method);

    size_t n = std::min({src_colors.size(), ref_colors.size()});
    if (n == 0) return result;

    src_colors.resize(n);
    ref_colors.resize(n);

    // 2. 白平衡标定
    WBCalibParams wb_calib_params;
    wb_calib_params.method = wb_method;
    wb_calib_params.stat_method = stat_method;
    result.wb_params = WhiteBalance::calibrateFromColors(src_colors, ref_colors, wb_calib_params);

    // 3. 应用白平衡
    cv::Mat wb_image = WhiteBalance::apply(image, result.wb_params);

    // 4. 重新提取白平衡后的颜色
    std::vector<cv::Vec3d> wb_colors = WhiteBalance::extractColors(wb_image, rois, stat_method);
    wb_colors.resize(n);

    // 5. CCM标定
    CCMCalibParams ccm_calib_params;
    ccm_calib_params.method = ccm_method;
    result.ccm_params = ColorCorrectionMatrix::calibrate(wb_colors, ref_colors, ccm_calib_params);

    // 6. 应用CCM
    cv::Mat ccm_image = ColorCorrectionMatrix::apply(wb_image, result.ccm_params);
    result.corrected_image = ccm_image;

    // 7. 计算误差
    std::vector<cv::Vec3d> final_colors = WhiteBalance::extractColors(ccm_image, rois, stat_method);
    final_colors.resize(n);

    double sum_de = 0;
    for (size_t i = 0; i < n; ++i) {
        cv::Vec3d lab1 = ColorErrorCalculator::rgbToLab(final_colors[i]);
        cv::Vec3d lab2 = ColorErrorCalculator::rgbToLab(ref_colors[i]);
        double de = ColorErrorCalculator::deltaE00(lab1, lab2);
        result.deltaE00.push_back(de);
        sum_de += de;
    }
    result.mean_deltaE00 = sum_de / n;

    return result;
}

double QuickColorCalibrator::autoExposure(const cv::Mat& image, const cv::Rect& roi,
                                           double target_min, double target_max) {
    double brightness = WhiteBalance::computeBrightness(image, roi);
    if (brightness < 1e-6) return 1.0;

    double target_mid = (target_min + target_max) / 2.0;
    return target_mid / brightness;
}

bool QuickColorCalibrator::checkBrightness(double brightness) {
    return brightness >= 0.55 && brightness <= 0.65;
}

std::vector<cv::Rect> QuickColorCalibrator::generate24ColorROI(
    int image_width, int image_height,
    int chart_x, int chart_y, int chart_w, int chart_h) {

    std::vector<cv::Rect> rois;
    int cols = 4, rows = 6;
    int patch_w = chart_w / cols;
    int patch_h = chart_h / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int x = chart_x + c * patch_w + patch_w / 4;
            int y = chart_y + r * patch_h + patch_h / 4;
            int w = patch_w / 2;
            int h = patch_h / 2;
            rois.push_back(cv::Rect(x, y, w, h));
        }
    }

    return rois;
}

} // namespace mvtk
