#pragma once
/**
 * @file image_viewer.h
 * @brief 图像查看器组件
 */

#include <opencv2/core.hpp>
#include <string>
#include <imgui.h>

namespace mvtk {
namespace ui {

class ImageViewer {
public:
    ImageViewer(const std::string& label = "ImageViewer");
    ~ImageViewer();

    void setImage(const cv::Mat& image);
    void render(const ImVec2& size = ImVec2(0, 0));
    cv::Point2d getImageCoordinate() const { return img_coord_; }
    bool isDragging() const { return dragging_; }
    float getZoom() const { return zoom_; }
    void resetView();
    void fitToWindow();

private:
    std::string label_;
    void* texture_id_ = nullptr;
    int tex_width_ = 0, tex_height_ = 0;
    float zoom_ = 1.0f;
    cv::Point2d img_coord_;
    bool dragging_ = false;
    bool has_image_ = false;

    void createTexture();
    void updateTexture(const cv::Mat& image);
};

} // namespace ui
} // namespace mvtk
