/**
 * @file flat_field_panel.cpp
 * @brief 平场校正面板实现
 */

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <imgui.h>
#include <string>

#include "ui/flat_field_panel.h"
#include "utils/image_io.h"

namespace mvtk {
namespace ui {

FlatFieldPanel::FlatFieldPanel() {
    calib_params_.mode = LSCCalibMode::CENTER;
    calib_params_.edge_fill = 2;
    calib_params_.smooth_kernel = 64;
    calib_params_.target_brightness = 128.0;
}

void FlatFieldPanel::render() {
    ImGui::Begin("Flat Field Correction (LSC)");

    const char* sources[] = {"Local Image", "Camera"};

    ImGui::Text("Flat Field Image");
    int prev_flat_source = flat_source_;
    ImGui::Combo("Flat Source", &flat_source_, sources, 2);

    if (prev_flat_source != flat_source_) {
        if (camera_opened_) {
            camera_.release();
            camera_opened_ = false;
        }
    }

    if (flat_source_ == 0) {
        static char flat_buf[256] = "";
        ImGui::InputText("Flat Image Path", flat_buf, sizeof(flat_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load Flat")) {
            src_image_ = mvtk::utils::imreadUtf8(flat_buf);
            if (!src_image_.empty()) {
                status_msg_ = "Flat image loaded: " + std::to_string(src_image_.cols) + "x" + std::to_string(src_image_.rows);
            } else {
                status_msg_ = "Failed to load flat image: " + std::string(flat_buf);
            }
        }
    } else {
        if (!camera_opened_) {
            if (ImGui::Button("Open Camera##flat")) {
                camera_.open(0);
                if (camera_.isOpened()) {
                    camera_opened_ = true;
                    status_msg_ = "Camera opened successfully";
                } else {
                    status_msg_ = "Failed to open camera";
                }
            }
        } else {
            if (ImGui::Button("Capture Flat")) {
                camera_ >> src_image_;
                if (!src_image_.empty()) {
                    status_msg_ = "Flat image captured";
                } else {
                    status_msg_ = "Failed to capture flat image";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close Camera##flat")) {
                camera_.release();
                camera_opened_ = false;
                status_msg_ = "Camera closed";
            }
        }
    }

    ImGui::Checkbox("Use Dark Frame", &use_dark_);
    if (use_dark_) {
        int prev_dark_source = dark_source_;
        ImGui::Combo("Dark Source", &dark_source_, sources, 2);

        if (prev_dark_source != dark_source_) {
            if (camera_opened_) {
                camera_.release();
                camera_opened_ = false;
            }
        }
        if (dark_source_ == 0) {
            static char dark_buf[256] = "";
            ImGui::InputText("Dark Image Path", dark_buf, sizeof(dark_buf));
            ImGui::SameLine();
            if (ImGui::Button("Load Dark")) {
                dark_image_ = mvtk::utils::imreadUtf8(dark_buf);
                if (!dark_image_.empty()) {
                    status_msg_ = "Dark image loaded";
                } else {
                    status_msg_ = "Failed to load dark image";
                }
            }
        } else {
            if (!camera_opened_) {
                if (ImGui::Button("Open Camera##dark")) {
                    camera_.open(0);
                    if (camera_.isOpened()) {
                        camera_opened_ = true;
                        status_msg_ = "Camera opened successfully";
                    } else {
                        status_msg_ = "Failed to open camera";
                    }
                }
            } else {
                if (ImGui::Button("Capture Dark")) {
                    camera_ >> dark_image_;
                    if (!dark_image_.empty()) {
                        status_msg_ = "Dark image captured";
                    } else {
                        status_msg_ = "Failed to capture dark image";
                    }
                }
            }
        }
    }

    if (!status_msg_.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
    }

    ImGui::Separator();

    ImGui::Text("Calibration Parameters");

    const char* modes[] = {"Center Reference", "Brightest Area", "Target Brightness"};
    int mode_idx = static_cast<int>(calib_params_.mode);
    ImGui::Combo("Calibration Mode", &mode_idx, modes, 3);
    calib_params_.mode = static_cast<LSCCalibMode>(mode_idx);

    ImGui::SliderInt("Edge Fill", &calib_params_.edge_fill, 0, 10);
    ImGui::SliderInt("Smooth Kernel", &calib_params_.smooth_kernel, 3, 128);

    if (calib_params_.mode == LSCCalibMode::TARGET_LUMA) {
        ImGui::SliderInt("Target Brightness", reinterpret_cast<int*>(&calib_params_.target_brightness), 0, 255);
    }

    const char* ch_modes[] = {"Independent Channels", "Unified Gain"};
    ImGui::Combo("Channel Mode", &calib_params_.channel_mode, ch_modes, 2);

    if (ImGui::Button("Calibrate")) {
        doCalibrate();
    }

    ImGui::Separator();

    if (!calib_result_.gain_map.empty()) {
        ImGui::Text("Calibration Result");
        ImGui::Text("Uniformity: %.2f%%", calib_result_.uniformity * 100.0);
        ImGui::Text("Center Brightness: %.1f", calib_result_.center_brightness);
        ImGui::Text("Edge Brightness: %.1f", calib_result_.edge_brightness);

        double min_g, max_g;
        cv::minMaxLoc(calib_result_.gain_map, &min_g, &max_g);
        ImGui::Text("Gain Range: [%.3f, %.3f]", min_g, max_g);
    }

    ImGui::Separator();

    if (ImGui::Button("Apply Correction")) {
        doApply();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Gain Map")) doExportGain();
    ImGui::SameLine();
    if (ImGui::Button("Import Gain Map")) doImportGain();

    if (!src_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Flat Field Image");
        flat_viewer_.setImage(src_image_);
        flat_viewer_.render(ImVec2(400, 300));
    }

    if (use_dark_ && !dark_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Dark Frame");
        dark_viewer_.setImage(dark_image_);
        dark_viewer_.render(ImVec2(400, 300));
    }

    if (!corrected_image_.empty()) {
        ImGui::Separator();
        ImGui::Text("Corrected Image");
        result_viewer_.setImage(corrected_image_);
        result_viewer_.render(ImVec2(400, 300));
    }

    ImGui::End();
}

void FlatFieldPanel::doCalibrate() {
    if (src_image_.empty()) {
        status_msg_ = "Please load or capture a flat field image first";
        return;
    }

    if (use_dark_ && !dark_image_.empty()) {
        calib_result_ = FlatFieldCorrector::calibrateWithDark(src_image_, dark_image_, calib_params_);
    } else {
        calib_result_ = FlatFieldCorrector::calibrate(src_image_, calib_params_);
    }
    status_msg_ = "Calibration completed";
}

void FlatFieldPanel::doApply() {
    if (src_image_.empty()) {
        status_msg_ = "Please load or capture an image first";
        return;
    }
    if (calib_result_.gain_map.empty()) {
        status_msg_ = "Please calibrate first";
        return;
    }
    corrected_image_ = FlatFieldCorrector::apply(src_image_, calib_result_.gain_map, calib_params_.edge_fill);
    status_msg_ = "Correction applied";
}

void FlatFieldPanel::doExportGain() {
    if (calib_result_.gain_map.empty()) {
        status_msg_ = "Please calibrate first";
        return;
    }
    FlatFieldCorrector::exportGainMap(calib_result_.gain_map, gain_path_);
    status_msg_ = "Gain map exported";
}

void FlatFieldPanel::doImportGain() {
    calib_result_.gain_map = FlatFieldCorrector::importGainMap(gain_path_);
    if (!calib_result_.gain_map.empty()) {
        status_msg_ = "Gain map imported";
    } else {
        status_msg_ = "Failed to import gain map";
    }
}

} // namespace ui
} // namespace mvtk
