#pragma once
/**
 * @file gamma.h
 * @brief Gamma校正算法
 *
 * 实现Gamma校正功能，支持指定Gamma值和自定义Gamma曲线。
 * 包括LRGB2SRGB转换。
 */

#include <opencv2/core.hpp>
#include <vector>

namespace mvtk {

/**
 * @brief Gamma校正类
 */
class GammaCorrector {
public:
    /**
     * @brief 应用Gamma校正
     * @param image 输入图像
     * @param gamma Gamma值
     * @return 校正后的图像
     */
    static cv::Mat apply(const cv::Mat& image, double gamma);

    /**
     * @brief 应用自定义Gamma曲线
     * @param image 输入图像
     * @param curve LUT曲线（256个值）
     * @return 校正后的图像
     */
    static cv::Mat applyCurve(const cv::Mat& image, const std::vector<uint8_t>& curve);

    /**
     * @brief 生成Gamma LUT
     * @param gamma Gamma值
     * @param bits 位深（8/10/12/16）
     * @return LUT表
     */
    static std::vector<uint16_t> generateLUT(double gamma, int bits = 8);

    /**
     * @brief 线性RGB转sRGB
     * @param linear 线性值（0-1）
     * @return sRGB值（0-1）
     */
    static double linearToSRGB(double linear);

    /**
     * @brief sRGB转线性RGB
     * @param srgb sRGB值（0-1）
     * @return 线性值（0-1）
     */
    static double sRGBToLinear(double srgb);
};

} // namespace mvtk
