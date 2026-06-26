#pragma once
/**
 * @file color_error.h
 * @brief 色彩误差计算算法
 */

#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace mvtk {

struct ColorErrorSettings {
    bool use_srgb = false;
};

struct ColorPairError {
    cv::Vec3d src_color;
    cv::Vec3d ref_color;
    double deltaE00;
    double deltaE76;
    double deltaE94;
    double deltaL;
    double deltaC;
    double deltaH;
};

struct ColorErrorResult {
    std::vector<ColorPairError> pairs;
    double mean_deltaE00;
    double max_deltaE00;
    double min_deltaE00;
};

class ColorErrorCalculator {
public:
    static ColorErrorResult calculate(
        const std::vector<cv::Vec3d>& src_colors,
        const std::vector<cv::Vec3d>& ref_colors,
        const ColorErrorSettings& params = {});

    static double deltaE00(const cv::Vec3d& lab1, const cv::Vec3d& lab2);
    static double deltaE76(const cv::Vec3d& lab1, const cv::Vec3d& lab2);
    static double deltaE94(const cv::Vec3d& lab1, const cv::Vec3d& lab2);
    static cv::Vec3d rgbToLab(const cv::Vec3d& rgb, bool use_srgb = false);
    static double srgbDecode(double c);
    static std::vector<cv::Vec3d> importColorsFromIni(const std::string& filepath);
    static bool exportResult(const ColorErrorResult& result, const std::string& filepath);
};

} // namespace mvtk
