/**
 * @file ccm.cpp
 * @brief 色彩校正矩阵（CCM）标定与校正算法实现
 */

#include "algo/ccm.h"
#include "algo/gamma.h"
#include "algo/color_error.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mvtk {

CCMCorrectParams ColorCorrectionMatrix::calibrate(
    const std::vector<cv::Vec3d>& src_colors,
    const std::vector<cv::Vec3d>& ref_colors,
    const CCMCalibParams& params) {

    CCMCorrectParams result;
    if (src_colors.size() != ref_colors.size() || src_colors.empty()) return result;

    size_t n = src_colors.size();

    // 构建线性方程组: A * ccm_flat = b
    // 每个颜色对提供3个方程
    // A矩阵: 3n x 9, b向量: 3n x 1
    cv::Mat A = cv::Mat::zeros(static_cast<int>(n * 3), 9, CV_64F);
    cv::Mat b = cv::Mat::zeros(static_cast<int>(n * 3), 1, CV_64F);

    for (size_t i = 0; i < n; ++i) {
        // 应用Gamma预处理
        cv::Vec3d src = src_colors[i];
        cv::Vec3d ref = ref_colors[i];

        if (params.gamma_type == GammaType::LRGB2SRGB) {
            for (int c = 0; c < 3; ++c) {
                src[c] = GammaCorrector::sRGBToLinear(src[c] / 255.0) * 255.0;
                ref[c] = GammaCorrector::sRGBToLinear(ref[c] / 255.0) * 255.0;
            }
        } else if (params.gamma_type == GammaType::VALUE) {
            for (int c = 0; c < 3; ++c) {
                src[c] = 255.0 * std::pow(src[c] / 255.0, params.gamma_value);
                ref[c] = 255.0 * std::pow(ref[c] / 255.0, params.gamma_value);
            }
        }

        // 归一化到0-1
        cv::Vec3d src_n = src / 255.0;
        cv::Vec3d ref_n = ref / 255.0;

        int row = static_cast<int>(i * 3);
        // R方程: src_r * ccm[0] + src_g * ccm[1] + src_b * ccm[2] = ref_r
        A.at<double>(row, 0) = src_n[0]; A.at<double>(row, 1) = src_n[1]; A.at<double>(row, 2) = src_n[2];
        b.at<double>(row) = ref_n[0];

        // G方程: src_r * ccm[3] + src_g * ccm[4] + src_b * ccm[5] = ref_g
        A.at<double>(row + 1, 3) = src_n[0]; A.at<double>(row + 1, 4) = src_n[1]; A.at<double>(row + 1, 5) = src_n[2];
        b.at<double>(row + 1) = ref_n[1];

        // B方程: src_r * ccm[6] + src_g * ccm[7] + src_b * ccm[8] = ref_b
        A.at<double>(row + 2, 6) = src_n[0]; A.at<double>(row + 2, 7) = src_n[1]; A.at<double>(row + 2, 8) = src_n[2];
        b.at<double>(row + 2) = ref_n[2];
    }

    cv::Mat ccm_flat;
    if (params.method == CCMMethod::OPTIMAL) {
        // 最优法：使用SVD求解最小二乘
        cv::solve(A, b, ccm_flat, cv::DECOMP_SVD);
    } else {
        // 最小二乘法
        cv::solve(A, b, ccm_flat, cv::DECOMP_NORMAL);
    }

    // 填充3x3矩阵
    result.ccm = cv::Matx33d(
        ccm_flat.at<double>(0), ccm_flat.at<double>(1), ccm_flat.at<double>(2),
        ccm_flat.at<double>(3), ccm_flat.at<double>(4), ccm_flat.at<double>(5),
        ccm_flat.at<double>(6), ccm_flat.at<double>(7), ccm_flat.at<double>(8)
    );
    result.offset = cv::Vec3d(0, 0, 0);
    result.quant_coeff = params.quant_coeff;

    return result;
}

cv::Mat ColorCorrectionMatrix::apply(const cv::Mat& image, const CCMCorrectParams& params) {
    if (image.empty()) return image.clone();

    cv::Mat result = cv::Mat::zeros(image.size(), image.type());
    cv::Mat img_f;
    image.convertTo(img_f, CV_64F);

    for (int y = 0; y < img_f.rows; ++y) {
        for (int x = 0; x < img_f.cols; ++x) {
            cv::Vec3d pixel;
            if (img_f.channels() == 3) {
                // BGR -> RGB
                pixel = cv::Vec3d(img_f.at<cv::Vec3d>(y, x)[2],
                                  img_f.at<cv::Vec3d>(y, x)[1],
                                  img_f.at<cv::Vec3d>(y, x)[0]);
            } else {
                pixel = cv::Vec3d(img_f.at<double>(y, x),
                                  img_f.at<double>(y, x),
                                  img_f.at<double>(y, x));
            }

            // 归一化
            cv::Vec3d p = pixel / 255.0;

            // 应用CCM: RGB_out = CCM * RGB_in + offset
            cv::Vec3d out;
            out[0] = params.ccm(0,0)*p[0] + params.ccm(0,1)*p[1] + params.ccm(0,2)*p[2] + params.offset[0];
            out[1] = params.ccm(1,0)*p[0] + params.ccm(1,1)*p[1] + params.ccm(1,2)*p[2] + params.offset[1];
            out[2] = params.ccm(2,0)*p[0] + params.ccm(2,1)*p[1] + params.ccm(2,2)*p[2] + params.offset[2];

            // 反归一化
            out *= 255.0;

            // 截断
            for (int c = 0; c < 3; ++c) {
                out[c] = std::max(0.0, std::min(255.0, out[c]));
            }

            if (img_f.channels() == 3) {
                // RGB -> BGR
                result.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uint8_t>(std::round(out[2])),
                    static_cast<uint8_t>(std::round(out[1])),
                    static_cast<uint8_t>(std::round(out[0]))
                );
            } else {
                result.at<uint8_t>(y, x) = static_cast<uint8_t>(std::round((out[0] + out[1] + out[2]) / 3.0));
            }
        }
    }

    return result;
}

cv::Mat ColorCorrectionMatrix::applyGamma(const cv::Mat& image, GammaType gamma_type, double gamma_value) {
    if (gamma_type == GammaType::NONE) return image.clone();
    if (gamma_type == GammaType::VALUE) {
        return GammaCorrector::apply(image, gamma_value);
    }
    if (gamma_type == GammaType::LRGB2SRGB) {
        return linearToSRGB(image);
    }
    return image.clone();
}

cv::Mat ColorCorrectionMatrix::linearToSRGB(const cv::Mat& image) {
    cv::Mat result;
    image.convertTo(result, CV_64F, 1.0 / 255.0);

    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            if (result.channels() == 3) {
                cv::Vec3d& p = result.at<cv::Vec3d>(y, x);
                for (int c = 0; c < 3; ++c) p[c] = GammaCorrector::linearToSRGB(p[c]);
            } else {
                result.at<double>(y, x) = GammaCorrector::linearToSRGB(result.at<double>(y, x));
            }
        }
    }

    result.convertTo(result, image.type(), 255.0);
    return result;
}

cv::Mat ColorCorrectionMatrix::sRGBToLinear(const cv::Mat& image) {
    cv::Mat result;
    image.convertTo(result, CV_64F, 1.0 / 255.0);

    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            if (result.channels() == 3) {
                cv::Vec3d& p = result.at<cv::Vec3d>(y, x);
                for (int c = 0; c < 3; ++c) p[c] = GammaCorrector::sRGBToLinear(p[c]);
            } else {
                result.at<double>(y, x) = GammaCorrector::sRGBToLinear(result.at<double>(y, x));
            }
        }
    }

    result.convertTo(result, image.type(), 255.0);
    return result;
}

std::vector<double> ColorCorrectionMatrix::computeErrors(
    const std::vector<cv::Vec3d>& src_colors,
    const std::vector<cv::Vec3d>& ref_colors,
    const CCMCorrectParams& ccm_params) {
    std::vector<double> errors;
    for (size_t i = 0; i < src_colors.size() && i < ref_colors.size(); ++i) {
        cv::Vec3d p = src_colors[i] / 255.0;
        cv::Vec3d out;
        out[0] = ccm_params.ccm(0,0)*p[0] + ccm_params.ccm(0,1)*p[1] + ccm_params.ccm(0,2)*p[2];
        out[1] = ccm_params.ccm(1,0)*p[0] + ccm_params.ccm(1,1)*p[1] + ccm_params.ccm(1,2)*p[2];
        out[2] = ccm_params.ccm(2,0)*p[0] + ccm_params.ccm(2,1)*p[1] + ccm_params.ccm(2,2)*p[2];
        out *= 255.0;

        cv::Vec3d lab1 = mvtk::ColorErrorCalculator::rgbToLab(out);
        cv::Vec3d lab2 = mvtk::ColorErrorCalculator::rgbToLab(ref_colors[i]);
        errors.push_back(mvtk::ColorErrorCalculator::deltaE00(lab1, lab2));
    }
    return errors;
}

namespace {
double computeR2(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    
    size_t n = x.size();
    double sum_x = std::accumulate(x.begin(), x.end(), 0.0);
    double sum_y = std::accumulate(y.begin(), y.end(), 0.0);
    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    
    for (size_t i = 0; i < n; ++i) {
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
        sum_y2 += y[i] * y[i];
    }
    
    double numerator = n * sum_xy - sum_x * sum_y;
    double denominator = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
    
    if (denominator < 1e-10) return 0.0;
    
    double r = numerator / denominator;
    return r * r;
}
}

LinearityCheckResult ColorCorrectionMatrix::checkLinearity(
    const std::vector<cv::Vec3d>& src_colors,
    const std::vector<cv::Vec3d>& ref_colors,
    double min_r2_threshold) {
    
    LinearityCheckResult result;
    result.is_linear = false;
    result.r_channel_r2 = 0.0;
    result.g_channel_r2 = 0.0;
    result.b_channel_r2 = 0.0;
    result.avg_r2 = 0.0;
    result.suggested_min_luma = 0.0;
    result.suggested_max_luma = 0.0;
    
    if (src_colors.size() != ref_colors.size() || src_colors.size() < 3) {
        result.message = "Insufficient color samples, at least 3 color samples required";
        return result;
    }
    
    std::vector<std::pair<double, size_t>> indexed_colors;
    for (size_t i = 0; i < src_colors.size(); ++i) {
        double ref_luma = (ref_colors[i][0] + ref_colors[i][1] + ref_colors[i][2]) / 3.0;
        indexed_colors.emplace_back(ref_luma, i);
    }
    
    std::sort(indexed_colors.begin(), indexed_colors.end());
    
    std::vector<double> ref_luma_values;
    std::vector<double> src_r_values, src_g_values, src_b_values;
    std::vector<double> ref_r_values, ref_g_values, ref_b_values;
    
    for (const auto& entry : indexed_colors) {
        size_t idx = entry.second;
        ref_luma_values.push_back(entry.first);
        src_r_values.push_back(src_colors[idx][0]);
        src_g_values.push_back(src_colors[idx][1]);
        src_b_values.push_back(src_colors[idx][2]);
        ref_r_values.push_back(ref_colors[idx][0]);
        ref_g_values.push_back(ref_colors[idx][1]);
        ref_b_values.push_back(ref_colors[idx][2]);
    }
    
    result.r_channel_r2 = computeR2(ref_r_values, src_r_values);
    result.g_channel_r2 = computeR2(ref_g_values, src_g_values);
    result.b_channel_r2 = computeR2(ref_b_values, src_b_values);
    result.avg_r2 = (result.r_channel_r2 + result.g_channel_r2 + result.b_channel_r2) / 3.0;
    
    result.is_linear = (result.avg_r2 >= min_r2_threshold);
    
    if (result.is_linear) {
        result.message = "Sensor linearity verification passed";
    } else {
        result.message = "Sensor linearity verification failed: R channel R^2=" + std::to_string(result.r_channel_r2) +
                         ", G channel R^2=" + std::to_string(result.g_channel_r2) +
                         ", B channel R^2=" + std::to_string(result.b_channel_r2) +
                         ". Please adjust exposure settings to ensure color chart brightness falls within the camera's linear response region.";
    }
    
    double min_luma = ref_luma_values.front();
    double max_luma = ref_luma_values.back();
    double range = max_luma - min_luma;
    
    result.suggested_min_luma = min_luma + range * 0.1;
    result.suggested_max_luma = max_luma - range * 0.1;
    
    return result;
}

} // namespace mvtk
