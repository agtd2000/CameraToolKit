/**
 * @file flat_field.cpp
 * @brief 平场校正（镜头阴影校正 LSC）算法实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <limits>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "algo/flat_field.h"

namespace mvtk {

FlatFieldResult FlatFieldCorrector::calibrate(const cv::Mat& flat_image, const FlatFieldCalibParams& params) {
    FlatFieldResult result;
    if (flat_image.empty()) return result;

    cv::Mat flat_f;
    flat_image.convertTo(flat_f, CV_64F);

    int rows = flat_f.rows;
    int cols = flat_f.cols;

    // 确定基准亮度
    double reference_brightness = 0.0;
    cv::Scalar mean_scalar = cv::mean(flat_f);

    switch (params.mode) {
    case LSCCalibMode::CENTER: {
        // 取中心区域均值作为基准
        int cx = cols / 2, cy = rows / 2;
        int radius = std::min(cols, rows) / 8;
        cv::Rect center_roi(cx - radius, cy - radius, radius * 2, radius * 2);
        cv::Scalar center_mean = cv::mean(flat_f(center_roi));
        if (flat_f.channels() == 1) {
            reference_brightness = center_mean[0];
        } else {
            reference_brightness = (center_mean[0] + center_mean[1] + center_mean[2]) / 3.0;
        }
        break;
    }
    case LSCCalibMode::BRIGHTEST: {
        // 取最亮区域均值作为基准
        double max_val = 0.0;
        int block_size = std::min(cols, rows) / 8;
        for (int y = 0; y < rows - block_size; y += block_size) {
            for (int x = 0; x < cols - block_size; x += block_size) {
                cv::Rect roi(x, y, block_size, block_size);
                cv::Scalar block_mean = cv::mean(flat_f(roi));
                double val = (block_mean[0] + block_mean[1] + block_mean[2]) / (flat_f.channels() > 1 ? 3.0 : 1.0);
                if (val > max_val) max_val = val;
            }
        }
        reference_brightness = max_val;
        break;
    }
    case LSCCalibMode::TARGET_LUMA:
        reference_brightness = params.target_brightness;
        break;
    }

    // 计算增益图
    result.gain_map = cv::Mat::zeros(rows, cols, CV_64FC(flat_f.channels()));

    if (flat_f.channels() == 1) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                double val = flat_f.at<double>(y, x);
                double gain = (val > 1e-6) ? reference_brightness / val : 1.0;
                result.gain_map.at<double>(y, x) = gain;
            }
        }
    } else {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                cv::Vec3d val = flat_f.at<cv::Vec3d>(y, x);
                if (params.channel_mode == 0) {
                    for (int c = 0; c < 3; ++c) {
                        double gain = (val[c] > 1e-6) ? reference_brightness / val[c] : 1.0;
                        result.gain_map.at<cv::Vec3d>(y, x)[c] = gain;
                    }
                } else {
                    double avg = (val[0] + val[1] + val[2]) / 3.0;
                    double gain = (avg > 1e-6) ? reference_brightness / avg : 1.0;
                    result.gain_map.at<cv::Vec3d>(y, x) = cv::Vec3d(gain, gain, gain);
                }
            }
        }
    }

    // 对增益图进行高斯平滑，避免校正后出现块状伪影
    cv::Mat smoothed;
    int ksize = params.smooth_kernel;
    if (ksize % 2 == 0) ksize++;
    if (ksize >= 3) {
        cv::GaussianBlur(result.gain_map, smoothed, cv::Size(ksize, ksize), 0);
    } else {
        smoothed = result.gain_map;
    }

    // 边缘填充处理
    if (params.edge_fill > 0) {
        cv::copyMakeBorder(smoothed, result.gain_map, params.edge_fill, params.edge_fill,
                           params.edge_fill, params.edge_fill, cv::BORDER_REFLECT_101);
        // 裁剪回原始尺寸
        result.gain_map = result.gain_map(cv::Rect(params.edge_fill, params.edge_fill, cols, rows)).clone();
    } else {
        result.gain_map = smoothed;
    }

    // 应用校正
    result.corrected_image = apply(flat_image, result.gain_map, params.edge_fill);

    // 计算均匀性指标
    result.uniformity = computeUniformity(result.corrected_image);

    // 计算中心和边缘亮度
    int cx = cols / 2, cy = rows / 2;
    int r = std::min(cols, rows) / 8;
    cv::Rect center_roi(cx - r, cy - r, r * 2, r * 2);
    cv::Scalar center_mean = cv::mean(result.corrected_image(center_roi));
    result.center_brightness = (center_mean[0] + center_mean[1] + center_mean[2]) / (flat_f.channels() > 1 ? 3.0 : 1.0);

    // 边缘区域（取四角均值）
    int edge_r = std::min(cols, rows) / 8;
    double edge_sum = 0;
    int edge_count = 0;
    std::vector<cv::Rect> corners = {
        cv::Rect(0, 0, edge_r, edge_r),
        cv::Rect(cols - edge_r, 0, edge_r, edge_r),
        cv::Rect(0, rows - edge_r, edge_r, edge_r),
        cv::Rect(cols - edge_r, rows - edge_r, edge_r, edge_r)
    };
    for (const auto& cr : corners) {
        cv::Scalar m = cv::mean(result.corrected_image(cr));
        edge_sum += (m[0] + m[1] + m[2]) / (flat_f.channels() > 1 ? 3.0 : 1.0);
        edge_count++;
    }
    result.edge_brightness = edge_sum / edge_count;

    return result;
}

FlatFieldResult FlatFieldCorrector::calibrateWithDark(
    const cv::Mat& flat_image, const cv::Mat& dark_image, const FlatFieldCalibParams& params) {
    cv::Mat flat_f, dark_f;
    flat_image.convertTo(flat_f, CV_64F);
    dark_image.convertTo(dark_f, CV_64F);

    cv::Mat corrected_flat = flat_f - dark_f;
    cv::max(corrected_flat, 0.0, corrected_flat);

    FlatFieldResult result;
    if (corrected_flat.empty()) return result;

    int rows = corrected_flat.rows;
    int cols = corrected_flat.cols;

    double reference_brightness = 0.0;

    switch (params.mode) {
    case LSCCalibMode::CENTER: {
        int cx = cols / 2, cy = rows / 2;
        int radius = std::min(cols, rows) / 8;
        cv::Rect center_roi(cx - radius, cy - radius, radius * 2, radius * 2);
        cv::Scalar center_mean = cv::mean(corrected_flat(center_roi));
        if (corrected_flat.channels() == 1) {
            reference_brightness = center_mean[0];
        } else {
            reference_brightness = (center_mean[0] + center_mean[1] + center_mean[2]) / 3.0;
        }
        break;
    }
    case LSCCalibMode::BRIGHTEST: {
        double max_val = 0.0;
        int block_size = std::min(cols, rows) / 8;
        for (int y = 0; y < rows - block_size; y += block_size) {
            for (int x = 0; x < cols - block_size; x += block_size) {
                cv::Rect roi(x, y, block_size, block_size);
                cv::Scalar block_mean = cv::mean(corrected_flat(roi));
                double val = (block_mean[0] + block_mean[1] + block_mean[2]) / (corrected_flat.channels() > 1 ? 3.0 : 1.0);
                if (val > max_val) max_val = val;
            }
        }
        reference_brightness = max_val;
        break;
    }
    case LSCCalibMode::TARGET_LUMA:
        reference_brightness = params.target_brightness;
        break;
    }

    result.gain_map = cv::Mat::zeros(rows, cols, CV_64FC(corrected_flat.channels()));

    if (corrected_flat.channels() == 1) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                double val = corrected_flat.at<double>(y, x);
                double gain = (val > 1e-6) ? reference_brightness / val : 1.0;
                result.gain_map.at<double>(y, x) = gain;
            }
        }
    } else {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                cv::Vec3d val = corrected_flat.at<cv::Vec3d>(y, x);
                if (params.channel_mode == 0) {
                    for (int c = 0; c < 3; ++c) {
                        double gain = (val[c] > 1e-6) ? reference_brightness / val[c] : 1.0;
                        result.gain_map.at<cv::Vec3d>(y, x)[c] = gain;
                    }
                } else {
                    double avg = (val[0] + val[1] + val[2]) / 3.0;
                    double gain = (avg > 1e-6) ? reference_brightness / avg : 1.0;
                    result.gain_map.at<cv::Vec3d>(y, x) = cv::Vec3d(gain, gain, gain);
                }
            }
        }
    }

    cv::Mat smoothed;
    int ksize = params.smooth_kernel;
    if (ksize % 2 == 0) ksize++;
    if (ksize >= 3) {
        cv::GaussianBlur(result.gain_map, smoothed, cv::Size(ksize, ksize), 0);
    } else {
        smoothed = result.gain_map;
    }

    if (params.edge_fill > 0) {
        cv::copyMakeBorder(smoothed, result.gain_map, params.edge_fill, params.edge_fill,
                           params.edge_fill, params.edge_fill, cv::BORDER_REFLECT_101);
        result.gain_map = result.gain_map(cv::Rect(params.edge_fill, params.edge_fill, cols, rows)).clone();
    } else {
        result.gain_map = smoothed;
    }

    result.corrected_image = apply(flat_image, result.gain_map, params.edge_fill);
    result.uniformity = computeUniformity(result.corrected_image);

    int cx = cols / 2, cy = rows / 2;
    int r = std::min(cols, rows) / 8;
    cv::Rect center_roi(cx - r, cy - r, r * 2, r * 2);
    cv::Scalar center_mean = cv::mean(result.corrected_image(center_roi));
    result.center_brightness = (center_mean[0] + center_mean[1] + center_mean[2]) / (corrected_flat.channels() > 1 ? 3.0 : 1.0);

    int edge_r = std::min(cols, rows) / 8;
    double edge_sum = 0;
    int edge_count = 0;
    std::vector<cv::Rect> corners = {
        cv::Rect(0, 0, edge_r, edge_r),
        cv::Rect(cols - edge_r, 0, edge_r, edge_r),
        cv::Rect(0, rows - edge_r, edge_r, edge_r),
        cv::Rect(cols - edge_r, rows - edge_r, edge_r, edge_r)
    };
    for (const auto& cr : corners) {
        cv::Scalar m = cv::mean(result.corrected_image(cr));
        edge_sum += (m[0] + m[1] + m[2]) / (corrected_flat.channels() > 1 ? 3.0 : 1.0);
        edge_count++;
    }
    result.edge_brightness = edge_sum / edge_count;

    return result;
}

cv::Mat FlatFieldCorrector::apply(const cv::Mat& image, const cv::Mat& gain_map, int edge_fill) {
    if (image.empty() || gain_map.empty()) return image.clone();
    if (image.size() != gain_map.size()) return image.clone();

    cv::Mat img_f;
    image.convertTo(img_f, CV_64F);

    cv::Mat result_f;
    if (img_f.channels() == 1 && gain_map.channels() == 1) {
        result_f = img_f.mul(gain_map);
    } else if (img_f.channels() == 3 && gain_map.channels() == 3) {
        result_f = img_f.mul(gain_map);
    } else if (img_f.channels() == 3 && gain_map.channels() == 1) {
        cv::Mat gain_3ch;
        cv::merge(std::vector<cv::Mat>{gain_map, gain_map, gain_map}, gain_3ch);
        result_f = img_f.mul(gain_3ch);
    } else {
        result_f = img_f.mul(gain_map);
    }

    double max_val = 255.0;
    int depth = image.depth();
    if (depth == CV_8U) {
        max_val = 255.0;
    } else if (depth == CV_8S) {
        max_val = 127.0;
    } else if (depth == CV_16U) {
        max_val = 65535.0;
    } else if (depth == CV_16S) {
        max_val = 32767.0;
    } else if (depth == CV_32S) {
        max_val = static_cast<double>(std::numeric_limits<int>::max());
    } else if (depth == CV_32F) {
        max_val = 1.0;
    } else if (depth == CV_64F) {
        max_val = 1.0;
    }

    cv::min(result_f, max_val, result_f);
    cv::max(result_f, 0.0, result_f);

    cv::Mat result;
    result_f.convertTo(result, image.type());
    return result;
}

bool FlatFieldCorrector::exportGainMap(const cv::Mat& gain_map, const std::string& filepath) {
    return cv::FileStorage(filepath, cv::FileStorage::WRITE).isOpened() &&
           [&]() { cv::FileStorage fs(filepath, cv::FileStorage::WRITE); fs << "gain_map" << gain_map; return true; }();
}

cv::Mat FlatFieldCorrector::importGainMap(const std::string& filepath) {
    cv::FileStorage fs(filepath, cv::FileStorage::READ);
    cv::Mat gain_map;
    if (fs.isOpened()) {
        fs["gain_map"] >> gain_map;
    }
    return gain_map;
}

double FlatFieldCorrector::computeUniformity(const cv::Mat& image, cv::Rect roi) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    if (roi.area() > 0) gray = gray(roi);

    cv::Scalar mean, stddev;
    cv::meanStdDev(gray, mean, stddev);

    double m = mean[0];
    double s = stddev[0];

    // 均匀性 = 1 - 变异系数
    return (m > 1e-6) ? (1.0 - s / m) : 0.0;
}

cv::Mat FlatFieldCorrector::generateVignetting(int width, int height, double falloff) {
    cv::Mat result = cv::Mat::zeros(height, width, CV_64FC1);
    double cx = width / 2.0;
    double cy = height / 2.0;
    double max_dist = std::sqrt(cx * cx + cy * cy);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            double normalized_dist = dist / max_dist;
            double brightness = 1.0 - falloff * normalized_dist * normalized_dist;
            result.at<double>(y, x) = brightness * 255.0;
        }
    }

    cv::Mat result_u8;
    result.convertTo(result_u8, CV_8U);
    return result_u8;
}

} // namespace mvtk
