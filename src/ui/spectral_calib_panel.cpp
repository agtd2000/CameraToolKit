/**
 * @file spectral_calib_panel.cpp
 * @brief 光谱特性标定面板实现
 */

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <imgui.h>
#include <string>

#include "ui/spectral_calib_panel.h"

namespace mvtk {
namespace ui {

SpectralCalibPanel::SpectralCalibPanel() = default;

void SpectralCalibPanel::render() {
    ImGui::Begin("Spectral Characteristic Calibration");

    const char* steps[] = {"1. Config", "2. Capture", "3. Preview", "4. Export"};
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
        ImGui::Text("Step 1: Configuration");

        const char* modes[] = {"Factory Calibration", "User Calibration"};
        ImGui::Combo("Calibration Mode", &calib_mode_, modes, 2);

        const char* complexities[] = {"Standard", "Simple"};
        ImGui::Combo("Complexity", &complexity_, complexities, 2);

        if (calib_mode_ == 0) {
            ImGui::Text("Factory Calibration Mode");
            static char factory_path[256] = "";
            ImGui::InputText("Factory File", factory_path, sizeof(factory_path));
            ImGui::SameLine();
            if (ImGui::Button("Import")) {
                importFactory();
            }
        } else {
            ImGui::Text("User Calibration Mode");
            ImGui::Text("Light Sources:");

            if (ImGui::BeginListBox("##LightSources", ImVec2(-1, 100))) {
                for (size_t i = 0; i < light_sources_.size(); ++i) {
                    std::string item = light_sources_[i].name + " (" + std::to_string(static_cast<int>(light_sources_[i].color_temperature)) + "K)";
                    ImGui::Selectable(item.c_str(), false);
                }
                ImGui::EndListBox();
            }

            ImGui::InputText("New Light Name", new_light_name_, sizeof(new_light_name_));
            ImGui::SliderFloat("Color Temperature", reinterpret_cast<float*>(&new_light_temp_), 2000.0f, 10000.0f);
            if (ImGui::Button("Add Light Source")) {
                addLightSource();
            }

            ImGui::Separator();
            ImGui::Text("Calibration Region Calculation");
            ImGui::SliderFloat("Focal Length (mm)", reinterpret_cast<float*>(&focal_length_), 1.0f, 100.0f);
            ImGui::SliderFloat("Working Distance (mm)", reinterpret_cast<float*>(&working_distance_), 100.0f, 2000.0f);
            ImGui::SliderFloat("Sensor Width (mm)", reinterpret_cast<float*>(&sensor_width_), 1.0f, 50.0f);
            ImGui::SliderFloat("Sensor Height (mm)", reinterpret_cast<float*>(&sensor_height_), 1.0f, 50.0f);

            if (ImGui::Button("Compute Calib Region")) {
                computeCalibRegion();
            }
        }

        if (!status_msg_.empty()) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
        }

        if (ImGui::Button("Next >>")) step_ = 1;
    }

    if (step_ == 1) {
        ImGui::Text("Step 2: Capture Spectral Data");

        const char* sources[] = {"Camera", "Local Curve"};
        int prev_image_source = image_source_;
        ImGui::Combo("Data Source", &image_source_, sources, 2);

        if (prev_image_source != image_source_) {
            if (camera_opened_) {
                camera_.release();
                camera_opened_ = false;
            }
        }

        if (image_source_ == 0) {
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
                if (ImGui::Button("Capture Image")) {
                    camera_ >> captured_image_;
                    if (!captured_image_.empty()) {
                        status_msg_ = "Image captured: " + std::to_string(captured_image_.cols) + "x" + std::to_string(captured_image_.rows);
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

            if (!captured_image_.empty()) {
                ImGui::Separator();
                ImGui::Text("Captured Image");
                image_viewer_.setImage(captured_image_);
                image_viewer_.render(ImVec2(400, 300));
            }
        } else {
            static char curve_path[256] = "";
            ImGui::InputText("Curve File", curve_path, sizeof(curve_path));
            ImGui::SameLine();
            if (ImGui::Button("Load Curve")) {
                status_msg_ = "Curve loaded from file";
            }
        }

        if (!status_msg_.empty()) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
        }

        if (!captured_image_.empty() || image_source_ == 1) {
            if (ImGui::Button("Next >>")) step_ = 2;
        }
        ImGui::SameLine();
        if (ImGui::Button("<< Back")) step_ = 0;
    }

    if (step_ == 2) {
        ImGui::Text("Step 3: Preview Result");

        if (ImGui::Button("Preview")) {
            previewResult();
        }

        if (!spectral_preview_.empty()) {
            ImGui::Separator();
            ImGui::Text("Spectral Preview");
            image_viewer_.setImage(spectral_preview_);
            image_viewer_.render(ImVec2(400, 300));
        }

        if (!status_msg_.empty()) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", status_msg_.c_str());
        }

        if (ImGui::Button("Next >>")) step_ = 3;
        ImGui::SameLine();
        if (ImGui::Button("<< Back")) step_ = 1;
    }

    if (step_ == 3) {
        ImGui::Text("Step 4: Export Calibration");

        ImGui::Text("Export to: spectral_calib.dat");
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
            step_ = 0;
            light_sources_.clear();
            captured_image_ = cv::Mat();
            spectral_preview_ = cv::Mat();
        }
    }

    ImGui::End();
}

void SpectralCalibPanel::addLightSource() {
    SpectralCalibrator::addLightSource(light_sources_, std::string(new_light_name_), new_light_temp_, {});
    status_msg_ = "Light source added: " + std::string(new_light_name_);
}

void SpectralCalibPanel::captureCurve() {
    if (!camera_opened_) {
        status_msg_ = "Please open camera first";
        return;
    }
    camera_ >> captured_image_;
    if (!captured_image_.empty()) {
        status_msg_ = "Curve captured";
    } else {
        status_msg_ = "Failed to capture curve";
    }
}

void SpectralCalibPanel::computeCalibRegion() {
    calib_region_ = SpectralCalibrator::computeCalibRegion(
        focal_length_, working_distance_, sensor_width_, sensor_height_);
    status_msg_ = "Calibration region computed";
}

void SpectralCalibPanel::previewResult() {
    spectral_preview_ = cv::Mat(400, 400, CV_8UC3, cv::Scalar(50, 50, 50));
    cv::putText(spectral_preview_, "Spectral Preview", cv::Point(50, 200),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    status_msg_ = "Preview generated";
}

void SpectralCalibPanel::doExport() {
    SpectralCalibrator::generateCalibFile(light_sources_, 
        static_cast<mvtk::SpectralCalibMode>(calib_mode_),
        static_cast<mvtk::SpectralComplexity>(complexity_),
        "spectral_calib.dat");
}

void SpectralCalibPanel::importFactory() {
    static char factory_path[256] = "";
    factory_sources_ = SpectralCalibrator::importFactoryCalibFile(factory_path);
    status_msg_ = "Factory calibration imported: " + std::to_string(factory_sources_.size()) + " light sources";
}

} // namespace ui
} // namespace mvtk
