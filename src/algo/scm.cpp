/**
 * @file scm.cpp
 * @brief SCM 传感器特性分析模块实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <map>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "algo/scm.h"

namespace mvtk {

void SCMEvaluator::computeMeanVariance(const std::vector<cv::Mat>& images,
                                            const cv::Rect& roi,
                                            double& out_mean,
                                            double& out_variance) {
    if (images.empty()) {
        out_mean = 0.0;
        out_variance = 0.0;
        return;
    }

    cv::Mat first_img = images[0];
    cv::Rect actual_roi = roi;
    if (roi.empty()) {
        actual_roi = cv::Rect(0, 0, first_img.cols, first_img.rows);
    }
    actual_roi &= cv::Rect(0, 0, first_img.cols, first_img.rows);

    int rows = actual_roi.height;
    int cols = actual_roi.width;
    int channels = first_img.channels();
    size_t total_pixels = static_cast<size_t>(rows) * cols * channels;
    int num_frames = static_cast<int>(images.size());

    std::vector<double> frame_means(num_frames, 0.0);

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int f = 0; f < num_frames; ++f) {
        cv::Mat img_f;
        images[f].convertTo(img_f, CV_64F);
        cv::Mat region = img_f(actual_roi);

        double sum = 0.0;
        if (channels == 1) {
            for (int y = 0; y < rows; ++y) {
                const double* row = region.ptr<double>(y);
                for (int x = 0; x < cols; ++x) {
                    sum += row[x];
                }
            }
        } else {
            for (int y = 0; y < rows; ++y) {
                const cv::Vec3d* row = region.ptr<cv::Vec3d>(y);
                for (int x = 0; x < cols; ++x) {
                    sum += (row[x][0] + row[x][1] + row[x][2]) / 3.0;
                }
            }
        }
        frame_means[f] = sum / (rows * cols);
    }

    out_mean = 0.0;
    for (double m : frame_means) out_mean += m;
    out_mean /= num_frames;

    out_variance = 0.0;
    for (double m : frame_means) {
        double diff = m - out_mean;
        out_variance += diff * diff;
    }
    out_variance /= (num_frames - 1);
}

void SCMEvaluator::linearFit(const std::vector<double>& x,
                                  const std::vector<double>& y,
                                  double& out_slope,
                                  double& out_intercept,
                                  double& out_r2) {
    if (x.size() < 2 || x.size() != y.size()) {
        out_slope = 0.0;
        out_intercept = 0.0;
        out_r2 = 0.0;
        return;
    }

    size_t n = x.size();
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;

    for (size_t i = 0; i < n; ++i) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
    }

    double denom = n * sum_x2 - sum_x * sum_x;
    if (denom < 1e-10) {
        out_slope = 0.0;
        out_intercept = sum_y / n;
        out_r2 = 0.0;
        return;
    }

    out_slope = (n * sum_xy - sum_x * sum_y) / denom;
    out_intercept = (sum_y - out_slope * sum_x) / n;

    double ss_tot = 0.0, ss_res = 0.0;
    double mean_y = sum_y / n;
    for (size_t i = 0; i < n; ++i) {
        ss_tot += (y[i] - mean_y) * (y[i] - mean_y);
        double y_pred = out_slope * x[i] + out_intercept;
        ss_res += (y[i] - y_pred) * (y[i] - y_pred);
    }

    out_r2 = 1.0 - ss_res / ss_tot;
}

std::vector<PTCDataPoint> SCMEvaluator::computePTC(
    const std::map<double, std::vector<cv::Mat>>& bright_images_map,
    const std::map<double, std::vector<cv::Mat>>& dark_images_map,
    const SCMParams& params) {

    std::vector<PTCDataPoint> ptc_points;

    for (const auto& entry : bright_images_map) {
        double exp_us = entry.first;
        const auto& bright_imgs = entry.second;

        if (bright_imgs.size() < params.min_frame_count) continue;

        double bright_mean = 0.0, bright_var = 0.0;
        computeMeanVariance(bright_imgs, params.roi, bright_mean, bright_var);

        double dark_mean = 0.0, dark_var = 0.0;
        auto dark_it = dark_images_map.find(exp_us);
        if (dark_it != dark_images_map.end()) {
            computeMeanVariance(dark_it->second, params.roi, dark_mean, dark_var);
        }

        double corrected_mean = bright_mean - dark_mean;
        double corrected_var = bright_var - dark_var;

        if (corrected_mean > 0 && corrected_var > 0) {
            PTCDataPoint pt;
            pt.exposure_time_us = exp_us;
            pt.mean = corrected_mean;
            pt.variance = corrected_var;
            pt.signal_e = corrected_mean;
            ptc_points.push_back(pt);
        }
    }

    std::sort(ptc_points.begin(), ptc_points.end(),
              [](const PTCDataPoint& a, const PTCDataPoint& b) {
                  return a.mean < b.mean;
              });

    return ptc_points;
}

double SCMEvaluator::computeSystemGain(const std::vector<PTCDataPoint>& ptc_points) {
    if (ptc_points.size() < 3) return 1.0;

    std::vector<double> means, vars;
    for (const auto& pt : ptc_points) {
        if (pt.mean > 0 && pt.variance > 0) {
            means.push_back(pt.mean);
            vars.push_back(pt.variance);
        }
    }

    if (means.size() < 3) return 1.0;

    double slope, intercept, r2;
    linearFit(means, vars, slope, intercept, r2);

    if (slope < 0 || r2 < 0.9) {
        double avg_slope = 0.0;
        int count = 0;
        for (size_t i = 1; i < ptc_points.size(); ++i) {
            double dm = ptc_points[i].mean - ptc_points[i-1].mean;
            double dv = ptc_points[i].variance - ptc_points[i-1].variance;
            if (dm > 0) {
                avg_slope += dv / dm;
                count++;
            }
        }
        if (count > 0) slope = avg_slope / count;
    }

    if (slope <= 0) slope = 1.0;

    return 1.0 / slope;
}

double SCMEvaluator::computeDarkNoise(
    const std::map<double, std::vector<cv::Mat>>& dark_images_map,
    const SCMParams& params,
    double& out_dark_signal,
    double& out_dark_current) {

    out_dark_signal = 0.0;
    out_dark_current = 0.0;

    if (dark_images_map.empty()) return 0.0;

    std::vector<double> exp_times_us;
    std::vector<double> dark_means;
    std::vector<double> dark_vars;

    for (const auto& entry : dark_images_map) {
        double exp_us = entry.first;
        const auto& imgs = entry.second;

        if (imgs.size() < params.min_frame_count) continue;

        double mean = 0.0, var = 0.0;
        computeMeanVariance(imgs, params.roi, mean, var);

        exp_times_us.push_back(exp_us);
        dark_means.push_back(mean);
        dark_vars.push_back(var);
    }

    if (exp_times_us.size() < 2) {
        out_dark_signal = dark_means.empty() ? 0.0 : dark_means[0];
        double avg_var = 0.0;
        for (double v : dark_vars) avg_var += v;
        avg_var /= dark_vars.size();
        return std::sqrt(avg_var);
    }

    double slope, intercept, r2;
    linearFit(exp_times_us, dark_means, slope, intercept, r2);

    out_dark_signal = intercept;
    out_dark_current = slope * 1e6;

    double min_var = dark_vars[0];
    for (double v : dark_vars) {
        if (v < min_var) min_var = v;
    }

    return std::sqrt(min_var);
}

double SCMEvaluator::computeFullWellCapacity(
    const std::vector<PTCDataPoint>& ptc_points,
    double system_gain,
    double max_dn) {

    if (ptc_points.empty()) return 0.0;

    double last_mean = ptc_points.back().mean;
    double last_var = ptc_points.back().variance;

    double ptc_slope = 1.0 / system_gain;
    double ideal_var = last_mean * ptc_slope;

    double saturation_ratio = ideal_var / last_var;

    double fwc_dn = last_mean * saturation_ratio;
    if (fwc_dn > max_dn * 0.95) {
        fwc_dn = max_dn * 0.95;
    }

    return fwc_dn * system_gain;
}

double SCMEvaluator::computeDynamicRange(double full_well_capacity_e, double dark_noise_e) {
    if (dark_noise_e <= 0) return 0.0;
    return 20.0 * std::log10(full_well_capacity_e / dark_noise_e);
}

void SCMEvaluator::computeSNRCurve(
    const std::vector<PTCDataPoint>& ptc_points,
    double system_gain,
    double dark_noise_e,
    double& snr_at_10p,
    double& snr_at_50p,
    double& snr_at_90p,
    double& peak_snr) {

    snr_at_10p = 0.0;
    snr_at_50p = 0.0;
    snr_at_90p = 0.0;
    peak_snr = 0.0;

    if (ptc_points.empty()) return;

    double max_mean = ptc_points.back().mean;

    auto findSnrAtRatio = [&](double ratio) -> double {
        double target_mean = max_mean * ratio;
        for (size_t i = 1; i < ptc_points.size(); ++i) {
            if (ptc_points[i].mean >= target_mean) {
                double signal_e = ptc_points[i].mean * system_gain;
                double noise_e = std::sqrt(ptc_points[i].variance * system_gain * system_gain + dark_noise_e * dark_noise_e);
                return noise_e > 0 ? 20.0 * std::log10(signal_e / noise_e) : 0.0;
            }
        }
        return 0.0;
    };

    snr_at_10p = findSnrAtRatio(0.1);
    snr_at_50p = findSnrAtRatio(0.5);
    snr_at_90p = findSnrAtRatio(0.9);

    for (const auto& pt : ptc_points) {
        double signal_e = pt.mean * system_gain;
        double noise_e = std::sqrt(pt.variance * system_gain * system_gain + dark_noise_e * dark_noise_e);
        if (noise_e > 0) {
            double snr = 20.0 * std::log10(signal_e / noise_e);
            if (snr > peak_snr) peak_snr = snr;
        }
    }
}

SCMResult SCMEvaluator::evaluate(
    const std::map<double, std::vector<cv::Mat>>& dark_images_map,
    const std::map<double, std::vector<cv::Mat>>& bright_images_map,
    const SCMParams& params) {

    SCMResult result;

    result.sensor_name = params.sensor_name;
    result.temperature_c = params.temperature_c;
    result.sensor_bits = params.sensor_bits;
    result.max_digital_value = (1 << params.sensor_bits) - 1;

    std::vector<double> exp_times;
    for (const auto& entry : bright_images_map) {
        exp_times.push_back(entry.first);
    }
    result.exposure_times_us = exp_times;

    result.dark_means.resize(exp_times.size(), 0.0);
    result.dark_variances.resize(exp_times.size(), 0.0);
    result.bright_means.resize(exp_times.size(), 0.0);
    result.bright_variances.resize(exp_times.size(), 0.0);

    for (size_t i = 0; i < exp_times.size(); ++i) {
        double exp_us = exp_times[i];

        auto dark_it = dark_images_map.find(exp_us);
        if (dark_it != dark_images_map.end()) {
            computeMeanVariance(dark_it->second, params.roi,
                               result.dark_means[i], result.dark_variances[i]);
        }

        auto bright_it = bright_images_map.find(exp_us);
        if (bright_it != bright_images_map.end()) {
            computeMeanVariance(bright_it->second, params.roi,
                               result.bright_means[i], result.bright_variances[i]);
        }
    }

    result.ptc_points = computePTC(bright_images_map, dark_images_map, params);

    result.system_gain_e_per_dn = computeSystemGain(result.ptc_points);
    result.system_gain_dn_per_e = 1.0 / result.system_gain_e_per_dn;

    result.dark_noise_dn = computeDarkNoise(dark_images_map, params,
                                            result.dark_signal_dn,
                                            result.dark_current_e_per_sec);
    result.dark_noise_e = result.dark_noise_dn * result.system_gain_e_per_dn;

    result.full_well_capacity_e = computeFullWellCapacity(result.ptc_points,
                                                          result.system_gain_e_per_dn,
                                                          result.max_digital_value);
    result.full_well_capacity_dn = result.full_well_capacity_e / result.system_gain_e_per_dn;

    result.dynamic_range_db = computeDynamicRange(result.full_well_capacity_e, result.dark_noise_e);
    result.dynamic_range_bits = result.dynamic_range_db / 6.02;

    computeSNRCurve(result.ptc_points, result.system_gain_e_per_dn,
                    result.dark_noise_e,
                    result.snr_at_10_percent,
                    result.snr_at_50_percent,
                    result.snr_at_90_percent,
                    result.peak_snr_db);

    result.is_valid = (result.system_gain_e_per_dn > 0 &&
                       result.full_well_capacity_e > 0 &&
                       result.dynamic_range_db > 0);

    if (result.is_valid) {
        result.message = "EMVA 1288 评估完成";
    } else {
        result.message = "评估无效：数据不足或计算异常";
    }

    result.is_snr_sufficient_for_ccm = (result.snr_at_50_percent >= 20.0);
    result.is_uniformity_sufficient_for_lsc = true;

    if (!result.exposure_times_us.empty()) {
        double mid_exp = result.exposure_times_us[result.exposure_times_us.size() / 2];
        result.recommended_ccm_exposure_us = mid_exp;
        result.recommended_lsc_exposure_us = mid_exp;
    }

    return result;
}

std::string SCMEvaluator::generateReport(const SCMResult& result) {
    std::ostringstream oss;

    oss << "============================================\n";
    oss << "EMVA 1288 相机性能评估报告\n";
    oss << "============================================\n\n";

    oss << "【基本信息】\n";
    oss << "传感器名称: " << result.sensor_name << "\n";
    oss << "测试温度: " << result.temperature_c << " ℃\n";
    oss << "传感器位深: " << result.sensor_bits << " bit\n";
    oss << "最大数字值: " << result.max_digital_value << " DN\n";
    oss << "\n";

    oss << "【核心指标】\n";
    oss << "系统增益: " << result.system_gain_e_per_dn << " e-/DN ("
        << result.system_gain_dn_per_e << " DN/e-)\n";
    oss << "暗噪声: " << result.dark_noise_e << " e- ("
        << result.dark_noise_dn << " DN)\n";
    oss << "暗信号: " << result.dark_signal_dn << " DN\n";
    oss << "暗电流: " << result.dark_current_e_per_sec << " e-/s\n";
    oss << "\n";

    oss << "【动态范围指标】\n";
    oss << "满阱容量: " << result.full_well_capacity_e << " e- ("
        << result.full_well_capacity_dn << " DN)\n";
    oss << "动态范围: " << result.dynamic_range_db << " dB ("
        << result.dynamic_range_bits << " bit)\n";
    oss << "\n";

    oss << "【SNR指标】\n";
    oss << "10%信号SNR: " << result.snr_at_10_percent << " dB\n";
    oss << "50%信号SNR: " << result.snr_at_50_percent << " dB\n";
    oss << "90%信号SNR: " << result.snr_at_90_percent << " dB\n";
    oss << "峰值SNR: " << result.peak_snr_db << " dB\n";
    oss << "\n";

    oss << "【评估结论】\n";
    oss << "评估有效性: " << (result.is_valid ? "有效" : "无效") << " (系统增益>0且满阱容量>0且动态范围>0)\n";
    oss << "SNR满足CCM标定: " << (result.is_snr_sufficient_for_ccm ? "是" : "否") << " (50%信号SNR >= 20dB)\n";
    oss << "推荐CCM标定曝光: " << result.recommended_ccm_exposure_us << " us\n";
    oss << "推荐LSC标定曝光: " << result.recommended_lsc_exposure_us << " us\n";
    oss << "\n";

    oss << "============================================\n";

    return oss.str();
}

bool SCMEvaluator::exportResult(const SCMResult& result, const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs) return false;

    ofs << "{\n";
    ofs << "  \"sensor_name\": \"" << result.sensor_name << "\",\n";
    ofs << "  \"temperature_c\": " << result.temperature_c << ",\n";
    ofs << "  \"sensor_bits\": " << result.sensor_bits << ",\n";
    ofs << "  \"system_gain_e_per_dn\": " << result.system_gain_e_per_dn << ",\n";
    ofs << "  \"dark_noise_e\": " << result.dark_noise_e << ",\n";
    ofs << "  \"full_well_capacity_e\": " << result.full_well_capacity_e << ",\n";
    ofs << "  \"dynamic_range_db\": " << result.dynamic_range_db << ",\n";
    ofs << "  \"peak_snr_db\": " << result.peak_snr_db << ",\n";
    ofs << "  \"is_valid\": " << (result.is_valid ? "true" : "false") << "\n";
    ofs << "}\n";

    return true;
}

SCMResult SCMEvaluator::importResult(const std::string& filepath) {
    SCMResult result;
    std::ifstream ifs(filepath);
    if (!ifs) return result;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find("sensor_name") != std::string::npos) {
            size_t start = line.find("\"") + 1;
            size_t end = line.find("\"", start);
            result.sensor_name = line.substr(start, end - start);
        } else if (line.find("system_gain_e_per_dn") != std::string::npos) {
            result.system_gain_e_per_dn = std::stod(line.substr(line.find(":") + 1));
        } else if (line.find("dark_noise_e") != std::string::npos) {
            result.dark_noise_e = std::stod(line.substr(line.find(":") + 1));
        } else if (line.find("dynamic_range_db") != std::string::npos) {
            result.dynamic_range_db = std::stod(line.substr(line.find(":") + 1));
        }
    }

    result.is_valid = true;
    return result;
}

cv::Mat SCMEvaluator::visualizePTC(const std::vector<PTCDataPoint>& ptc_points, double system_gain) {
    cv::Mat img(400, 600, CV_8UC3, cv::Scalar(255, 255, 255));

    if (ptc_points.empty()) return img;

    double max_mean = ptc_points.back().mean;
    double max_var = 0.0;
    for (const auto& pt : ptc_points) {
        if (pt.variance > max_var) max_var = pt.variance;
    }

    double slope = 1.0 / system_gain;

    cv::rectangle(img, cv::Rect(50, 50, 500, 300), cv::Scalar(200, 200, 200), 2);

    for (const auto& pt : ptc_points) {
        int x = 50 + static_cast<int>(pt.mean / max_mean * 500);
        int y = 350 - static_cast<int>(pt.variance / max_var * 300);
        cv::circle(img, cv::Point(x, y), 4, cv::Scalar(0, 0, 255), -1);
    }

    cv::line(img, cv::Point(50, 350),
             cv::Point(550, 350 - static_cast<int>(slope * max_mean / max_var * 300)),
             cv::Scalar(0, 255, 0), 2);

    cv::putText(img, "Photon Transfer Curve (PTC)", cv::Point(50, 30),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
    cv::putText(img, "Mean (DN)", cv::Point(250, 380),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    cv::putText(img, "Variance (DN^2)", cv::Point(10, 200),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);

    std::ostringstream gain_str;
    gain_str << "Gain: " << system_gain << " e-/DN";
    cv::putText(img, gain_str.str(), cv::Point(50, 360),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

    return img;
}

cv::Mat SCMEvaluator::visualizeSNR(const std::vector<PTCDataPoint>& ptc_points,
                                         double system_gain, double dark_noise_e) {
    cv::Mat img(400, 600, CV_8UC3, cv::Scalar(255, 255, 255));

    if (ptc_points.empty()) return img;

    double max_mean = ptc_points.back().mean;
    double max_snr = 0.0;
    std::vector<double> snrs;

    for (const auto& pt : ptc_points) {
        double signal_e = pt.mean * system_gain;
        double noise_e = std::sqrt(pt.variance * system_gain * system_gain + dark_noise_e * dark_noise_e);
        double snr = noise_e > 0 ? 20.0 * std::log10(signal_e / noise_e) : 0.0;
        snrs.push_back(snr);
        if (snr > max_snr) max_snr = snr;
    }

    cv::rectangle(img, cv::Rect(50, 50, 500, 300), cv::Scalar(200, 200, 200), 2);

    for (size_t i = 0; i < ptc_points.size(); ++i) {
        int x = 50 + static_cast<int>(ptc_points[i].mean / max_mean * 500);
        int y = 350 - static_cast<int>(snrs[i] / max_snr * 300);
        cv::circle(img, cv::Point(x, y), 3, cv::Scalar(255, 0, 0), -1);
    }

    cv::putText(img, "SNR Curve", cv::Point(50, 30),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
    cv::putText(img, "Signal (DN)", cv::Point(250, 380),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    cv::putText(img, "SNR (dB)", cv::Point(10, 200),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);

    std::ostringstream peak_str;
    peak_str << "Peak SNR: " << max_snr << " dB";
    cv::putText(img, peak_str.str(), cv::Point(50, 360),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);

    return img;
}

cv::Mat SCMEvaluator::visualizeResult(const SCMResult& result) {
    cv::Mat ptc_img = visualizePTC(result.ptc_points, result.system_gain_e_per_dn);
    cv::Mat snr_img = visualizeSNR(result.ptc_points, result.system_gain_e_per_dn, result.dark_noise_e);

    cv::Mat combined(ptc_img.rows, ptc_img.cols + snr_img.cols, CV_8UC3, cv::Scalar(255, 255, 255));

    ptc_img.copyTo(combined(cv::Rect(0, 0, ptc_img.cols, ptc_img.rows)));
    snr_img.copyTo(combined(cv::Rect(ptc_img.cols, 0, snr_img.cols, snr_img.rows)));

    return combined;
}

} // namespace mvtk
