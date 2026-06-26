/**
 * @file gamma.cpp
 * @brief Gamma校正算法实现
 */

#include "algo/gamma.h"
#include <cmath>
#include <algorithm>

namespace mvtk {

cv::Mat GammaCorrector::apply(const cv::Mat& image, double gamma) {
    if (image.empty()) return image.clone();

    // 构建LUT
    std::vector<uint8_t> lut(256);
    double inv_gamma = 1.0 / gamma;
    for (int i = 0; i < 256; ++i) {
        double val = std::pow(i / 255.0, inv_gamma) * 255.0;
        lut[i] = static_cast<uint8_t>(std::round(std::max(0.0, std::min(255.0, val))));
    }

    cv::Mat result;
    cv::LUT(image, lut, result);
    return result;
}

cv::Mat GammaCorrector::applyCurve(const cv::Mat& image, const std::vector<uint8_t>& curve) {
    if (image.empty() || curve.size() != 256) return image.clone();
    cv::Mat result;
    cv::LUT(image, curve, result);
    return result;
}

std::vector<uint16_t> GammaCorrector::generateLUT(double gamma, int bits) {
    int max_val = (1 << bits) - 1;
    std::vector<uint16_t> lut(max_val + 1);
    double inv_gamma = 1.0 / gamma;

    for (int i = 0; i <= max_val; ++i) {
        double val = std::pow(static_cast<double>(i) / max_val, inv_gamma) * max_val;
        lut[i] = static_cast<uint16_t>(std::round(std::max(0.0, std::min(static_cast<double>(max_val), val))));
    }

    return lut;
}

double GammaCorrector::linearToSRGB(double linear) {
    linear = std::max(0.0, std::min(1.0, linear));
    if (linear <= 0.0031308) {
        return 12.92 * linear;
    }
    return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

double GammaCorrector::sRGBToLinear(double srgb) {
    srgb = std::max(0.0, std::min(1.0, srgb));
    if (srgb <= 0.04045) {
        return srgb / 12.92;
    }
    return std::pow((srgb + 0.055) / 1.055, 2.4);
}

} // namespace mvtk
