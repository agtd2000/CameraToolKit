#pragma once
/**
 * @file white_balance.h
 * @brief 白平衡标定与校正算法
 *
 * 实现灰度世界算法、完美反射算法以及基于色卡的白平衡标定。
 * 支持平均法和解析法两种标定方法，可对RGB图像进行白平衡校正。
 */

#include <opencv2/core.hpp>
#include <vector>

namespace mvtk {

/**
 * @brief 白平衡标定方法
 */
enum class WBMethod {
    AVERAGE,   ///< 平均法：各色块分别计算WB参数后取平均
    ANALYTIC   ///< 解析法：按误差最小原则计算WB参数
};

/**
 * @brief RGB统计方法
 */
enum class RGBStatMethod {
    MEAN,   ///< 均值法
    MEDIAN  ///< 中值法（默认，抗干扰更强）
};

/**
 * @brief 白平衡标定参数
 */
struct WBCalibParams {
    WBMethod method = WBMethod::AVERAGE;     ///< 标定方法
    RGBStatMethod stat_method = RGBStatMethod::MEDIAN; ///< RGB统计方法
    int quant_coeff = 1024;                  ///< 量化系数
};

/**
 * @brief 白平衡校正参数
 */
struct WBCorrectParams {
    double r_gain = 1.0; ///< R通道增益
    double g_gain = 1.0; ///< G通道增益
    double b_gain = 1.0; ///< B通道增益
    int quant_coeff = 1024; ///< 量化系数
};

/**
 * @brief 白平衡类
 */
class WhiteBalance {
public:
    /**
     * @brief 基于色卡颜色进行白平衡标定
     * @param src_colors 输入图像中色卡各色块的RGB值
     * @param ref_colors 参考图像中色卡各色块的RGB值（标准模式下可为空）
     * @param params 标定参数
     * @return 白平衡校正参数
     */
    static WBCorrectParams calibrateFromColors(
        const std::vector<cv::Vec3d>& src_colors,
        const std::vector<cv::Vec3d>& ref_colors,
        const WBCalibParams& params);

    /**
     * @brief 灰度世界算法自动白平衡
     * @param image 输入RGB图像
     * @return 白平衡校正参数
     */
    static WBCorrectParams grayWorld(const cv::Mat& image);

    /**
     * @brief 完美反射算法自动白平衡
     * @param image 输入RGB图像
     * @param ratio 亮度前ratio比例的像素作为参考白
     * @return 白平衡校正参数
     */
    static WBCorrectParams perfectReflector(const cv::Mat& image, double ratio = 0.1);

    /**
     * @brief 应用白平衡校正
     * @param image 输入RGB图像
     * @param params 校正参数
     * @return 校正后的图像
     */
    static cv::Mat apply(const cv::Mat& image, const WBCorrectParams& params);

    /**
     * @brief 从ROI区域提取RGB颜色值
     * @param image 输入图像
     * @param rois ROI区域列表
     * @param method 统计方法
     * @return 各ROI的RGB颜色值
     */
    static std::vector<cv::Vec3d> extractColors(const cv::Mat& image,
                                                 const std::vector<cv::Rect>& rois,
                                                 RGBStatMethod method = RGBStatMethod::MEDIAN);

    /**
     * @brief 计算当前亮度值（G通道归一化值）
     * @param image 输入图像
     * @param roi 检测区域
     * @return 亮度值（0~1范围）
     */
    static double computeBrightness(const cv::Mat& image, const cv::Rect& roi);
};

} // namespace mvtk
