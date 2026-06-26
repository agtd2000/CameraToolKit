#pragma once
/**
 * @file dead_pixel.h
 * @brief 坏点检测与校正算法
 *
 * 实现相机传感器坏点的自动检测、手动编辑和校正功能。
 * 支持亮坏点和暗坏点检测，可对Mono和Bayer格式图像进行坏点计算。
 * 检测结果可导出为CSV文件，支持多帧统计确认真实坏点。
 */

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace mvtk {

/**
 * @brief 坏点类型枚举
 */
enum class DeadPixelType {
    BRIGHT,  ///< 亮坏点：像素亮度异常偏高
    DARK     ///< 暗坏点：像素亮度异常偏低
};

/**
 * @brief 坏点信息结构体
 */
struct DeadPixel {
    int x;              ///< 坏点x坐标
    int y;              ///< 坏点y坐标
    DeadPixelType type; ///< 坏点类型
    double value;       ///< 坏点像素值
};

/**
 * @brief 坏点检测参数
 */
struct DeadPixelDetectParams {
    DeadPixelType type = DeadPixelType::BRIGHT; ///< 坏点类型
    int threshold = 30;                         ///< 检测阈值（与邻域均值的差值）
    double avg_brightness_min = 0.0;            ///< 平均亮度最小值
    double avg_brightness_max = 255.0;          ///< 平均亮度最大值
    int kernel_size = 3;                        ///< 邻域核大小（3x3或5x5）
};

/**
 * @brief 坏点校正参数
 */
struct DeadPixelCorrectParams {
    int method = 0; ///< 校正方法：0=邻域均值插值，1=邻域中值插值
};

/**
 * @brief 坏点检测与校正类
 */
class DeadPixelDetector {
public:
    /**
     * @brief 自动检测图像中的坏点
     * @param image 输入图像（Mono或Bayer格式）
     * @param params 检测参数
     * @return 检测到的坏点列表
     */
    static std::vector<DeadPixel> detect(const cv::Mat& image, const DeadPixelDetectParams& params);

    /**
     * @brief 多帧统计检测坏点（仅当坏点在每帧中都存在时才确认为真实坏点）
     * @param images 多帧图像
     * @param params 检测参数
     * @return 确认的坏点列表
     */
    static std::vector<DeadPixel> detectMultiFrame(const std::vector<cv::Mat>& images, const DeadPixelDetectParams& params);

    /**
     * @brief 校正图像中的坏点
     * @param image 输入图像
     * @param pixels 坏点列表
     * @param params 校正参数
     * @return 校正后的图像
     */
    static cv::Mat correct(const cv::Mat& image, const std::vector<DeadPixel>& pixels, const DeadPixelCorrectParams& params);

    /**
     * @brief 导出坏点信息到CSV文件
     * @param pixels 坏点列表
     * @param filepath 保存路径
     * @return 是否成功
     */
    static bool exportCSV(const std::vector<DeadPixel>& pixels, const std::string& filepath);

    /**
     * @brief 从CSV文件导入坏点信息
     * @param filepath CSV文件路径
     * @return 坏点列表
     */
    static std::vector<DeadPixel> importCSV(const std::string& filepath);

    /**
     * @brief 在图像上标记坏点位置
     * @param image 输入图像
     * @param pixels 坏点列表
     * @return 标记后的图像（彩色）
     */
    static cv::Mat visualize(const cv::Mat& image, const std::vector<DeadPixel>& pixels);
};

} // namespace mvtk
