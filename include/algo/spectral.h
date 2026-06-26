#pragma once
/**
 * @file spectral.h
 * @brief 光谱特性标定算法
 *
 * 实现光谱特性标定文件的生成，用于相机的自动色彩校正功能。
 * 支持出厂标定和用户标定两种模式，以及标准和简易两种操作复杂度。
 * 光谱标定文件最多可记录20种光源的数据。
 */

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace mvtk {

/**
 * @brief 标定模式
 */
enum class SpectralCalibMode {
    FACTORY,  ///< 出厂标定：从零开始标定典型光源
    USER      ///< 用户标定：基于出厂标定文件增加更多光源
};

/**
 * @brief 操作复杂度
 */
enum class SpectralComplexity {
    STANDARD,  ///< 标准：需采集各种光源在24色背景下的反射光谱曲线
    SIMPLE     ///< 简易：仅需采集白色背景下的反射光谱曲线
};

/**
 * @brief 光谱曲线数据
 */
struct SpectralCurve {
    std::vector<double> wavelengths; ///< 波长数组(nm)
    std::vector<double> intensities; ///< 强度数组
};

/**
 * @brief 光源信息
 */
struct LightSourceInfo {
    std::string name;           ///< 光源名称
    double color_temperature;   ///< 色温(K)
    std::vector<SpectralCurve> curves; ///< 各色卡/被测物的光谱曲线
};

/**
 * @brief 标定区域计算结果
 */
struct CalibRegionResult {
    double center_offset;    ///< 标定中心相对位置(mm)
    double region_size;      ///< 区域对应物面大小(mm)
};

/**
 * @brief 光谱特性标定类
 */
class SpectralCalibrator {
public:
    /**
     * @brief 采集光谱曲线（模拟接口，实际需连接光谱仪）
     * @param image 输入图像
     * @param roi 采集区域
     * @return 光谱曲线数据
     */
    static SpectralCurve captureCurve(const cv::Mat& image, const cv::Rect& roi);

    /**
     * @brief 添加光源信息
     * @param light_sources 已有光源列表
     * @param name 光源名称
     * @param color_temp 色温
     * @param curves 光谱曲线数据
     */
    static void addLightSource(std::vector<LightSourceInfo>& light_sources,
                               const std::string& name,
                               double color_temp,
                               const std::vector<SpectralCurve>& curves);

    /**
     * @brief 计算标定区域
     * @param focal_length 镜头焦距(mm)
     * @param working_distance 工作距离(mm)
     * @param sensor_width 传感器宽度(mm)
     * @param sensor_height 传感器高度(mm)
     * @param method 计算方法(1=焦距+工作距离, 2=物方视野)
     * @return 标定区域结果
     */
    static CalibRegionResult computeCalibRegion(
        double focal_length, double working_distance,
        double sensor_width, double sensor_height,
        int method = 1);

    /**
     * @brief 生成标定文件
     * @param light_sources 光源信息列表
     * @param mode 标定模式
     * @param complexity 操作复杂度
     * @param save_path 保存路径
     * @return 是否成功
     */
    static bool generateCalibFile(const std::vector<LightSourceInfo>& light_sources,
                                  SpectralCalibMode mode,
                                  SpectralComplexity complexity,
                                  const std::string& save_path);

    /**
     * @brief 导入出厂标定文件
     * @param filepath 文件路径
     * @return 光源信息列表
     */
    static std::vector<LightSourceInfo> importFactoryCalibFile(const std::string& filepath);

    /**
     * @brief 预览标定结果（生成光谱曲线可视化）
     * @param light_sources 光源信息列表
     * @return 可视化图像
     */
    static cv::Mat previewSpectralCurves(const std::vector<LightSourceInfo>& light_sources);
};

} // namespace mvtk
