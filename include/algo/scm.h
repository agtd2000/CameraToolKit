#pragma once
/**
 * @file scm.h
 * @brief SCM 传感器特性分析模块
 *
 * SCM (Sensor Characterization Module) - 传感器特性分析模块
 * 基于 EMVA 1288 标准定义的相机性能指标评估，包括：
 * - 光子传递曲线 (PTC, Photon Transfer Curve)
 * - 系统增益 (System Gain, K)
 * - 暗噪声 (Dark Noise, σ_dark)
 * - 满阱容量 (Full Well Capacity, FWC)
 * - 动态范围 (Dynamic Range, DR)
 * - 信噪比曲线 (SNR Curve)
 * - 暗电流 (Dark Current)
 *
 * 评估流程：
 * 1. 在不同曝光时间下采集暗场图像（至少50帧）
 * 2. 在不同曝光时间下采集亮场图像（至少50帧，使用均匀面光源如积分球）
 * 3. 计算各曝光时间下的均值和方差
 * 4. 拟合光子传递曲线，推导系统增益
 * 5. 计算所有性能指标
 *
 * 参考标准：EMVA Standard 1288 - Standard for Characterization of Image Sensors
 */

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace mvtk {

/**
 * @brief PTC数据点（均值-方差对）
 */
struct PTCDataPoint {
    double exposure_time_us;  ///< 曝光时间(微秒)
    double mean;              ///< 均值(DN)
    double variance;          ///< 方差(DN^2)
    double signal_e;          ///< 信号量(e-)
};

/**
 * @brief SCM 评估参数
 */
struct SCMParams {
    cv::Rect roi;                        ///< 评估区域（空表示全图）
    int min_frame_count = 50;            ///< 每曝光时间点的最小采集帧数
    bool use_roi_mask = false;           ///< 是否使用ROI掩码去除坏点
    double saturation_threshold = 0.95;  ///< 饱和度阈值（超过此比例视为饱和）
    double temperature_c = 25.0;         ///< 传感器温度(℃)
    std::string sensor_name;             ///< 传感器名称
    int sensor_bits = 12;                ///< 传感器位深
};

/**
 * @brief SCM 评估结果
 */
struct SCMResult {
    // 基本信息
    std::string sensor_name;             ///< 传感器名称
    double temperature_c;                ///< 测试温度(℃)
    int sensor_bits;                     ///< 传感器位深
    double max_digital_value;            ///< 最大数字值(DN)

    // 核心指标
    double system_gain_e_per_dn;         ///< 系统增益(e-/DN)
    double system_gain_dn_per_e;         ///< 系统增益(DN/e-)
    double dark_noise_e;                 ///< 暗噪声(e-)
    double dark_noise_dn;                ///< 暗噪声(DN)
    double dark_signal_dn;               ///< 暗信号(DN)
    double dark_current_e_per_sec;       ///< 暗电流(e-/s)
    double dark_current_e_per_sec_per_mm2; ///< 暗电流密度(e-/s/mm²)

    // 动态范围指标
    double full_well_capacity_e;         ///< 满阱容量(e-)
    double full_well_capacity_dn;        ///< 满阱容量(DN)
    double dynamic_range_db;             ///< 动态范围(dB)
    double dynamic_range_bits;           ///< 动态范围(bit)

    // SNR指标
    double snr_at_10_percent;            ///< 10%信号时的SNR(dB)
    double snr_at_50_percent;            ///< 50%信号时的SNR(dB)
    double snr_at_90_percent;            ///< 90%信号时的SNR(dB)
    double peak_snr_db;                  ///< 峰值SNR(dB)

    // PTC数据
    std::vector<PTCDataPoint> ptc_points; ///< 光子传递曲线数据点

    // 图像统计
    std::vector<double> exposure_times_us; ///< 测试的曝光时间序列(us)
    std::vector<double> dark_means;       ///< 各曝光时间下的暗场均值
    std::vector<double> dark_variances;   ///< 各曝光时间下的暗场方差
    std::vector<double> bright_means;     ///< 各曝光时间下的亮场均值
    std::vector<double> bright_variances; ///< 各曝光时间下的亮场方差

    // 评估结论
    bool is_valid;                       ///< 评估是否有效
    std::string message;                 ///< 评估消息/警告

    // 与其他模块的衔接建议
    bool is_snr_sufficient_for_ccm;      ///< SNR是否满足CCM标定要求
    double recommended_ccm_exposure_us;  ///< 推荐的CCM标定曝光时间(us)
    bool is_uniformity_sufficient_for_lsc; ///< 均匀性是否满足LSC标定要求
    double recommended_lsc_exposure_us;  ///< 推荐的LSC标定曝光时间(us)
};

/**
 * @brief SCM 传感器特性分析类
 */
class SCMEvaluator {
public:
    /**
     * @brief 执行完整的SCM评估
     * @param dark_images_map 暗场图像映射：曝光时间 -> 图像列表
     * @param bright_images_map 亮场图像映射：曝光时间 -> 图像列表
     * @param params 评估参数
     * @return 评估结果
     */
    static SCMResult evaluate(
        const std::map<double, std::vector<cv::Mat>>& dark_images_map,
        const std::map<double, std::vector<cv::Mat>>& bright_images_map,
        const SCMParams& params);

    /**
     * @brief 计算光子传递曲线
     * @param bright_images_map 亮场图像映射
     * @param dark_images_map 暗场图像映射
     * @param params 评估参数
     * @return PTC数据点列表
     */
    static std::vector<PTCDataPoint> computePTC(
        const std::map<double, std::vector<cv::Mat>>& bright_images_map,
        const std::map<double, std::vector<cv::Mat>>& dark_images_map,
        const SCMParams& params);

    /**
     * @brief 从PTC数据计算系统增益
     * @param ptc_points PTC数据点
     * @return 系统增益(e-/DN)
     */
    static double computeSystemGain(const std::vector<PTCDataPoint>& ptc_points);

    /**
     * @brief 计算暗噪声和暗信号
     * @param dark_images_map 暗场图像映射
     * @param params 评估参数
     * @param out_dark_signal 输出暗信号(DN)
     * @param out_dark_current 输出暗电流(e-/s)
     * @return 暗噪声(e-)
     */
    static double computeDarkNoise(
        const std::map<double, std::vector<cv::Mat>>& dark_images_map,
        const SCMParams& params,
        double& out_dark_signal,
        double& out_dark_current);

    /**
     * @brief 计算满阱容量
     * @param ptc_points PTC数据点
     * @param system_gain 系统增益(e-/DN)
     * @param max_dn 最大数字值
     * @return 满阱容量(e-)
     */
    static double computeFullWellCapacity(
        const std::vector<PTCDataPoint>& ptc_points,
        double system_gain,
        double max_dn);

    /**
     * @brief 计算动态范围
     * @param full_well_capacity_e 满阱容量(e-)
     * @param dark_noise_e 暗噪声(e-)
     * @return 动态范围(dB)
     */
    static double computeDynamicRange(double full_well_capacity_e, double dark_noise_e);

    /**
     * @brief 计算SNR曲线
     * @param ptc_points PTC数据点
     * @param system_gain 系统增益(e-/DN)
     * @param dark_noise_e 暗噪声(e-)
     * @param snr_at_10p 10%信号时的SNR(dB)
     * @param snr_at_50p 50%信号时的SNR(dB)
     * @param snr_at_90p 90%信号时的SNR(dB)
     * @param peak_snr 峰值SNR(dB)
     */
    static void computeSNRCurve(
        const std::vector<PTCDataPoint>& ptc_points,
        double system_gain,
        double dark_noise_e,
        double& snr_at_10p,
        double& snr_at_50p,
        double& snr_at_90p,
        double& peak_snr);

    /**
     * @brief 生成评估报告文本
     * @param result 评估结果
     * @return 报告文本
     */
    static std::string generateReport(const SCMResult& result);

    /**
     * @brief 导出评估结果到JSON文件
     * @param result 评估结果
     * @param filepath 保存路径
     * @return 是否成功
     */
    static bool exportResult(const SCMResult& result, const std::string& filepath);

    /**
     * @brief 从JSON文件导入评估结果
     * @param filepath 文件路径
     * @return 评估结果
     */
    static SCMResult importResult(const std::string& filepath);

    /**
     * @brief 生成PTC曲线可视化图像
     * @param ptc_points PTC数据点
     * @param system_gain 系统增益
     * @return 可视化图像
     */
    static cv::Mat visualizePTC(const std::vector<PTCDataPoint>& ptc_points, double system_gain);

    /**
     * @brief 生成SNR曲线可视化图像
     * @param ptc_points PTC数据点
     * @param system_gain 系统增益(e-/DN)
     * @param dark_noise_e 暗噪声(e-)
     * @return 可视化图像
     */
    static cv::Mat visualizeSNR(const std::vector<PTCDataPoint>& ptc_points,
                                 double system_gain, double dark_noise_e);

    /**
     * @brief 生成评估结果概览图像
     * @param result 评估结果
     * @return 概览图像
     */
    static cv::Mat visualizeResult(const SCMResult& result);

private:
    /**
     * @brief 计算多帧图像的均值和方差
     * @param images 图像列表
     * @param roi 感兴趣区域
     * @param out_mean 输出均值
     * @param out_variance 输出方差
     */
    static void computeMeanVariance(const std::vector<cv::Mat>& images,
                                     const cv::Rect& roi,
                                     double& out_mean,
                                     double& out_variance);

    /**
     * @brief 线性拟合
     * @param x 自变量
     * @param y 因变量
     * @param out_slope 输出斜率
     * @param out_intercept 输出截距
     * @param out_r2 输出R²值
     */
    static void linearFit(const std::vector<double>& x,
                          const std::vector<double>& y,
                          double& out_slope,
                          double& out_intercept,
                          double& out_r2);
};

} // namespace mvtk
