/**
 * @file color_error_panel.cpp
 * @brief 色彩误差计算面板实现
 */

#include "ui/color_error_panel.h"
#include <imgui.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>

#include "algo/white_balance.h"
#include "utils/image_io.h"

namespace mvtk {
namespace ui {

ColorErrorPanel::ColorErrorPanel() = default;

void ColorErrorPanel::render() {
    ImGui::Begin("Color Error Calculator");

    // 图像来源选择
    ImGui::Text("Source Image");
    const char* sources[] = {"Local Image", "Camera"};
    int prev_src_source = src_source_;
    ImGui::Combo("Source", &src_source_, sources, 2);

    if (prev_src_source != src_source_) {
        if (camera_opened_) {
            camera_.release();
            camera_opened_ = false;
        }
    }

    if (src_source_ == 0) {
        static char src_buf[256] = "";
        ImGui::InputText("Source Image Path", src_buf, sizeof(src_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load##src")) {
            src_image_ = mvtk::utils::imreadUtf8(src_buf);
            if (!src_image_.empty()) {
                status_msg_ = "Source image loaded: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows);
                has_src_image_ = true;
            } else {
                status_msg_ = "Failed to load source image: " + std::string(src_buf);
                has_src_image_ = false;
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
            if (ImGui::Button("Capture##src")) {
                camera_ >> src_image_;
                if (!src_image_.empty()) {
                    status_msg_ = "Image captured: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows);
                    has_src_image_ = true;
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

    // 参考图像
    ImGui::Text("Reference Image");
    int prev_ref_source = ref_source_;
    ImGui::Combo("Reference Source", &ref_source_, sources, 2);

    if (prev_ref_source != ref_source_) {
        if (camera_opened_) {
            camera_.release();
            camera_opened_ = false;
        }
    }

    if (ref_source_ == 0) {
        static char ref_buf[256] = "";
        ImGui::InputText("Reference Image Path", ref_buf, sizeof(ref_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load##ref")) {
            ref_image_ = mvtk::utils::imreadUtf8(ref_buf);
            if (!ref_image_.empty()) {
                status_msg_ = "Reference image loaded: " + std::to_string(ref_image_.cols) + "x" + std::to_string(ref_image_.rows);
                has_ref_image_ = true;
            } else {
                status_msg_ = "Failed to load reference image: " + std::string(ref_buf);
                has_ref_image_ = false;
            }
        }
    } else {
        if (!camera_opened_) {
            if (ImGui::Button("Open Camera##ref")) {
                camera_.open(0);
                if (camera_.isOpened()) {
                    camera_opened_ = true;
                    status_msg_ = "Camera opened successfully";
                } else {
                    status_msg_ = "Failed to open camera";
                }
            }
        } else {
            if (ImGui::Button("Capture##ref")) {
                camera_ >> ref_image_;
                if (!ref_image_.empty()) {
                    status_msg_ = "Reference image captured";
                    has_ref_image_ = true;
                } else {
                    status_msg_ = "Failed to capture reference image";
                }
            }
        }
    }

    // 状态提示
    if (!status_msg_.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
    }

    // 图像预览
    if (!src_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Source Image");
        src_viewer_.setImage(src_image_);
        src_viewer_.render(ImVec2(400, 300));
    }

    if (!ref_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Reference Image");
        ref_viewer_.setImage(ref_image_);
        ref_viewer_.render(ImVec2(400, 300));
    }

    ImGui::Separator();

    ImGui::Text("ROI Settings");
    if (ImGui::Button("Draw 24-Color ROI")) {
        rois_ = ROIManager::generate24ColorROI(100, 50, 30, 30, 2);
    }
    ImGui::SameLine();
    if (ImGui::Button("Draw 140-Color ROI")) {
        rois_ = ROIManager::generate140ColorROI(50, 50, 15, 15, 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear ROI")) {
        rois_.clear();
    }

    ImGui::Text("ROI Count: %zu", rois_.size());

    if (!rois_.empty() && !src_image_.empty()) {
        std::vector<cv::Rect> rects;
        for (const auto& roi : rois_) rects.push_back(roi.rect);
        src_colors_ = WhiteBalance::extractColors(src_image_, rects, mvtk::RGBStatMethod::MEDIAN);
    }
    if (!rois_.empty() && !ref_image_.empty()) {
        std::vector<cv::Rect> rects;
        for (const auto& roi : rois_) rects.push_back(roi.rect);
        ref_colors_ = WhiteBalance::extractColors(ref_image_, rects, mvtk::RGBStatMethod::MEDIAN);
    }

    ImGui::Separator();
    ImGui::Text("Color Pairs");
    if (ImGui::Button("+ Add Row")) {
        src_colors_.push_back(cv::Vec3d(128, 128, 128));
        ref_colors_.push_back(cv::Vec3d(128, 128, 128));
    }
    ImGui::SameLine();
    if (ImGui::Button("Import Color File")) {
        doImportColors();
    }

    if (!src_colors_.empty()) {
        if (ImGui::BeginTable("ColorPairs", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("Src R", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Src G", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Src B", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Ref R", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Ref G", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Ref B", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < src_colors_.size() && i < ref_colors_.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.0f", src_colors_[i][0]);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.0f", src_colors_[i][1]);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.0f", src_colors_[i][2]);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.0f", ref_colors_[i][0]);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.0f", ref_colors_[i][1]);
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%.0f", ref_colors_[i][2]);
            }
            ImGui::EndTable();
        }
    }

    ImGui::Separator();

    ImGui::Checkbox("sRGB Color Space", &params_.use_srgb);

    if (ImGui::Button("Calculate")) {
        doCalculate();
    }

    if (has_result_) {
        ImGui::Separator();
        ImGui::Text("Calculation Result");
        ImGui::Text("Mean DeltaE00: %.2f", result_.mean_deltaE00);
        ImGui::Text("Max DeltaE00: %.2f", result_.max_deltaE00);
        ImGui::Text("Min DeltaE00: %.2f", result_.min_deltaE00);

        if (ImGui::BeginTable("ErrorResult", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("DeltaE00", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("DeltaE76", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("DeltaL", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("DeltaC", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < result_.pairs.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", i);
                ImGui::TableSetColumnIndex(1);
                float de = static_cast<float>(result_.pairs[i].deltaE00);
                ImVec4 color = (de < 3.0f) ? ImVec4(0, 1, 0, 1) :
                               (de < 6.0f) ? ImVec4(1, 1, 0, 1) : ImVec4(1, 0, 0, 1);
                ImGui::TextColored(color, "%.2f", de);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f", result_.pairs[i].deltaE76);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f", result_.pairs[i].deltaL);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.2f", result_.pairs[i].deltaC);
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("Export Result")) {
            doExport();
        }
    }

    ImGui::End();
}

void ColorErrorPanel::doCalculate() {
    if (src_colors_.empty() || ref_colors_.empty()) {
        status_msg_ = "Please provide source and reference colors";
        return;
    }
    if (src_colors_.size() != ref_colors_.size()) {
        status_msg_ = "Source and reference color counts must match";
        return;
    }

    result_ = ColorErrorCalculator::calculate(src_colors_, ref_colors_, params_);
    has_result_ = true;
    status_msg_ = "Calculation completed: " + std::to_string(src_colors_.size()) + " color pairs";
}

void ColorErrorPanel::doExport() {
    ColorErrorCalculator::exportResult(result_, "color_error_result.csv");
    status_msg_ = "Result exported to color_error_result.csv";
}

void ColorErrorPanel::doImportColors() {
    auto colors = ColorErrorCalculator::importColorsFromIni(color_file_path_);
    if (!colors.empty()) {
        ref_colors_ = colors;
        status_msg_ = "Colors imported: " + std::to_string(colors.size()) + " colors";
    } else {
        status_msg_ = "Failed to import colors";
    }
}

} // namespace ui
} // namespace mvtk
