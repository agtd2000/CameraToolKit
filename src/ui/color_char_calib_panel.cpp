/**
 * @file color_char_calib_panel.cpp
 * @brief 色彩特性标定面板实现
 */

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <imgui.h>
#include <string>
#include <algorithm>
#include <fstream>

#include "ui/color_char_calib_panel.h"
#include "algo/quick_color_calib.h"
#include "utils/image_io.h"

namespace mvtk {
namespace ui {

ColorCharCalibPanel::ColorCharCalibPanel() = default;

void ColorCharCalibPanel::render() {
    ImGui::Begin("Color Characteristic Calibration");

    const char* steps[] = {"1. Light Sources", "2. Parameters", "3. Result", "4. Export"};
    ImGui::PushItemWidth(-1);
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine();
        bool active = (i == step_);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::Button(steps[i], ImVec2(120, 30));
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopItemWidth();

    ImGui::Separator();

    if (step_ == 0) {
        ImGui::Text("Step 1: Add Light Sources");

        ImGui::Text("Light Sources List");
        if (ImGui::BeginListBox("##LightSources", ImVec2(-1, 150))) {
            for (size_t i = 0; i < light_sources_.size(); ++i) {
                bool selected = (current_light_idx_ == static_cast<int>(i));
                std::string item = light_sources_[i].name + " (" + std::to_string(static_cast<int>(light_sources_[i].color_temp)) + "K)";
                if (ImGui::Selectable(item.c_str(), selected)) {
                    current_light_idx_ = static_cast<int>(i);
                }
            }
            ImGui::EndListBox();
        }

        ImGui::Text("Current Light Source Info");
        if (current_light_idx_ >= 0 && current_light_idx_ < static_cast<int>(light_sources_.size())) {
            ImGui::Text("Name: %s", light_sources_[current_light_idx_].name.c_str());
            ImGui::Text("Color Temp: %.0f K", light_sources_[current_light_idx_].color_temp);
            ImGui::Text("Image: %dx%d", light_sources_[current_light_idx_].image.cols, light_sources_[current_light_idx_].image.rows);
            ImGui::Text("ROIs: %zu", light_sources_[current_light_idx_].rois.size());

            if (!light_sources_[current_light_idx_].image.empty()) {
                ImGui::Separator();
                ImGui::Text("Image Preview");
                image_viewer_.setImage(light_sources_[current_light_idx_].image);
                image_viewer_.render(ImVec2(400, 300));
            }
        }

        ImGui::Separator();
        ImGui::Text("Add New Light Source");

        ImGui::InputText("Light Name", new_light_name_, sizeof(new_light_name_));
        ImGui::SliderFloat("Color Temperature", reinterpret_cast<float*>(&new_light_temp_), 2000.0f, 10000.0f);

        const char* sources[] = {"Local Image", "Camera"};
        int prev_image_source = image_source_;
        ImGui::Combo("Image Source", &image_source_, sources, 2);

        if (prev_image_source != image_source_) {
            if (camera_opened_) {
                camera_.release();
                camera_opened_ = false;
            }
        }

        static char img_path[256] = "";
        if (image_source_ == 0) {
            ImGui::InputText("Image Path", img_path, sizeof(img_path));
            ImGui::SameLine();
            if (ImGui::Button("Load Image")) {
                cv::Mat img = mvtk::utils::imreadUtf8(img_path);
                if (!img.empty()) {
                    LightSourceUIData ls;
                    ls.name = std::string(new_light_name_);
                    ls.color_temp = new_light_temp_;
                    ls.image = img;
                    ls.rois = ROIManager::generate24ColorROI(50, 50, 30, 30, 2);
                    light_sources_.push_back(ls);
                    current_light_idx_ = static_cast<int>(light_sources_.size()) - 1;
                    status_msg_ = "Light source added from file";
                } else {
                    status_msg_ = "Failed to load image";
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
                    cv::Mat img;
                    camera_ >> img;
                    if (!img.empty()) {
                        LightSourceUIData ls;
                        ls.name = std::string(new_light_name_);
                        ls.color_temp = new_light_temp_;
                        ls.image = img;
                        ls.rois = ROIManager::generate24ColorROI(50, 50, 30, 30, 2);
                        light_sources_.push_back(ls);
                        current_light_idx_ = static_cast<int>(light_sources_.size()) - 1;
                        status_msg_ = "Light source added from camera";
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

        if (current_light_idx_ >= 0) {
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                removeLightSource(current_light_idx_);
            }
        }

        if (!status_msg_.empty()) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
        }

        if (!light_sources_.empty()) {
            if (ImGui::Button("Next >>")) step_ = 1;
        }
    }

    if (step_ == 1) {
        ImGui::Text("Step 2: Calibration Parameters");

        const char* wb_methods[] = {"Average", "Analytic"};
        int wb_idx = static_cast<int>(wb_method_);
        ImGui::Combo("WB Method", &wb_idx, wb_methods, 2);
        wb_method_ = static_cast<mvtk::WBMethod>(wb_idx);

        const char* ccm_methods[] = {"Optimal", "Least Squares"};
        int ccm_idx = static_cast<int>(ccm_method_);
        ImGui::Combo("CCM Method", &ccm_idx, ccm_methods, 2);
        ccm_method_ = static_cast<mvtk::CCMMethod>(ccm_idx);

        const char* color_std[] = {"Standard 24 Colors", "Custom Colors"};
        ImGui::Combo("Standard Colors", &standard_color_, color_std, 2);

        ImGui::Checkbox("Quantization", &quantize_);
        if (quantize_) {
            ImGui::InputInt("WB Quantization", &wb_quant_);
            ImGui::InputInt("CCM Quantization", &ccm_quant_);
        }

        if (ImGui::Button("Calibrate")) {
            doCalibrate();
        }
        ImGui::SameLine();
        if (ImGui::Button("<< Back")) step_ = 0;
    }

    if (step_ == 2) {
        ImGui::Text("Step 3: Calibration Result");

        ImGui::Text("Light Sources: %zu", light_sources_.size());
        ImGui::Text("WB Results: %zu", wb_results_.size());
        ImGui::Text("CCM Results: %zu", ccm_results_.size());

        if (!wb_results_.empty()) {
            ImGui::Text("White Balance Parameters for each light source:");
            for (size_t i = 0; i < wb_results_.size() && i < light_sources_.size(); ++i) {
                ImGui::Text("%s: R=%.4f, G=%.4f, B=%.4f",
                            light_sources_[i].name.c_str(),
                            wb_results_[i].r_gain,
                            wb_results_[i].g_gain,
                            wb_results_[i].b_gain);
            }
        }

        if (!ccm_results_.empty()) {
            ImGui::Text("CCM Matrix (first light source):");
            for (int i = 0; i < 3; ++i) {
                ImGui::Text("  [%.4f, %.4f, %.4f]",
                            ccm_results_[0].ccm(i, 0),
                            ccm_results_[0].ccm(i, 1),
                            ccm_results_[0].ccm(i, 2));
            }
        }

        if (ImGui::Button("Next >>")) step_ = 3;
        ImGui::SameLine();
        if (ImGui::Button("<< Back")) step_ = 1;
    }

    if (step_ == 3) {
        ImGui::Text("Step 4: Export Calibration");

        ImGui::Text("Export to: color_char_calib.dat");
        if (ImGui::Button("Export")) {
            doExport();
            status_msg_ = "Calibration data exported";
        }

        if (!status_msg_.empty()) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
        }

        if (ImGui::Button("<< Back")) step_ = 2;
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            light_sources_.clear();
            wb_results_.clear();
            ccm_results_.clear();
            current_light_idx_ = -1;
            step_ = 0;
        }
    }

    ImGui::End();
}

void ColorCharCalibPanel::addLightSource() {
    LightSourceUIData ls;
    ls.name = std::string(new_light_name_);
    ls.color_temp = new_light_temp_;
    light_sources_.push_back(ls);
    current_light_idx_ = static_cast<int>(light_sources_.size()) - 1;
}

void ColorCharCalibPanel::removeLightSource(int idx) {
    if (idx >= 0 && idx < static_cast<int>(light_sources_.size())) {
        light_sources_.erase(light_sources_.begin() + idx);
        current_light_idx_ = -1;
    }
}

void ColorCharCalibPanel::doCalibrate() {
    if (light_sources_.empty()) {
        status_msg_ = "Please add at least one light source";
        return;
    }

    wb_results_.clear();
    ccm_results_.clear();

    std::vector<cv::Vec3d> ref_colors = StandardColorChart::getXriteClassic24();

    for (const auto& ls : light_sources_) {
        if (ls.image.empty()) continue;

        std::vector<cv::Rect> rects;
        for (const auto& roi : ls.rois) rects.push_back(roi.rect);

        std::vector<cv::Vec3d> src_colors = WhiteBalance::extractColors(ls.image, rects);

        mvtk::WBCalibParams wb_params_;
        wb_params_.method = wb_method_;
        auto wb_params = WhiteBalance::calibrateFromColors(src_colors, ref_colors, wb_params_);
        wb_results_.push_back(wb_params);

        mvtk::CCMCalibParams ccm_params_;
        ccm_params_.method = ccm_method_;
        auto ccm_params = ColorCorrectionMatrix::calibrate(src_colors, ref_colors, ccm_params_);
        ccm_results_.push_back(ccm_params);
    }

    status_msg_ = "Calibration completed: " + std::to_string(wb_results_.size()) + " light sources";
    step_ = 2;
}

void ColorCharCalibPanel::doExport() {
    std::ofstream ofs("color_char_calib.dat");
    if (!ofs) return;

    ofs << "COLOR_CHAR_CALIB" << std::endl;
    ofs << light_sources_.size() << std::endl;

    for (size_t i = 0; i < light_sources_.size() && i < wb_results_.size() && i < ccm_results_.size(); ++i) {
        ofs << light_sources_[i].name << std::endl;
        ofs << light_sources_[i].color_temp << std::endl;
        ofs << wb_results_[i].r_gain << " " << wb_results_[i].g_gain << " " << wb_results_[i].b_gain << std::endl;
        for (int j = 0; j < 3; ++j) {
            ofs << ccm_results_[i].ccm(j, 0) << " " << ccm_results_[i].ccm(j, 1) << " " << ccm_results_[i].ccm(j, 2) << std::endl;
        }
    }

    ofs.close();
}

} // namespace ui
} // namespace mvtk
