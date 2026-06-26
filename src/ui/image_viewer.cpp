/**
 * @file image_viewer.cpp
 * @brief 图像查看器组件实现
 */

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3_loader.h>

#include "ui/image_viewer.h"

namespace mvtk {
namespace ui {

ImageViewer::ImageViewer(const std::string& label) : label_(label) {}

ImageViewer::~ImageViewer() {
    if (texture_id_) {
        GLuint tid = static_cast<GLuint>(reinterpret_cast<intptr_t>(texture_id_));
        glDeleteTextures(1, &tid);
    }
}

void ImageViewer::createTexture() {
    GLuint tid;
    glGenTextures(1, &tid);
    texture_id_ = reinterpret_cast<void*>(static_cast<intptr_t>(tid));
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
}

void ImageViewer::updateTexture(const cv::Mat& image) {
    if (image.empty()) return;

    cv::Mat rgba;
    if (image.channels() == 1) {
        cv::cvtColor(image, rgba, cv::COLOR_GRAY2RGBA);
    } else if (image.channels() == 3) {
        cv::cvtColor(image, rgba, cv::COLOR_BGR2RGBA);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, rgba, cv::COLOR_BGRA2RGBA);
    } else {
        return;
    }

    tex_width_ = rgba.cols;
    tex_height_ = rgba.rows;

    if (!texture_id_) createTexture();

    GLuint tid = static_cast<GLuint>(reinterpret_cast<intptr_t>(texture_id_));
    glBindTexture(GL_TEXTURE_2D, tid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_, tex_height_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
    has_image_ = true;
}

void ImageViewer::setImage(const cv::Mat& image) {
    updateTexture(image);
    fitToWindow();
}

void ImageViewer::render(const ImVec2& size) {
    if (!has_image_) {
        ImGui::Text("No image loaded");
        return;
    }

    ImVec2 avail = (size.x > 0 && size.y > 0) ? size : ImGui::GetContentRegionAvail();

    float img_aspect = static_cast<float>(tex_width_) / tex_height_;
    float area_aspect = avail.x / avail.y;

    float display_w, display_h;
    if (img_aspect > area_aspect) {
        display_w = avail.x * zoom_;
        display_h = display_w / img_aspect;
    } else {
        display_h = avail.y * zoom_;
        display_w = display_h * img_aspect;
    }

    ImGui::Image(texture_id_,
                 ImVec2(display_w, display_h),
                 ImVec2(0, 0), ImVec2(1, 1));

    if (ImGui::IsItemHovered()) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 item_min = ImGui::GetItemRectMin();
        ImVec2 item_max = ImGui::GetItemRectMax();

        float rel_x = (mouse_pos.x - item_min.x) / (item_max.x - item_min.x);
        float rel_y = (mouse_pos.y - item_min.y) / (item_max.y - item_min.y);

        img_coord_ = cv::Point2d(rel_x * tex_width_, rel_y * tex_height_);

        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            zoom_ *= (1.0f + wheel * 0.1f);
            zoom_ = std::max(0.1f, std::min(10.0f, zoom_));
        }
    }

    ImGui::Text("Pos: (%.0f, %.0f) Zoom: %.1fx", img_coord_.x, img_coord_.y, zoom_);
}

void ImageViewer::resetView() {
    zoom_ = 1.0f;
}

void ImageViewer::fitToWindow() {
    zoom_ = 1.0f;
}

} // namespace ui
} // namespace mvtk
