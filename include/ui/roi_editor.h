#pragma once
/**
 * @file roi_editor.h
 * @brief ROI编辑器组件
 *
 * 提供ROI的交互式绘制和编辑功能。
 */

#include "algo/roi.h"
#include <opencv2/core.hpp>
#include <vector>

namespace mvtk {
namespace ui {

/**
 * @brief ROI编辑器类
 */
class ROIEditor {
public:
    ROIEditor();

    /**
     * @brief 渲染ROI编辑器
     * @param rois ROI列表（可修改）
     */
    void render(std::vector<mvtk::ROIInfo>& rois);

    /**
     * @brief 获取当前选中的ROI索引
     */
    int getSelectedIndex() const { return selected_idx_; }

    /**
     * @brief 设置ROI类型
     */
    void setROIType(mvtk::ROIType type) { current_type_ = type; }

    /**
     * @brief 是否正在绘制ROI
     */
    bool isDrawing() const { return drawing_; }

private:
    int selected_idx_ = -1;
    mvtk::ROIType current_type_ = mvtk::ROIType::FREE;
    bool drawing_ = false;
    cv::Rect draw_rect_;
    cv::Point draw_start_;
};

} // namespace ui
} // namespace mvtk
