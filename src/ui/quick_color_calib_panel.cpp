/**
 * @file quick_color_calib_panel.cpp
 * @brief 快速色彩标定面板实现
 */

#include "ui/quick_color_calib_panel.h"
#include <imgui.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <string>

#include "utils/image_io.h"

namespace mvtk {
namespace ui {

QuickColorCalibPanel::QuickColorCalibPanel() = default;

void QuickColorCalibPanel::render() {
    ImGui::Begin("Quick Color Calibration");

    const char* steps[] = {"1. Acquire", "2. Draw ROI", "3. Reference", "4. Result"};
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

    const char* modes[] = {"Standard Mode", "Reference Mode"};
    ImGui::Combo("Calibration Mode", &calib_mode_, modes, 2);

    if (step_ == 0) {
        ImGui::Text("Step 1: Acquire Image");

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
            static char path_buf[256] = "";
            ImGui::InputText("Image Path", path_buf, sizeof(path_buf));
            ImGui::SameLine();
            if (ImGui::Button("Load Image")) {
                src_image_ = mvtk::utils::imreadUtf8(path_buf);
                if (!src_image_.empty()) {
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
        ImGui::Text("Exposure & Gain");
        static float exposure = 10000.0f;
        static float gain = 1.0f;
        ImGui::SliderFloat("Exposure", &exposure, 100.0f, 100000.0f);
        ImGui::SliderFloat("Gain", &gain, 0.1f, 10.0f);
        ImGui::SameLine();
        if (ImGui::Button("Auto")) {
            doAutoExposure();
        }

        if (!src_image_.empty()) {
            ImGui::Separator();
            ImGui::Text("Source Image");
            src_viewer_.setImage(src_image_);
            src_viewer_.render(ImVec2(400, 300));
        }

        if (!src_image_.empty()) {
            if (ImGui::Button("Next >>")) step_ = 1;
        }
    }

    if (step_ == 1) {
        ImGui::Text("Step 2: Draw ROI on Color Chart");

        ImGui::Checkbox("Auto Generate 24-Color ROI", &auto_roi_);
        if (auto_roi_) {
            ImGui::InputInt("Chart X", &chart_x_);
            ImGui::InputInt("Chart Y", &chart_y_);
            ImGui::InputInt("Chart Width", &chart_w_);
            ImGui::InputInt("Chart Height", &chart_h_);
            if (ImGui::Button("Generate ROI")) {
                generateROI();
            }
        }

        if (!rois_.empty() && !src_image_.empty()) {
            cv::Rect check_roi;
            if (rois_.size() >= 23) {
                check_roi = rois_[22].rect;
            }
            current_brightness_ = mvtk::WhiteBalance::computeBrightness(src_image_, check_roi);

            bool in_range = QuickColorCalibrator::checkBrightness(current_brightness_);
            ImGui::TextColored(in_range ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                               "Current Brightness: %.3f %s",
                               current_brightness_,
                               in_range ? "(OK)" : "(Out of Range)");
            ImGui::Text("Recommended: 0.55 ~ 0.65");
        }

        if (!rois_.empty()) {
            ImGui::Text("ROI List: %zu regions", rois_.size());
        }

        if (!src_image_.empty()) {
            ImGui::Separator();
            ImGui::Text("Source Image");
            src_viewer_.setImage(src_image_);
            src_viewer_.render(ImVec2(400, 300));
        }

        if (!rois_.empty()) {
            if (ImGui::Button("Next >>")) step_ = 2;
        }
        ImGui::SameLine();
        if (ImGui::Button("<< Back")) step_ = 0;
    }

    if (step_ == 2) {
        ImGui::Text("Step 3: Reference Settings");

        if (calib_mode_ == 0) {
            ImGui::Text("Standard Mode: Using X-Rite Classic 24 reference colors");
        } else {
            const char* sources[] = {"Local Image", "Camera"};
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
                if (ImGui::Button("Load Reference")) {
                    ref_image_ = mvtk::utils::imreadUtf8(ref_buf);
                    if (!ref_image_.empty()) {
                        status_msg_ = "Reference image loaded";
                    } else {
                        status_msg_ = "Failed to load reference image";
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
                    if (ImGui::Button("Capture Reference")) {
                        camera_ >> ref_image_;
                        if (!ref_image_.empty()) {
                            status_msg_ = "Reference image captured";
                        } else {
                            status_msg_ = "Failed to capture reference image";
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Close Camera##ref")) {
                        camera_.release();
                        camera_opened_ = false;
                        status_msg_ = "Camera closed";
                    }
                }
            }

            if (!status_msg_.empty()) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
            }

            if (!ref_image_.empty()) {
                ImGui::Separator();
                ImGui::Text("Reference Image");
                ref_viewer_.setImage(ref_image_);
                ref_viewer_.render(ImVec2(400, 300));
            }
        }

        if (ImGui::CollapsingHeader("Advanced Settings")) {
            const char* stat_methods[] = {"Mean", "Median"};
            int stat_idx = static_cast<int>(stat_method_);
            ImGui::Combo("RGB Stat Method", &stat_idx, stat_methods, 2);
            stat_method_ = static_cast<mvtk::RGBStatMethod>(stat_idx);

            const char* wb_methods[] = {"Average", "Analytic"};
            int wb_idx = static_cast<int>(wb_method_);
            ImGui::Combo("WB Method", &wb_idx, wb_methods, 2);
            wb_method_ = static_cast<mvtk::WBMethod>(wb_idx);

            const char* ccm_methods[] = {"Optimal", "Least Squares"};
            int ccm_idx = static_cast<int>(ccm_method_);
            ImGui::Combo("CCM Method", &ccm_idx, ccm_methods, 2);
            ccm_method_ = static_cast<mvtk::CCMMethod>(ccm_idx);
        }

        if (ImGui::Button("Calibrate")) {
            doCalibrate();
        }
        ImGui::SameLine();
        if (ImGui::Button("<< Back")) step_ = 1;
    }

    if (step_ == 3) {
        ImGui::Text("Step 4: Calibration Result");

        ImGui::Text("White Balance Parameters");
        ImGui::Text("  R Gain: %.4f", calib_result_.wb_params.r_gain);
        ImGui::Text("  G Gain: %.4f", calib_result_.wb_params.g_gain);
        ImGui::Text("  B Gain: %.4f", calib_result_.wb_params.b_gain);

        ImGui::Text("CCM Matrix");
        for (int i = 0; i < 3; ++i) {
            ImGui::Text("  [%.4f, %.4f, %.4f]",
                        calib_result_.ccm_params.ccm(i, 0),
                        calib_result_.ccm_params.ccm(i, 1),
                        calib_result_.ccm_params.ccm(i, 2));
        }

        ImGui::Text("Color Error (DeltaE00)");
        ImGui::Text("  Mean: %.2f", calib_result_.mean_deltaE00);

        if (!calib_result_.deltaE00.empty()) {
            if (ImGui::BeginTable("DeltaE00", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 150))) {
                ImGui::TableSetupColumn("Patch", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("DeltaE00", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (size_t i = 0; i < calib_result_.deltaE00.size(); ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Patch %zu", i);
                    ImGui::TableSetColumnIndex(1);
                    float de = static_cast<float>(calib_result_.deltaE00[i]);
                    ImVec4 color = (de < 3.0f) ? ImVec4(0, 1, 0, 1) :
                                   (de < 6.0f) ? ImVec4(1, 1, 0, 1) : ImVec4(1, 0, 0, 1);
                    ImGui::TextColored(color, "%.2f", de);
                }
                ImGui::EndTable();
            }
        }

        if (ImGui::Button("<< Back")) step_ = 2;
        ImGui::SameLine();
        if (ImGui::Button("Reset")) reset();
    }

    ImGui::End();
}

void QuickColorCalibPanel::doCalibrate() {
    if (src_image_.empty()) {
        status_msg_ = "Please load or capture an image first";
        return;
    }
    if (rois_.empty()) {
        status_msg_ = "Please generate or draw ROIs first";
        return;
    }

    std::vector<cv::Rect> rects;
    for (const auto& r : rois_) rects.push_back(r.rect);

    if (calib_mode_ == 0) {
        calib_result_ = QuickColorCalibrator::calibrateStandard(
            src_image_, rects, wb_method_, ccm_method_, stat_method_);
    } else {
        if (ref_image_.empty()) {
            status_msg_ = "Please load or capture a reference image";
            return;
        }
        calib_result_ = QuickColorCalibrator::calibrateReference(
            src_image_, ref_image_, rects, wb_method_, ccm_method_, stat_method_);
    }

    status_msg_ = "Calibration completed";
    step_ = 3;
}

void QuickColorCalibPanel::doAutoExposure() {
    if (src_image_.empty() || rois_.empty()) {
        status_msg_ = "Please load image and generate ROIs first";
        return;
    }
    cv::Rect check_roi = rois_.back().rect;
    double factor = QuickColorCalibrator::autoExposure(src_image_, check_roi);
    status_msg_ = "Auto exposure factor: " + std::to_string(factor);
}

void QuickColorCalibPanel::generateROI() {
    if (src_image_.empty()) {
        status_msg_ = "Please load or capture an image first";
        return;
    }
    auto roi_infos = ROIManager::generate24ColorROI(chart_x_, chart_y_, 30, 30, 2);
    rois_ = roi_infos;
    status_msg_ = "ROI generated: " + std::to_string(rois_.size()) + " regions";
}

void QuickColorCalibPanel::reset() {
    step_ = 0;
    rois_.clear();
    calib_result_ = mvtk::QuickCalibResult();
    corrected_image_ = cv::Mat();
    status_msg_ = "Reset completed";
}

} // namespace ui
} // namespace mvtk
