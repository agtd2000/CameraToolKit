/**
 * @file dead_pixel.cpp
 * @brief 坏点检测与校正算法实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>
#include <set>

#include "algo/dead_pixel.h"

namespace mvtk {

std::vector<DeadPixel> DeadPixelDetector::detect(const cv::Mat& image, const DeadPixelDetectParams& params) {
    std::vector<DeadPixel> result;
    if (image.empty()) return result;

    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    // 转换为double进行计算
    cv::Mat dbl;
    gray.convertTo(dbl, CV_64F);

    int half_k = params.kernel_size / 2;
    int rows = dbl.rows;
    int cols = dbl.cols;

    for (int y = half_k; y < rows - half_k; ++y) {
        for (int x = half_k; x < cols - half_k; ++x) {
            double center_val = dbl.at<double>(y, x);

            // 计算邻域统计量（排除中心像素）
            std::vector<double> neighbors;
            double sum = 0.0;
            int count = 0;
            for (int dy = -half_k; dy <= half_k; ++dy) {
                for (int dx = -half_k; dx <= half_k; ++dx) {
                    if (dy == 0 && dx == 0) continue;
                    double val = dbl.at<double>(y + dy, x + dx);
                    neighbors.push_back(val);
                    sum += val;
                    ++count;
                }
            }

            double mean = sum / count;
            double diff = center_val - mean;

            // 检查平均亮度范围
            if (mean < params.avg_brightness_min || mean > params.avg_brightness_max) continue;

            bool is_dead = false;
            if (params.type == DeadPixelType::BRIGHT && diff > params.threshold) {
                is_dead = true;
            } else if (params.type == DeadPixelType::DARK && diff < -params.threshold) {
                is_dead = true;
            }

            if (is_dead) {
                DeadPixel dp;
                dp.x = x;
                dp.y = y;
                dp.type = params.type;
                dp.value = center_val;
                result.push_back(dp);
            }
        }
    }

    return result;
}

std::vector<DeadPixel> DeadPixelDetector::detectMultiFrame(
    const std::vector<cv::Mat>& images, const DeadPixelDetectParams& params) {
    if (images.empty()) return {};

    // 检测每帧的坏点
    std::vector<std::vector<DeadPixel>> frame_pixels;
    for (const auto& img : images) {
        frame_pixels.push_back(detect(img, params));
    }

    if (frame_pixels.size() == 1) return frame_pixels[0];

    // 统计坏点出现次数，仅当在每帧中都存在时才确认为真实坏点
    std::map<std::pair<int,int>, int> pixel_count;
    for (const auto& pixels : frame_pixels) {
        std::set<std::pair<int,int>> frame_set;
        for (const auto& dp : pixels) {
            frame_set.insert({dp.x, dp.y});
        }
        for (const auto& p : frame_set) {
            pixel_count[p]++;
        }
    }

    int total_frames = static_cast<int>(images.size());
    std::vector<DeadPixel> confirmed;
    for (const auto& entry : pixel_count) {
        const std::pair<int, int>& pos = entry.first;
        int count = entry.second;
        if (count >= total_frames) {
            // 从最后一帧获取坏点信息
            for (const auto& dp : frame_pixels.back()) {
                if (dp.x == pos.first && dp.y == pos.second) {
                    confirmed.push_back(dp);
                    break;
                }
            }
        }
    }

    return confirmed;
}

cv::Mat DeadPixelDetector::correct(const cv::Mat& image,
                                    const std::vector<DeadPixel>& pixels,
                                    const DeadPixelCorrectParams& params) {
    if (image.empty() || pixels.empty()) return image.clone();

    cv::Mat result = image.clone();
    int half_k = 1; // 3x3邻域

    for (const auto& dp : pixels) {
        if (dp.x < half_k || dp.x >= image.cols - half_k ||
            dp.y < half_k || dp.y >= image.rows - half_k) continue;

        if (image.channels() == 1) {
            std::vector<double> neighbors;
            for (int dy = -half_k; dy <= half_k; ++dy) {
                for (int dx = -half_k; dx <= half_k; ++dx) {
                    if (dy == 0 && dx == 0) continue;
                    neighbors.push_back(image.at<uint8_t>(dp.y + dy, dp.x + dx));
                }
            }

            double val;
            if (params.method == 0) {
                // 邻域均值
                val = std::accumulate(neighbors.begin(), neighbors.end(), 0.0) / neighbors.size();
            } else {
                // 邻域中值
                std::sort(neighbors.begin(), neighbors.end());
                val = neighbors[neighbors.size() / 2];
            }
            result.at<uint8_t>(dp.y, dp.x) = static_cast<uint8_t>(std::round(val));
        } else {
            // 多通道处理
            for (int c = 0; c < image.channels(); ++c) {
                std::vector<double> neighbors;
                for (int dy = -half_k; dy <= half_k; ++dy) {
                    for (int dx = -half_k; dx <= half_k; ++dx) {
                        if (dy == 0 && dx == 0) continue;
                        neighbors.push_back(image.at<cv::Vec3b>(dp.y + dy, dp.x + dx)[c]);
                    }
                }

                double val;
                if (params.method == 0) {
                    val = std::accumulate(neighbors.begin(), neighbors.end(), 0.0) / neighbors.size();
                } else {
                    std::sort(neighbors.begin(), neighbors.end());
                    val = neighbors[neighbors.size() / 2];
                }
                result.at<cv::Vec3b>(dp.y, dp.x)[c] = static_cast<uint8_t>(std::round(val));
            }
        }
    }

    return result;
}

bool DeadPixelDetector::exportCSV(const std::vector<DeadPixel>& pixels, const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << "x,y,type,value\n";
    for (const auto& dp : pixels) {
        ofs << dp.x << "," << dp.y << ","
            << (dp.type == DeadPixelType::BRIGHT ? "bright" : "dark") << ","
            << std::fixed << dp.value << "\n";
    }
    return true;
}

std::vector<DeadPixel> DeadPixelDetector::importCSV(const std::string& filepath) {
    std::vector<DeadPixel> result;
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return result;

    std::string line;
    std::getline(ifs, line); // 跳过标题行

    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string x_str, y_str, type_str, val_str;
        if (!std::getline(iss, x_str, ',')) continue;
        if (!std::getline(iss, y_str, ',')) continue;
        if (!std::getline(iss, type_str, ',')) continue;
        if (!std::getline(iss, val_str, ',')) continue;

        DeadPixel dp;
        dp.x = std::stoi(x_str);
        dp.y = std::stoi(y_str);
        dp.type = (type_str == "bright") ? DeadPixelType::BRIGHT : DeadPixelType::DARK;
        dp.value = std::stod(val_str);
        result.push_back(dp);
    }

    return result;
}

cv::Mat DeadPixelDetector::visualize(const cv::Mat& image, const std::vector<DeadPixel>& pixels) {
    cv::Mat vis;
    if (image.channels() == 1) {
        cv::cvtColor(image, vis, cv::COLOR_GRAY2BGR);
    } else {
        vis = image.clone();
    }

    for (const auto& dp : pixels) {
        cv::Scalar color = (dp.type == DeadPixelType::BRIGHT)
                               ? cv::Scalar(0, 0, 255)   // 红色=亮坏点
                               : cv::Scalar(255, 0, 0);  // 蓝色=暗坏点
        cv::circle(vis, cv::Point(dp.x, dp.y), 3, color, 1);
    }

    return vis;
}

} // namespace mvtk
