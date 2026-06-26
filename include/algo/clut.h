#pragma once
/**
 * @file clut.h
 * @brief 颜色查找表（CLUT）算法
 *
 * 实现基于三维颜色查找表的颜色转换，通过色调映射将原始RGB值
 * 转换为目标RGB值。支持量化系数和平滑因子调节。
 */

#include <opencv2/core.hpp>
#include <vector>

namespace mvtk {

/**
 * @brief CLUT标定参数
 */
struct CLUTCalibParams {
    int quant_coeff = 1;      ///< 量化系数
    double smooth_factor = 1.0; ///< 平滑因子
    int grid_size = 33;       ///< LUT网格大小
};

/**
 * @brief 颜色查找表类
 */
class ColorLookupTable {
public:
    /**
     * @brief 基于原始颜色和参考颜色标定3D LUT
     * @param src_colors 原始颜色
     * @param ref_colors 参考颜色
     * @param params 标定参数
     * @return 3D LUT数据
     */
    static cv::Mat calibrate(
        const std::vector<cv::Vec3d>& src_colors,
        const std::vector<cv::Vec3d>& ref_colors,
        const CLUTCalibParams& params);

    /**
     * @brief 应用3D LUT校正
     * @param image 输入RGB图像
     * @param lut 3D LUT数据
     * @return 校正后的图像
     */
    static cv::Mat apply(const cv::Mat& image, const cv::Mat& lut);

    /**
     * @brief 导出LUT到文件
     * @param lut LUT数据
     * @param filepath 保存路径
     * @return 是否成功
     */
    static bool exportLUT(const cv::Mat& lut, const std::string& filepath);

    /**
     * @brief 从文件导入LUT
     * @param filepath 文件路径
     * @return LUT数据
     */
    static cv::Mat importLUT(const std::string& filepath);
};

} // namespace mvtk
