#pragma once
/**
 * @file ccm.h
 * @brief 色彩校正矩阵（CCM）标定与校正算法
 *
 * 实现基于3x3色彩校正矩阵的色彩校正。支持最优法和最小二乘法
 * 两种标定方法，可对白平衡后的图像进行进一步色彩校正，使图像
 * 颜色校正至标准值，整体色彩更加鲜艳。
 */

#include <opencv2/core.hpp>
#include <vector>

namespace mvtk {

/**
 * @brief CCM标定方法
 */
enum class CCMMethod {
    OPTIMAL,       ///< 最优法：获得更高标定精度及计算效率
    LEAST_SQUARES  ///< 最小二乘法
};

/**
 * @brief Gamma类型
 */
enum class GammaType {
    NONE,         ///< 不启用
    VALUE,        ///< 指定Gamma值
    CURVE,        ///< 自定义Gamma曲线
    LRGB2SRGB    ///< 线性RGB转sRGB
};

/**
 * @brief CCM标定参数
 */
struct CCMCalibParams {
    CCMMethod method = CCMMethod::OPTIMAL; ///< 标定方法
    GammaType gamma_type = GammaType::NONE; ///< Gamma类型
    double gamma_value = 1.0;              ///< Gamma值（GAMMA_VALUE模式）
    int quant_coeff = 1024;                ///< 量化系数
};

/**
 * @brief CCM校正参数
 */
struct CCMCorrectParams {
    cv::Matx33d ccm;        ///< 3x3色彩校正矩阵
    cv::Vec3d offset;       ///< 偏移向量
    int quant_coeff = 1024; ///< 量化系数
};

/**
 * @brief 传感器线性度验证结果
 */
struct LinearityCheckResult {
    bool is_linear;           ///< 是否在线性响应区
    double r_channel_r2;      ///< R通道线性拟合R²值
    double g_channel_r2;      ///< G通道线性拟合R²值
    double b_channel_r2;      ///< B通道线性拟合R²值
    double avg_r2;            ///< 平均R²值
    double suggested_min_luma; ///< 建议的最小亮度值
    double suggested_max_luma; ///< 建议的最大亮度值
    std::string message;      ///< 验证消息
};

/**
 * @brief 色彩校正矩阵类
 */
class ColorCorrectionMatrix {
public:
    /**
     * @brief 基于原始颜色和参考颜色标定CCM
     * @param src_colors 输入图像中的颜色值
     * @param ref_colors 参考颜色值
     * @param params 标定参数
     * @return CCM校正参数
     */
    static CCMCorrectParams calibrate(
        const std::vector<cv::Vec3d>& src_colors,
        const std::vector<cv::Vec3d>& ref_colors,
        const CCMCalibParams& params);

    /**
     * @brief 应用CCM校正
     * @param image 输入RGB图像
     * @param params 校正参数
     * @return 校正后的图像
     */
    static cv::Mat apply(const cv::Mat& image, const CCMCorrectParams& params);

    /**
     * @brief 应用Gamma校正
     * @param image 输入图像
     * @param gamma_type Gamma类型
     * @param gamma_value Gamma值
     * @return 校正后的图像
     */
    static cv::Mat applyGamma(const cv::Mat& image, GammaType gamma_type, double gamma_value = 1.0);

    /**
     * @brief 线性RGB转sRGB
     * @param image 输入线性RGB图像
     * @return sRGB图像
     */
    static cv::Mat linearToSRGB(const cv::Mat& image);

    /**
     * @brief sRGB转线性RGB
     * @param image 输入sRGB图像
     * @return 线性RGB图像
     */
    static cv::Mat sRGBToLinear(const cv::Mat& image);

    /**
     * @brief 计算CCM校正后的颜色误差（DeltaE00）
     * @param src_colors 原始颜色
     * @param ref_colors 参考颜色
     * @param ccm_params CCM参数
     * @return 各颜色对的DeltaE00误差
     */
    static std::vector<double> computeErrors(
        const std::vector<cv::Vec3d>& src_colors,
        const std::vector<cv::Vec3d>& ref_colors,
        const CCMCorrectParams& ccm_params);

    /**
     * @brief 验证传感器线性度（CCM标定前的预处理步骤）
     * @param src_colors 输入图像中的颜色值
     * @param ref_colors 参考颜色值（应按亮度排序）
     * @param min_r2_threshold R²最小值阈值（默认0.98）
     * @return 线性度验证结果
     */
    static LinearityCheckResult checkLinearity(
        const std::vector<cv::Vec3d>& src_colors,
        const std::vector<cv::Vec3d>& ref_colors,
        double min_r2_threshold = 0.98);
};

} // namespace mvtk
