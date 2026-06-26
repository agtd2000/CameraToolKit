#pragma once
/**
 * @file roi.h
 * @brief ROI（感兴趣区域）管理
 *
 * 提供ROI的绘制、编辑和管理功能，支持24色卡ROI、140色卡ROI、
 * 自定义ROI和自由ROI等多种模式。
 */

#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace mvtk {

/**
 * @brief ROI类型
 */
enum class ROIType {
    COLOR_24,    ///< 24色卡ROI
    COLOR_140,   ///< 140色卡ROI
    CUSTOM,      ///< 自定义ROI
    FREE         ///< 自由ROI
};

/**
 * @brief ROI信息
 */
struct ROIInfo {
    ROIType type = ROIType::FREE; ///< ROI类型
    cv::Rect rect;                ///< 矩形区域
    std::string label;            ///< 标签
    int index = -1;               ///< 序号
};

/**
 * @brief ROI管理类
 */
class ROIManager {
public:
    /**
     * @brief 生成24色卡ROI（6行4列）
     * @param start_x 起始x
     * @param start_y 起始y
     * @param patch_w 色块宽度
     * @param patch_h 色块高度
     * @param gap 间距
     * @return 24个ROI信息
     */
    static std::vector<ROIInfo> generate24ColorROI(
        int start_x, int start_y, int patch_w, int patch_h, int gap = 2);

    /**
     * @brief 生成140色卡ROI
     * @param start_x 起始x
     * @param start_y 起始y
     * @param patch_w 色块宽度
     * @param patch_h 色块高度
     * @param gap 间距
     * @return 140个ROI信息
     */
    static std::vector<ROIInfo> generate140ColorROI(
        int start_x, int start_y, int patch_w, int patch_h, int gap = 2);

    /**
     * @brief 在图像上绘制ROI标记
     * @param image 输入图像
     * @param rois ROI列表
     * @param selected 当前选中的ROI索引（-1表示无）
     * @return 标记后的图像
     */
    static cv::Mat drawROIs(const cv::Mat& image, const std::vector<ROIInfo>& rois, int selected = -1);

    /**
     * @brief 使用OpenCV MCC模块自动检测色卡
     * @param image 输入图像
     * @param type 色卡类型（COLOR_24或COLOR_140）
     * @return 检测到的ROI列表，未检测到则返回空列表
     */
    static std::vector<ROIInfo> detectColorChecker(const cv::Mat& image, ROIType type);

    /**
     * @brief 判断点是否在某个ROI内
     * @param point 点坐标
     * @param rois ROI列表
     * @return ROI索引，-1表示不在任何ROI内
     */
    static int hitTest(const cv::Point& point, const std::vector<ROIInfo>& rois);
};

} // namespace mvtk
