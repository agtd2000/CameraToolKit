/**
 * @file color_error.cpp
 * @brief 色彩误差计算算法实现（CIE DeltaE00）
 */

#include "algo/color_error.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mvtk {

// CIE标准光源D65白点
static constexpr double D65_X = 0.95047;
static constexpr double D65_Y = 1.00000;
static constexpr double D65_Z = 1.08883;

double ColorErrorCalculator::srgbDecode(double c) {
    c = std::max(0.0, std::min(1.0, c));
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

cv::Vec3d ColorErrorCalculator::rgbToLab(const cv::Vec3d& rgb, bool use_srgb) {
    // RGB归一化到0-1
    double r = rgb[0] / 255.0;
    double g = rgb[1] / 255.0;
    double b = rgb[2] / 255.0;

    // sRGB解码
    if (use_srgb) {
        r = srgbDecode(r);
        g = srgbDecode(g);
        b = srgbDecode(b);
    }

    // RGB -> XYZ (sRGB D65矩阵)
    double x = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
    double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
    double z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;

    // XYZ归一化
    x /= D65_X;
    y /= D65_Y;
    z /= D65_Z;

    // XYZ -> Lab (CIE标准)
    auto f = [](double t) -> double {
        constexpr double delta = 6.0 / 29.0;
        constexpr double delta3 = delta * delta * delta;
        if (t > delta3) {
            return std::cbrt(t);
        }
        return t / (3.0 * delta * delta) + 4.0 / 29.0;
    };

    double L = 116.0 * f(y) - 16.0;
    double a = 500.0 * (f(x) - f(y));
    double b_val = 200.0 * (f(y) - f(z));

    return cv::Vec3d(L, a, b_val);
}

double ColorErrorCalculator::deltaE76(const cv::Vec3d& lab1, const cv::Vec3d& lab2) {
    double dL = lab1[0] - lab2[0];
    double da = lab1[1] - lab2[1];
    double db = lab1[2] - lab2[2];
    return std::sqrt(dL * dL + da * da + db * db);
}

double ColorErrorCalculator::deltaE94(const cv::Vec3d& lab1, const cv::Vec3d& lab2) {
    double dL = lab1[0] - lab2[0];
    double C1 = std::sqrt(lab1[1] * lab1[1] + lab1[2] * lab1[2]);
    double C2 = std::sqrt(lab2[1] * lab2[1] + lab2[2] * lab2[2]);
    double dC = C1 - C2;

    double da = lab1[1] - lab2[1];
    double db = lab1[2] - lab2[2];
    double dH = std::sqrt(std::max(0.0, da * da + db * db - dC * dC));

    double SL = 1.0;
    double SC = 1.0 + 0.045 * C1;
    double SH = 1.0 + 0.015 * C1;

    double val = (dL / SL) * (dL / SL) + (dC / SC) * (dC / SC) + (dH / SH) * (dH / SH);
    return std::sqrt(std::max(0.0, val));
}

double ColorErrorCalculator::deltaE00(const cv::Vec3d& lab1, const cv::Vec3d& lab2) {
    // CIEDE2000算法实现
    double L1 = lab1[0], a1 = lab1[1], b1 = lab1[2];
    double L2 = lab2[0], a2 = lab2[1], b2 = lab2[2];

    // Step 1: 计算C'ab和h'ab
    double C1_ab = std::sqrt(a1 * a1 + b1 * b1);
    double C2_ab = std::sqrt(a2 * a2 + b2 * b2);
    double C_ab_avg = (C1_ab + C2_ab) / 2.0;

    double C_ab_avg_7 = std::pow(C_ab_avg, 7.0);
    double G = 0.5 * (1.0 - std::sqrt(C_ab_avg_7 / (C_ab_avg_7 + std::pow(25.0, 7.0))));

    double a1_prime = a1 * (1.0 + G);
    double a2_prime = a2 * (1.0 + G);

    double C1_prime = std::sqrt(a1_prime * a1_prime + b1 * b1);
    double C2_prime = std::sqrt(a2_prime * a2_prime + b2 * b2);

    auto calc_h_prime = [](double a, double b) -> double {
        double h = std::atan2(b, a) * 180.0 / M_PI;
        if (h < 0) h += 360.0;
        return h;
    };

    double h1_prime = calc_h_prime(a1_prime, b1);
    double h2_prime = calc_h_prime(a2_prime, b2);

    // Step 2: 计算ΔL', ΔC', ΔH'
    double dL_prime = L2 - L1;
    double dC_prime = C2_prime - C1_prime;

    double dh_prime;
    if (C1_prime * C2_prime == 0.0) {
        dh_prime = 0.0;
    } else if (std::abs(h2_prime - h1_prime) <= 180.0) {
        dh_prime = h2_prime - h1_prime;
    } else if (h2_prime - h1_prime > 180.0) {
        dh_prime = h2_prime - h1_prime - 360.0;
    } else {
        dh_prime = h2_prime - h1_prime + 360.0;
    }

    double dH_prime = 2.0 * std::sqrt(C1_prime * C2_prime) * std::sin(dh_prime / 2.0 * M_PI / 180.0);

    // Step 3: 计算CIEDE2000
    double L_prime_avg = (L1 + L2) / 2.0;
    double C_prime_avg = (C1_prime + C2_prime) / 2.0;

    double h_prime_avg;
    if (C1_prime * C2_prime == 0.0) {
        h_prime_avg = h1_prime + h2_prime;
    } else if (std::abs(h1_prime - h2_prime) <= 180.0) {
        h_prime_avg = (h1_prime + h2_prime) / 2.0;
    } else if (h1_prime + h2_prime < 360.0) {
        h_prime_avg = (h1_prime + h2_prime + 360.0) / 2.0;
    } else {
        h_prime_avg = (h1_prime + h2_prime - 360.0) / 2.0;
    }

    double T = 1.0
        - 0.17 * std::cos((h_prime_avg - 30.0) * M_PI / 180.0)
        + 0.24 * std::cos(2.0 * h_prime_avg * M_PI / 180.0)
        + 0.32 * std::cos((3.0 * h_prime_avg + 6.0) * M_PI / 180.0)
        - 0.20 * std::cos((4.0 * h_prime_avg - 63.0) * M_PI / 180.0);

    double SL = 1.0 + 0.015 * (L_prime_avg - 50.0) * (L_prime_avg - 50.0) / std::sqrt(20.0 + (L_prime_avg - 50.0) * (L_prime_avg - 50.0));
    double SC = 1.0 + 0.045 * C_prime_avg;
    double SH = 1.0 + 0.015 * C_prime_avg * T;

    double C_prime_avg_7 = std::pow(C_prime_avg, 7.0);
    double RT = -std::sin(2.0 * (30.0 * std::exp(-((h_prime_avg - 275.0) / 25.0) * ((h_prime_avg - 275.0) / 25.0))) * M_PI / 180.0)
        * 2.0 * std::sqrt(C_prime_avg_7 / (C_prime_avg_7 + std::pow(25.0, 7.0)));

    double val = (dL_prime / SL) * (dL_prime / SL)
        + (dC_prime / SC) * (dC_prime / SC)
        + (dH_prime / SH) * (dH_prime / SH)
        + RT * (dC_prime / SC) * (dH_prime / SH);

    return std::sqrt(std::max(0.0, val));
}

ColorErrorResult ColorErrorCalculator::calculate(
    const std::vector<cv::Vec3d>& src_colors,
    const std::vector<cv::Vec3d>& ref_colors,
    const ColorErrorSettings& params) {

    ColorErrorResult result;
    if (src_colors.size() != ref_colors.size() || src_colors.empty()) return result;

    double sum_de00 = 0, max_de00 = 0, min_de00 = 1e9;

    for (size_t i = 0; i < src_colors.size(); ++i) {
        cv::Vec3d lab1 = rgbToLab(src_colors[i], params.use_srgb);
        cv::Vec3d lab2 = rgbToLab(ref_colors[i], params.use_srgb);

        ColorPairError pair;
        pair.src_color = src_colors[i];
        pair.ref_color = ref_colors[i];
        pair.deltaE00 = deltaE00(lab1, lab2);
        pair.deltaE76 = deltaE76(lab1, lab2);
        pair.deltaE94 = deltaE94(lab1, lab2);
        pair.deltaL = lab1[0] - lab2[0];

        double C1 = std::sqrt(lab1[1] * lab1[1] + lab1[2] * lab1[2]);
        double C2 = std::sqrt(lab2[1] * lab2[1] + lab2[2] * lab2[2]);
        pair.deltaC = C1 - C2;

        double da = lab1[1] - lab2[1];
        double db = lab1[2] - lab2[2];
        pair.deltaH = std::sqrt(std::max(0.0, da * da + db * db - pair.deltaC * pair.deltaC));

        result.pairs.push_back(pair);

        sum_de00 += pair.deltaE00;
        max_de00 = std::max(max_de00, pair.deltaE00);
        min_de00 = std::min(min_de00, pair.deltaE00);
    }

    result.mean_deltaE00 = sum_de00 / src_colors.size();
    result.max_deltaE00 = max_de00;
    result.min_deltaE00 = min_de00;

    return result;
}

std::vector<cv::Vec3d> ColorErrorCalculator::importColorsFromIni(const std::string& filepath) {
    std::vector<cv::Vec3d> colors;
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return colors;

    int color_num = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find("ColorNum") != std::string::npos) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                color_num = std::stoi(line.substr(pos + 1));
            }
        }
    }

    // 重新读取文件获取颜色值
    ifs.clear();
    ifs.seekg(0);
    while (std::getline(ifs, line)) {
        if (line.find("Color[") != std::string::npos) {
            auto eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string hex_str = line.substr(eq_pos + 1);
                // 去除空白
                hex_str.erase(0, hex_str.find_first_not_of(" \t"));
                hex_str.erase(hex_str.find_last_not_of(" \t\r\n") + 1);

                if (hex_str[0] == '#') {
                    hex_str = hex_str.substr(1);
                }

                if (hex_str.length() >= 6) {
                    int r = std::stoi(hex_str.substr(0, 2), nullptr, 16);
                    int g = std::stoi(hex_str.substr(2, 2), nullptr, 16);
                    int b = std::stoi(hex_str.substr(4, 2), nullptr, 16);
                    colors.push_back(cv::Vec3d(r, g, b));
                }
            }
        }
    }

    return colors;
}

bool ColorErrorCalculator::exportResult(const ColorErrorResult& result, const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << "Index,Src_R,Src_G,Src_B,Ref_R,Ref_G,Ref_B,DeltaE00,DeltaE76,DeltaE94,DeltaL,DeltaC,DeltaH\n";
    for (size_t i = 0; i < result.pairs.size(); ++i) {
        const auto& p = result.pairs[i];
        ofs << i << ","
            << p.src_color[0] << "," << p.src_color[1] << "," << p.src_color[2] << ","
            << p.ref_color[0] << "," << p.ref_color[1] << "," << p.ref_color[2] << ","
            << std::fixed << p.deltaE00 << "," << p.deltaE76 << "," << p.deltaE94 << ","
            << p.deltaL << "," << p.deltaC << "," << p.deltaH << "\n";
    }

    ofs << "\nMean_DeltaE00," << result.mean_deltaE00 << "\n";
    ofs << "Max_DeltaE00," << result.max_deltaE00 << "\n";
    ofs << "Min_DeltaE00," << result.min_deltaE00 << "\n";

    return true;
}

} // namespace mvtk
