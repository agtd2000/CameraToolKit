/**
 * @file white_balance.cpp
 * @brief 白平衡标定与校正算法实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <algorithm>
#include <numeric>
#include <cmath>

#include "algo/white_balance.h"

namespace mvtk {

WBCorrectParams WhiteBalance::calibrateFromColors(
    const std::vector<cv::Vec3d>& src_colors,
    const std::vector<cv::Vec3d>& ref_colors,
    const WBCalibParams& params) {

    WBCorrectParams result;
    if (src_colors.empty()) return result;

    if (params.method == WBMethod::AVERAGE) {
        // 平均法：各色块分别计算WB参数后取平均
        double r_sum = 0, g_sum = 0, b_sum = 0;
        double r_ref_sum = 0, g_ref_sum = 0, b_ref_sum = 0;

        for (size_t i = 0; i < src_colors.size(); ++i) {
            r_sum += src_colors[i][0];
            g_sum += src_colors[i][1];
            b_sum += src_colors[i][2];
        }

        // 标准模式：以G通道为基准
        double avg_r = r_sum / src_colors.size();
        double avg_g = g_sum / src_colors.size();
        double avg_b = b_sum / src_colors.size();

        if (avg_g > 1e-6) {
            result.r_gain = avg_g / avg_r;
            result.g_gain = 1.0;
            result.b_gain = avg_g / avg_b;
        }
    } else {
        // 解析法：按误差最小原则计算WB参数
        // 最小化 sum(||WB*src - ref||^2)
        // 简化：对R和B通道分别求解最优增益
        double sr = 0, sg = 0, sb = 0;
        double rr = 0, rg = 0, rb = 0;

        for (size_t i = 0; i < src_colors.size(); ++i) {
            sr += src_colors[i][0];
            sg += src_colors[i][1];
            sb += src_colors[i][2];

            if (!ref_colors.empty() && i < ref_colors.size()) {
                rr += ref_colors[i][0];
                rg += ref_colors[i][1];
                rb += ref_colors[i][2];
            }
        }

        if (!ref_colors.empty()) {
            // 有参考颜色时，以参考颜色为目标
            if (sr > 1e-6) result.r_gain = rr / sr;
            result.g_gain = 1.0;
            if (sb > 1e-6) result.b_gain = rb / sb;
        } else {
            // 无参考颜色时，以G通道为基准
            if (sr > 1e-6) result.r_gain = sg / sr;
            result.g_gain = 1.0;
            if (sb > 1e-6) result.b_gain = sg / sb;
        }
    }

    result.quant_coeff = params.quant_coeff;
    return result;
}

WBCorrectParams WhiteBalance::grayWorld(const cv::Mat& image) {
    WBCorrectParams result;
    if (image.empty()) return result;

    cv::Scalar mean = cv::mean(image);
    double avg_g = mean[1];

    if (mean[0] > 1e-6) result.r_gain = avg_g / mean[0];
    result.g_gain = 1.0;
    if (mean[2] > 1e-6) result.b_gain = avg_g / mean[2];

    return result;
}

WBCorrectParams WhiteBalance::perfectReflector(const cv::Mat& image, double ratio) {
    WBCorrectParams result;
    if (image.empty()) return result;

    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    // 计算亮度直方图
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat hist;
    cv::calcHist(&gray, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);

    // 找到亮度前ratio比例的像素阈值
    int total = gray.rows * gray.cols;
    int count = 0;
    int threshold = 255;
    for (int i = 255; i >= 0; --i) {
        count += static_cast<int>(hist.at<float>(i));
        if (count >= total * ratio) {
            threshold = i;
            break;
        }
    }

    // 计算高亮区域的平均RGB
    cv::Mat mask;
    cv::compare(gray, threshold, mask, cv::CMP_GE);
    cv::Scalar mean = cv::mean(image, mask);

    double avg_g = mean[1];
    if (mean[0] > 1e-6) result.r_gain = avg_g / mean[0];
    result.g_gain = 1.0;
    if (mean[2] > 1e-6) result.b_gain = avg_g / mean[2];

    return result;
}

cv::Mat WhiteBalance::apply(const cv::Mat& image, const WBCorrectParams& params) {
    if (image.empty()) return image.clone();

    cv::Mat result;
    image.convertTo(result, CV_64F);

    if (result.channels() == 3) {
        // BGR顺序
        for (int y = 0; y < result.rows; ++y) {
            for (int x = 0; x < result.cols; ++x) {
                cv::Vec3d& pixel = result.at<cv::Vec3d>(y, x);
                pixel[0] *= params.b_gain; // B
                pixel[1] *= params.g_gain; // G
                pixel[2] *= params.r_gain; // R
            }
        }
    } else {
        result *= params.g_gain;
    }

    // 截断
    double max_val = (image.depth() == CV_8U) ? 255.0 : 65535.0;
    cv::min(result, max_val, result);
    cv::max(result, 0.0, result);

    result.convertTo(result, image.type());
    return result;
}

std::vector<cv::Vec3d> WhiteBalance::extractColors(
    const cv::Mat& image, const std::vector<cv::Rect>& rois, RGBStatMethod method) {
    std::vector<cv::Vec3d> colors;
    cv::Mat img_f;
    image.convertTo(img_f, CV_64F);

    for (const auto& roi : rois) {
        cv::Rect clamped = roi & cv::Rect(0, 0, image.cols, image.rows);
        if (clamped.area() <= 0) {
            colors.push_back(cv::Vec3d(0, 0, 0));
            continue;
        }

        cv::Mat region = img_f(clamped);
        if (method == RGBStatMethod::MEAN) {
            cv::Scalar m = cv::mean(region);
            colors.push_back(cv::Vec3d(m[2], m[1], m[0])); // BGR->RGB
        } else {
            // 中值法
            cv::Scalar median;
            for (int c = 0; c < region.channels(); ++c) {
                std::vector<double> vals;
                for (int y = 0; y < region.rows; ++y) {
                    for (int x = 0; x < region.cols; ++x) {
                        if (region.channels() == 1) {
                            vals.push_back(region.at<double>(y, x));
                        } else {
                            vals.push_back(region.at<cv::Vec3d>(y, x)[c]);
                        }
                    }
                }
                std::sort(vals.begin(), vals.end());
                median[c] = vals[vals.size() / 2];
            }
            colors.push_back(cv::Vec3d(median[2], median[1], median[0])); // BGR->RGB
        }
    }

    return colors;
}

double WhiteBalance::computeBrightness(const cv::Mat& image, const cv::Rect& roi) {
    cv::Rect clamped = roi & cv::Rect(0, 0, image.cols, image.rows);
    if (clamped.area() <= 0) return 0.0;

    cv::Mat region = image(clamped);
    cv::Scalar mean = cv::mean(region);

    // G通道归一化到0~1
    double g_mean = (image.channels() >= 2) ? mean[1] : mean[0];
    return g_mean / 255.0;
}

} // namespace mvtk
