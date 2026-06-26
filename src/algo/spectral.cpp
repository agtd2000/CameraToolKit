/**
 * @file spectral.cpp
 * @brief 光谱特性标定算法实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>

#include "algo/spectral.h"

namespace mvtk {

SpectralCurve SpectralCalibrator::captureCurve(const cv::Mat& image, const cv::Rect& roi) {
    SpectralCurve curve;
    if (image.empty()) return curve;

    cv::Rect clamped = roi & cv::Rect(0, 0, image.cols, image.rows);
    if (clamped.area() <= 0) return curve;

    cv::Mat region = image(clamped);
    cv::Scalar mean = cv::mean(region);

    // 模拟光谱曲线：基于图像颜色生成380-780nm范围的反射光谱
    // 实际应用中需要光谱仪采集真实数据
    int num_bands = 41; // 10nm间隔，380-780nm
    curve.wavelengths.resize(num_bands);
    curve.intensities.resize(num_bands);

    double r = mean[2] / 255.0;
    double g = mean[1] / 255.0;
    double b = mean[0] / 255.0;

    for (int i = 0; i < num_bands; ++i) {
        double wl = 380.0 + i * 10.0;
        curve.wavelengths[i] = wl;

        // 简化的光谱响应模型
        double intensity = 0.0;
        if (wl < 440) {
            intensity = b * (440.0 - wl) / 60.0 * 0.5;
        } else if (wl < 490) {
            double t = (wl - 440.0) / 50.0;
            intensity = b * (1.0 - t) + g * t;
        } else if (wl < 510) {
            double t = (wl - 490.0) / 20.0;
            intensity = g * (1.0 - t) + g * t;
        } else if (wl < 580) {
            double t = (wl - 510.0) / 70.0;
            intensity = g * (1.0 - t) + r * t;
        } else if (wl < 645) {
            double t = (wl - 580.0) / 65.0;
            intensity = r * (1.0 - t * 0.3);
        } else {
            intensity = r * 0.7 * (700.0 - wl) / 55.0;
        }

        curve.intensities[i] = std::max(0.0, std::min(1.0, intensity));
    }

    return curve;
}

void SpectralCalibrator::addLightSource(std::vector<LightSourceInfo>& light_sources,
                                         const std::string& name,
                                         double color_temp,
                                         const std::vector<SpectralCurve>& curves) {
    LightSourceInfo info;
    info.name = name;
    info.color_temperature = color_temp;
    info.curves = curves;
    light_sources.push_back(info);
}

CalibRegionResult SpectralCalibrator::computeCalibRegion(
    double focal_length, double working_distance,
    double sensor_width, double sensor_height,
    int method) {
    CalibRegionResult result;

    if (method == 1) {
        // 方法1：已知镜头焦距和工作距离
        double magnification = focal_length / (working_distance - focal_length);
        double object_width = sensor_width / magnification;
        double object_height = sensor_height / magnification;
        result.region_size = std::sqrt(object_width * object_width + object_height * object_height);
        result.center_offset = 0.0;
    } else {
        // 方法2：已知物方视野范围
        result.region_size = std::sqrt(sensor_width * sensor_width + sensor_height * sensor_height);
        result.center_offset = 0.0;
    }

    return result;
}

bool SpectralCalibrator::generateCalibFile(
    const std::vector<LightSourceInfo>& light_sources,
    SpectralCalibMode mode,
    SpectralComplexity complexity,
    const std::string& save_path) {

    if (light_sources.empty()) return false;
    if (light_sources.size() > 20) return false; // 最多20种光源

    std::ofstream ofs(save_path, std::ios::binary);
    if (!ofs.is_open()) return false;

    // 写入文件头
    uint32_t magic = 0x53504342; // "SPCB"
    uint32_t version = 1;
    uint32_t num_sources = static_cast<uint32_t>(light_sources.size());
    uint32_t mode_val = static_cast<uint32_t>(mode);
    uint32_t complexity_val = static_cast<uint32_t>(complexity);

    ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));
    ofs.write(reinterpret_cast<const char*>(&num_sources), sizeof(num_sources));
    ofs.write(reinterpret_cast<const char*>(&mode_val), sizeof(mode_val));
    ofs.write(reinterpret_cast<const char*>(&complexity_val), sizeof(complexity_val));

    // 写入各光源数据
    for (const auto& ls : light_sources) {
        // 光源名称（固定64字节）
        char name_buf[64] = {};
        strncpy(name_buf, ls.name.c_str(), 63);
        ofs.write(name_buf, 64);

        // 色温
        double ct = ls.color_temperature;
        ofs.write(reinterpret_cast<const char*>(&ct), sizeof(ct));

        // 曲线数量
        uint32_t num_curves = static_cast<uint32_t>(ls.curves.size());
        ofs.write(reinterpret_cast<const char*>(&num_curves), sizeof(num_curves));

        for (const auto& curve : ls.curves) {
            uint32_t num_bands = static_cast<uint32_t>(curve.wavelengths.size());
            ofs.write(reinterpret_cast<const char*>(&num_bands), sizeof(num_bands));

            for (size_t i = 0; i < num_bands; ++i) {
                double wl = curve.wavelengths[i];
                double intensity = curve.intensities[i];
                ofs.write(reinterpret_cast<const char*>(&wl), sizeof(wl));
                ofs.write(reinterpret_cast<const char*>(&intensity), sizeof(intensity));
            }
        }
    }

    return true;
}

std::vector<LightSourceInfo> SpectralCalibrator::importFactoryCalibFile(const std::string& filepath) {
    std::vector<LightSourceInfo> result;
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) return result;

    uint32_t magic, version, num_sources, mode_val, complexity_val;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    ifs.read(reinterpret_cast<char*>(&num_sources), sizeof(num_sources));
    ifs.read(reinterpret_cast<char*>(&mode_val), sizeof(mode_val));
    ifs.read(reinterpret_cast<char*>(&complexity_val), sizeof(complexity_val));

    if (magic != 0x53504342) return result;

    for (uint32_t i = 0; i < num_sources; ++i) {
        LightSourceInfo ls;
        char name_buf[64] = {};
        ifs.read(name_buf, 64);
        ls.name = name_buf;

        ifs.read(reinterpret_cast<char*>(&ls.color_temperature), sizeof(double));

        uint32_t num_curves;
        ifs.read(reinterpret_cast<char*>(&num_curves), sizeof(num_curves));

        for (uint32_t j = 0; j < num_curves; ++j) {
            SpectralCurve curve;
            uint32_t num_bands;
            ifs.read(reinterpret_cast<char*>(&num_bands), sizeof(num_bands));

            curve.wavelengths.resize(num_bands);
            curve.intensities.resize(num_bands);

            for (uint32_t k = 0; k < num_bands; ++k) {
                ifs.read(reinterpret_cast<char*>(&curve.wavelengths[k]), sizeof(double));
                ifs.read(reinterpret_cast<char*>(&curve.intensities[k]), sizeof(double));
            }

            ls.curves.push_back(curve);
        }

        result.push_back(ls);
    }

    return result;
}

cv::Mat SpectralCalibrator::previewSpectralCurves(const std::vector<LightSourceInfo>& light_sources) {
    int width = 800, height = 600;
    cv::Mat canvas = cv::Mat::zeros(height, width, CV_8UC3);
    canvas.setTo(cv::Scalar(30, 30, 30));

    int margin = 60;
    int plot_w = width - 2 * margin;
    int plot_h = height - 2 * margin;

    // 绘制坐标轴
    cv::line(canvas, cv::Point(margin, margin), cv::Point(margin, height - margin), cv::Scalar(200, 200, 200));
    cv::line(canvas, cv::Point(margin, height - margin), cv::Point(width - margin, height - margin), cv::Scalar(200, 200, 200));

    // 标注
    cv::putText(canvas, "Wavelength (nm)", cv::Point(width / 2 - 60, height - 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200));
    cv::putText(canvas, "Intensity", cv::Point(5, height / 2), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200));

    // 颜色列表
    cv::Scalar colors[] = {
        cv::Scalar(0, 200, 255), cv::Scalar(0, 255, 0), cv::Scalar(255, 200, 0),
        cv::Scalar(255, 0, 0), cv::Scalar(255, 0, 255), cv::Scalar(0, 255, 255),
        cv::Scalar(200, 200, 0), cv::Scalar(200, 0, 200)
    };

    int color_idx = 0;
    for (const auto& ls : light_sources) {
        cv::Scalar color = colors[color_idx % 8];
        color_idx++;

        for (const auto& curve : ls.curves) {
            if (curve.wavelengths.size() < 2) continue;

            for (size_t i = 1; i < curve.wavelengths.size(); ++i) {
                double wl1 = curve.wavelengths[i - 1];
                double wl2 = curve.wavelengths[i];
                double int1 = curve.intensities[i - 1];
                double int2 = curve.intensities[i];

                int x1 = margin + static_cast<int>((wl1 - 380.0) / 400.0 * plot_w);
                int y1 = height - margin - static_cast<int>(int1 * plot_h);
                int x2 = margin + static_cast<int>((wl2 - 380.0) / 400.0 * plot_w);
                int y2 = height - margin - static_cast<int>(int2 * plot_h);

                cv::line(canvas, cv::Point(x1, y1), cv::Point(x2, y2), color, 1);
            }
        }

        // 图例
        int legend_y = margin + 20 + color_idx * 20;
        cv::putText(canvas, ls.name + " (" + std::to_string(static_cast<int>(ls.color_temperature)) + "K)",
                    cv::Point(width - margin - 150, legend_y), cv::FONT_HERSHEY_SIMPLEX, 0.4, color);
    }

    return canvas;
}

} // namespace mvtk
