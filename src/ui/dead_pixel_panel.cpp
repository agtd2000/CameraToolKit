/**
 * @file dead_pixel_panel.cpp
 * @brief 坏点校正面板实现
 */

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <imgui.h>
#include <string>

#include "ui/dead_pixel_panel.h"
#include "utils/image_io.h"

namespace mvtk {
namespace ui {

DeadPixelPanel::DeadPixelPanel() {
    detect_params_.type = DeadPixelType::BRIGHT;
    detect_params_.threshold = 30;
    detect_params_.avg_brightness_min = 0;
    detect_params_.avg_brightness_max = 255;
}

void DeadPixelPanel::render() {
    ImGui::Begin("Dead Pixel Correction");

    ImGui::Text("Image Source");
    const char* sources[] = {"Local Image", "Camera"};
    int prev_image_source = image_source_;
    ImGui::Combo("Source", &image_source_, sources, 2);

    if (prev_image_source != image_source_) {
        if (camera_opened_) {
            camera_.release();
            camera_opened_ = false;
        }
    }

    if (image_source_ == 0) {
        static char path_buf[256] = "";
        ImGui::InputText("Image Path", path_buf, sizeof(path_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            src_image_ = mvtk::utils::imreadUtf8(path_buf);
            if (!src_image_.empty()) {
                vis_image_ = src_image_.clone();
                status_msg_ = "Image loaded: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows);
            } else {
                status_msg_ = "Failed to load image: " + std::string(path_buf);
            }
        }
    } else {
        if (!camera_opened_) {
            if (ImGui::Button("Open Camera")) {
                camera_.open(0);
                if (camera_.isOpened()) {
                    camera_opened_ = true;
                    status_msg_ = "Camera opened successfully";
                } else {
                    status_msg_ = "Failed to open camera";
                }
            }
        } else {
            if (ImGui::Button("Capture")) {
                camera_ >> src_image_;
                if (!src_image_.empty()) {
                    vis_image_ = src_image_.clone();
                    status_msg_ = "Image captured: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows);
                } else {
                    status_msg_ = "Failed to capture image";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close Camera")) {
                camera_.release();
                camera_opened_ = false;
                status_msg_ = "Camera closed";
            }
        }
    }

    if (!status_msg_.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
    }

    ImGui::Separator();

    ImGui::Text("Detection Parameters");

    const char* dp_types[] = {"Bright Dead Pixel", "Dark Dead Pixel"};
    int dp_type = static_cast<int>(detect_params_.type);
    ImGui::Combo("Dead Pixel Type", &dp_type, dp_types, 2);
    detect_params_.type = static_cast<DeadPixelType>(dp_type);

    ImGui::SliderInt("Threshold", &detect_params_.threshold, 1, 100);
    ImGui::SliderInt("Avg Brightness Min", reinterpret_cast<int*>(&detect_params_.avg_brightness_min), 0, 255);
    ImGui::SliderInt("Avg Brightness Max", reinterpret_cast<int*>(&detect_params_.avg_brightness_max), 0, 255);
    ImGui::SliderInt("Kernel Size", &detect_params_.kernel_size, 3, 5);

    if (ImGui::Button("Auto Detect")) {
        doDetect();
    }
    ImGui::SameLine();
    if (ImGui::Button("Detect (Multi-Frame)")) {
    }

    ImGui::Separator();

    ImGui::Text("Dead Pixel List (%zu pixels)", dead_pixels_.size());
    if (ImGui::BeginTable("DeadPixels", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 150))) {
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < dead_pixels_.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", dead_pixels_[i].x);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", dead_pixels_[i].y);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", dead_pixels_[i].type == DeadPixelType::BRIGHT ? "Bright" : "Dark");
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f", dead_pixels_[i].value);
        }
        ImGui::EndTable();
    }

    ImGui::Checkbox("Show Detected", &show_detected_);
    ImGui::SameLine();
    if (ImGui::Button("Export CSV")) doExport();
    ImGui::SameLine();
    if (ImGui::Button("Import CSV")) doImport();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) doClear();

    ImGui::Separator();

    ImGui::Text("Correction Parameters");
    const char* methods[] = {"Mean Interpolation", "Median Interpolation"};
    ImGui::Combo("Method", &correct_params_.method, methods, 2);

    if (ImGui::Button("Correct")) {
        doCorrect();
    }

    if (!vis_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Source Image");
        src_viewer_.setImage(vis_image_);
        src_viewer_.render(ImVec2(400, 300));
    }

    if (!corrected_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Corrected Image");
        result_viewer_.setImage(corrected_image_);
        result_viewer_.render(ImVec2(400, 300));
    }

    ImGui::End();
}

void DeadPixelPanel::doDetect() {
    if (src_image_.empty()) {
        status_msg_ = "Please load or capture an image first";
        return;
    }
    dead_pixels_ = DeadPixelDetector::detect(src_image_, detect_params_);
    vis_image_ = DeadPixelDetector::visualize(src_image_, dead_pixels_);
    status_msg_ = "Detected " + std::to_string(dead_pixels_.size()) + " dead pixels";
}

void DeadPixelPanel::doCorrect() {
    if (src_image_.empty()) {
        status_msg_ = "Please load or capture an image first";
        return;
    }
    if (dead_pixels_.empty()) {
        status_msg_ = "No dead pixels to correct";
        return;
    }
    corrected_image_ = DeadPixelDetector::correct(src_image_, dead_pixels_, correct_params_);
    status_msg_ = "Correction completed";
}

void DeadPixelPanel::doExport() {
    DeadPixelDetector::exportCSV(dead_pixels_, csv_path_);
    status_msg_ = "Dead pixels exported to " + csv_path_;
}

void DeadPixelPanel::doImport() {
    dead_pixels_ = DeadPixelDetector::importCSV(csv_path_);
    if (!src_image_.empty()) {
        vis_image_ = DeadPixelDetector::visualize(src_image_, dead_pixels_);
    }
    status_msg_ = "Dead pixels imported: " + std::to_string(dead_pixels_.size());
}

void DeadPixelPanel::doClear() {
    dead_pixels_.clear();
    vis_image_ = src_image_.clone();
    status_msg_ = "Dead pixel list cleared";
}

} // namespace ui
} // namespace mvtk
