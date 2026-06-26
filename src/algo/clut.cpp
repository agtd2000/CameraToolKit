/**
 * @file clut.cpp
 * @brief 颜色查找表（CLUT）算法实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>

#include "algo/clut.h"

namespace mvtk {

cv::Mat ColorLookupTable::calibrate(
    const std::vector<cv::Vec3d>& src_colors,
    const std::vector<cv::Vec3d>& ref_colors,
    const CLUTCalibParams& params) {

    int grid = params.grid_size;
    // 3D LUT: grid x grid x grid x 3
    cv::Mat lut = cv::Mat::zeros(grid, grid * grid, CV_64FC3);

    if (src_colors.empty() || src_colors.size() != ref_colors.size()) return lut;

    // 简化的3D LUT标定：基于散点数据插值
    // 1. 将源颜色映射到LUT网格位置
    // 2. 在对应位置设置目标颜色值
    // 3. 对未设置的网格点进行三线性插值

    // 标记哪些网格点有数据
    cv::Mat visited = cv::Mat::zeros(grid, grid * grid, CV_8U);

    for (size_t i = 0; i < src_colors.size(); ++i) {
        cv::Vec3d src = src_colors[i] / 255.0;
        cv::Vec3d ref = ref_colors[i] / 255.0;

        // 映射到网格索引
        int r_idx = std::min(grid - 1, static_cast<int>(src[0] * (grid - 1)));
        int g_idx = std::min(grid - 1, static_cast<int>(src[1] * (grid - 1)));
        int b_idx = std::min(grid - 1, static_cast<int>(src[2] * (grid - 1)));

        lut.at<cv::Vec3d>(r_idx, g_idx * grid + b_idx) = ref;
        visited.at<uint8_t>(r_idx, g_idx * grid + b_idx) = 1;
    }

    // 三线性插值填充空缺网格点
    // 简化实现：使用高斯平滑进行扩散
    cv::Mat lut_float;
    lut.convertTo(lut_float, CV_32FC3);

    // 多次高斯模糊实现平滑扩散
    for (int iter = 0; iter < static_cast<int>(params.smooth_factor * 10); ++iter) {
        cv::GaussianBlur(lut_float, lut_float, cv::Size(3, 3), 0);
    }

    lut_float.convertTo(lut, CV_64FC3);
    return lut;
}

cv::Mat ColorLookupTable::apply(const cv::Mat& image, const cv::Mat& lut) {
    if (image.empty() || lut.empty()) return image.clone();

    int grid = lut.rows;
    cv::Mat result = cv::Mat::zeros(image.size(), image.type());

    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            cv::Vec3b pixel = image.at<cv::Vec3b>(y, x);

            // BGR -> RGB
            double r = pixel[2] / 255.0;
            double g = pixel[1] / 255.0;
            double b = pixel[0] / 255.0;

            // 映射到LUT索引
            int r_idx = std::min(grid - 1, static_cast<int>(r * (grid - 1)));
            int g_idx = std::min(grid - 1, static_cast<int>(g * (grid - 1)));
            int b_idx = std::min(grid - 1, static_cast<int>(b * (grid - 1)));

            cv::Vec3d mapped = lut.at<cv::Vec3d>(r_idx, g_idx * grid + b_idx);

            // RGB -> BGR
            result.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uint8_t>(std::round(std::max(0.0, std::min(255.0, mapped[2] * 255.0)))),
                static_cast<uint8_t>(std::round(std::max(0.0, std::min(255.0, mapped[1] * 255.0)))),
                static_cast<uint8_t>(std::round(std::max(0.0, std::min(255.0, mapped[0] * 255.0))))
            );
        }
    }

    return result;
}

bool ColorLookupTable::exportLUT(const cv::Mat& lut, const std::string& filepath) {
    cv::FileStorage fs(filepath, cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;
    fs << "clut" << lut;
    return true;
}

cv::Mat ColorLookupTable::importLUT(const std::string& filepath) {
    cv::FileStorage fs(filepath, cv::FileStorage::READ);
    cv::Mat lut;
    if (fs.isOpened()) {
        fs["clut"] >> lut;
    }
    return lut;
}

} // namespace mvtk
