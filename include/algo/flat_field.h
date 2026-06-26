#pragma once
/**
 * @file flat_field.h
 * @brief 平场校正算法（镜头阴影校正 LSC）
 *
 * 实现镜头阴影校正（Lens Shading Correction），消除因镜头渐晕效应
 * 导致的图像中心亮、边缘暗的非均匀性问题。支持多种标定基准方式，
 * 包括中心为基准、最亮区域为基准和目标亮度基准。
 */

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace mvtk {

/**
 * @brief LSC标定基准方式
 */
enum class LSCCalibMode {
    CENTER,       ///< 中心为基准
    BRIGHTEST,    ///< 最亮区域为基准
    TARGET_LUMA   ///< 目标亮度为基准
};

/**
 * @brief 平场校正（LSC）标定参数
 */
struct FlatFieldCalibParams {
    LSCCalibMode mode = LSCCalibMode::CENTER; ///< 标定基准方式
    int edge_fill = 2;                         ///< 边缘填充系数
    double target_brightness = 128.0;          ///< 目标亮度（仅TARGET_LUMA模式有效）
    int smooth_kernel = 64;                    ///< 平滑核大小
    int channel_mode = 0;                      ///< 通道模式：0=独立通道，1=统一增益
};

/**
 * @brief 平场校正结果
 */
struct FlatFieldResult {
    cv::Mat gain_map;          ///< 增益图（每个像素的校正系数）
    cv::Mat corrected_image;   ///< 校正后的图像
    double center_brightness;  ///< 中心亮度
    double edge_brightness;    ///< 边缘亮度
    double uniformity;         ///< 均匀性指标（校正后）
};

/**
 * @brief 平场校正类
 */
class FlatFieldCorrector {
public:
    /**
     * @brief 使用均匀面光源图像进行标定
     * @param flat_image 均匀面光源图像（如积分球拍摄的白场图像）
     * @param params 标定参数
     * @return 标定结果
     */
    static FlatFieldResult calibrate(const cv::Mat& flat_image, const FlatFieldCalibParams& params);

    /**
     * @brief 使用暗场图像和亮场图像进行标定（减去暗电流后计算增益）
     * @param flat_image 亮场图像
     * @param dark_image 暗场图像
     * @param params 标定参数
     * @return 标定结果
     */
    static FlatFieldResult calibrateWithDark(const cv::Mat& flat_image, const cv::Mat& dark_image, const FlatFieldCalibParams& params);

    /**
     * @brief 应用增益图校正图像
     * @param image 输入图像
     * @param gain_map 增益图
     * @param edge_fill 边缘填充系数
     * @return 校正后的图像
     */
    static cv::Mat apply(const cv::Mat& image, const cv::Mat& gain_map, int edge_fill = 2);

    /**
     * @brief 导出增益图到文件
     * @param gain_map 增益图
     * @param filepath 保存路径
     * @return 是否成功
     */
    static bool exportGainMap(const cv::Mat& gain_map, const std::string& filepath);

    /**
     * @brief 从文件导入增益图
     * @param filepath 文件路径
     * @return 增益图
     */
    static cv::Mat importGainMap(const std::string& filepath);

    /**
     * @brief 计算图像均匀性指标
     * @param image 输入图像
     * @param roi 评估区域（空表示全图）
     * @return 均匀性（0~1，1表示完全均匀）
     */
    static double computeUniformity(const cv::Mat& image, cv::Rect roi = cv::Rect());

    /**
     * @brief 生成径向衰减模拟图（用于测试）
     * @param width 图像宽度
     * @param height 图像高度
     * @param falloff 衰减系数
     * @return 模拟的渐晕图像
     */
    static cv::Mat generateVignetting(int width, int height, double falloff = 0.5);
};

} // namespace mvtk
