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
#ifdef _OPENMP
#include <omp.h>
#endif

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

    cv::Mat dbl;
    gray.convertTo(dbl, CV_64F);

    cv::Mat gradient_mag;
    if (params.use_gradient_check) {
        cv::Mat grad_x, grad_y;
        cv::Sobel(dbl, grad_x, CV_64F, 1, 0, 3);
        cv::Sobel(dbl, grad_y, CV_64F, 0, 1, 3);
        cv::magnitude(grad_x, grad_y, gradient_mag);
    }

    int half_k = params.kernel_size / 2;
    int rows = dbl.rows;
    int cols = dbl.cols;

#ifdef _OPENMP
    std::vector<std::vector<DeadPixel>> thread_results(omp_get_max_threads());
#endif

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int y = half_k; y < rows - half_k; ++y) {
#ifdef _OPENMP
        int tid = omp_get_thread_num();
#endif
        for (int x = half_k; x < cols - half_k; ++x) {
            double center_val = dbl.at<double>(y, x);

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

            if (mean < params.avg_brightness_min || mean > params.avg_brightness_max) continue;

            double diff = center_val - mean;

            double threshold = params.threshold;
            if (params.use_dynamic_threshold) {
                double variance = 0.0;
                for (double val : neighbors) {
                    variance += (val - mean) * (val - mean);
                }
                variance /= count;
                double stddev = std::sqrt(variance);
                threshold = params.dynamic_factor * stddev;
            }

            bool is_dead = false;
            if (params.type == DeadPixelType::BRIGHT && diff > threshold) {
                is_dead = true;
            } else if (params.type == DeadPixelType::DARK && diff < -threshold) {
                is_dead = true;
            }

            if (is_dead && params.use_gradient_check) {
                double grad_val = gradient_mag.at<double>(y, x);
                if (grad_val > params.gradient_threshold) {
                    is_dead = false;
                }
            }

            if (is_dead) {
                DeadPixel dp;
                dp.x = x;
                dp.y = y;
                dp.type = params.type;
                dp.value = center_val;
#ifdef _OPENMP
                thread_results[tid].push_back(dp);
#else
                result.push_back(dp);
#endif
            }
        }
    }

#ifdef _OPENMP
    for (auto& tr : thread_results) {
        result.insert(result.end(), tr.begin(), tr.end());
    }
#endif

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
    cv::Mat img_f;
    image.convertTo(img_f, CV_64F);
    
    int half_k = 1;
    int depth = image.depth();
    double max_val = 255.0;
    
    if (depth == CV_8U) {
        max_val = 255.0;
    } else if (depth == CV_16U) {
        max_val = 65535.0;
    } else if (depth == CV_32F || depth == CV_64F) {
        max_val = 1.0;
    }

    for (const auto& dp : pixels) {
        if (dp.x < half_k || dp.x >= image.cols - half_k ||
            dp.y < half_k || dp.y >= image.rows - half_k) continue;

        for (int c = 0; c < image.channels(); ++c) {
            std::vector<double> neighbors;
            for (int dy = -half_k; dy <= half_k; ++dy) {
                for (int dx = -half_k; dx <= half_k; ++dx) {
                    if (dy == 0 && dx == 0) continue;
                    if (image.channels() == 1) {
                        neighbors.push_back(img_f.at<double>(dp.y + dy, dp.x + dx));
                    } else {
                        neighbors.push_back(img_f.at<cv::Vec3d>(dp.y + dy, dp.x + dx)[c]);
                    }
                }
            }

            double val;
            if (params.method == 0) {
                val = std::accumulate(neighbors.begin(), neighbors.end(), 0.0) / neighbors.size();
            } else {
                std::sort(neighbors.begin(), neighbors.end());
                val = neighbors[neighbors.size() / 2];
            }
            
            val = std::max(0.0, std::min(max_val, val));
            
            if (image.channels() == 1) {
                if (depth == CV_8U) {
                    result.at<uint8_t>(dp.y, dp.x) = static_cast<uint8_t>(std::round(val));
                } else if (depth == CV_16U) {
                    result.at<uint16_t>(dp.y, dp.x) = static_cast<uint16_t>(std::round(val));
                } else if (depth == CV_32F) {
                    result.at<float>(dp.y, dp.x) = static_cast<float>(val);
                } else if (depth == CV_64F) {
                    result.at<double>(dp.y, dp.x) = val;
                }
            } else {
                if (depth == CV_8U) {
                    result.at<cv::Vec3b>(dp.y, dp.x)[c] = static_cast<uint8_t>(std::round(val));
                } else if (depth == CV_16U) {
                    result.at<cv::Vec3w>(dp.y, dp.x)[c] = static_cast<uint16_t>(std::round(val));
                }
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
