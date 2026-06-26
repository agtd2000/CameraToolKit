#pragma once
/**
 * @file quick_color_calib.h
 * @brief 快速色彩标定算法
 *
 * 实现快速色彩标定功能，使相机在任意照明条件下实现标准值或
 * 参考图像的色彩效果。支持标准模式和参考模式两种标定模式，
 * 集成白平衡和CCM标定流程。
 */

#include "white_balance.h"
#include "ccm.h"
#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace mvtk {

/**
 * @brief 快速色彩标定模式
 */
enum class QuickCalibMode {
    STANDARD,  ///< 标准模式：使相机实现标准值的色彩效果
    REFERENCE  ///< 参考模式：使相机实现参考图像的色彩效果
};

/**
 * @brief 标准24色卡参考颜色（sRGB D65）
 */
struct StandardColorChart {
    /**
     * @brief 获取标准24色卡的sRGB参考值
     * @return 24个色块的RGB参考值
     */
    static std::vector<cv::Vec3d> getXriteClassic24();

    /**
     * @brief 获取标准140色卡的sRGB参考值
     * @return 140个色块的RGB参考值
     */
    static std::vector<cv::Vec3d> getXriteDigitalSG140();
};

/**
 * @brief 快速色彩标定结果
 */
struct QuickCalibResult {
    WBCorrectParams wb_params;    ///< 白平衡参数
    CCMCorrectParams ccm_params;  ///< CCM参数
    std::vector<double> deltaE00; ///< 各色块的DeltaE00误差
    double mean_deltaE00;         ///< 平均DeltaE00
    cv::Mat corrected_image;      ///< 校正后图像
};

/**
 * @brief 快速色彩标定类
 */
class QuickColorCalibrator {
public:
    /**
     * @brief 执行快速色彩标定（标准模式）
     * @param image 输入图像
     * @param rois 色卡ROI区域
     * @param wb_method 白平衡标定方法
     * @param ccm_method CCM标定方法
     * @param stat_method RGB统计方法
     * @return 标定结果
     */
    static QuickCalibResult calibrateStandard(
        const cv::Mat& image,
        const std::vector<cv::Rect>& rois,
        WBMethod wb_method = WBMethod::AVERAGE,
        CCMMethod ccm_method = CCMMethod::OPTIMAL,
        RGBStatMethod stat_method = RGBStatMethod::MEDIAN);

    /**
     * @brief 执行快速色彩标定（参考模式）
     * @param image 输入图像
     * @param ref_image 参考图像
     * @param rois 输入图像的色卡ROI区域
     * @param ref_rois 参考图像的色卡ROI区域（可选，为空时使用rois）
     * @param wb_method 白平衡标定方法
     * @param ccm_method CCM标定方法
     * @param stat_method RGB统计方法
     * @return 标定结果
     */
    static QuickCalibResult calibrateReference(
        const cv::Mat& image,
        const cv::Mat& ref_image,
        const std::vector<cv::Rect>& rois,
        const std::vector<cv::Rect>& ref_rois = std::vector<cv::Rect>(),
        WBMethod wb_method = WBMethod::AVERAGE,
        CCMMethod ccm_method = CCMMethod::OPTIMAL,
        RGBStatMethod stat_method = RGBStatMethod::MEDIAN);

    /**
     * @brief 自动调节曝光使亮度值在建议范围内
     * @param image 输入图像
     * @param roi 检测区域
     * @param target_min 目标亮度最小值(0.55)
     * @param target_max 目标亮度最大值(0.65)
     * @return 建议的曝光时间和增益调整
     */
    static double autoExposure(const cv::Mat& image, const cv::Rect& roi,
                               double target_min = 0.55, double target_max = 0.65);

    /**
     * @brief 检查当前亮度是否在建议范围内
     * @param brightness 当前亮度值
     * @return true=在范围内(绿色)，false=超出范围(红色)
     */
    static bool checkBrightness(double brightness);

    /**
     * @brief 生成24色卡标准ROI位置
     * @param image_width 图像宽度
     * @param image_height 图像高度
     * @param chart_x 色卡左上角x
     * @param chart_y 色卡左上角y
     * @param chart_w 色卡宽度
     * @param chart_h 色卡高度
     * @return 24个ROI区域
     */
    static std::vector<cv::Rect> generate24ColorROI(
        int image_width, int image_height,
        int chart_x, int chart_y, int chart_w, int chart_h);
};

} // namespace mvtk
